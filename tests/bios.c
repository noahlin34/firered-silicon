// BIOS syscall tests (src/platform/bios.c).
//
// Pure: no engine boot, no save blocks. These were the first two smoke tests in
// src/platform/main.c, split into one concern per test so a failure names the
// syscall instead of "the smoke test failed".

#include <string.h>

#include "registry.h"

// Div/Sqrt/ArcTan2 are the GBA BIOS arithmetic SWIs the engine calls directly.
FIRERED_TEST("bios/div", "bios pure", bios_div)
{
    TEST_EQ(Div(100, 5), 20);
    TEST_EQ(Div(-42, 7), -6);
    TEST_EQ(Div(123, 0), 0);      // divide-by-zero is defined as 0, not a trap
}

FIRERED_TEST("bios/sqrt", "bios pure", bios_sqrt)
{
    TEST_EQ(Sqrt(144), 12);
    TEST_EQ(Sqrt(0), 0);
    TEST_EQ(Sqrt(1000000), 1000);
}

FIRERED_TEST("bios/arctan2", "bios pure", bios_arctan2)
{
    TEST_EQ(ArcTan2(1, 0), 0);
}

// CpuSet/CpuFastSet are the fill/copy primitives the engine uses for VRAM and
// palette work. Both take a "source is a fixed value" flag bit that must be
// respected, since the fill paths depend on it.
FIRERED_TEST("bios/cpuset copies halfwords", "bios pure", bios_cpuset_copy)
{
    u16 src[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    u16 dst[8] = { 0 };
    int i;

    CpuSet(src, dst, 8);
    for (i = 0; i < 8; i++)
        TEST_EQ(dst[i], src[i]);
}

FIRERED_TEST("bios/cpuset fills from a fixed source", "bios pure", bios_cpuset_fill)
{
    u16 fill = 0xABCD;
    u16 dst[8] = { 0 };
    int i;

    CpuSet(&fill, dst, CPU_SET_SRC_FIXED | 8);
    for (i = 0; i < 8; i++)
        TEST_EQ(dst[i], 0xABCD);
}

FIRERED_TEST("bios/cpufastset fills 32-bit units", "bios pure", bios_cpufastset_fill)
{
    u32 fill = 0x12345678;
    u32 dst[16] = { 0 };
    int i;

    CpuFastSet(&fill, dst, CPU_FAST_SET_SRC_FIXED | 16);
    for (i = 0; i < 16; i++)
        TEST_EQ(dst[i], 0x12345678);
}

// LZ77 decompression. The stream encodes "HELLO HELLO " as 8 literals followed
// by a back-reference of length 4 at offset 6 -- i.e. the copy overlaps the
// window it is reading from, which is the case a naive implementation gets wrong.
FIRERED_TEST("bios/lz77 decompresses with an overlapping back-reference", "bios pure", bios_lz77)
{
    static const u8 compressed[] = {
        0x10, 0x0C, 0x00, 0x00,                     // header: 12 bytes uncompressed
        0x00, 'H', 'E', 'L', 'L', 'O', ' ', 'H', 'E', // 8 literals
        0x80, 0x10, 0x05,                           // copy 4 bytes from offset 6
    };
    u8 out[16] = { 0 };

    LZ77UnCompWram(compressed, out);
    TEST_MEM_EQ(out, "HELLO HELLO ", 12);
}
