// Assertion reporting. Failures are recorded and printed with both operand
// values, then execution continues -- a broken assertion early in a test must
// not hide the ones after it.
//
// --explain turns the suite into a probe: every assertion prints what it observed
// and nothing fails. That is what lets one file serve as the reproduction while
// investigating a bug and as the regression test afterwards, so ALL checks report
// in explain mode, not just the boolean ones.

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "registry.h"

static int sFailures;
static int sExplain;

int Test_ExplainMode(void) { return sExplain; }
int Test_FailureCount(void) { return sFailures; }

void Test_SetExplainMode(int on) { sExplain = on; }

void Test_Fail(const char *file, int line, const char *fmt, ...)
{
    va_list args;

    printf("    FAIL %s:%d: ", file, line);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");

    if (!sExplain)
        sFailures++;
}

// Explain-mode observation line, prefixed so it is greppable and visually
// distinct from a failure.
static void Observe(const char *file, int line, const char *fmt, ...)
{
    va_list args;

    printf("      %s:%d: ", file, line);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

void Test_Check(const char *file, int line, const char *expr, int ok)
{
    if (sExplain)
        Observe(file, line, "%s -> %s", expr, ok ? "true" : "false");
    if (ok)
        return;
    Test_Fail(file, line, "expected true: %s", expr);
}

void Test_CheckEq(const char *file, int line, const char *ea, const char *eb,
                  long long a, long long b)
{
    if (sExplain)
        Observe(file, line, "%s == %s  (%lld, %lld)", ea, eb, a, b);
    if (a == b)
        return;
    Test_Fail(file, line, "%s == %s  (got %lld vs %lld)", ea, eb, a, b);
}

void Test_CheckNe(const char *file, int line, const char *ea, const char *eb,
                  long long a, long long b)
{
    if (sExplain)
        Observe(file, line, "%s != %s  (%lld, %lld)", ea, eb, a, b);
    if (a != b)
        return;
    Test_Fail(file, line, "%s != %s  (both %lld)", ea, eb, a);
}

void Test_CheckGe(const char *file, int line, const char *ea, const char *eb,
                  long long a, long long b)
{
    if (sExplain)
        Observe(file, line, "%s >= %s  (%lld, %lld)", ea, eb, a, b);
    if (a >= b)
        return;
    Test_Fail(file, line, "%s >= %s  (got %lld vs %lld)", ea, eb, a, b);
}

void Test_CheckPtr(const char *file, int line, const char *ea, const char *eb,
                   const void *a, const void *b)
{
    if (sExplain)
        Observe(file, line, "%s == %s  (%p, %p)", ea, eb, a, b);
    if (a == b)
        return;
    Test_Fail(file, line, "%s == %s  (got %p vs %p)", ea, eb, a, b);
}

void Test_CheckNotNull(const char *file, int line, const char *expr, const void *p)
{
    if (sExplain)
        Observe(file, line, "%s -> %p", expr, p);
    if (p != NULL)
        return;
    Test_Fail(file, line, "expected non-NULL: %s", expr);
}

void Test_CheckStr(const char *file, int line, const char *ea, const char *eb,
                   const char *a, const char *b)
{
    if (sExplain)
        Observe(file, line, "%s == %s  (\"%s\", \"%s\")", ea, eb,
                a ? a : "(null)", b ? b : "(null)");
    if (a != NULL && b != NULL && strcmp(a, b) == 0)
        return;
    Test_Fail(file, line, "%s == %s\n           got: \"%s\"\n      expected: \"%s\"",
              ea, eb, a ? a : "(null)", b ? b : "(null)");
}

void Test_CheckMem(const char *file, int line, const char *ea, const char *eb,
                   const void *a, const void *b, size_t n)
{
    const u8 *pa = a;
    const u8 *pb = b;
    size_t i;

    if (sExplain)
        Observe(file, line, "%s == %s over %zu bytes", ea, eb, n);

    if (a != NULL && b != NULL && memcmp(a, b, n) == 0)
        return;

    for (i = 0; i < n; i++)
    {
        if (pa == NULL || pb == NULL || pa[i] != pb[i])
        {
            Test_Fail(file, line, "%s == %s  (first difference at byte %zu: %02X vs %02X)",
                      ea, eb, i,
                      pa ? pa[i] : 0, pb ? pb[i] : 0);
            return;
        }
    }
    Test_Fail(file, line, "%s == %s over %zu bytes", ea, eb, n);
}
