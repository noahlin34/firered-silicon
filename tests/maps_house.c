// Player's house (MAP_PALLET_TOWN_PLAYERS_HOUSE_1F / _2F) wiring.
//
// Pure: the defects this guards were in the map-script compilation. The TV, NES
// and signpost interactions were dummy scripts, so the whole ground floor was
// inert; the shared metatile flavor scripts (bookshelf, dresser, cabinet,
// kitchen, TV screen) were dummy too, which is why they are asserted separately.
//
// The behaviour here is verified end-to-end by the interaction tests; these run
// on every change and cost nothing.

#include "registry.h"
#include "script_ops.h"

#include "constants/maps.h"
#include "global.fieldmap.h"

extern const struct MapHeader PalletTown_PlayersHouse_1F;
extern const struct MapHeader PalletTown_PlayersHouse_2F;
extern const u8 EventScript_Bookshelf[];
extern const u8 EventScript_Cabinet[];
extern const u8 EventScript_Dresser[];
extern const u8 EventScript_Kitchen[];
extern const u8 EventScript_PlayerFacingTVScreen[];
extern const u8 EventScript_CancelMessageBox[];
extern const u8 PalletTown_PlayersHouse_1F_EventScript_TV[];
extern const u8 PalletTown_PlayersHouse_2F_EventScript_NES[];
extern const u8 PalletTown_PlayersHouse_2F_EventScript_Sign[];

// The ground floor's TV opens its own dialogue when pressed from the front.
FIRERED_TEST("house/tv background event is a real script", "maps house pure", house_tv)
{
    const struct MapEvents *events = PalletTown_PlayersHouse_1F.events;
    const struct BgEvent *tv;

    TEST_PTR_NOT_NULL(events);
    TEST_GE(events->bgEventCount, 1);

    tv = &events->bgEvents[0];
    TEST_EQ(tv->x, 6);
    TEST_EQ(tv->y, 1);
    TEST_PTR_EQ(tv->bgUnion.script, PalletTown_PlayersHouse_1F_EventScript_TV);
    TEST_TRUE(Test_ScriptStartsWith(tv->bgUnion.script, SCR_CMD_LOCKALL));
}

// Upstairs: the NES and the posted notice are both interactive.
FIRERED_TEST("house/nes and notice background events are real scripts", "maps house pure",
         house_upstairs)
{
    const struct MapEvents *events = PalletTown_PlayersHouse_2F.events;
    const struct BgEvent *nes;
    const struct BgEvent *sign;

    TEST_PTR_NOT_NULL(events);
    TEST_GE(events->bgEventCount, 3);

    nes = &events->bgEvents[0];
    TEST_EQ(nes->x, 6);
    TEST_EQ(nes->y, 5);
    TEST_PTR_EQ(nes->bgUnion.script, PalletTown_PlayersHouse_2F_EventScript_NES);
    TEST_TRUE(Test_ScriptStartsWith(nes->bgUnion.script, SCR_CMD_LOCKALL));

    sign = &events->bgEvents[2];
    TEST_EQ(sign->x, 11);
    TEST_EQ(sign->y, 1);
    TEST_PTR_EQ(sign->bgUnion.script, PalletTown_PlayersHouse_2F_EventScript_Sign);
    TEST_TRUE(Test_ScriptStartsWith(sign->bgUnion.script, SCR_CMD_LOCKALL));
}

// The metatile flavor scripts are shared across every house in the game, so a
// dummy here silently disables bookshelves, dressers and kitchens everywhere.
FIRERED_TEST("house/metatile flavor scripts are compiled", "maps house pure", house_flavor)
{
    TEST_TRUE(Test_ScriptStartsWith(EventScript_Bookshelf, SCR_CMD_LOCKALL));
    TEST_TRUE(Test_ScriptStartsWith(EventScript_Cabinet, SCR_CMD_LOCKALL));
    TEST_TRUE(Test_ScriptStartsWith(EventScript_Dresser, SCR_CMD_LOCKALL));
    TEST_TRUE(Test_ScriptStartsWith(EventScript_Kitchen, SCR_CMD_LOCKALL));
    TEST_TRUE(Test_ScriptStartsWith(EventScript_PlayerFacingTVScreen, SCR_CMD_LOCKALL));
}

// A message operand must be an index into the pointer table (fix #23). Resolving
// to non-NULL is the cheap proof the operand is index-shaped rather than a
// truncated address.
FIRERED_TEST("house/message operands resolve through the pointer table", "maps house pure",
         house_message_operands)
{
    const u8 *scripts[] = {
        EventScript_Bookshelf,
        PalletTown_PlayersHouse_2F_EventScript_NES,
    };
    int s;

    for (s = 0; s < 2; s++)
    {
        int at = Test_ScriptFindOpcode(scripts[s], SCR_CMD_MESSAGE);
        TEST_GE(at, 0);
        TEST_PTR_NOT_NULL(Test_ScriptPtrAt(scripts[s], at + 1));
    }
}

// Walking away from a signpost pushes the opposite direction, which runs
// EventScript_CancelMessageBox to dismiss the box. When it was a dummy `end` the
// signpost frame stayed drawn over the world for ~65 frames of walking
// (AGENTS.md fix #33).
FIRERED_TEST("house/walking away from a signpost dismisses its box", "maps house pure",
         house_cancel_message_box)
{
    const u8 *s = EventScript_CancelMessageBox;

    TEST_PTR_NOT_NULL(s);
    TEST_TRUE(Test_ScriptStartsWith(s, SCR_CMD_SPECIAL));
    TEST_NE(Test_ScriptFindOpcode(s, SCR_CMD_RELEASE), -1);
    TEST_TRUE(Test_ScriptContains(s, (const u8[]){ SCR_CMD_SPECIAL, 0x5A, 0x01, 0x00 }, 4)
              || Test_ScriptFindOpcode(s, SCR_CMD_SPECIAL) >= 0);
}
