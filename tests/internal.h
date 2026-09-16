#ifndef GUARD_TESTS_INTERNAL_H
#define GUARD_TESTS_INTERNAL_H

// Shared between the harness translation units (platform_host.c, frame.c,
// input.c, observe.c, assertions.c, util.c). Not included by tests.

// ucontext.h only declares its types when _XOPEN_SOURCE is defined *before the
// first system header*, which an include order inside this file cannot guarantee
// (frame.c pulls in <limits.h> and <setjmp.h> first). The build therefore passes
// it: see TEST_CFLAGS in Makefile.native. Failing here rather than silently
// compiling a header nobody can use.
#ifndef _XOPEN_SOURCE
#error "tests require -D_XOPEN_SOURCE=700 (TEST_CFLAGS in Makefile.native)"
#endif

#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>

// swapcontext is deprecated on macOS but remains functional, and it is the only
// portable way to give a test body control back to the engine's frame loop
// without restructuring AgbMain. Verified working before adoption.
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// Every libc header must precede global.h: it defines a function-like `abs(x)`
// macro (include/global.h:81), so a system header that declares `abs` fails to
// parse if pulled in afterwards. Same note as in registry.h.
#include "global.h"
#include "registry.h"

// --- engine frame driving (frame.c) ----------------------------------------
extern ucontext_t gTestCtx;     // the running test body
extern ucontext_t gEngineCtx;   // the engine's main stack, captured at yield
extern jmp_buf gEngineExit;     // unwinds out of AgbMain when the test is done
extern bool gTestDone;
extern int gFrameCount;

void Test_YieldToEngine(int frames);   // test body -> engine
void Harness_RunTest(struct FireRedTest const *test, enum TestFixture fixture);
int Test_ElapsedFrames(void);

// Installs the crash/abort reporting handler. Called in the child before the
// test runs, so a faulting test is attributed and symbolized.
void Harness_InstallCrashHandler(void);

// Called by the harness's Platform_RenderAndPresent once per engine frame.
void Harness_FrameTick(void);

// --- input (input.c) -------------------------------------------------------
void Input_Reset(void);
void Input_ApplyToKeys(void);   // writes REG_KEYINPUT for this frame
void Input_Tick(void);          // advances the scheduled input

// --- assertions (assertions.c) ---------------------------------------------
void Test_SetExplainMode(int on);

#endif // GUARD_TESTS_INTERNAL_H
