// Software PPU compositor.
//
// The PPU is a pure function of engine memory -- it holds no mutable file-static
// state and takes the framebuffer as a parameter -- so these tests build a scene
// in VRAM/palettes/OAM, render into their own buffer, and inspect the result. No
// window, no renderer: the SDL layer is not linked into this binary at all.
//
// The standalone scene test that used to live in src/platform/main.c (`--test`)
// rendered 120 frames and saved a screenshot; it asserted nothing. These assert.

#include <string.h>

#include "registry.h"

#include "gba/defines.h"
#include "gba/io_reg.h"
#include "gba/macro.h"
#include "platform/platform.h"
#include "platform/ppu.h"

// Builds the same simulated scene the old `--test` mode did: a BG0 checkerboard
// of one 4bpp tile over a backdrop, plus one 8x8 OAM sprite.
static void SetupTestScene(void)
{
    u16 *bgPal = (u16 *)BG_PLTT;
    u16 *objPal = (u16 *)OBJ_PLTT;
    u8 *bgTile = (u8 *)BG_CHAR_ADDR(0) + 32;
    u16 *bgMap = (u16 *)BG_SCREEN_ADDR(28);
    u8 *objTile = (u8 *)OBJ_VRAM0;
    struct OamData *oam = (struct OamData *)OAM_;
    int row, col, x, y;

    bgPal[0] = RGB(3, 5, 12);   // backdrop: deep navy
    bgPal[1] = RGB(28, 5, 5);   // brick red
    bgPal[2] = RGB(31, 31, 31); // white
    bgPal[3] = RGB(4, 4, 6);    // shadow black

    objPal[1] = RGB(31, 28, 4);
    objPal[2] = RGB(31, 14, 2);
    objPal[3] = RGB(31, 31, 31);

    // One 8x8 4bpp tile with a border, into tile slot 1.
    for (row = 0; row < 8; row++)
    {
        for (col = 0; col < 4; col++)
        {
            if (row == 0 || row == 7 || col == 0)
                bgTile[row * 4 + col] = 0x33;
            else
                bgTile[row * 4 + col] = 0x11;
        }
    }

    for (y = 0; y < 32; y++)
    {
        for (x = 0; x < 32; x++)
        {
            if ((x >= 2 && x < 28) && (y >= 2 && y < 18) && ((x + y) % 2 == 0))
                bgMap[y * 32 + x] = 1;
            else
                bgMap[y * 32 + x] = 0;
        }
    }

    REG_BG0CNT = BGCNT_PRIORITY(1) | BGCNT_CHARBASE(0) | BGCNT_SCREENBASE(28)
               | BGCNT_16COLOR | BGCNT_TXT256x256;
    REG_BG0HOFS = 0;
    REG_BG0VOFS = 0;

    // An 8x8 diamond in the sprite's palette.
    memset(objTile, 0, 32);
    objTile[1 * 4 + 1] = 0x01;
    objTile[1 * 4 + 2] = 0x10;
    objTile[2 * 4 + 1] = 0x22;
    objTile[2 * 4 + 2] = 0x22;
    objTile[3 * 4 + 0] = 0x01;
    objTile[3 * 4 + 1] = 0x22;
    objTile[3 * 4 + 2] = 0x22;
    objTile[3 * 4 + 3] = 0x10;
    objTile[4 * 4 + 0] = 0x01;
    objTile[4 * 4 + 1] = 0x22;
    objTile[4 * 4 + 2] = 0x22;
    objTile[4 * 4 + 3] = 0x10;
    objTile[5 * 4 + 1] = 0x22;
    objTile[5 * 4 + 2] = 0x22;
    objTile[6 * 4 + 1] = 0x01;
    objTile[6 * 4 + 2] = 0x10;

    memset(oam, 0, sizeof(struct OamData) * 128);
    for (x = 0; x < 128; x++)
        oam[x].affineMode = ST_OAM_AFFINE_OFF;

    oam[0].y = 76;
    oam[0].affineMode = ST_OAM_AFFINE_OFF;
    oam[0].objMode = ST_OAM_OBJ_NORMAL;
    oam[0].bpp = ST_OAM_4BPP;
    oam[0].shape = ST_OAM_SQUARE;
    oam[0].x = 116;
    oam[0].size = ST_OAM_SIZE_0;
    oam[0].tileNum = 0;
    oam[0].priority = 0;
    oam[0].paletteNum = 0;

    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_BG0_ON | DISPCNT_OBJ_ON;
}

// A Mode 0 text background composites its tilemap: the checkerboard means the
// rendered frame must contain more than one colour, and the sprite region must
// differ from what the background alone would draw there.
FIRERED_TEST("ppu/mode 0 background composites", "ppu pure", ppu_mode0_background)
{
    SetupTestScene();
    Test_RenderFrame();

    TEST_GE(Test_DistinctColors(0, 0, 240, 160), 2);
    TEST_FALSE(Test_RegionIsUniform(0, 0, 240, 160));
}

// Scrolling must move what is drawn: the same scene at two scroll offsets cannot
// produce identical frames.
FIRERED_TEST("ppu/background scroll changes the rendered frame", "ppu pure", ppu_bg_scroll)
{
    u16 baseline[240 * 160];
    int differed;

    SetupTestScene();
    REG_BG0HOFS = 0;
    Test_RenderFrame();
    memcpy(baseline, Test_Framebuffer(), sizeof(baseline));

    REG_BG0HOFS = 16;
    Test_RenderFrame();
    differed = Test_RegionDiffers(0, 0, 240, 160, baseline);

    TEST_GE(differed, 1);
}

// An OAM sprite must be drawn where its OAM entry says. Moving the sprite's
// coordinates must move the drawn pixels -- asserted differentially so the test
// does not pin exact colours.
FIRERED_TEST("ppu/sprite draws at its oam coordinates", "ppu pure", ppu_sprite_position)
{
    u16 withoutSprite[240 * 160];
    struct OamData *oam = (struct OamData *)OAM_;

    SetupTestScene();

    // Capture with the sprite hidden...
    oam[0].affineMode = ST_OAM_AFFINE_OFF | 2;   // bit 1 = disabled
    Test_RenderFrame();
    memcpy(withoutSprite, Test_Framebuffer(), sizeof(withoutSprite));

    // ...then re-enable it, so the harness's current framebuffer is the
    // sprite-drawn one that Test_RegionDiffers compares against.
    oam[0].affineMode = ST_OAM_AFFINE_OFF;
    Test_RenderFrame();

    // The sprite's own 8x8 box must differ by its opaque pixels (a diamond, so
    // fewer than 64 but more than zero).
    TEST_GE(Test_RegionDiffers(116, 76, 8, 8, withoutSprite), 1);

    // ...and nothing outside that box may change: a compositor that mis-indexes
    // OAM, or ignores the object-mode/priority rules, paints outside it.
    TEST_EQ(Test_RegionDiffers(0, 0, 116, 160, withoutSprite), 0);
    TEST_EQ(Test_RegionDiffers(124, 0, 116, 160, withoutSprite), 0);
    TEST_EQ(Test_RegionDiffers(116, 0, 8, 76, withoutSprite), 0);
    TEST_EQ(Test_RegionDiffers(116, 84, 8, 76, withoutSprite), 0);
}

// HBlank-timed DMA must write the SAME register on every scanline.
//
// The control word packs its flags in the high halfword and the transfer count in
// the low one; reading a step bit out of the low halfword reads the count, which
// decodes as "destination increments", so each scanline's write walks one
// register further through the I/O block -- clobbering WIN0H/WIN0V/BLDCNT/BLDY
// and tearing the battle transitions (AGENTS.md fix #52). Consecutive
// destinations make that walk observable.
FIRERED_TEST("ppu/hblank dma writes one register every scanline", "ppu pure", ppu_hblank_dma)
{
    static const u16 src[] = { 0x1111, 0x2222, 0x3333, 0x4444 };
    volatile u16 dest[4] = { 0, 0, 0, 0 };
    int line;

    DmaSet(0, src, (void *)&dest[0],
           1 | ((DMA_SRC_INC | DMA_DEST_FIXED | DMA_REPEAT | DMA_16BIT
                 | DMA_START_HBLANK | DMA_ENABLE) << 16));

    for (line = 0; line < 4; line++)
    {
        Platform_RunHBlankDma();
        TEST_EQ(dest[0], src[line]);   // pinned register, advancing source
        TEST_EQ(dest[1], 0);           // and nothing past it
        TEST_EQ(dest[2], 0);
        TEST_EQ(dest[3], 0);
    }

    DmaStop(0);
    TEST_EQ(dest[0], src[3]);

    // A stopped channel must not keep writing.
    dest[0] = 0;
    Platform_RunHBlankDma();
    TEST_EQ(dest[0], 0);
}
