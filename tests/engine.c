// Engine fixture tests: the harness boots the engine and a test observes it.
//
// These prove the seam end to end before any behaviour is asserted on top:
// if the engine reaches the overworld with the SDL translation units excluded
// from the link, then every control and observation primitive works.

#include "internal.h"

#include "constants/maps.h"
#include "main.h"
#include "overworld.h"
#include "script.h"

FIRERED_TEST("engine/boots into the bedroom and accepts input", "engine fixture:bedroom",
         engine_boots_bedroom)
{
    Test_RequireFixture(FIXTURE_BEDROOM);

    // The runner only starts this body once the fixture is live, so these are
    // assertions about the boot, not about timing.
    TEST_EQ(Test_InOverworld(), TRUE);
    TEST_EQ(Test_MapGroup(), MAP_GROUP(MAP_PALLET_TOWN_PLAYERS_HOUSE_2F));
    TEST_EQ(Test_MapNum(), MAP_NUM(MAP_PALLET_TOWN_PLAYERS_HOUSE_2F));
    TEST_EQ(Test_FieldLocked(), FALSE);

    // The player spawned facing north at the bedroom's start position.
    TEST_EQ(Test_PlayerX(), 6);
    TEST_EQ(Test_PlayerY(), 6);
}

// Movement proves input injection reaches the engine through the same path SDL
// uses (REG_KEYINPUT -> ReadKeys -> gMain.heldKeys), and that the field is not
// locked by the dev boot.
FIRERED_TEST("engine/holding a direction moves the player one tile", "engine fixture:bedroom",
         engine_player_walks)
{
    s16 startX, startY;

    Test_RequireFixture(FIXTURE_BEDROOM);

    startX = Test_PlayerX();
    startY = Test_PlayerY();

    // Walk south: the bedroom's start position has floor in that direction.
    Test_Hold(DPAD_DOWN, 60);
    Test_RunFrames(70);

    TEST_EQ(Test_PlayerY() > startY, TRUE);
    TEST_EQ(Test_PlayerX(), startX);   // straight line, no drift
}

// A warp is the primitive every interaction test needs, and it exercises the
// real fade/map-load/hand-off path (SetWarpDestination + DoWarp +
// ResetInitialPlayerAvatarState, exactly what ScrCmd_warp applies).
FIRERED_TEST("engine/warping to a map loads it and unlocks the field",
         "engine fixture:bedroom", engine_warp)
{
    Test_RequireFixture(FIXTURE_BEDROOM);

    Test_WarpTo(MAP_GROUP(MAP_PALLET_TOWN_PLAYERS_HOUSE_1F),
                MAP_NUM(MAP_PALLET_TOWN_PLAYERS_HOUSE_1F), 5, 8);
    Test_RunFramesToWarp();

    TEST_EQ(Test_MapGroup(), MAP_GROUP(MAP_PALLET_TOWN_PLAYERS_HOUSE_1F));
    TEST_EQ(Test_MapNum(), MAP_NUM(MAP_PALLET_TOWN_PLAYERS_HOUSE_1F));
    TEST_EQ(Test_InOverworld(), TRUE);
    TEST_EQ(Test_FieldLocked(), FALSE);
}

// Rendering is available on demand, into the harness's own buffer, with no
// window and no presentation. The assertion is structural on purpose: pinning
// exact pixels would break on legitimate compositor changes.
FIRERED_TEST("engine/renders a non-uniform frame without a window", "engine fixture:bedroom",
         engine_renders)
{
    Test_RequireFixture(FIXTURE_BEDROOM);

    Test_RenderFrame();

    // The bedroom is a drawn map, so the frame cannot be one flat colour.
    TEST_GE(Test_DistinctColors(0, 0, 240, 160), 4);
}
