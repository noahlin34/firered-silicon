// CUT, prompted from the tree the map carries.
//
// The seven src/fldeff_*.c field-move sources were unlinked, and
// src/platform/battle_peripheral_stubs.c served all eight SetUpFieldMove_*
// entry points with `return FALSE`. That is the same shape as the other inert
// subsystems in AGENTS.md (#62, #85): the party menu offers the move, the player
// confirms it, and CursorCB_FieldMove gets FALSE back and reports "can't use
// that here".
//
// The prompt is the object event's own script: each cut tree in
// data/maps/ViridianCity/map.json carries `script: EventScript_CutTree`, which
// was sDummyScript until the field-move labels joined NATIVE_SCRIPT_ROOTS (an
// A-press on a dummy script is a silent no-op -- fix #27's class). These tests
// assert the shape of that script and of the effect chain it drives, plus the
// move's own gate, which is what the stub used to answer FALSE to.
//
// Frame counts are deliberately avoided in favour of observing state, so an
#include "registry.h"
#include "script_ops.h"

#include "constants/field_effects.h"
#include "constants/flags.h"
#include "constants/maps.h"
#include "constants/moves.h"
#include "constants/party_menu.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "field_effect.h"
#include "fieldmap.h"
#include "fldeff.h"
#include "global.fieldmap.h"
#include "main.h"
#include "metatile_behavior.h"
#include "overworld.h"
#include "party_menu.h"
#include "pokemon.h"

// The tree's local id in data/maps/ViridianCity/map.json.
#define CUT_TREE_LOCAL_ID 2

// Find the tree object event by its local id, so "the tree is gone" is asserted
// against the object the map actually spawned rather than an index guess.
//
// Note the inverted convention: TryGetObjectEventIdByLocalIdAndMap returns TRUE
// when the lookup FAILED (it reports "id == OBJECT_EVENTS_COUNT"). trainer_see.c
// and the field-effect sprite callbacks all branch on it that way.
static bool8 TreeIsPresent(void)
{
    u8 objectEventId;

    if (TryGetObjectEventIdByLocalIdAndMap(CUT_TREE_LOCAL_ID,
                                            MAP_NUM(MAP_VIRIDIAN_CITY),
                                            MAP_GROUP(MAP_VIRIDIAN_CITY),
                                            &objectEventId))
        return FALSE;   // no live object with that local id
    return gObjectEvents[objectEventId].active;
}

// The cut tree at (11,24) in Viridian City -- object_events[1] in map.json, at
// the player's own elevation. The player stands at (11,25) and faces north.
#define CUT_TREE_X 11
#define CUT_TREE_Y 24

extern const u8 *const gFieldEffectScriptPointers[];
extern const u8 EventScript_CutTree[];
extern const u8 EventScript_CutTreeDown[];
extern const u8 EventScript_FldEffCut[];
extern const u8 EventScript_UseStrength[];
extern const u8 EventScript_FldEffStrength[];

// Learn CUT in slot 0 and let the badge gate pass. A real playthrough has both
// by the time it reaches this tree; the test asserts the move's behaviour, not
// the badge.
static void GiveCutAndBadge(void)
{
    u16 move = MOVE_CUT;
    u8 pp = 30;

    SetMonData(&gPlayerParty[0], MON_DATA_MOVE1, &move);
    SetMonData(&gPlayerParty[0], MON_DATA_PP1, &pp);
    FlagSet(FLAG_BADGE02_GET);
}

// Walk to the tree and face it. The player is one tile south of it, so a
// one-frame press turns him north without stepping onto it.
static void StandBeforeTree(void)
{
    Test_WarpTo(MAP_GROUP(MAP_VIRIDIAN_CITY), MAP_NUM(MAP_VIRIDIAN_CITY),
                CUT_TREE_X, CUT_TREE_Y + 1);
    Test_RunFramesToWarp();
    Test_RunUntilIdle(240);

    GiveCutAndBadge();
    Test_Hold(DPAD_UP, 2);
    Test_RunFrames(6);
}

// SetUpFieldMove_Cut is the function the party menu's CUT entry calls. The stub
// answered FALSE unconditionally, which is what "CUT is offered and then
// refuses" looked like. The real body inspects the gfx in front of the player,
// so this also proves the tree object is resident and that the player is facing
// it -- if either is wrong the call returns FALSE for an honest reason.
FIRERED_TEST("field-moves/cut accepts a tree in front of the player",
         "engine fixture:dex field-moves", field_move_cut_setup)
{
    StandBeforeTree();

    TEST_EQ(Test_PlayerFacing(), DIR_NORTH);
    TEST_TRUE(SetUpFieldMove_Cut());
    // The setup installs the callback the party menu runs after it closes; a
    // FALSE return leaves this NULL and the menu reports the move unusable.
    TEST_PTR_NOT_NULL(gPostMenuFieldCallback);

    // Away from any cuttable thing the same call must decline, so the assertion
    // above is about the tree and not about the function always answering TRUE.
    Test_WarpTo(MAP_GROUP(MAP_PALLET_TOWN_PLAYERS_HOUSE_2F),
                MAP_NUM(MAP_PALLET_TOWN_PLAYERS_HOUSE_2F), 4, 4);
    Test_RunFramesToWarp();
    Test_RunUntilIdle(240);
    gPostMenuFieldCallback = NULL;
    gFieldCallback2 = NULL;
    Test_Hold(DPAD_UP, 2);
    Test_RunFrames(6);
    TEST_FALSE(SetUpFieldMove_Cut());
    TEST_PTR_EQ(gPostMenuFieldCallback, NULL);
}

// The prompt the tree carries, and the tail the YES branch jumps to. Before the
// field-move labels were compiled this label was sDummyScript (`{ 0x02 }`), so
// pressing A on the tree ended the script instantly and nothing happened.
FIRERED_TEST("field-moves/the tree carries the real cut prompt",
         "pure field-moves", field_move_cut_prompt)
{
    const u8 *script = EventScript_CutTree;

    TEST_PTR_NOT_NULL(script);

    // No longer the one-byte dummy. The real script opens with the
    // `goto_if_questlog` expansion (a specialvar + compare + goto_if), so the
    // first byte is SCR_CMD_SPECIALVAR, not `end` -- which is exactly what the
    // dummy was, and why an A-press on the tree did nothing (fix #27's class: a
    // dummy script is an instant end, indistinguishable from no script).
    TEST_TRUE(Test_ScriptStartsWith(script, SCR_CMD_SPECIALVAR));
    TEST_NE(script[0], SCR_CMD_END);

    // The prompt asks a question and the mon/move names are buffered into
    // STR_VAR_1/STR_VAR_2 first, because Text_CutTreeDown and Text_MonUsedMove
    // both print them.
    TEST_GE(Test_ScriptFindOpcode(script, SCR_CMD_YESNOBOX), 0);
    TEST_GE(Test_ScriptFindOpcode(script, SCR_CMD_BUFFERPARTYMONNICK), 0);
    TEST_GE(Test_ScriptFindOpcode(script, SCR_CMD_BUFFERMOVENAME), 0);
    // checkpartymove writes PARTY_SIZE to VAR_RESULT when no mon knows CUT, and
    // its result is what setfieldeffectargument forwards to the effect.
    TEST_GE(Test_ScriptFindOpcode(script, SCR_CMD_CHECKPARTYMOVE), 0);
    TEST_GE(Test_ScriptFindOpcode(script, SCR_CMD_SETFIELDEFFECTARGUMENT), 0);
    TEST_GE(Test_ScriptFindOpcode(script, SCR_CMD_DOFIELDEFFECT), 0);

    // The YES branch's tail takes the tree out of the map; this is the part the
    // dummy could never do.
    TEST_TRUE(Test_ScriptStartsWith(EventScript_CutTreeDown, SCR_CMD_APPLYMOVEMENT));
    TEST_GE(Test_ScriptFindOpcode(EventScript_CutTreeDown, SCR_CMD_REMOVEOBJECT), 0);

    // ...and the effect FldEff_UseCutOnTree sets up is the one that runs it.
    TEST_TRUE(Test_ScriptStartsWith(EventScript_FldEffCut, SCR_CMD_LOCKALL));
    TEST_GE(Test_ScriptFindOpcode(EventScript_FldEffCut, SCR_CMD_DOFIELDEFFECT), 0);
}

// The other two field moves reach the game by the same route: a gfx object in
// front of the player carrying a script label. Strength's is the boulder path,
// and it is the one that sets a flag the movement code reads rather than
// removing an object.
FIRERED_TEST("field-moves/strength prompt compiles with its flag",
         "pure field-moves", field_move_strength_prompt)
{
    TEST_TRUE(Test_ScriptStartsWith(EventScript_FldEffStrength, SCR_CMD_LOCKALL));
    // The tail sets FLAG_SYS_USE_STRENGTH, which MB_PUSHABLE_BOULDER's movement
    // permission is gated on -- without it the boulder stays put.
    TEST_GE(Test_ScriptFindOpcode(EventScript_UseStrength, SCR_CMD_SETFLAG), 0);
}

// Every field move must have a script in the table, not just CUT: the script
// command `dofieldeffect <id>` reads it by index and FieldEffectStart returns
// without adding the id when it is NULL, so the `waitstate` that follows would
// wait for an effect that never started (a locked field, not a no-op).
FIRERED_TEST("field-moves/all eight move effects are registered",
         "pure field-moves", field_move_effects_registered)
{
    static const u8 ids[] = {
        FLDEFF_USE_CUT_ON_TREE,
        FLDEFF_USE_CUT_ON_GRASS,
        FLDEFF_CUT_GRASS,
        FLDEFF_USE_ROCK_SMASH,
        FLDEFF_USE_DIG,
        FLDEFF_USE_STRENGTH,
        FLDEFF_USE_TELEPORT,
        FLDEFF_SWEET_SCENT,
    };
    int i;

    for (i = 0; i < (int)(sizeof(ids) / sizeof(ids[0])); i++)
        TEST_PTR_NOT_NULL(gFieldEffectScriptPointers[ids[i]]);
}

// The full flow, on the real user path. This is the test the stubbed port could
// not pass at all: with `return FALSE` in SetUpFieldMove_Cut the menu reports
// "can't use that here" and the tree stays; with the field-move labels missing
// from NATIVE_SCRIPT_ROOTS the tree has no prompt to run.
//
// Drives it ONCE and observes, rather than calling the setup twice: FieldEffectStart
// has no already-active guard, so a second run would add the effect id to the
// active list again.
FIRERED_TEST("field-moves/cut menu entry removes the tree",
         "engine fixture:dex field-moves slow", field_move_cut_end_to_end)
{
    extern bool8 gPartyMenuUseExitCallback;
    u16 treeBefore;

    StandBeforeTree();

    // The tree is there before we start, so "it is gone" afterwards means
    // something. A local id that never resolved would make the assertion below
    // vacuous.
    TEST_TRUE(TreeIsPresent());

    // The move must be offered. This is the gate the stub answered FALSE to, and
    // it is also what tells us the mon knows CUT at the moment the menu builds
    // its action list.
    TEST_TRUE(SetUpFieldMove_Cut());
    // SetUpFieldMove_Cut installs gPostMenuFieldCallback itself; asserting it is
    // what proves the move will actually run once the menu closes.
    TEST_PTR_NOT_NULL(gPostMenuFieldCallback);

    // START -> POKéMON -> the first mon.
    //
    // This fixture owns a dex, so SetUpStartMenu_NormalField appends POKéDEX
    // first and POKéMON is entry 1 (start_menu.c:215-218): the START press opens
    // the menu, then one DPAD_DOWN reaches POKéMON. tests/summary.c presses A
    // immediately only because its fixture:lab save has
    // FLAG_SYS_POKEMON_GET set and FLAG_SYS_POKEDEX_GET clear.
    Test_Press(START_BUTTON);
    Test_RunFrames(40);
    Test_Press(DPAD_DOWN);         // move the cursor to POKéMON
    Test_RunFrames(10);
    Test_Press(A_BUTTON);          // POKéMON
    Test_RunFrames(240);           // fade out, load, fade in
    Test_Press(A_BUTTON);          // choose the first mon -> action window
    Test_RunFrames(60);

    // The action window is up and its first entry is SUMMARY; CUT is one row
    // below it, and only present because the mon knows the move.
    Test_Press(DPAD_DOWN);
    Test_RunFrames(10);
    Test_Press(A_BUTTON);          // CUT
    Test_RunFrames(120);           // menu closes, field callback runs

    // The menu path has NO Yes/No box: EventScript_FldEffCut (which
    // FldEff_UseCutOnTree sets up) is `lockall, dofieldeffect, waitstate, goto
    // EventScript_CutTreeDown`. Only EventScript_CutTree -- the label the tree
    // object carries for an A-press -- asks "Would you like to CUT it?", because
    // that path has to run checkpartymove/bufferpartymonnick first. The
    // confirmation the player gives here is picking CUT in the party menu.
    //
    // So no input is needed after the menu closes: the effect runs, the script
    // falls into CutTreeDown, and the object is removed. Wait for the engine
    // rather than pressing at it, because an A press during the field-move
    // show-mon animation is just noise.
    Test_RunUntilIdle(1200);
    Test_RunFrames(60);

    // The observable outcome: the tree object left the map.
    TEST_FALSE(TreeIsPresent());

    // And the field is usable again -- a prompt that never releases is as broken
    // as one that never opens.
    TEST_TRUE(Test_InOverworld());
    TEST_FALSE(Test_FieldLocked());
}

// The grass half of CUT: the same effect chain with a different first script.
// FldEff_CutGrass sweeps a 3x3 of grass metatiles around the player into their
// cut versions (MapGridSetMetatileIdAt + DrawWholeMapView), which is a visible
// change to the map itself rather than to an object.
//
// The tile is FOUND, not guessed: MetatileAtCoordsIsGrassTile (the same
// predicate SetUpFieldMove_Cut uses) is checked against the live map grid, so
// the test warps to a coordinate it has confirmed is cuttable grass. An earlier
// version guessed a Route 1 coordinate and asserted nothing unless the move
// happened to be accepted, which measured nothing either way.
static bool8 FindGrassInFrontOf(s16 *outX, s16 *outY, s16 *playerX, s16 *playerY)
{
    s16 x, y;

    for (y = 0; y < 40; y++)
    {
        for (x = 0; x < 48; x++)
        {
            // The player must stand on a walkable tile with grass directly
            // north, because a one-frame UP press only turns him.
            if (TestMetatileAttributeBit(MapGridGetMetatileAttributeAt(x, y, METATILE_ATTRIBUTE_TERRAIN), TILE_TERRAIN_GRASS)
                && !MapGridGetCollisionAt(x, y + 1)
                && !TestMetatileAttributeBit(MapGridGetMetatileAttributeAt(x, y + 1, METATILE_ATTRIBUTE_TERRAIN), TILE_TERRAIN_GRASS))
            {
                *outX = x;
                *outY = y;
                *playerX = x;
                *playerY = y + 1;
                return TRUE;
            }
        }
    }
    return FALSE;
}

// The grass half of CUT: the same effect chain with a different first script.
// FldEff_CutGrass rewrites every grass metatile in the 3x3 block around the
// player into its cut version (SetCutGrassMetatileAt + DrawWholeMapView), which
// is a change to the MAP itself rather than to an object.
//
// The assertion is a whole-grid diff, not a sample of the 3x3. Two reasons,
// both learned the hard way here:
//   - the sweep's origin is the player's DESTINATION (PlayerGetDestCoords), and
//     this port has several coordinate spaces in play (gSaveBlock1Ptr->pos is
//     what Test_PlayerX reads or object-event currentCoords, depending on the
//     path), so deriving the window in the test risks sampling the wrong tiles;
//   - which of the nine change depends on where the grass actually is.
// A count of differing tiles is unambiguous and needs no coordinate reasoning.
//
// The map is only 24x40, so the scan is cheap.
static void SnapshotMapGrid(u32 *out)
{
    s32 x, y;

    for (y = 0; y < 40; y++)
        for (x = 0; x < 24; x++)
            *out++ = MapGridGetMetatileIdAt(x, y);
}

FIRERED_TEST("field-moves/cut rewrites grass metatiles on the map",
         "engine fixture:dex field-moves", field_move_cut_grass)
{
    static u32 before[24 * 40];
    static u32 after[24 * 40];
    s16 grassX, grassY, px, py;
    int changed = 0;
    int i;

    // The scan runs on the LIVE grid after the warp: the metatile attribute
    // table is only valid for the loaded map, and SetUpFieldMove_Cut asks the
    // same predicate (MetatileAtCoordsIsGrassTile) before it accepts.
    Test_WarpTo(MAP_GROUP(MAP_ROUTE1), MAP_NUM(MAP_ROUTE1), 5, 11);
    Test_RunFramesToWarp();
    Test_RunUntilIdle(240);

    if (!FindGrassInFrontOf(&grassX, &grassY, &px, &py))
    {
        TEST_FAIL("Route 1 has no grass tile with a walkable tile below it");
        return;
    }

    // Stand below the grass and face it. One warp is enough: warping twice left
    // the avatar's coordinates and the save block's pos disagreeing, and the
    // sweep then ran somewhere the test was not looking.
    Test_WarpTo(MAP_GROUP(MAP_ROUTE1), MAP_NUM(MAP_ROUTE1), px, py);
    Test_RunFramesToWarp();
    Test_RunUntilIdle(240);
    GiveCutAndBadge();
    Test_Hold(DPAD_UP, 2);
    Test_RunFrames(6);

    // The move must accept the tile the scan found. If this fails, the scan and
    // SetUpFieldMove_Cut disagree, which is itself worth knowing.
    TEST_TRUE(SetUpFieldMove_Cut());
    // Guard before calling through it: with the move stubbed (the historical
    // defect) this pointer is NULL, and Test_Fail-then-crash would report the
    // bug as a segmentation fault instead of a failed assertion.
    TEST_PTR_NOT_NULL(gPostMenuFieldCallback);
    if (gPostMenuFieldCallback == NULL)
        return;

    SnapshotMapGrid(before);

    // Run the effect chain: SetUpFieldMove_Cut installed FieldCallback_CutGrass,
    // which starts FLDEFF_USE_CUT_ON_GRASS; that native runs FLDEFF_CUT_GRASS,
    // which does the rewriting.
    gFieldCallback2 = NULL;
    gPostMenuFieldCallback();
    Test_RunFrames(120);

    SnapshotMapGrid(after);

    for (i = 0; i < 24 * 40; i++)
    {
        if (before[i] != after[i])
            changed++;
    }
    TEST_GE(changed, 1);

    Test_RunUntilIdle(600);
    TEST_FALSE(Test_FieldLocked());
}
