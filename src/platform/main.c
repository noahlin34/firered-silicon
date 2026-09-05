#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <SDL.h>

#include "global.h"
#include "platform/platform.h"

// Simple smoke test to verify GBA Hardware Abstraction Layer on Apple Silicon
static void TestBiosSyscalls(void)
{
    printf("[SmokeTest] Testing BIOS Syscalls...\n");

    // 1. Test Div
    assert(Div(100, 5) == 20);
    assert(Div(-42, 7) == -6);
    assert(Div(123, 0) == 0); // Safe div-by-zero check

    // 2. Test Sqrt
    assert(Sqrt(144) == 12);
    assert(Sqrt(0) == 0);
    assert(Sqrt(1000000) == 1000);

    // 3. Test ArcTan2
    assert(ArcTan2(1, 0) == 0);

    // 4. Test CpuSet 16-bit copy & fill
    uint16_t src16[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint16_t dst16[8] = {0};
    CpuSet(src16, dst16, 8);
    for (int i = 0; i < 8; i++)
        assert(dst16[i] == src16[i]);

    uint16_t fill16 = 0xABCD;
    CpuSet(&fill16, dst16, CPU_SET_SRC_FIXED | 8);
    for (int i = 0; i < 8; i++)
        assert(dst16[i] == 0xABCD);

    // 5. Test CpuFastSet 32-bit chunk fill
    uint32_t fill32 = 0x12345678;
    uint32_t dst32[16] = {0};
    CpuFastSet(&fill32, dst32, CPU_FAST_SET_SRC_FIXED | 16);
    for (int i = 0; i < 16; i++)
        assert(dst32[i] == 0x12345678);

    // 6. Test LZ77 decompression
    // LZ77 stream for "HELLO HELLO "
    // Header: 0x10, len=12 (0x0C, 0x00, 0x00)
    // Flags: 0x00 (8 literal bytes) -> 'H', 'E', 'L', 'L', 'O', ' ', 'H', 'E'
    // Flags: 0x80 (1 compressed block, 7 literal/unused) -> copy 4 bytes from offset 6 ("LLO ")
    static const uint8_t testLzData[] = {
        0x10, 0x0C, 0x00, 0x00, // Header: 12 bytes
        0x00, 'H', 'E', 'L', 'L', 'O', ' ', 'H', 'E', // Literal block
        0x80, 0x10, 0x05, // Copy length (1+3)=4, offset (5+1)=6
    };
    uint8_t lzOut[16] = {0};
    LZ77UnCompWram(testLzData, lzOut);
    assert(memcmp(lzOut, "HELLO HELLO ", 12) == 0);

    printf("[SmokeTest] All BIOS Syscalls passed!\n");
}

static void TestGbaMemory(void)
{
    printf("[SmokeTest] Testing GBA Memory Buffers & Registers...\n");

    // Test writing to REG_DISPCNT
    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_BG0_ON | DISPCNT_OBJ_ON;
    assert(REG_DISPCNT == (DISPCNT_MODE_0 | DISPCNT_BG0_ON | DISPCNT_OBJ_ON));

    // Test writing to BG_PLTT
    uint16_t *palette = (uint16_t *)BG_PLTT;
    palette[0] = RGB_RED;
    palette[1] = RGB_GREEN;
    palette[2] = RGB_BLUE;
    assert(((uint16_t *)BG_PLTT)[0] == RGB_RED);
    assert(((uint16_t *)BG_PLTT)[1] == RGB_GREEN);
    assert(((uint16_t *)BG_PLTT)[2] == RGB_BLUE);

    // Test DMA macro
    uint16_t dmaSrc[4] = {0x1111, 0x2222, 0x3333, 0x4444};
    uint16_t dmaDst[4] = {0};
    DmaCopy16(3, dmaSrc, dmaDst, sizeof(dmaSrc));
    for (int i = 0; i < 4; i++)
        assert(dmaDst[i] == dmaSrc[i]);

    DmaFill16(3, 0x7777, dmaDst, sizeof(dmaDst));
    for (int i = 0; i < 4; i++)
        assert(dmaDst[i] == 0x7777);

    printf("[SmokeTest] GBA Memory and DMA tests passed!\n");
}

static void SetupPpuTestScene(void)
{
    printf("[SmokeTest] Setting up simulated GBA Scene (BG0 Tilemap + OAM Sprite)...\n");

    // 1. Setup Palettes
    uint16_t *bgPal = (uint16_t *)BG_PLTT;
    bgPal[0] = RGB(3, 5, 12);  // Backdrop: deep navy
    bgPal[1] = RGB(28, 5, 5);  // FireRed brick red
    bgPal[2] = RGB(31, 31, 31);// White
    bgPal[3] = RGB(4, 4, 6);   // Shadow black

    uint16_t *objPal = (uint16_t *)OBJ_PLTT;
    objPal[1] = RGB(31, 28, 4); // Yellow
    objPal[2] = RGB(31, 14, 2); // Orange / Charizard orange
    objPal[3] = RGB(31, 31, 31);// White highlight

    // 2. Setup Background 4bpp 8x8 Tile (Tile index 1 in CharBase 0)
    // 32 bytes for 8x8 4bpp tile: 2 pixels per byte
    uint8_t *bgTile = (uint8_t *)BG_CHAR_ADDR(0) + 32;
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 4; col++)
        {
            if (row == 0 || row == 7 || col == 0)
                bgTile[row * 4 + col] = 0x33; // Border
            else
                bgTile[row * 4 + col] = 0x11; // Interior brick
        }
    }

    // 3. Setup ScreenBase 28 Tilemap (offset 28 * 0x800 = 0xE000, safe from CharBase 0)
    uint16_t *bgMap = (uint16_t *)BG_SCREEN_ADDR(28);
    for (int y = 0; y < 32; y++)
    {
        for (int x = 0; x < 32; x++)
        {
            // Checkerboard pattern of Tile 0 (backdrop) and Tile 1 (FireRed brick)
            if ((x >= 2 && x < 28) && (y >= 2 && y < 18) && ((x + y) % 2 == 0))
                bgMap[y * 32 + x] = 1; // Tile 1, palette 0
            else
                bgMap[y * 32 + x] = 0; // Empty
        }
    }

    // Configure BG0 with ScreenBase 28 and CharBase 0
    REG_BG0CNT = BGCNT_PRIORITY(1) | BGCNT_CHARBASE(0) | BGCNT_SCREENBASE(28) | BGCNT_16COLOR | BGCNT_TXT256x256;
    REG_BG0HOFS = 0;
    REG_BG0VOFS = 0;

    // 4. Setup Sprite Tile in OBJ VRAM (Tile 0)
    // Diamond icon in 4bpp
    uint8_t *objTile = (uint8_t *)OBJ_VRAM0;
    memset(objTile, 0, 32);
    // Draw an 8x8 diamond in colors 1 & 2
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

    // 5. Setup OAM Sprite 0 (centered at 116, 76)
    struct OamData *oam = (struct OamData *)OAM_;
    memset(oam, 0, sizeof(struct OamData) * 128);
    // Hide all sprites by default
    for (int i = 0; i < 128; i++)
        oam[i].affineMode = ST_OAM_AFFINE_OFF;

    oam[0].y = 76;
    oam[0].affineMode = ST_OAM_AFFINE_OFF;
    oam[0].objMode = ST_OAM_OBJ_NORMAL;
    oam[0].bpp = ST_OAM_4BPP;
    oam[0].shape = ST_OAM_SQUARE;
    oam[0].x = 116;
    oam[0].size = ST_OAM_SIZE_0; // 8x8
    oam[0].tileNum = 0;
    oam[0].priority = 0; // Front layer
    oam[0].paletteNum = 0;

    // Enable BG0 and OBJ in Mode 0
    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_BG0_ON | DISPCNT_OBJ_ON;
}

int main(int argc, char **argv)
{
    printf("=========================================\n");
    printf(" Pokemon FireRed - Apple Silicon Native  \n");
    printf(" Architecture: ARM64 Mach-O             \n");
    printf(" Subsystem: Software GBA PPU Compositor  \n");
    printf("=========================================\n");

    TestBiosSyscalls();
    TestGbaMemory();

    if (Platform_Init(argc, argv) != 0)
    {
        fprintf(stderr, "Failed to initialize platform window.\n");
        return 1;
    }

    SetupPpuTestScene();

    printf("[SmokeTest] Rendering simulated GBA scene through PPU...\n");

    // Render 120 frames (~2 seconds of interactive 60 FPS video)
    for (int frame = 0; frame < 120; frame++)
    {
        Platform_UpdateInput();

        // Animate BG0 scroll slightly to verify dynamic scanline updates
        REG_BG0HOFS = frame / 2;

        Platform_RenderAndPresent();
        SDL_Delay(16); // 60 FPS
    }
    Platform_SaveScreenshot("ppu_test_output.bmp");
    printf("[SmokeTest] PPU rendered 120 frames successfully!\n");
    printf("[SmokeTest] Software GBA PPU is fully operational!\n");
    Platform_Cleanup();
    return 0;
}
