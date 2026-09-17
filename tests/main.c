// Test runner.
//
// Isolation model: one process per test. The parent enumerates the registry,
// filters, then forks a child per test. The child boots the fixture, runs
// exactly one test, and exits with its failure count. This buys four things a
// single-process suite cannot have:
//
//   * a crash (there are several in this port's history: the door-animation
//     SEGV, the trainer-card SEGV, the BAG list-menu segfault) is attributed to
//     the one test that caused it, and the run continues;
//   * a hang is caught by alarm() and reported instead of wedging the suite;
//   * engine state never leaks between tests -- each child starts from the same
//     freshly booted parent image via copy-on-write;
//   * a test that corrupts memory cannot silently change a later test's result.
//
// Cost: nothing measurable. fork() over a 7.5 MB image is sub-millisecond, and
// the fixture boot happens once per child that needs it.
//
// Usage:
//   firered-tests                      run everything
//   firered-tests --list               names, tags, fixture cost
//   firered-tests --filter <substr>    name or tag substring (repeatable)
//   firered-tests --explain            print observations, never fail
//   firered-tests --timeout <sec>      per-test hang timeout (default 30)

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "internal.h"

#include "constants/maps.h"

// Mach-O section bounds. The linker synthesizes these for any section name; they
// are the whole discovery mechanism, so there is no test list to maintain.
extern const struct FireRedTest firered_tests_start __asm("section$start$__DATA$firered_tests");
extern const struct FireRedTest firered_tests_end __asm("section$end$__DATA$firered_tests");

#define CRASH_EXIT_CODE 99
#define MAX_TESTS 512
#define MAX_FILTERS 32

struct TestEntry
{
    const struct FireRedTest *test;
    enum TestFixture fixture;
};

// The fixture a test needs, taken from its tags. A test with no fixture tag is
// a pure-function test: it never touches the engine and needs no boot.
static enum TestFixture FixtureForTest(const struct FireRedTest *test)
{
    if (strstr(test->tags, "fixture:dex") != NULL)
        return FIXTURE_DEX;
    if (strstr(test->tags, "fixture:lab") != NULL)
        return FIXTURE_OAKS_LAB;
    if (strstr(test->tags, "fixture:bedroom") != NULL)
        return FIXTURE_BEDROOM;
    return FIXTURE_BEDROOM;
}

static bool NeedsEngine(const struct FireRedTest *test)
{
    return strstr(test->tags, "engine") != NULL;
}

// A `slow` test is excluded unless --slow is given. It exists for cases where a
// whole flow is genuinely the contract (a full boot, a multi-scene sequence) and
// would dominate the suite's runtime. Run it with --slow, or by naming it with
// --filter (an explicit filter overrides the exclusion, so addressing one
// directly always works).
static bool IsSlow(const struct FireRedTest *test)
{
    const char *p = test->tags;

    // Token match, not substring: "slowdown-is-fine" must not count as `slow`.
    while ((p = strstr(p, "slow")) != NULL)
    {
        bool leftOk = (p == test->tags) || (p[-1] == ' ');
        bool rightOk = (p[4] == '\0') || (p[4] == ' ');

        if (leftOk && rightOk)
            return true;
        p += 4;
    }
    return false;
}

static const char *FixtureName(enum TestFixture fixture)
{
    switch (fixture)
    {
    case FIXTURE_BEDROOM:  return "bedroom";
    case FIXTURE_OAKS_LAB: return "lab";
    case FIXTURE_DEX:      return "dex";
    }
    return "?";
}

static int Collect(struct TestEntry *entries, int max)
{
    const struct FireRedTest *p;
    int n = 0;

    for (p = &firered_tests_start; p < &firered_tests_end; p++)
    {
        if (n >= max)
            break;
        entries[n].test = p;
        entries[n].fixture = FixtureForTest(p);
        n++;
    }
    return n;
}

static bool Matches(const struct TestEntry *e, char **filters, int filterCount)
{
    int i;

    if (filterCount == 0)
        return true;

    for (i = 0; i < filterCount; i++)
    {
        if (strstr(e->test->name, filters[i]) != NULL)
            return true;
        if (strstr(e->test->tags, filters[i]) != NULL)
            return true;
    }
    return false;
}

// --- child: boot the fixture, run one test ----------------------------------
static void RunOneChild(const struct TestEntry *entry, int timeoutSeconds)
{
    if (NeedsEngine(entry->test))
    {
        // The engine is booted by Harness_RunTest; this never returns.
        Harness_RunTest(entry->test, entry->fixture);
        _exit(Test_FailureCount() > 0 ? 1 : 0);
    }

    // Pure test: no boot, no engine state.
    alarm((unsigned)timeoutSeconds);
    entry->test->fn();
    _exit(Test_FailureCount() > 0 ? 1 : 0);
}

static int RunOne(const struct TestEntry *entry, int timeoutSeconds)
{
    pid_t pid;
    int status;

    printf("  %-58s [%s]", entry->test->name,
           NeedsEngine(entry->test) ? FixtureName(entry->fixture) : "pure");
    fflush(stdout);

    pid = fork();
    if (pid < 0)
    {
        printf(" FORK FAILED (%s)\n", strerror(errno));
        return 1;
    }
    if (pid == 0)
    {
        Harness_InstallCrashHandler();
        alarm((unsigned)timeoutSeconds);
        RunOneChild(entry, timeoutSeconds);
        _exit(0);
    }

    if (waitpid(pid, &status, 0) < 0)
    {
        printf(" WAIT FAILED\n");
        return 1;
    }

    if (WIFSIGNALED(status))
    {
        int sig = WTERMSIG(status);

        if (sig == SIGALRM)
            printf(" TIMEOUT after %ds\n", timeoutSeconds);
        else if (sig == SIGABRT)
            printf(" ABORTED\n");
        else
            printf(" CRASHED (signal %d)\n", sig);
        return 1;
    }

    if (WEXITSTATUS(status) == CRASH_EXIT_CODE)
    {
        printf(" CRASHED (see trace above)\n");
        return 1;
    }

    if (WEXITSTATUS(status) != 0)
    {
        printf(" FAILED\n");
        return 1;
    }

    printf(" ok\n");
    return 0;
}

// --- parent -----------------------------------------------------------------
// Boots the engine through the harness and stops once the fixture is live, with
// no test body. Proves the seam end to end: if this returns 0, the engine
// reached the overworld with the SDL translation units excluded.
static int RunEngineSelfTest(void)
{
    static const struct FireRedTest probe = { "harness/boots-the-engine",
                                          "engine fixture:bedroom", NULL };
    pid_t pid;
    int status;

    printf("  booting the engine through the harness...\n");
    fflush(stdout);

    pid = fork();
    if (pid == 0)
    {
        Harness_InstallCrashHandler();
        alarm(180);
        Harness_RunTest(&probe, FIXTURE_BEDROOM);
        _exit(Test_FailureCount() > 0 ? 1 : 0);
    }

    if (waitpid(pid, &status, 0) < 0)
        return 1;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        printf("  engine self-test FAILED (status %d)\n", status);
        return 1;
    }

    printf("  ok: engine booted, overworld live, no SDL linked\n");
    return 0;
}

int main(int argc, char **argv)
{
    static struct TestEntry entries[MAX_TESTS];
    static char *filters[MAX_FILTERS];
    int entryCount, filterCount = 0, explain = 0, timeout = 30, includeSlow = 0;
    int skippedSlow = 0;
    int i, run = 0, failed = 0, skipped = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--list") == 0)
        {
            int n = Collect(entries, MAX_TESTS);
            int j;

            printf("%d tests\n", n);
            for (j = 0; j < n; j++)
                printf("  %-58s %s%s\n", entries[j].test->name, entries[j].test->tags,
                       IsSlow(entries[j].test) ? "  [slow]" : "");
            return 0;
        }
        else if (strcmp(argv[i], "--filter") == 0 && i + 1 < argc)
        {
            if (filterCount < MAX_FILTERS)
                filters[filterCount++] = argv[++i];
        }
        else if (strcmp(argv[i], "--explain") == 0)
        {
            explain = 1;
            Test_SetExplainMode(1);
        }
        else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc)
        {
            timeout = atoi(argv[++i]);
            if (timeout < 1)
                timeout = 1;
        }
        else if (strcmp(argv[i], "--slow") == 0)
        {
            includeSlow = 1;
        }
        else if (strcmp(argv[i], "--selftest") == 0)
        {
            return RunEngineSelfTest();
        }
        else if (strcmp(argv[i], "--help") == 0)
        {
            printf("usage: firered-tests [--list] [--filter <name|tag>]... "
                   "[--explain] [--slow] [--timeout <sec>] [--selftest]\n");
            return 0;
        }
        else
        {
            fprintf(stderr, "unknown option: %s (try --help)\n", argv[i]);
            return 2;
        }
    }

    entryCount = Collect(entries, MAX_TESTS);

    printf("FireRed native port -- %d tests registered\n", entryCount);
    printf("============================================================\n");

    for (i = 0; i < entryCount; i++)
    {
        if (!Matches(&entries[i], filters, filterCount))
        {
            skipped++;
            continue;
        }
        // `slow` is always excluded unless --slow is given. Making this
        // independent of --filter keeps it predictable: filtering a broad tag
        // (say "engine") must not silently pull in long-running tests.
        if (IsSlow(entries[i].test) && !includeSlow)
        {
            skippedSlow++;
            continue;
        }
        run++;
        failed += RunOne(&entries[i], timeout);
    }

    printf("============================================================\n");
    if (explain)
        printf("explain mode: %d tests run, failures reported but not counted\n", run);
    else
        printf("%d/%d passed", run - failed, run);
    if (skipped > 0)
        printf(" (%d filtered out)", skipped);
    if (skippedSlow > 0)
        printf(" (%d slow skipped; --slow to include)", skippedSlow);
    printf("\n");

    return failed > 0 ? 1 : 0;
}
