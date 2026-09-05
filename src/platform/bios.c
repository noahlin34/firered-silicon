#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "global.h"
#include "platform/platform.h"

void CpuSet(const void *src, void *dest, u32 control)
{
    if (dest == NULL || src == NULL)
        return;

    int count = control & 0x1FFFFF;
    const u8 *source = (const u8 *)src;
    u8 *dst = (u8 *)dest;
    bool is32Bit = (control >> 26) & 1;
    bool isFixed = (control >> 24) & 1;

    if (is32Bit)
    {
        if (isFixed)
        {
            u32 value = *(const u32 *)source;
            while (count > 0)
            {
                *(u32 *)dst = value;
                dst += 4;
                count--;
            }
        }
        else
        {
            while (count > 0)
            {
                *(u32 *)dst = *(const u32 *)source;
                source += 4;
                dst += 4;
                count--;
            }
        }
    }
    else
    {
        if (isFixed)
        {
            u16 value = *(const u16 *)source;
            while (count > 0)
            {
                *(u16 *)dst = value;
                dst += 2;
                count--;
            }
        }
        else
        {
            while (count > 0)
            {
                *(u16 *)dst = *(const u16 *)source;
                source += 2;
                dst += 2;
                count--;
            }
        }
    }
}

void CpuFastSet(const void *src, void *dest, u32 control)
{
    if (dest == NULL || src == NULL)
        return;

    int count = control & 0x1FFFFF;
    const u8 *source = (const u8 *)src;
    u8 *dst = (u8 *)dest;
    bool isFixed = (control >> 24) & 1;

    if (isFixed)
    {
        u32 value = *(const u32 *)source;
        while (count > 0)
        {
            for (int i = 0; i < 8; i++)
            {
                *(u32 *)dst = value;
                dst += 4;
            }
            count -= 8;
        }
    }
    else
    {
        while (count > 0)
        {
            for (int i = 0; i < 8; i++)
            {
                *(u32 *)dst = *(const u32 *)source;
                source += 4;
                dst += 4;
            }
            count -= 8;
        }
    }
}

void LZ77UnCompWram(const void *src, void *dest)
{
    const u8 *source = (const u8 *)src;
    u8 *dst = (u8 *)dest;
    if (!source || !dst) return;

    u32 header = source[0] | (source[1] << 8) | (source[2] << 16) | (source[3] << 24);
    source += 4;
    int len = header >> 8;

    while (len > 0)
    {
        u8 flags = *source++;
        for (int i = 0; i < 8; i++)
        {
            if (flags & 0x80)
            {
                u16 data = (*source++) << 8;
                data |= *source++;
                int length = (data >> 12) + 3;
                int offset = (data & 0x0FFF) + 1;
                const u8 *windowOffset = dst - offset;
                for (int j = 0; j < length; j++)
                {
                    *dst++ = *windowOffset++;
                    len--;
                    if (len == 0) return;
                }
            }
            else
            {
                *dst++ = *source++;
                len--;
                if (len == 0) return;
            }
            flags <<= 1;
        }
    }
}

void LZ77UnCompVram(const void *src, void *dest)
{
    LZ77UnCompWram(src, dest);
}

void RLUnCompWram(const void *src, void *dest)
{
    const u8 *source = (const u8 *)src;
    u8 *dst = (u8 *)dest;
    if (!source || !dst) return;

    u32 header = source[0] | (source[1] << 8) | (source[2] << 16) | (source[3] << 24);
    source += 4;
    int remaining = header >> 8;

    while (remaining > 0)
    {
        u8 blockHeader = *source++;
        if (blockHeader & 0x80)
        {
            int count = (blockHeader & 0x7F) + 3;
            u8 byte = *source++;
            while (count-- && remaining)
            {
                *dst++ = byte;
                remaining--;
            }
        }
        else
        {
            int count = blockHeader + 1;
            while (count-- && remaining)
            {
                *dst++ = *source++;
                remaining--;
            }
        }
    }
}

void RLUnCompVram(const void *src, void *dest)
{
    RLUnCompWram(src, dest);
}

s32 Div(s32 num, s32 denom)
{
    if (denom == 0)
        return 0;
    return num / denom;
}

u16 Sqrt(u32 num)
{
    return (u16)sqrt((double)num);
}

u16 ArcTan2(s16 x, s16 y)
{
    double angle = atan2((double)y, (double)x);
    if (angle < 0)
        angle += 2.0 * M_PI;
    return (u16)((angle / (2.0 * M_PI)) * 65536.0);
}

void RegisterRamReset(u32 resetFlags)
{
    if (resetFlags & RESET_PALETTE)
        memset(PLTT_, 0, sizeof(PLTT_));
    if (resetFlags & RESET_VRAM)
        memset(VRAM_, 0, sizeof(VRAM_));
    if (resetFlags & RESET_OAM)
        memset(OAM_, 0, sizeof(OAM_));
    if (resetFlags & RESET_REGS)
        memset(REG_BASE, 0, sizeof(REG_BASE));
}

void SoftReset(u32 resetFlags)
{
    (void)resetFlags;
}

void VBlankIntrWait(void)
{
    Platform_UpdateInput();
}
