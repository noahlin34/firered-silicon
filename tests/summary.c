// The POKéMON summary screen (START menu -> POKéMON -> a mon -> SUMMARY).
//
// Picking SUMMARY did nothing: the party menu's CursorCB_Summary sets
// sPartyMenuInternal->exitCallback to CB2_ShowPokemonSummaryScreen and closes
// the menu, and that callback called ShowPokemonSummaryScreen -- which was an
// inert stub in src/platform/battle_peripheral_stubs.c that returned without
// installing a callback of its own. The menu faded out, the stub ran, and
// nothing ever drew again: a black screen with the engine still ticking.
//
// src/pokemon_summary_screen.c is linked now (PREPROC_SRCS), along with
// src/mon_markings.c for the marking sprite, so the real screen owns the frame.
//
// These tests drive the real user path and assert what the player observes: the
// screen draws, its pages are navigable, and B hands the frame back to the party
// menu it came from -- a screen that cannot be left is as broken as one that
// never opens.

#include "registry.h"

#include "constants/maps.h"
#include "constants/party_menu.h"
#include "gba/defines.h"
#include "main.h"
#include "overworld.h"
#include "party_menu.h"
#include "pokemon.h"

// START -> POKéMON -> the first mon's action window -> SUMMARY.
//
// On the lab fixture FLAG_SYS_POKEDEX_GET is clear, so POKéMON is the first
// START-menu entry and A selects it without moving the cursor. The party menu
// opens with PARTY_ACTION_CHOOSE_MON, so A on a mon opens its action window
// (Task_TryCreateSelectionWindow), whose first entry is always SUMMARY
// (SetPartyMonFieldSelectionActions appends CURSOR_OPTION_SUMMARY first).
static void OpenSummaryFromStartMenu(void)
{
    Test_Press(START_BUTTON);
    Test_RunFrames(40);
    Test_Press(A_BUTTON);           // POKéMON (first entry)
    Test_RunFrames(240);            // fade out, load, fade in
    Test_Press(A_BUTTON);           // choose the first mon -> action window
    Test_RunFrames(60);
    Test_Press(A_BUTTON);           // SUMMARY (first action)
    Test_RunFrames(300);            // fade out, set up, fade in
}

FIRERED_TEST("summary/party menu opens the summary screen and B returns",
             "engine fixture:lab menu:summary", summary_open_and_close)
{
    static u16 summaryPage[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    MainCallback overworldCB = gMain.callback2;

    Test_RequireFixture(FIXTURE_OAKS_LAB);
    TEST_GE(Test_PartyCount(), 1);

    OpenSummaryFromStartMenu();

    // The screen owns the frame. Before this it did not: the stub returned
    // without installing anything, so callback2 was left pointing at the
    // party menu's post-close callback forever.
    TEST_NE((const void *)gMain.callback2, (const void *)overworldCB);

    // ...and it drew its own screen. Pre-fix this region rendered as a single
    // flat colour over the whole display, because nothing had drawn since the
    // menu faded out.
    Test_RenderFrame();
    TEST_GE(Test_DistinctColors(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT), 8);
    memcpy(summaryPage, Test_Framebuffer(), sizeof(summaryPage));

    // The mon page: the picture is drawn on the info page, so the picture area
    // carries more than its background. Asserting on the whole frame alone would
    // pass on any loaded BG.
    TEST_GE(Test_DistinctColors(150, 20, 70, 70), 4);

    // B leaves the summary back to the party menu it was opened from
    // (CB2_ReturnToPartyMenuFromSummaryScreen re-inits with the same exit
    // callback). That is the engine's own path, so the assertion is that a
    // different screen is up and it is not the black field.
    Test_Press(B_BUTTON);
    Test_RunFrames(300);
    Test_RenderFrame();
    TEST_GE(Test_RegionDiffers(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, summaryPage), 2000);

    // Back out to the field. Each screen in this stack holds the field locked
    // while it is up, and the exit callbacks chain (the summary returns to the
    // party menu, which returns to CB2_ReturnToFieldWithOpenMenu, which leaves
    // the START menu open), so press B until no screen holds the field rather
    // than counting screens. gMain.callback1 stays CB1_Overworld throughout, so
    // the lock is the only signal that distinguishes "menu" from "field".
    for (int i = 0; i < 8 && Test_FieldLocked(); i++)
    {
        Test_Press(B_BUTTON);
        Test_RunFrames(120);
    }
    Test_RunUntilIdle(900);
    TEST_TRUE(Test_InOverworld());
    TEST_FALSE(Test_FieldLocked());
}

// The summary is a multi-page screen, so "it opened" and "it works" are
// different claims: RIGHT flips from the info page to the skills page, and each
// page is its own layout. The pre-fix black screen has no pages at all.
FIRERED_TEST("summary/pages flip between info and skills",
             "engine fixture:lab menu:summary", summary_pages_flip)
{
    static u16 infoPage[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    static u16 skillsPage[DISPLAY_WIDTH * DISPLAY_HEIGHT];

    Test_RequireFixture(FIXTURE_OAKS_LAB);

    OpenSummaryFromStartMenu();

    Test_RenderFrame();
    TEST_GE(Test_DistinctColors(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT), 8);
    memcpy(infoPage, Test_Framebuffer(), sizeof(infoPage));

    // RIGHT flips the page; the flip is an animation, so allow it to settle.
    Test_Press(DPAD_RIGHT);
    Test_RunFrames(300);
    Test_RenderFrame();

    // The skills page draws stat bars and different header text, so it differs
    // from the info page across most of the frame.
    TEST_GE(Test_RegionDiffers(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, infoPage), 2000);
    memcpy(skillsPage, Test_Framebuffer(), sizeof(skillsPage));

    // LEFT flips back, to the same page the flip started from.
    Test_Press(DPAD_LEFT);
    Test_RunFrames(300);
    Test_RenderFrame();
    TEST_EQ(Test_RegionDiffers(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, infoPage), 0);

    // And the screen is still leavable after flipping (same unwind as above).
    for (int i = 0; i < 8 && Test_FieldLocked(); i++)
    {
        Test_Press(B_BUTTON);
        Test_RunFrames(120);
    }
    Test_RunUntilIdle(900);
    TEST_FALSE(Test_FieldLocked());
}
