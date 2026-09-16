// Oak's interception cutscene on Pallet Town's northern exit.
//
// tests/maps_pallet.c asserts the *wiring* of the two coord events (they point at
// their real scripts and start with lockall). This file asserts the *behaviour*:
// stand south of an exit tile, hold north, and Oak stops the player, shouts his
// line, walks up, and escorts him into the lab.
//
// Why behaviour and not just wiring: a coord event whose script is NULL is simply
// WALKABLE (AGENTS.md fix #27), so the failure mode -- "nothing happens and the
// player strolls out of town" -- is invisible to a pure test that only reads the
// generated tables. The wiring test would still pass if the trigger fired and the
// closure then did nothing useful.
//
// Both exit tiles are covered, because they are two different code paths: (12,1)
// sets VAR_TEMP_1 = 0 and calls the left-hand Oak approach and escort, (13,1) sets
// it to 1 and calls the mirrored right-hand pair.
//
// The cutscene is driven the way a player drives it. Every box in it blocks on
// JOY_NEW(A_BUTTON|B_BUTTON) (TextPrinterWaitWithDownArrow), so a held key would
// stall it forever: the presses below are a press/release cadence, one every 12
// frames, which is what Test_Press schedules (4 frames down, 8 up).

#include "registry.h"

#include "constants/map_event_ids.h"
#include "constants/flags.h"
#include "constants/maps.h"
#include "constants/vars.h"
#include "global.fieldmap.h"

// Oak's Pallet Town object event, hidden by FLAG_HIDE_OAK_IN_PALLET_TOWN until
// the cutscene's addobject spawns him. The scene sets no flag when he appears, so
// the live object event is the observable.
static bool8 OakIsSpawned(void)
{
    int i;

    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        if (gObjectEvents[i].active
            && gObjectEvents[i].localId == LOCALID_PALLET_PROF_OAK
            && gObjectEvents[i].mapGroup == MAP_GROUP(MAP_PALLET_TOWN))
            return TRUE;
    }
    return FALSE;
}

static bool8 InOaksLab(void)
{
    return Test_MapGroup() == MAP_GROUP(MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB)
        && Test_MapNum() == MAP_NUM(MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB);
}

// Run frames until the cutscene takes the field away from the player.
static int WaitForLock(int maxFrames)
{
    int i;

    for (i = 0; i < maxFrames; i++)
    {
        if (Test_FieldLocked())
            return i;
        Test_RunFrames(1);
    }
    return -1;
}

// Run frames until the current dialogue contains `needle`. Returns the frames
// waited, or -1. A bounded wait rather than a fixed frame count: every one of the
// cutscene's delays and movement lengths is data, and a test that samples a fixed
// frame number breaks on any legitimate edit to them.
static int WaitForText(const char *needle, int maxFrames)
{
    int i;

    for (i = 0; i < maxFrames; i++)
    {
        if (strstr(Test_StringVar4(), needle) != NULL)
            return i;
        Test_RunFrames(1);
    }
    return -1;
}

// Advance the cutscene -- one A press per 12 frames, so consecutive edges stay
// separate -- until `done` holds. Returns the frames used, or -1.
static int AdvanceUntil(bool8 (*done)(void), int maxFrames)
{
    int i;

    for (i = 0; i < maxFrames; i++)
    {
        if (done())
            return i;
        if (i % 12 == 0)
            Test_Press(A_BUTTON);
        Test_RunFrames(1);
    }
    return -1;
}

// The trigger itself: a fresh save, so the exit belongs to the player and the
// cutscene has not run. Walking into it must take the field away from the player.
FIRERED_TEST("pallet/oak stops the player at the north exit",
             "engine fixture:bedroom npc:oak cutscene", pallet_oak_trigger_fires)
{
    Test_RequireFixture(FIXTURE_BEDROOM);

    // Preconditions the interception is defined against: the scene var is 0, so
    // the coord events arm, and Oak is hidden in town.
    TEST_EQ(Test_Var(VAR_MAP_SCENE_PALLET_TOWN_OAK), 0);
    TEST_EQ(Test_Flag(FLAG_HIDE_OAK_IN_PALLET_TOWN), TRUE);
    TEST_EQ(OakIsSpawned(), FALSE);

    // Two tiles south of the left exit tile (12,1), then walk north into it.
    Test_WarpTo(MAP_GROUP(MAP_PALLET_TOWN), MAP_NUM(MAP_PALLET_TOWN), 12, 3);
    Test_RunFramesToWarp();
    TEST_EQ(Test_FieldLocked(), FALSE);

    Test_Hold(DPAD_UP, 60);

    // The coord event fires on the step that LANDS on (12,1), so the script starts
    // with the player at y=2 mid-step: it then turns him around, which is what
    // pulls him onto the tile. Wait for the lock rather than a frame count.
    TEST_EQ(WaitForLock(180) >= 0, TRUE);

    TEST_EQ(Test_MapGroup(), MAP_GROUP(MAP_PALLET_TOWN));
    TEST_EQ(Test_MapNum(), MAP_NUM(MAP_PALLET_TOWN));
    TEST_EQ(Test_PlayerX(), 12);
    TEST_EQ(Test_PlayerY(), 1);
    TEST_EQ(Test_FieldLocked(), TRUE);

    TEST_GE(WaitForText("Hey! Wait!", 300), 0);
    TEST_STR_EQ(Test_StringVar4(), "OAK: Hey! Wait!\nDon’t go out!");

    // Dismissing the box makes the player turn around (walk_in_place_faster_down)
    // and then spawns Oak with the exclamation emote.
    Test_Press(A_BUTTON);
    TEST_GE(AdvanceUntil(OakIsSpawned, 900), 0);

    // He appears where the movement table leaves him: one tile south of the tile
    // the player is standing on, having walked up from his map.json default.
    TEST_EQ(Test_PlayerFacing(), DIR_SOUTH);
    TEST_EQ(Test_FieldLocked(), TRUE);

    // The scene var still has not moved -- it is written by the last few
    // statements of the cutscene, after the escort reaches the lab door.
    TEST_EQ(Test_Var(VAR_MAP_SCENE_PALLET_TOWN_OAK), 0);
}

// The whole cutscene, through the right-hand exit tile: the escort, the lab door
// and the warp, ending in Oak's lab with the state the rest of the game reads.
FIRERED_TEST("pallet/oak escorts the player from the north exit into the lab",
             "engine fixture:bedroom npc:oak cutscene", pallet_oak_escort_to_lab)
{
    Test_RequireFixture(FIXTURE_BEDROOM);

    // The right exit tile (13,1): VAR_TEMP_1 = 1 selects Oak's right-hand approach
    // and the mirrored escort path.
    Test_WarpTo(MAP_GROUP(MAP_PALLET_TOWN), MAP_NUM(MAP_PALLET_TOWN), 13, 3);
    Test_RunFramesToWarp();

    Test_Hold(DPAD_UP, 60);
    TEST_EQ(WaitForLock(180) >= 0, TRUE);
    TEST_EQ(Test_PlayerX(), 13);
    TEST_EQ(Test_PlayerY(), 1);
    TEST_EQ(Test_FieldLocked(), TRUE);

    // Advance every box and wait out every movement until the escort's own warp
    // hands the player over to the lab's map load.
    TEST_GE(AdvanceUntil(InOaksLab, 6000), 0);

    // What the cutscene must leave behind, or the game is stuck: the scene var
    // that stops the trigger re-arming, and Oak moved out of town into the lab
    // (the lab's starter scene is gated on FLAG_HIDE_OAK_IN_HIS_LAB being clear).
    TEST_EQ(Test_Var(VAR_MAP_SCENE_PALLET_TOWN_OAK), 1);
    TEST_EQ(Test_Flag(FLAG_HIDE_OAK_IN_PALLET_TOWN), TRUE);
    TEST_EQ(Test_Flag(FLAG_HIDE_OAK_IN_HIS_LAB), FALSE);

    // The cutscene writes the lab scene var to 1 on its last statements, and the
    // lab's own ON_FRAME starter scene then advances it to 2 -- both are correct,
    // so the assertion is that the cutscene's write landed at all.
    TEST_GE(Test_Var(VAR_MAP_SCENE_PALLET_TOWN_PROFESSOR_OAKS_LAB), 1);

    // The warp target is the lab's door mat, (6,12) -- the `warp
    // MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB, 6, 12` on the cutscene's last lines.
    // Asserted on the applied warp rather than on the player's live position: the
    // lab's ON_FRAME starter scene walks him north from that mat automatically, so
    // a position sampled here depends on how many frames the map load took.
    TEST_EQ(gSaveBlock1Ptr->location.mapGroup, MAP_GROUP(MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB));
    TEST_EQ(gSaveBlock1Ptr->location.x, 6);
    TEST_EQ(gSaveBlock1Ptr->location.y, 12);
}
