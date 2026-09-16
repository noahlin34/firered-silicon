// Rival's house (MAP_PALLET_TOWN_RIVALS_HOUSE) and Daisy's TOWN MAP gift.
//
// Pure. The whole room was `sDummyScript` -- its mapScripts header was
// sEmptyMapScripts and both object events were dummy scripts -- so the TOWN MAP
// was unreachable in the port (AGENTS.md fix #57, §6 item 9).

#include "registry.h"
#include "script_ops.h"

#include "constants/map_scripts.h"
#include "constants/flags.h"
#include "constants/maps.h"
#include "global.fieldmap.h"

extern const struct MapHeader PalletTown_RivalsHouse;
extern const u8 PalletTown_RivalsHouse_EventScript_Daisy[];
extern const u8 PalletTown_RivalsHouse_EventScript_TownMap[];
extern const u8 PalletTown_RivalsHouse_EventScript_GiveTownMap[];
extern const u8 PalletTown_RivalsHouse_EventScript_Bookshelf[];
extern const u8 PalletTown_RivalsHouse_EventScript_Picture[];
extern const u8 PalletTown_RivalsHouse_OnTransition[];
extern u16 (*const gSpecials[])(void);

// Opcodes for the gift sequence.
#define SCR_CMD_ADDITEM       0x44

// Daisy and the TOWN MAP object must run real scripts, and the map object must
// be hidden by the flag the gift sets.
FIRERED_TEST("daisy/room object events are real scripts", "maps daisy pure", daisy_objects)
{
    const struct MapEvents *events = PalletTown_RivalsHouse.events;

    TEST_PTR_NOT_NULL(events);
    TEST_EQ(events->objectEventCount, 2);

    TEST_PTR_EQ(events->objectEvents[0].script, PalletTown_RivalsHouse_EventScript_Daisy);
    TEST_PTR_EQ(events->objectEvents[1].script, PalletTown_RivalsHouse_EventScript_TownMap);
    TEST_EQ(events->objectEvents[1].flagId, FLAG_HIDE_TOWN_MAP);

    // Daisy locks and turns to face the player before speaking.
    TEST_TRUE(Test_ScriptStartsWith(PalletTown_RivalsHouse_EventScript_Daisy, SCR_CMD_LOCK));
    TEST_EQ(PalletTown_RivalsHouse_EventScript_Daisy[1], SCR_CMD_FACEPLAYER);
}

// The bookshelves and the picture are metatile-flavor interactions.
FIRERED_TEST("daisy/background events are real scripts", "maps daisy pure", daisy_bg)
{
    const struct MapEvents *events = PalletTown_RivalsHouse.events;

    TEST_PTR_NOT_NULL(events);
    TEST_EQ(events->bgEventCount, 3);

    TEST_PTR_EQ(events->bgEvents[0].bgUnion.script, PalletTown_RivalsHouse_EventScript_Bookshelf);
    TEST_PTR_EQ(events->bgEvents[1].bgUnion.script, PalletTown_RivalsHouse_EventScript_Bookshelf);
    TEST_PTR_EQ(events->bgEvents[2].bgUnion.script, PalletTown_RivalsHouse_EventScript_Picture);
}

// ON_TRANSITION moves Daisy to the table before the parcel errand. Without it she
// stands at her map.json default and never walks over, so the gift branch is
// never reachable by talking to her where she appears to be.
FIRERED_TEST("daisy/on-transition is compiled and resolves", "maps daisy pure", daisy_transition)
{
    const u8 *scripts = PalletTown_RivalsHouse.mapScripts;

    TEST_PTR_NOT_NULL(scripts);
    TEST_EQ(scripts[0], MAP_SCRIPT_ON_TRANSITION);
    TEST_PTR_NOT_NULL(Test_ScriptPtrAt(scripts, 1));
}

// The gift branch must contain the sequence that grants the TOWN MAP: the map
// object is removed (removeobject), the scene var advances, and the item is
// added. Asserted as presence of the operations, not their offsets.
FIRERED_TEST("daisy/give-town-map branch removes the object and grants the item",
         "maps daisy pure", daisy_give)
{
    const u8 *s = PalletTown_RivalsHouse_EventScript_GiveTownMap;

    TEST_PTR_NOT_NULL(s);

    // Scan the whole window: 0x02 is a legitimate operand byte, so a scan that
    // stops at the first one truncates early and reports a false negative.
    TEST_EQ(Test_ScriptContains(s, (const u8[]){ SCR_CMD_REMOVEOBJECT }, 1), TRUE);
    TEST_EQ(Test_ScriptContains(s, (const u8[]){ SCR_CMD_ADDITEM }, 1), TRUE);
}

// The five specials her branch calls must be registered. An unregistered one
// prints "special N is not ported" and leaves the script stuck mid-branch, so
// the player never receives the item.
FIRERED_TEST("daisy/her specials are registered", "maps daisy pure", daisy_specials)
{
    TEST_PTR_NOT_NULL((const void *)gSpecials[124]);   // BufferMonNickname
    TEST_PTR_NOT_NULL((const void *)gSpecials[159]);   // ChoosePartyMon
    TEST_PTR_NOT_NULL((const void *)gSpecials[230]);   // GetLeadMonFriendship
    TEST_PTR_NOT_NULL((const void *)gSpecials[327]);   // GetPartyMonSpecies
    TEST_PTR_NOT_NULL((const void *)gSpecials[407]);   // DaisyMassageServices
}
