// Frame driving: gives a test body control between engine frames.
//
// The engine's frame loop calls Platform_RenderAndPresent() exactly once per
// frame (src/main.c's WaitForVBlank). The harness owns that symbol, so the tick
// below is where a test regains control -- no engine edit is needed and nothing
// in src/ knows tests exist.
//
// Control transfer uses ucontext: the engine keeps the main stack, the test body
// runs on its own stack. A test that wants N frames yields; the engine runs the
// frames; the next tick resumes the test. Verified working before adoption.
//
// Each test runs in its own process (see tests/main.c), so this context dance is
// entirely intra-process: the child forks, arms the fixture, boots the engine,
// runs exactly one test, and exits. Tests never share mutated engine state.

#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>

#include "internal.h"

#include "constants/maps.h"
#include "field_fadetransition.h"
#include "fieldmap.h"
#include "main.h"
#include "overworld.h"
#include "platform/platform.h"
#include "script.h"

// The engine's boot-test frame counter. Zero means "no frame limit", which is
// the state the harness requires: the frame-limit path saves a screenshot and
// calls exit(0), which a test must never hit.
extern int gEngineMaxFrames;

ucontext_t gTestCtx;
ucontext_t gEngineCtx;
jmp_buf gEngineExit;
bool gTestDone;
int gFrameCount;

static char sTestStack[256 * 1024];
static const struct FireRedTest *sCurrentTest;
static enum TestFixture sFixture;
static bool sArmTest;          // hand control to the test once the fixture is live
static bool sFixtureReady;
static bool sEngineRunning;   // set only while Harness_RunTest drives AgbMain
static int sWantFrame = INT_MAX;
static int sFixtureStartFrame;

// --- fixture readiness ------------------------------------------------------
// A boot that never reaches the fixture is the harness's own failure mode, so it
// is reported with the engine's actual state rather than left to a timeout.
#define FIXTURE_READY_DEADLINE 20000

static void FixtureNeverReady(void)
{
    const struct WarpData *loc = (gSaveBlock1Ptr != NULL) ? &gSaveBlock1Ptr->location : NULL;

    printf("    FAIL fixture '%s' never became ready within %d frames\n",
           sFixture == FIXTURE_OAKS_LAB ? "lab" : "bedroom", FIXTURE_READY_DEADLINE);
    printf("         callback1=%s callback2=%p location=%s",
           gMain.callback1 == CB1_Overworld ? "CB1_Overworld" : "(not overworld)",
           (void *)gMain.callback2,
           loc != NULL ? "" : "(gSaveBlock1Ptr is NULL)");
    if (loc != NULL)
        printf(" group=%d num=%d", loc->mapGroup, loc->mapNum);
    printf("\n");

    Test_Fail(__FILE__, __LINE__, "fixture never became ready");
    gTestDone = true;
    longjmp(gEngineExit, 1);
}

// "Ready" means the dev boot has fully finished: the overworld is running, the
// destination map load and its fade are done, and the field accepts input.
//
// Checking the map alone is not enough -- gSaveBlock1Ptr->location is updated as
// soon as the load begins, while the screen is still fading and the player's
// object event has not been placed yet. A test that started there observed
// locked controls and stale coordinates (this was caught by the fixture tests
// themselves). The unlock is the engine's own "the warp is over" signal, the same
// one Test_RunFramesToWarp waits on.
static bool FixtureIsReady(void)
{
    const struct WarpData *loc;

    if (gTestDone)
        return true;
    if (gMain.callback1 != CB1_Overworld)
        return false;
    if (ArePlayerFieldControlsLocked() || ScriptContext_IsEnabled())
        return false;

    // The player's current map lives in SaveBlock1.location -- struct MapHeader
    // carries no map identity (only the layout id).
    if (gSaveBlock1Ptr == NULL)
        return false;
    loc = &gSaveBlock1Ptr->location;

    switch (sFixture)
    {
    case FIXTURE_BEDROOM:
        return loc->mapGroup == MAP_GROUP(MAP_PALLET_TOWN_PLAYERS_HOUSE_2F)
            && loc->mapNum == MAP_NUM(MAP_PALLET_TOWN_PLAYERS_HOUSE_2F);
    case FIXTURE_OAKS_LAB:
        return loc->mapGroup == MAP_GROUP(MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB)
            && loc->mapNum == MAP_NUM(MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB);
    }
    return false;
}

// Runs on the test's own stack. Starts only when the engine first yields to it.
static void Test_Trampoline(void)
{
    // A NULL body is the boot-only probe: reaching here at all means the fixture
    // finished loading, which is what --selftest asserts.
    if (sCurrentTest->fn != NULL)
        sCurrentTest->fn();
    gTestDone = true;
    swapcontext(&gTestCtx, &gEngineCtx);   // final yield; the tick unwinds
}

// The test's yield point: hand the engine `frames` frames, then resume here.
void Test_YieldToEngine(int frames)
{
    // A test that drives the engine but was not tagged `engine` never gets here:
    // the runner would not have booted anything, and swapping to a context that
    // is not running would either hang or corrupt the stack. This is a loud stop
    // rather than a mystery timeout -- forgetting the tag is an easy mistake and
    // the symptom would otherwise be inexplicable.
    if (!sEngineRunning)
    {
        TEST_FAIL("this test drives the engine but lacks the `engine` tag "
                  "(add it, e.g. \"engine fixture:bedroom\")");
        return;
    }

    if (frames < 1)
        frames = 1;
    sWantFrame = gFrameCount + frames;
    swapcontext(&gTestCtx, &gEngineCtx);
}

// Called from Platform_RenderAndPresent -- i.e. once per engine frame.
void Harness_FrameTick(void)
{
    Input_Tick();
    gFrameCount++;

    if (gTestDone)
        longjmp(gEngineExit, 1);

    if (sArmTest && !sFixtureReady && FixtureIsReady())
    {
        sFixtureReady = true;
        sFixtureStartFrame = gFrameCount;
        sWantFrame = gFrameCount;   // start the test on this very tick
    }

    if (!sFixtureReady)
    {
        // A fixture that never becomes ready would otherwise present as a bare
        // timeout with no explanation, which is expensive to diagnose. Say what
        // the engine is actually doing instead.
        if (gFrameCount == FIXTURE_READY_DEADLINE)
            FixtureNeverReady();
        return;
    }

    if (gFrameCount >= sWantFrame)
    {
        swapcontext(&gEngineCtx, &gTestCtx);
        if (gTestDone)
            longjmp(gEngineExit, 1);   // unwind the engine now, not next frame
    }
}

// Boot the engine and run one test. Never returns normally: either the test
// finishes (engine unwound, exit code = failure count) or the process is killed
// by a signal the runner reports.
void Harness_RunTest(const struct FireRedTest *test, enum TestFixture fixture)
{
    sCurrentTest = test;
    sFixture = fixture;
    gTestDone = false;
    sFixtureReady = false;
    sArmTest = true;
    sEngineRunning = true;
    sWantFrame = INT_MAX;

    // The fixture is the existing dev boot, reused rather than reinvented: it
    // builds a fresh save and hands off to CB2_NewGame (--skip-intro), or goes
    // one scene further (--post-rival). AgbMain calls Platform_DevBootNewGame()
    // when gPlatformSkipIntro is set, which is how the game's own flags work --
    // so the harness sets the same flags instead of adding a second boot path.
    gPlatformSkipIntro = true;
    gPlatformSkipStory = (fixture == FIXTURE_OAKS_LAB);

    // The engine's own scripted input and screenshot/exit path are gated on
    // gEngineMaxFrames; leaving it at 0 means no synthetic input perturbs the
    // fixture and the frame-limit exit is unreachable. The harness owns frame
    // driving through Platform_RenderAndPresent instead.
    gEngineMaxFrames = 0;

    getcontext(&gTestCtx);
    gTestCtx.uc_stack.ss_sp = sTestStack;
    gTestCtx.uc_stack.ss_size = sizeof(sTestStack);
    gTestCtx.uc_link = NULL;
    makecontext(&gTestCtx, Test_Trampoline, 0);

    if (setjmp(gEngineExit) != 0)
        return;   // the test finished and the engine was unwound

    Input_Reset();

    extern void AgbMain(void);
    AgbMain();    // runs until the test signals completion
}

void Test_RequireFixture(enum TestFixture fixture)
{
    // The runner boots the fixture before the body starts; asking for a different
    // one here would silently test against the wrong state, so it is a hard stop.
    if (fixture != sFixture)
        TEST_FAIL("test requires a different fixture than the runner booted");
}

// --- public control primitives ---------------------------------------------
void Test_RunFrames(int frames)
{
    Test_YieldToEngine(frames);
}

void Test_RunUntilIdle(int maxFrames)
{
    int waited = 0;

    while (waited < maxFrames)
    {
        if (!ArePlayerFieldControlsLocked() && !ScriptContext_IsEnabled())
            return;
        Test_YieldToEngine(1);
        waited++;
    }
}

void Test_WarpTo(u8 mapGroup, u8 mapNum, s8 x, s8 y)
{
    // Exactly what ScrCmd_warp applies, from the test instead of from a script.
    SetWarpDestination(mapGroup, mapNum, -1, x, y);
    DoWarp();
    ResetInitialPlayerAvatarState();
}

void Test_RunFramesToWarp(void)
{
    int waited = 0;

    // A warp fades, loads the map, then hands off to CB2_Overworld.
    while (waited < 600)
    {
        Test_YieldToEngine(1);
        waited++;
        if (gMain.callback1 == CB1_Overworld && !ArePlayerFieldControlsLocked())
            return;
    }
}

int Test_ElapsedFrames(void) { return gFrameCount - sFixtureStartFrame; }
