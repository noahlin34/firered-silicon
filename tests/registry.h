#ifndef GUARD_TESTS_REGISTRY_H
#define GUARD_TESTS_REGISTRY_H

// Unit test registry for the native port.
//
// A test is a function in a .c file under tests/. It registers itself through
// FIRERED_TEST, which places a descriptor into the Mach-O section __DATA,firered_tests.
// The runner walks that section's bounds, so adding a test is adding a file --
// there is no list to edit, and nothing in src/ knows the tests exist.
//
// Tests link against the SAME engine objects as the game binary; only the three
// SDL-facing translation units (src/platform/{sdl2,dev_panel,main}.c) are
// replaced by the harness. See tests/platform_host.c for the seam.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// NOTE: every libc header a test needs must be included BEFORE global.h.
// include/global.h:81 defines a function-like `abs(x)` macro, so any system
// header pulled in afterwards that declares `abs` fails to parse:
//
//     /usr/include/_stdlib.h:148: error: expected identifier or '('
//
// which is confusing because the error names libc, not the engine. Include
// order here is therefore deliberate, and tests must not reorder it.

// The engine's fixed-width types (u8/u16/s8/bool8/...). Tests deliberately use
// the real headers rather than restating prototypes: a hand-written extern with
// the wrong type compiles and links silently, which is the failure class
// AGENTS.md fixes #57/#58 document.
#include "global.h"

typedef void (*TestFn)(void);

struct FireRedTest
{
    const char *name;   // human-readable, unique; what --filter matches
    const char *tags;   // space-separated: "bios", "maps", "npc:daisy", "slow"
    TestFn fn;
};

// The suite is discovered by walking this section at runtime. `sizeof` must be
// a multiple of the section's alignment or the bounds arithmetic desynchronises
// (an early prototype without aligned(8) segfaulted here).
#define FIRERED_TEST_SECTION "__DATA,firered_tests"
#define FIRERED_TEST_ALIGN 8

_Static_assert(sizeof(struct FireRedTest) % FIRERED_TEST_ALIGN == 0,
               "struct FireRedTest must stay a multiple of the section alignment");

#define FIRERED_TEST(testName, testTags, fnName)                                  \
    static void fnName(void);                                                 \
    __attribute__((section(FIRERED_TEST_SECTION), used, aligned(FIRERED_TEST_ALIGN)))  \
    static const struct FireRedTest firered_test_##fnName = { testName, testTags, fnName }; \
    static void fnName(void)

// --- failures --------------------------------------------------------------
// Failures are recorded and printed, not fatal: one broken assertion should not
// hide the assertions after it. A crash or hang is handled by the runner, which
// runs each test in its own process.

void Test_Fail(const char *file, int line, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

// --explain turns the suite into a probe: assertions print what they observed
// and never fail, which is the interactive mode used while investigating a bug.
int Test_ExplainMode(void);
int Test_FailureCount(void);

#define TEST_FAIL(...)                 Test_Fail(__FILE__, __LINE__, __VA_ARGS__)
#define TEST_TRUE(expr)                Test_Check(__FILE__, __LINE__, #expr, (expr) ? 1 : 0)
#define TEST_FALSE(expr)               Test_Check(__FILE__, __LINE__, #expr, (expr) ? 0 : 1)
#define TEST_EQ(a, b)                  Test_CheckEq(__FILE__, __LINE__, #a, #b, \
                                                    (long long)(a), (long long)(b))
#define TEST_NE(a, b)                  Test_CheckNe(__FILE__, __LINE__, #a, #b, \
                                                    (long long)(a), (long long)(b))
#define TEST_GE(a, b)                  Test_CheckGe(__FILE__, __LINE__, #a, #b, \
                                                    (long long)(a), (long long)(b))
#define TEST_PTR_EQ(a, b)              Test_CheckPtr(__FILE__, __LINE__, #a, #b, \
                                                     (const void *)(a), (const void *)(b))
#define TEST_PTR_NOT_NULL(p)           Test_CheckNotNull(__FILE__, __LINE__, #p, \
                                                         (const void *)(p))
#define TEST_STR_EQ(a, b)              Test_CheckStr(__FILE__, __LINE__, #a, #b, (a), (b))
#define TEST_MEM_EQ(a, b, n)           Test_CheckMem(__FILE__, __LINE__, #a, #b, (a), (b), (n))

void Test_Check(const char *file, int line, const char *expr, int ok);
void Test_CheckEq(const char *file, int line, const char *ea, const char *eb,
                  long long a, long long b);
void Test_CheckNe(const char *file, int line, const char *ea, const char *eb,
                  long long a, long long b);
void Test_CheckGe(const char *file, int line, const char *ea, const char *eb,
                  long long a, long long b);
void Test_CheckPtr(const char *file, int line, const char *ea, const char *eb,
                   const void *a, const void *b);
void Test_CheckNotNull(const char *file, int line, const char *expr, const void *p);
void Test_CheckStr(const char *file, int line, const char *ea, const char *eb,
                   const char *a, const char *b);
void Test_CheckMem(const char *file, int line, const char *ea, const char *eb,
                   const void *a, const void *b, size_t n);

// --- engine control (tests/frame.c, tests/input.c) --------------------------
// Which booted state the fixture should be in before the test body runs.
enum TestFixture
{
    FIXTURE_BEDROOM,    // fresh save in PalletTown_PlayersHouse_2F (--skip-intro)
    FIXTURE_OAKS_LAB,   // starter received, first rival battle won (--post-rival)
};

void Test_RequireFixture(enum TestFixture fixture);  // call first, in the body
void Test_RunFrames(int frames);
void Test_RunUntilIdle(int maxFrames);   // until the field is unlocked and scripts stop
void Test_WarpTo(u8 mapGroup, u8 mapNum, s8 x, s8 y);
void Test_RunFramesToWarp(void);         // run until the warp's fade + load finish

// --- input (tests/input.c) --------------------------------------------------
// Buttons are the engine's A_BUTTON/B_BUTTON/... masks. Presses are scheduled as
// press/release cadences, because engine code (message boxes, battle controller
// states) waits on JOY_NEW and a held key never produces an edge.
void Test_Press(u16 button);            // one press/release cycle, 4 frames on, 8 off
void Test_PressRepeated(u16 button, int times);
void Test_Hold(u16 button, int frames);
void Test_ReleaseAll(void);
// Press `button` once per cycle until the engine goes idle or maxFrames elapse.
// This is the "advance dialogue" primitive.
void Test_PressUntilIdle(u16 button, int maxFrames);

// --- observation (tests/observe.c) ------------------------------------------
u16 Test_Var(u16 varId);
void Test_SetVar(u16 varId, u16 value);
bool8 Test_Flag(u16 flagId);
void Test_SetFlag(u16 flagId, bool8 value);
bool8 Test_HasItem(u16 itemId, u16 count);
u32 Test_Money(void);
u16 Test_LastTalked(void);
bool8 Test_FieldLocked(void);

const char *Test_StringVar4(void);       // gStringVar4, charmap-decoded
const char *Test_Decode(const u8 *encoded);
const char *Test_LastDecoded(void);      // the most recent Test_Decode result

u16 Test_MapGroup(void);
u16 Test_MapNum(void);
s16 Test_PlayerX(void);
s16 Test_PlayerY(void);
const struct ObjectEvent *Test_PlayerObject(void);
u8 Test_PlayerFacing(void);
bool8 Test_PlayerIsMoving(void);
bool8 Test_InOverworld(void);
int Test_PartyCount(void);
u16 Test_PartySpecies(int index);
u8 Test_PartyLevel(int index);
u16 Test_PartyHP(int index);
u16 Test_PartyMaxHP(int index);
void Test_SetPartyHP(int index, u16 hp);   // damage a mon to give a heal work to do

// --- script inspection (tests/script_util.c) --------------------------------
// Native scripts are bytecode with pointer operands stored as indices into
// gNativeScriptPtrs (fix #23). These helpers resolve that, so tests assert what
// a script DOES rather than how it is encoded.
#define TEST_SCRIPT_SCAN_LIMIT 512

const u8 *Test_ScriptPtrAt(const u8 *script, int offset);
u16 Test_ScriptHalfwordAt(const u8 *script, int offset);
u32 Test_ScriptWordAt(const u8 *script, int offset);
bool8 Test_ScriptContains(const u8 *script, const u8 *pattern, int patternLen);
bool8 Test_ScriptStartsWith(const u8 *script, u8 opcode);
int Test_ScriptFindOpcode(const u8 *script, u8 opcode);
const char *Test_ScriptDump(const u8 *script, int maxBytes);

// --- structural pixel checks (tests/util.c) ---------------------------------
// The PPU is a pure function of engine memory and takes the framebuffer as a
// parameter, so tests render into their own buffer. Prefer engine state; use
// these only for behaviour that exists solely as pixels.
void Test_RenderFrame(void);
const u16 *Test_Framebuffer(void);
int Test_RegionIsUniform(int x, int y, int w, int h);
int Test_DistinctColors(int x, int y, int w, int h);
int Test_RegionDiffers(int x, int y, int w, int h, const u16 *other);
void Test_SaveScreenshot(const char *filename);

#endif // GUARD_TESTS_REGISTRY_H
