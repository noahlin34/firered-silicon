// GBA hardware abstraction: host-backed register/VRAM/palette buffers and the
// PORTABLE DMA macros.
//
// Pure. This is the layer that makes the port possible at all: GBA physical
// addresses (0x04000000 I/O, 0x05000000 palette, 0x06000000 VRAM) would segfault
// if dereferenced on macOS, so they are host buffers, and DMA macros are
// synchronous memcpy/memset rather than register writes (AGENTS.md fixes #3/#4).

#include "registry.h"

#include "constants/rgb.h"
#include "gba/defines.h"
#include "gba/io_reg.h"
#include "gba/macro.h"

// A register write must land in the host buffer and read back identically --
// including the display control word, which every PPU path depends on.
FIRERED_TEST("memory/io registers read back what was written", "memory pure", memory_registers)
{
    REG_DISPCNT = DISPCNT_MODE_0 | DISPCNT_BG0_ON | DISPCNT_OBJ_ON;
    TEST_EQ(REG_DISPCNT, DISPCNT_MODE_0 | DISPCNT_BG0_ON | DISPCNT_OBJ_ON);
}

// Palette RAM is a host buffer, not a mapped address. RGB() packs 15-bit BGR555.
FIRERED_TEST("memory/palette ram is writable host memory", "memory pure", memory_palette)
{
    u16 *palette = (u16 *)BG_PLTT;

    palette[0] = RGB_RED;
    palette[1] = RGB_GREEN;
    palette[2] = RGB_BLUE;

    TEST_EQ(((u16 *)BG_PLTT)[0], RGB_RED);
    TEST_EQ(((u16 *)BG_PLTT)[1], RGB_GREEN);
    TEST_EQ(((u16 *)BG_PLTT)[2], RGB_BLUE);
}

// DMA must be synchronous under PORTABLE. On hardware these are register
// transfers; here they are immediate copies, and the engine depends on the data
// being present when the call returns (it does not wait for a completion flag).
FIRERED_TEST("memory/dma copies immediately", "memory pure", memory_dma_copy)
{
    u16 src[4] = { 0x1111, 0x2222, 0x3333, 0x4444 };
    u16 dst[4] = { 0 };
    int i;

    DmaCopy16(3, src, dst, sizeof(src));
    for (i = 0; i < 4; i++)
        TEST_EQ(dst[i], src[i]);
}

FIRERED_TEST("memory/dma fills immediately", "memory pure", memory_dma_fill)
{
    u16 dst[4] = { 0 };
    int i;

    DmaFill16(3, 0x7777, dst, sizeof(dst));
    for (i = 0; i < 4; i++)
        TEST_EQ(dst[i], 0x7777);
}

// The 64-bit hazard the whole port is built around: a DMA macro must not
// truncate a host pointer to 32 bits (fix #3). A src/dst pair above the 4 GB
// line cannot be arranged here, but a copy through the DMA path proves the
// macro routes through the portable implementation rather than the GBA register
// write that would truncate. Verified with heap-allocated buffers so the
// addresses are real host pointers.
FIRERED_TEST("memory/dma does not truncate host pointers", "memory pure", memory_dma_pointers)
{
    u16 *src = (u16 *)malloc(sizeof(u16) * 8);
    u16 *dst = (u16 *)malloc(sizeof(u16) * 8);
    int i, ok = 1;

    if (src == NULL || dst == NULL)
    {
        free(src);
        free(dst);
        TEST_FAIL("allocation failed");
        return;
    }

    for (i = 0; i < 8; i++)
        src[i] = (u16)(0x1000 + i);
    for (i = 0; i < 8; i++)
        dst[i] = 0;

    DmaCopy16(3, src, dst, sizeof(u16) * 8);

    for (i = 0; i < 8; i++)
    {
        if (dst[i] != src[i])
            ok = 0;
    }
    TEST_EQ(ok, 1);

    free(src);
    free(dst);
}
