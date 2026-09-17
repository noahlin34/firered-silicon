// Animated tilesets: the swaying flowers, water edges, fountains, gym doors.
//
// src/tileset_anims.c writes successive frames of a small tile block into BG VRAM
// every N frames; the block is ordinary field graphics, so the block itself and the
// pixels the PPU samples from it are the observable. There is no engine state that
// says "a flower is swaying" -- the whole point is that nothing but the tiles move.
//
// The subsystem was inert in this port: src/tileset_anims.c was unlinked and
// src/platform/overworld_stubs.c defined all ten of its entry points as empty
// no-ops (`InitTilesetAnimations`, `UpdateTilesetAnimations`,
// `TransferTilesetAnimsBuffer`, the six per-tileset drivers). UpdateTilesetAnimations
// therefore never queued anything, and `gTileset_General`'s flower -- its only
// callback in the whole primary tileset, used by Route 1, Route 21, Pallet Town and
// most of the overworld -- rendered one frozen frame forever.
//
// Asserted differentially (does the block / do the pixels change?) rather than
// against golden tiles: a legitimate edit to a frame table must not fail this.

#include "registry.h"

#include "constants/maps.h"
#include "gba/defines.h"
#include "gba/io_reg.h"
#include "tileset_anims.h"

#include "global.fieldmap.h"

// The general tileset's flower animation owns BG tiles 508-511 (a 2x2 block), as
// src/tileset_anims.c's QueueAnimTiles_General_Flower states. Metatile 4 of
// data/tilesets/primary/general is the flower that puts them on screen: its top
// layer is exactly these four tiles (see metatiles.bin).
#define FLOWER_TILE   508
#define FLOWER_TILES  (4 * TILE_SIZE_4BPP)
#define FLOWER_FRAMES 5     // the sway is a 5-frame cycle

extern const struct Tileset gTileset_General;
extern void InitTilesetAnim_General(void);

static void ReadFlowerTiles(u8 *out)
{
    memcpy(out, (const void *)(BG_VRAM + TILE_OFFSET_4BPP(FLOWER_TILE)), FLOWER_TILES);
}

// The tileset names the driver that animates it. A NULL callback here is the
// wiring half of the bug: nothing would ever be queued, whatever the driver does.
FIRERED_TEST("tileset-anims/general tileset names its animation driver",
             "tileset-anims pure", tileset_anims_wiring)
{
    TEST_PTR_EQ((void (*)(void))gTileset_General.callback, InitTilesetAnim_General);
}

// The driver body: it must cycle successive flower frames into BG VRAM.
//
// The sway writes a 5-frame cycle every 16 frames, so 80 frames of updates produce
// exactly five tile-block changes and all five frames must differ. The inert port
// produced zero changes -- the static tileset data was never written -- which is
// the failure this pins. Counting *changes* rather than distinct observations keeps
// the untouched static block (the state before the first update) out of the tally.
FIRERED_TEST("tileset-anims/general flower driver cycles frames into BG VRAM",
             "tileset-anims pure", tileset_anims_flower_driver)
{
    u8 frames[FLOWER_FRAMES + 1][FLOWER_TILES];
    u8 prev[FLOWER_TILES], now[FLOWER_TILES];
    int changes = 0;
    int frame, i, j;

    memset(frames, 0, sizeof(frames));
    InitTilesetAnim_General();
    ReadFlowerTiles(prev);

    for (frame = 0; frame < 16 * FLOWER_FRAMES; frame++)
    {
        UpdateTilesetAnimations();
        TransferTilesetAnimsBuffer();
        ReadFlowerTiles(now);

        if (memcmp(prev, now, FLOWER_TILES) == 0)
            continue;

        if (changes < FLOWER_FRAMES + 1)
            memcpy(frames[changes], now, FLOWER_TILES);
        changes++;
        memcpy(prev, now, FLOWER_TILES);
    }

    TEST_EQ(changes, FLOWER_FRAMES);

    // And the frames must genuinely differ from each other, not be copies of the
    // same block (a driver queueing the wrong source would look identical).
    for (i = 0; i < changes && i < FLOWER_FRAMES + 1; i++)
        for (j = i + 1; j < changes && j < FLOWER_FRAMES + 1; j++)
            TEST_NE(memcmp(frames[i], frames[j], FLOWER_TILES), 0);
}

// Build a mask of the screen pixels the PPU samples from the flower's tile block,
// by resolving each pixel through the same BG2 screen map the renderer reads. A
// mask rather than a fixed rectangle: the patch's screen position depends on the
// camera scroll, and this cannot drift out of date the way a hardcoded box would.
static int BuildFlowerMask(u8 *mask)
{
    unsigned int screenbase = (REG_BG2CNT >> 8) & 0x1F;
    unsigned int hoffs = REG_BG2HOFS & 0x1FF;
    unsigned int voffs = REG_BG2VOFS & 0x1FF;
    const u16 *map = (const u16 *)BG_SCREEN_ADDR(screenbase);
    int x, y, count = 0;

    memset(mask, 0, DISPLAY_WIDTH * DISPLAY_HEIGHT);

    for (y = 0; y < DISPLAY_HEIGHT; y++)
    {
        for (x = 0; x < DISPLAY_WIDTH; x++)
        {
            unsigned int xx = (x + hoffs) & 0xFF;
            unsigned int yy = (y + voffs) & 0xFF;
            unsigned int tile = map[(yy / 8) * 32 + xx / 8] & 0x3FF;

            if (tile >= FLOWER_TILE && tile < FLOWER_TILE + 4)
            {
                mask[y * DISPLAY_WIDTH + x] = 1;
                count++;
            }
        }
    }
    return count;
}

// End to end, on the real map, with no input: standing in Pallet Town (primary
// tileset = General; its flower patch is at metatile 4 around (5-8, 12-13)), the
// flower tiles in VRAM must change by themselves AND the pixels drawn from them
// must change on screen. That second half is the bug as the player reported it --
// "the flowers are just static" -- and is what a PPU that never resampled the
// block would still fail after the VRAM half was fixed.
FIRERED_TEST("tileset-anims/flowers sway on the field in Pallet Town",
             "engine fixture:bedroom tileset-anims", tileset_anims_field_flowers)
{
    static u8 mask[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    static u16 before[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    u8 tilesBefore[FLOWER_TILES], tilesNow[FLOWER_TILES];
    const u16 *f;
    int masked, waited;

    Test_RequireFixture(FIXTURE_BEDROOM);

    // South-east of the patch, on an open tile, so the flowers are on screen.
    Test_WarpTo(MAP_GROUP(MAP_PALLET_TOWN), MAP_NUM(MAP_PALLET_TOWN), 9, 13);
    Test_RunFramesToWarp();

    TEST_EQ(Test_MapGroup(), MAP_GROUP(MAP_PALLET_TOWN));
    TEST_EQ(Test_MapNum(), MAP_NUM(MAP_PALLET_TOWN));

    masked = BuildFlowerMask(mask);
    if (masked == 0)
        TEST_FAIL("no flower tiles are on screen at this position");

    Test_RenderFrame();
    memcpy(before, Test_Framebuffer(), sizeof(before));
    ReadFlowerTiles(tilesBefore);

    // Wait for an actual tile update -- up to a full 16-frame cycle plus slack,
    // so this does not depend on where the counter happened to be -- then render
    // again. Comparing before the first change (rather than after N frames) keeps
    // an 80-frame whole-cycle wrap from collapsing the difference to nothing.
    for (waited = 0; waited < 20; )
    {
        Test_RunFrames(1);
        waited++;
        ReadFlowerTiles(tilesNow);
        if (memcmp(tilesBefore, tilesNow, FLOWER_TILES) != 0)
            break;
    }

    TEST_NE(memcmp(tilesBefore, tilesNow, FLOWER_TILES), 0);

    Test_RenderFrame();
    f = Test_Framebuffer();

    {
        int x, y, changed = 0;
        for (y = 0; y < DISPLAY_HEIGHT; y++)
            for (x = 0; x < DISPLAY_WIDTH; x++)
                if (mask[y * DISPLAY_WIDTH + x]
                 && before[y * DISPLAY_WIDTH + x] != f[y * DISPLAY_WIDTH + x])
                    changed++;

        TEST_GE(changed, 1);
    }
}
