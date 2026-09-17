// The Pokédex screen (START menu -> POKéDEX).
//
// Before this, selecting POKéDEX did nothing at all: src/pokedex_screen.c was
// unlinked and src/platform/overworld_stubs.c defined CB2_OpenPokedexFromStartMenu
// as `printf("[Menu] POKéDEX scene is not ported")`. StartMenuPokedexCallback
// installed that stub as the main callback2 and returned TRUE, so the START menu
// closed and the field came back with no dex and no error. DexScreen_GetSetPokedexFlag
// and DexScreen_RegisterMonToPokedex were hand-mirrored stubs for the same reason.
//
// These tests drive the real user path -- START, choose POKéDEX, and the screen
// that opens -- and then close it again, because a scene that cannot hand the
// field back is as broken as one that never opens.
//
// Two fixtures are used: `fixture:dex` boots with Oak's parcel already delivered
// (Platform_DevBootApplyDexProgress), which is the shortest way to a save that
// owns a dex; `fixture:lab` starts before the delivery and drives the hand-over
// itself. Either way the START menu only lists POKéDEX while
// FLAG_SYS_POKEDEX_GET is set, and StartMenuPokedexSanityCheck refuses to open a
// dex that has counted nothing.

#include "registry.h"

#include "constants/flags.h"
#include "constants/items.h"
#include "constants/maps.h"
#include "constants/pokedex.h"
#include "constants/vars.h"
#include "gba/defines.h"
#include "gba/io_reg.h"
#include "item.h"
#include "main.h"
#include "overworld.h"
#include "pokedex.h"

// START opens the menu with POKéDEX on top (SetUpStartMenu_NormalField puts it
// first once the flag is set), and A on the first row enters it. The dex fades
// the field out, loads four BG screens and its own VBlank callback.
//
// Asserted on what the player observes: the scene owns callback2 (the field is
// gone), pixels were drawn, and B gives the field back with controls unlocked.
FIRERED_TEST("pokedex/start menu opens the dex and B returns to the field",
             "engine fixture:dex menu:pokedex", pokedex_open_and_close)
{
    // The precondition the fixture set up: the START menu offers POKéDEX.
    TEST_TRUE(Test_Flag(FLAG_SYS_POKEDEX_GET));
    TEST_TRUE(Test_HasItem(ITEM_POKE_BALL, 5));   // handed over with the dex

    // callback1 stays CB1_Overworld while a field menu owns the frame, so the
    // scene that actually changes is callback2.
    MainCallback overworldCB = gMain.callback2;
    TEST_PTR_NOT_NULL((const void *)overworldCB);

    Test_Press(START_BUTTON);
    Test_RunFrames(40);                           // the menu draws and locks the field
    TEST_TRUE(Test_FieldLocked());                // the START menu holds the field

    // POKéDEX is the first entry, so A selects it directly -- no cursor movement.
    Test_Press(A_BUTTON);
    Test_RunFrames(240);                          // fade out, load, fade in

    // The dex owns the frame now. Before this it did not: the stub returned
    // without installing any callback, so the field simply came back.
    TEST_NE((const void *)gMain.callback2, (const void *)overworldCB);

    // And it drew its own screen: the dex decompresses its background tiles into
    // BG VRAM block 0 (sBgTemplates' bg 3 has charBaseIndex 0, offset 0), so the
    // exact decompressed asset must be there. A pixel-count check would pass on
    // any non-blank frame; this compares against the real asset.
    {
        static ALIGNED(4) u8 tiles[TILE_SIZE_4BPP];
        extern const u32 sKantoDexTiles[];

        LZ77UnCompWram(sKantoDexTiles, tiles);
        TEST_MEM_EQ((const void *)BG_VRAM, tiles, TILE_SIZE_4BPP);
    }

    // B closes the dex; the engine returns to the field *with the START menu
    // open* (CB2_ReturnToFieldWithOpenMenu), so the field is still locked here.
    Test_Press(B_BUTTON);
    Test_RunFrames(300);
    TEST_PTR_EQ((const void *)gMain.callback2, (const void *)overworldCB);
    TEST_EQ(Test_MapGroup(), MAP_GROUP(MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB));
    TEST_TRUE(Test_FieldLocked());        // the reopened START menu holds the field

    // ...and B on the menu hands the field back, unlocked.
    Test_Press(B_BUTTON);
    Test_RunUntilIdle(600);
    TEST_TRUE(Test_InOverworld());
    TEST_FALSE(Test_FieldLocked());
}

// The dex's three levels -- table of contents, list, and a mon's page -- must be
// navigable, and each must actually draw. Asserted on the pixels each level
// produces from the level before it, because "which screen is this" is the whole
// contract here; the numbers behind it (seen/owned) are covered by the parcel
// test below.
//
// The cursor starts on NUMERICAL MODE, so A goes straight into the Kanto list
// without moving; with only Bulbasaur recorded the list has exactly one entry and
// A opens its dex page. That is the shortest real path through the screen.
FIRERED_TEST("pokedex/table of contents drills into a mon's page and back",
             "engine fixture:dex menu:pokedex", pokedex_drill_down)
{
    static u16 topMenu[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    static u16 monList[DISPLAY_WIDTH * DISPLAY_HEIGHT];

    Test_Press(START_BUTTON);
    Test_RunFrames(40);
    Test_Press(A_BUTTON);          // select POKéDEX (first entry)
    Test_RunFrames(240);           // fade out, load, fade in

    Test_RenderFrame();
    memcpy(topMenu, Test_Framebuffer(), sizeof(topMenu));

    Test_Press(A_BUTTON);          // NUMERICAL MODE -> the Kanto list
    Test_RunFrames(240);
    Test_RenderFrame();

    TEST_GE(Test_RegionDiffers(0, 0, 240, 160, topMenu), 2000);
    memcpy(monList, Test_Framebuffer(), sizeof(monList));

    Test_Press(A_BUTTON);          // the one entry -> its dex page
    Test_RunFrames(300);
    Test_RenderFrame();

    TEST_GE(Test_RegionDiffers(0, 0, 240, 160, monList), 2000);

    // The mon's own page must show the mon: its front pic is drawn into the page,
    // so the picture area is not uniform.
    TEST_GE(Test_DistinctColors(80, 20, 80, 80), 4);

    // Back returns to the list the mon page came from -- the same screen, not
    // merely a different one.
    Test_Press(B_BUTTON);
    Test_RunFrames(300);
    Test_RenderFrame();
    TEST_EQ(Test_RegionDiffers(0, 0, 240, 160, monList), 0);

    // B again returns to the table of contents. Small differences are allowed
    // here: the cursor arrow and the category icon blink on their own timer.
    Test_Press(B_BUTTON);
    Test_RunFrames(300);
    Test_RenderFrame();
    TEST_GE(200, Test_RegionDiffers(0, 0, 240, 160, topMenu));
}

// End to end on the real user path: the Pokédex is what delivering Oak's parcel
// leaves you with, and the START menu's POKéDEX entry opens the screen that
// parcel paid for.
//
// This is the whole chain the milestone describes, driven through the engine's
// own scripts rather than by writing the flags the scripts write: the Viridian
// Mart clerk's ParcelScene sets VAR_MAP_SCENE_VIRIDIAN_CITY_MART to 1 and hands
// over ITEM_OAKS_PARCEL; Oak's script then branches on that var into
// ReceiveDexScene, which is where FLAG_SYS_POKEDEX_GET and the five Poké Balls
// come from. The Mart half of that scene has its own coverage (the parcel chain);
// here the parcel is placed in the bag and the var set, so the test asserts the
// hand-over and everything downstream of it.
FIRERED_TEST("pokedex/delivering Oak's parcel yields a working dex",
             "engine fixture:lab menu:pokedex", pokedex_from_parcel)
{
    MainCallback overworldCB = gMain.callback2;

    // The state ViridianCity_Mart_EventScript_ParcelScene leaves behind.
    TEST_EQ(AddBagItem(ITEM_OAKS_PARCEL, 1), TRUE);
    Test_SetVar(VAR_MAP_SCENE_VIRIDIAN_CITY_MART, 1);

    TEST_FALSE(Test_Flag(FLAG_SYS_POKEDEX_GET));   // not yet

    // Stand below Oak and face him; his script does its own faceplayer/lock.
    Test_WarpTo(MAP_GROUP(MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB),
                MAP_NUM(MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB), 6, 4);
    Test_RunFramesToWarp();
    Test_Press(DPAD_UP);
    Test_RunFrames(3);

    // The scene runs the rival's entrance, Oak's walk and a dozen boxes; press A
    // until it hands the field back.
    Test_PressUntilIdle(A_BUTTON, 12000);
    Test_RunUntilIdle(600);

    // What the scene grants and consumes.
    TEST_FALSE(Test_HasItem(ITEM_OAKS_PARCEL, 1));
    TEST_TRUE(Test_Flag(FLAG_SYS_POKEDEX_GET));
    TEST_TRUE(Test_HasItem(ITEM_POKE_BALL, 5));
    TEST_EQ(Test_Var(VAR_MAP_SCENE_PALLET_TOWN_PROFESSOR_OAKS_LAB), 6);
    TEST_EQ(Test_Var(VAR_MAP_SCENE_VIRIDIAN_CITY_MART), 2);
    TEST_TRUE(Test_Flag(FLAG_HIDE_POKEDEX));   // the desk units came with it

    // And the menu entry that was dead now opens the screen.
    Test_Press(START_BUTTON);
    Test_RunFrames(40);
    Test_Press(A_BUTTON);
    Test_RunFrames(240);
    TEST_NE((const void *)gMain.callback2, (const void *)overworldCB);

    // Back out: dex -> START menu -> field.
    Test_Press(B_BUTTON);
    Test_RunFrames(300);
    Test_Press(B_BUTTON);
    Test_RunUntilIdle(600);
    TEST_PTR_EQ((const void *)gMain.callback2, (const void *)overworldCB);
    TEST_FALSE(Test_FieldLocked());
}
