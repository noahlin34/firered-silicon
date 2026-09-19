// Poké Mart shops: the clerk's `pokemart` command, the buy menu and the
// sell path (src/shop.c, src/buy_menu_helpers.c).
//
// Before this pair of sources was linked, `ScrCmd_pokemart` reported
// "[Script] pokemart is not ported" and left the script running, because
// CreatePokemartMenu was a no-op stub and stopping the script context would
// have locked the field with nothing to re-enable it (AGENTS.md fix #37).
// The clerk's dialogue therefore completed and the shop never opened.
//
// The buy menu needs the field it is drawn over (it composites the map the
// player is standing in) and an ITEMS pocket holding something, so these are
// engine tests on the dex fixture, which is the state a playthrough reaches the
// Mart with.

#include "registry.h"

#include "constants/items.h"
#include "constants/map_scripts.h"
#include "constants/maps.h"
#include "global.fieldmap.h"
#include "script_ops.h"
#include "event_data.h"
#include "item.h"
#include "main.h"
#include "menu.h"
#include "overworld.h"
#include "script.h"
#include "shop.h"

extern const u8 ViridianCity_Mart_EventScript_Clerk[];
extern const u8 ViridianCity_Mart_Items[];
extern const struct MapHeader ViridianCity_Mart;

// The clerk's shop branch is `message Text_MayIHelpYou` / `waitmessage` /
// `pokemart`. The first A opens the box, the second dismisses it and runs the
// command; the box blocks on JOY_NEW, so each press needs a release after it.
// Waits for the command's own effect -- ScriptContext_Stop, which is what hands
// input to the shop menu -- rather than counting frames.
static void OpenShopMenu(void)
{
    for (int i = 0; i < 30 && ScriptContext_IsEnabled(); i++)
    {
        Test_Press(A_BUTTON);
        Test_RunFrames(45);
    }
}

static bool8 WaitForCallback(MainCallback changedFrom, int maxFrames)
{
    for (int i = 0; i < maxFrames; i++)
    {
        Test_RunFrames(1);
        if (gMain.callback2 != changedFrom)
            return TRUE;
    }
    return FALSE;
}

// Walk to the clerk's counter, face him and open the shop menu.
static void TalkToClerk(void)
{
    // (4,3) is the tile east of the clerk on (3,3); the clerk faces right, and
    // the counter metatile means the game looks one tile further for him.
    Test_WarpTo(MAP_GROUP(MAP_VIRIDIAN_CITY_MART), MAP_NUM(MAP_VIRIDIAN_CITY_MART), 4, 3);
    Test_RunFramesToWarp();

    Test_Press(DPAD_LEFT);
    Test_RunFrames(3);
    Test_Press(A_BUTTON);
    Test_RunFrames(60);
    OpenShopMenu();
}

// --- wiring (pure) ----------------------------------------------------------

// The clerk must run the real `pokemart` command, and its operand must resolve
// through gNativeScriptPtrs to the item list. A NULL coord event or a dummy
// script here is what made the whole shop unreachable; an operand resolving to
// the wrong label prints garbage in the buy menu.
FIRERED_TEST("shop/the mart clerk runs the real pokemart command", "maps shop pure",
             shop_clerk_script)
{
    TEST_PTR_NOT_NULL(ViridianCity_Mart_EventScript_Clerk);
    TEST_TRUE(Test_ScriptStartsWith(ViridianCity_Mart_EventScript_Clerk, SCR_CMD_LOCK));
    TEST_GE(Test_ScriptFindOpcode(ViridianCity_Mart_EventScript_Clerk, SCR_CMD_POKEMART), 0);
}

// The four items Viridian City's Mart sells, as `data/maps/ViridianCity_Mart/
// scripts.inc` lists them, terminated by ITEM_NONE.
FIRERED_TEST("shop/the mart item list holds its four items", "maps shop pure",
             shop_item_list)
{
    const u16 *items = (const u16 *)ViridianCity_Mart_Items;

    TEST_EQ(items[0], ITEM_POKE_BALL);
    TEST_EQ(items[1], ITEM_POTION);
    TEST_EQ(items[2], ITEM_ANTIDOTE);
    TEST_EQ(items[3], ITEM_PARALYZE_HEAL);
    TEST_EQ(items[4], ITEM_NONE);
}

// The map header must carry the compiled ON_LOAD/ON_FRAME table: the parcel
// scene runs from ON_FRAME while the scene var is 0, and it is what advances the
// var past 0. A map that kept sEmptyMapScripts would run the parcel scene on
// every frame the player walked in.
FIRERED_TEST("shop/the mart map header runs its compiled scripts", "maps shop pure",
             shop_parcel_gate)
{
    const u8 *scripts = ViridianCity_Mart.mapScripts;

    TEST_PTR_NOT_NULL(scripts);

    // A `mapScripts` table is (tag, pointer) pairs terminated by a 0 tag, and the
    // tags are the map script ids themselves -- so the ON_LOAD entry is at
    // offset 0 and the pointer after it resolves through gNativeScriptPtrs
    // (fix #23). A map left on sEmptyMapScripts has no ON_LOAD tag here at all.
    TEST_EQ(scripts[0], MAP_SCRIPT_ON_LOAD);
    TEST_PTR_NOT_NULL(Test_ScriptPtrAt(scripts, 1));

    // The table must also carry ON_FRAME: that is the parcel scene's entry point
    // (`map_script_2 VAR_MAP_SCENE_VIRIDIAN_CITY_MART, 0, ...ParcelScene`), and
    // without it the scene that grants ITEM_OAKS_PARCEL never runs.
    int onFrame = Test_ScriptFindOpcode(scripts, MAP_SCRIPT_ON_FRAME_TABLE);
    TEST_GE(onFrame, 0);
    TEST_PTR_NOT_NULL(Test_ScriptPtrAt(scripts, onFrame + 1));
}

// --- buying -----------------------------------------------------------------

// Buying must move exactly one item into the bag and exactly its price out of
// the wallet. Asserted as a delta against ItemId_GetPrice rather than a literal
// price, so the contract under test is "you pay the price", not "a POKé BALL
// costs 200".
FIRERED_TEST("shop/buying a poke ball charges its price and adds the item",
             "engine fixture:dex menu:shop", shop_buy)
{
    Test_RequireFixture(FIXTURE_DEX);

    TalkToClerk();

    // `pokemart` stopped the clerk's script and the shop menu now owns the
    // field: input goes to the menu, and the player cannot walk away from it.
    TEST_FALSE(ScriptContext_IsEnabled());
    TEST_TRUE(Test_FieldLocked());

    u32 moneyBefore = Test_Money();
    u16 ballsBefore = BagGetQuantityByItemId(ITEM_POKE_BALL);

    MainCallback field = gMain.callback2;
    Test_Press(A_BUTTON);                    // BUY -> fade into the buy menu
    TEST_TRUE(WaitForCallback(field, 400));
    Test_RunFrames(150);                     // fade in + list build

    // Row 0 is POKé BALL. A opens the quantity box, A accepts 1, A answers YES
    // and A dismisses the thanks -- the money is deducted by the task that runs
    // after that box, so the loop waits for the charge rather than for the item,
    // which arrives one step earlier.
    for (int i = 0; i < 12 && Test_Money() == moneyBefore; i++)
    {
        Test_Press(A_BUTTON);
        Test_RunFrames(60);
    }

    TEST_EQ(BagGetQuantityByItemId(ITEM_POKE_BALL), ballsBefore + 1);
    TEST_EQ(Test_Money(), moneyBefore - ItemId_GetPrice(ITEM_POKE_BALL));
}

// The buy menu composites the map behind it and draws the item list, the money
// box and the description. Engine state cannot see any of that: without the
// INCBIN'd shop_menu assets and the four BG templates the screen would open,
// take input and bill the player while drawing nothing.
FIRERED_TEST("shop/the buy menu renders its list, money box and description",
             "engine fixture:dex menu:shop", shop_buy_renders)
{
    Test_RequireFixture(FIXTURE_DEX);

    static u16 field[240 * 160];

    TalkToClerk();
    Test_RenderFrame();
    memcpy(field, Test_Framebuffer(), sizeof(field));

    MainCallback cb = gMain.callback2;
    Test_Press(A_BUTTON);                    // BUY
    TEST_TRUE(WaitForCallback(cb, 400));
    Test_RunFrames(200);

    Test_RenderFrame();

    // A blank or single-colour screen is the failure this catches; the field it
    // replaced is the control, so the assertion is about the shop's own pixels.
    TEST_GE(Test_DistinctColors(0, 0, 240, 160), 8);
    TEST_GE(Test_RegionDiffers(0, 0, 240, 160, field), 2000);
}

// --- selling ----------------------------------------------------------------

// The SELL branch hands off to the bag menu in its shop location, where the
// item context menu becomes "SELL". The player must gain half the price and
// lose exactly one item.
FIRERED_TEST("shop/selling turns over an item for half its price",
             "engine fixture:dex menu:shop", shop_sell)
{
    Test_RequireFixture(FIXTURE_DEX);

    // The SELL bag opens in the ITEMS pocket; the dex fixture's only items are
    // POKé BALLs, which live in the POKé BALLS pocket. Give the player
    // something the shop buys, the way a playthrough would have.
    AddBagItem(ITEM_POTION, 1);

    u32 moneyBefore = Test_Money();
    u16 potionsBefore = BagGetQuantityByItemId(ITEM_POTION);

    TalkToClerk();

    Test_Press(DPAD_DOWN);                   // BUY -> SELL
    Test_RunFrames(20);

    MainCallback field = gMain.callback2;
    Test_Press(A_BUTTON);                    // SELL -> the bag menu
    TEST_TRUE(WaitForCallback(field, 400));
    Test_RunFrames(150);

    // Bag list -> item context menu -> SELL -> quantity -> confirmation -> YES.
    for (int i = 0; i < 12 && BagGetQuantityByItemId(ITEM_POTION) == potionsBefore; i++)
    {
        Test_Press(A_BUTTON);
        Test_RunFrames(60);
    }

    TEST_EQ(BagGetQuantityByItemId(ITEM_POTION), potionsBefore - 1);
    TEST_EQ(Test_Money(), moneyBefore + ItemId_GetPrice(ITEM_POTION) / 2);
}

// --- leaving ----------------------------------------------------------------

// Quitting the shop returns the field with controls unlocked and the clerk's
// script context re-enabled -- CreatePokemartMenu's own exit callback
// (ScriptContext_Enable) is what restores it, so a shop that never re-enabled
// would leave the player frozen at the counter.
FIRERED_TEST("shop/leaving the shop re-enables the clerk's script", "engine fixture:dex menu:shop",
             shop_exit)
{
    Test_RequireFixture(FIXTURE_DEX);

    TalkToClerk();

    MainCallback field = gMain.callback2;
    Test_Press(A_BUTTON);                    // BUY
    TEST_TRUE(WaitForCallback(field, 400));
    Test_RunFrames(200);

    // B out of the buy menu, then B out of the shop menu.
    for (int i = 0; i < 10; i++)
    {
        Test_Press(B_BUTTON);
        Test_RunFrames(60);
        if (gMain.callback2 == field && !ScriptContext_IsEnabled() && !Test_FieldLocked())
            break;
    }

    TEST_EQ(gMain.callback2, field);
    TEST_FALSE(Test_FieldLocked());
    TEST_EQ(Test_MapGroup(), MAP_GROUP(MAP_VIRIDIAN_CITY_MART));
    TEST_EQ(Test_MapNum(), MAP_NUM(MAP_VIRIDIAN_CITY_MART));
}
