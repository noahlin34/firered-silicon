// On-demand rendering and structural pixel queries.
//
// The PPU is a pure function of engine memory -- src/platform/ppu.c holds no
// mutable file-static state and takes the framebuffer as a parameter -- so a
// test renders into its own buffer and inspects it. No window, no renderer, no
// presentation: the SDL layer is not linked into this binary at all.
//
// Engine state is almost always the better thing to assert on. These helpers
// exist for behaviour that exists only as pixels (a window-masked highlight, a
// transition wipe): ask structural questions ("is this region no longer
// uniform?") rather than pinning exact bytes, because legitimate compositor
// changes move pixels without breaking behaviour (AGENTS.md fixes #51/#52).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

#include "platform/ppu.h"
#include "platform/platform.h"

static uint16_t sFramebuffer[GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT];
static int sRendered;

void Test_RenderFrame(void)
{
    PPU_RenderFrame(sFramebuffer);
    sRendered = 1;
}

const u16 *Test_Framebuffer(void)
{
    if (!sRendered)
        Test_RenderFrame();
    return sFramebuffer;
}

// Counts distinct colours inside a rectangle; 1 means the region is a flat fill.
static int CountColors(int x, int y, int w, int h, const u16 *fb)
{
    uint16_t seen[512];
    int n = 0;
    int px, py;

    if (fb == NULL)
        fb = Test_Framebuffer();

    for (py = y; py < y + h; py++)
    {
        if (py < 0 || py >= GBA_SCREEN_HEIGHT)
            continue;
        for (px = x; px < x + w; px++)
        {
            uint16_t c;
            int i, found = 0;

            if (px < 0 || px >= GBA_SCREEN_WIDTH)
                continue;
            c = fb[py * GBA_SCREEN_WIDTH + px];

            for (i = 0; i < n; i++)
            {
                if (seen[i] == c)
                {
                    found = 1;
                    break;
                }
            }
            if (!found && n < (int)(sizeof(seen) / sizeof(seen[0])))
                seen[n++] = c;
        }
    }

    return n;
}

int Test_DistinctColors(int x, int y, int w, int h)
{
    return CountColors(x, y, w, h, NULL);
}

int Test_RegionIsUniform(int x, int y, int w, int h)
{
    return CountColors(x, y, w, h, NULL) <= 1;
}

// Compares a region against another captured framebuffer; returns the number of
// differing pixels. This is the differential check the port has used by hand:
// capture with the effect, capture without, diff the region.
int Test_RegionDiffers(int x, int y, int w, int h, const u16 *other)
{
    const u16 *fb = Test_Framebuffer();
    int diff = 0;
    int px, py;

    if (other == NULL)
        return 0;

    for (py = y; py < y + h; py++)
    {
        if (py < 0 || py >= GBA_SCREEN_HEIGHT)
            continue;
        for (px = x; px < x + w; px++)
        {
            if (px < 0 || px >= GBA_SCREEN_WIDTH)
                continue;
            if (fb[py * GBA_SCREEN_WIDTH + px] != other[py * GBA_SCREEN_WIDTH + px])
                diff++;
        }
    }

    return diff;
}

u16 *Test_FramebufferMutable(void) { return sFramebuffer; }

// A capture is written directly from the harness's own framebuffer: the BMP is a
// development aid for a failing test, and it never touches a window.
void Test_SaveScreenshot(const char *filename)
{
    FILE *fp;
    int y, col;
    int w = GBA_SCREEN_WIDTH, h = GBA_SCREEN_HEIGHT;
    int rowSize = (w * 3 + 3) & ~3;
    int dataSize = rowSize * h;
    unsigned char header[54];
    unsigned char *line = calloc(1, rowSize);
    const u16 *fb = Test_Framebuffer();

    if (line == NULL)
        return;

    fp = fopen(filename, "wb");
    if (fp == NULL)
    {
        free(line);
        return;
    }

    memset(header, 0, sizeof(header));
    header[0] = 'B';
    header[1] = 'M';
    header[2] = (unsigned char)(54 + dataSize);
    header[3] = (unsigned char)((54 + dataSize) >> 8);
    header[4] = (unsigned char)((54 + dataSize) >> 16);
    header[5] = (unsigned char)((54 + dataSize) >> 24);
    header[10] = 54;
    header[14] = 40;
    header[18] = (unsigned char)w;
    header[19] = (unsigned char)(w >> 8);
    header[22] = (unsigned char)h;
    header[23] = (unsigned char)(h >> 8);
    header[26] = 1;
    header[28] = 24;
    fwrite(header, 1, sizeof(header), fp);

    // BMP rows are bottom-up, and the framebuffer is BGR555.
    for (y = h - 1; y >= 0; y--)
    {
        for (col = 0; col < w; col++)
        {
            uint16_t c = fb[y * w + col];
            unsigned char r = (unsigned char)((c & 0x1F) << 3);
            unsigned char g = (unsigned char)(((c >> 5) & 0x1F) << 3);
            unsigned char b = (unsigned char)(((c >> 10) & 0x1F) << 3);

            line[col * 3 + 0] = (unsigned char)(b | (b >> 5));
            line[col * 3 + 1] = (unsigned char)(g | (g >> 5));
            line[col * 3 + 2] = (unsigned char)(r | (r >> 5));
        }
        fwrite(line, 1, rowSize, fp);
    }

    fclose(fp);
    free(line);
    printf("    (screenshot: %s)\n", filename);
}
