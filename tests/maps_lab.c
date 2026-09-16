// Oak's Lab map wiring (MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB).
//
// Pure: the defect these guard against was entirely in the generated map data --
// every object and bg event carried sDummyScript, so pressing A on an aide, a
// starter ball or a sign did nothing at all. Nothing here needs a boot; the map
// structures are readable from any process, which is what makes these cheap
// enough to run on every change.
//
// Assertions are about wiring and behaviour, not operand bytes: a test that pins
// the generator's current encoding breaks on legitimate generator changes.

#include "registry.h"
#include "script_ops.h"

#include "constants/map_scripts.h"
#include "constants/maps.h"
#include "constants/vars.h"
#include "global.fieldmap.h"

extern const struct MapHeader PalletTown_ProfessorOaksLab;
extern const u8 PalletTown_ProfessorOaksLab_EventScript_Aide1[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_Aide2[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_Aide3[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_ProfOak[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_BulbasaurBall[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_SquirtleBall[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_CharmanderBall[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_Rival[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_Pokedex[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_Computer[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_LeftSign[];
extern const u8 PalletTown_ProfessorOaksLab_EventScript_RightSign[];
extern const u8 PalletTown_ProfessorOaksLab_ChooseStarterScene[];

// Every reachable A-press target must point at its real script. A NULL or dummy
// here is an interaction that silently does nothing -- the failure mode that made
// the whole lab inert before.
OMP_TEST("lab/object events point at their real scripts", "maps lab pure", lab_object_events)
{
    const struct MapEvents *events = PalletTown_ProfessorOaksLab.events;
    const u8 *const expected[] = {
        PalletTown_ProfessorOaksLab_EventScript_Aide1,          // localId 1
        PalletTown_ProfessorOaksLab_EventScript_Aide3,          // localId 2
        PalletTown_ProfessorOaksLab_EventScript_Aide2,          // localId 3
        PalletTown_ProfessorOaksLab_EventScript_ProfOak,        // localId 4
        PalletTown_ProfessorOaksLab_EventScript_BulbasaurBall,  // localId 5
        PalletTown_ProfessorOaksLab_EventScript_SquirtleBall,   // localId 6
        PalletTown_ProfessorOaksLab_EventScript_CharmanderBall, // localId 7
        PalletTown_ProfessorOaksLab_EventScript_Rival,          // localId 8
        PalletTown_ProfessorOaksLab_EventScript_Pokedex,        // localId 9
        PalletTown_ProfessorOaksLab_EventScript_Pokedex,        // localId 10
    };
    int i;

    TEST_PTR_NOT_NULL(events);
    TEST_EQ(events->objectEventCount, 10);

    for (i = 0; i < 10; i++)
    {
        if (events->objectEvents[i].script != expected[i])
            TEST_FAIL("object event %d points at %p, expected %p", i,
                      (const void *)events->objectEvents[i].script,
                      (const void *)expected[i]);
        TEST_PTR_NOT_NULL(events->objectEvents[i].script);
    }
}

// NPCs lock the field before talking (an A-press that does not lock lets the
// player walk away mid-dialogue); the starter balls do too, since they open a
// yes/no box.
OMP_TEST("lab/npc and ball scripts lock the field first", "maps lab pure", lab_scripts_lock)
{
    const struct MapEvents *events = PalletTown_ProfessorOaksLab.events;

    TEST_TRUE(Test_ScriptStartsWith(PalletTown_ProfessorOaksLab_EventScript_Aide1, SCR_CMD_LOCK));
    TEST_TRUE(Test_ScriptStartsWith(PalletTown_ProfessorOaksLab_EventScript_Aide2, SCR_CMD_LOCK));
    TEST_TRUE(Test_ScriptStartsWith(PalletTown_ProfessorOaksLab_EventScript_Aide3, SCR_CMD_LOCK));
    TEST_TRUE(Test_ScriptStartsWith(PalletTown_ProfessorOaksLab_EventScript_Rival, SCR_CMD_LOCK));
    TEST_TRUE(Test_ScriptStartsWith(PalletTown_ProfessorOaksLab_EventScript_ProfOak, SCR_CMD_LOCK));

    TEST_TRUE(Test_ScriptStartsWith(events->objectEvents[4].script, SCR_CMD_LOCK));
    TEST_TRUE(Test_ScriptStartsWith(events->objectEvents[5].script, SCR_CMD_LOCK));
    TEST_TRUE(Test_ScriptStartsWith(events->objectEvents[6].script, SCR_CMD_LOCK));
}

// The four background events (two computer terminals, two signs) all lock first.
OMP_TEST("lab/background events point at their real scripts", "maps lab pure", lab_bg_events)
{
    const struct MapEvents *events = PalletTown_ProfessorOaksLab.events;
    const u8 *const expected[] = {
        PalletTown_ProfessorOaksLab_EventScript_Computer,  // (2,1)
        PalletTown_ProfessorOaksLab_EventScript_Computer,  // (3,1)
        PalletTown_ProfessorOaksLab_EventScript_LeftSign,  // (6,1)
        PalletTown_ProfessorOaksLab_EventScript_RightSign, // (7,1)
    };
    int i;

    TEST_PTR_NOT_NULL(events);
    TEST_EQ(events->bgEventCount, 4);

    for (i = 0; i < 4; i++)
    {
        if (events->bgEvents[i].bgUnion.script != expected[i])
            TEST_FAIL("bg event %d points at %p, expected %p", i,
                      (const void *)events->bgEvents[i].bgUnion.script,
                      (const void *)expected[i]);
        TEST_TRUE(Test_ScriptStartsWith(events->bgEvents[i].bgUnion.script, SCR_CMD_LOCKALL));
    }
}

// An NPC's message operand must resolve through the pointer table. If the
// generator emits the text label as a raw pointer the relocation is unaligned and
// the link fails; if it emits the wrong index the box prints garbage. Resolving
// to a non-NULL script here is the cheap check that the operand is index-shaped.
OMP_TEST("lab/npc message operands resolve through the pointer table", "maps lab pure",
         lab_message_operand)
{
    const u8 *aide = PalletTown_ProfessorOaksLab_EventScript_Aide1;
    int messageAt = Test_ScriptFindOpcode(aide, SCR_CMD_MESSAGE);

    TEST_GE(messageAt, 0);
    TEST_PTR_NOT_NULL(Test_ScriptPtrAt(aide, messageAt + 1));
}

// The map-script header drives the starter scene: ON_TRANSITION brings Oak in,
// ON_WARP_INTO_MAP_TABLE positions the player, and ON_FRAME_TABLE runs
// ChooseStarterScene once the scene var is 1. Without the header the lab loads
// as a room where the starter scene never starts.
OMP_TEST("lab/map-script header runs the starter scene", "maps lab pure", lab_map_scripts)
{
    const u8 *scripts = PalletTown_ProfessorOaksLab.mapScripts;
    const u8 *onFrameTable;
    const u8 *starterScene;
    int i = 0;

    TEST_PTR_NOT_NULL(scripts);
    TEST_EQ(scripts[0], MAP_SCRIPT_ON_TRANSITION);

    // Walk the entry list rather than hard-coding offsets: entry layout is the
    // generator's business, the presence and resolution of the tables is not.
    while (scripts[i] != 0x00 && i < TEST_SCRIPT_SCAN_LIMIT)
    {
        u8 tag = scripts[i];
        const u8 *target = Test_ScriptPtrAt(scripts, i + 1);

        TEST_PTR_NOT_NULL(target);
        if (tag == MAP_SCRIPT_ON_FRAME_TABLE)
        {
            onFrameTable = target;
            // map_script_2 <var>, <value>, <script>: the entry is a var, a value,
            // then the script pointer.
            TEST_EQ(Test_ScriptHalfwordAt(onFrameTable, 2), 1);   // scene var == 1
            TEST_EQ(Test_ScriptHalfwordAt(onFrameTable, 0),
                    VAR_MAP_SCENE_PALLET_TOWN_PROFESSOR_OAKS_LAB);
            starterScene = Test_ScriptPtrAt(onFrameTable, 4);
            TEST_PTR_EQ(starterScene, PalletTown_ProfessorOaksLab_ChooseStarterScene);
        }
        i += 5;
    }

    TEST_PTR_EQ(starterScene, PalletTown_ProfessorOaksLab_ChooseStarterScene);
    TEST_TRUE(Test_ScriptStartsWith(PalletTown_ProfessorOaksLab_ChooseStarterScene,
                                    SCR_CMD_LOCKALL));
}
