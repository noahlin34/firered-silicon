#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "global.h"
#include "main.h"
#include "platform/platform.h"
#include "platform/ppu.h"

#define mosaicBGEffectX     (REG_MOSAIC & 0xF)
#define mosaicBGEffectY     ((REG_MOSAIC >> 4) & 0xF)
#define mosaicSpriteEffectX ((REG_MOSAIC >> 8) & 0xF)
#define mosaicSpriteEffectY ((REG_MOSAIC >> 12) & 0xF)
#define applyBGHorizontalMosaicEffect(x)     ((x) - ((x) % (mosaicBGEffectX + 1)))
#define applyBGVerticalMosaicEffect(y)       ((y) - ((y) % (mosaicBGEffectY + 1)))
#define applySpriteHorizontalMosaicEffect(x) ((x) - ((x) % (mosaicSpriteEffectX + 1)))
#define applySpriteVerticalMosaicEffect(y)   ((y) - ((y) % (mosaicSpriteEffectY + 1)))

#define getAlphaBit(x)     (((x) >> 15) & 1)
#define getRedChannel(x)   (((x) >>  0) & 0x1F)
#define getGreenChannel(x) (((x) >>  5) & 0x1F)
#define getBlueChannel(x)  (((x) >> 10) & 0x1F)
#define isbgEnabled(x)     (((REG_DISPCNT >> 8) & 0xF) & (1 << (x)))

#define WINMASK_BG0     (1 << 0)
#define WINMASK_BG1     (1 << 1)
#define WINMASK_BG2     (1 << 2)
#define WINMASK_BG3     (1 << 3)
#define WINMASK_OBJ     (1 << 4)
#define WINMASK_CLR     (1 << 5)

// Which GBA window a pixel belongs to. Priority is WIN0 > WIN1 > OBJWIN > outside.
enum
{
    WINREGION_OUTSIDE,
    WINREGION_WIN0,
    WINREGION_WIN1,
    WINREGION_OBJWIN,
};

struct scanlineData {
    uint16_t layers[4][DISPLAY_WIDTH];
    uint16_t spriteLayers[4][DISPLAY_WIDTH];
    uint16_t bgcnts[4];
    // Effective per-pixel layer enables, in WININ/WINOUT bit order
    // (bit 0-3 = BG0-3, bit 4 = OBJ, bit 5 = colour effect).
    uint16_t winMask[DISPLAY_WIDTH];
    uint8_t winRegion[DISPLAY_WIDTH];
    char bgtoprio[4];
    char prioritySortedBgs[4][4];
    char prioritySortedBgsCount[4];
};

static const uint16_t bgMapSizes[][2] =
{
    {32, 32},
    {64, 32},
    {32, 64},
    {64, 64},
};

static const uint8_t spriteSizes[][2] =
{
    {8, 16},
    {8, 32},
    {16, 32},
    {32, 64},
};

static void RenderBGScanline(int bgNum, uint16_t control, uint16_t hoffs, uint16_t voffs, int lineNum, uint16_t *line)
{
    unsigned int charBaseBlock = (control >> 2) & 3;
    unsigned int screenBaseBlock = (control >> 8) & 0x1F;
    unsigned int bitsPerPixel = ((control >> 7) & 1) ? 8 : 4;
    unsigned int mapWidth = bgMapSizes[control >> 14][0];
    unsigned int mapHeight = bgMapSizes[control >> 14][1];
    unsigned int mapWidthInPixels = mapWidth * 8;
    unsigned int mapHeightInPixels = mapHeight * 8;

    uint8_t *bgtiles = (uint8_t *)BG_CHAR_ADDR(charBaseBlock);
    uint16_t *pal = (uint16_t *)PLTT;

    if (control & BGCNT_MOSAIC)
        lineNum = applyBGVerticalMosaicEffect(lineNum);

    hoffs &= 0x1FF;
    voffs &= 0x1FF;

    for (unsigned int x = 0; x < DISPLAY_WIDTH; x++)
    {
        uint16_t *bgmap = (uint16_t *)BG_SCREEN_ADDR(screenBaseBlock);
        unsigned int xx;
        if (control & BGCNT_MOSAIC)
            xx = (applyBGHorizontalMosaicEffect(x) + hoffs) & 0x1FF;
        else
            xx = (x + hoffs) & 0x1FF;

        unsigned int yy = (lineNum + voffs) & 0x1FF;

        if (xx > 255 && mapWidthInPixels > 256)
            bgmap += 0x400;

        if (yy > 255 && mapHeightInPixels > 256)
            bgmap += (mapWidthInPixels > 256) ? 0x800 : 0x400;

        xx &= 0xFF;
        yy &= 0xFF;

        unsigned int mapX = xx / 8;
        unsigned int mapY = yy / 8;
        uint16_t entry = bgmap[mapY * 32 + mapX];

        unsigned int tileNum = entry & 0x3FF;
        unsigned int paletteNum = (entry >> 12) & 0xF;

        unsigned int tileX = xx % 8;
        unsigned int tileY = yy % 8;

        if (entry & (1 << 10))
            tileX = 7 - tileX;
        if (entry & (1 << 11))
            tileY = 7 - tileY;

        uint16_t tileLoc = tileNum * (bitsPerPixel * 8);
        uint16_t tileLocY = tileY * bitsPerPixel;
        uint16_t tileLocX = tileX;
        if (bitsPerPixel == 4)
            tileLocX /= 2;

        uint8_t pixel = bgtiles[tileLoc + tileLocY + tileLocX];

        if (bitsPerPixel == 4)
        {
            if (tileX & 1)
                pixel >>= 4;
            else
                pixel &= 0xF;

            if (pixel != 0)
                line[x] = pal[16 * paletteNum + pixel] | 0x8000;
        }
        else
        {
            if (pixel != 0)
                line[x] = pal[pixel] | 0x8000;
        }
    }
}
static int32_t ReadAffineReference(const void *address)
{
    uint32_t raw;

    memcpy(&raw, address, sizeof(raw));
    raw &= 0x0FFFFFFF;
    if (raw & 0x08000000)
        raw |= 0xF0000000;
    return (int32_t)raw;
}

static void RenderAffineBGScanline(uint16_t control, int lineNum, uint16_t *line)
{
    static const unsigned int sAffineBgSizes[] = {128, 256, 512, 1024};
    unsigned int charBaseBlock = (control >> 2) & 3;
    unsigned int screenBaseBlock = (control >> 8) & 0x1F;
    unsigned int size = sAffineBgSizes[control >> 14];
    unsigned int mapWidth = size / 8;
    const uint8_t *bgtiles = (const uint8_t *)BG_CHAR_ADDR(charBaseBlock);
    const uint8_t *bgmap = (const uint8_t *)BG_SCREEN_ADDR(screenBaseBlock);
    const uint16_t *pal = (const uint16_t *)PLTT;
    int32_t pa = (int16_t)REG_BG2PA;
    int32_t pb = (int16_t)REG_BG2PB;
    int32_t pc = (int16_t)REG_BG2PC;
    int32_t pd = (int16_t)REG_BG2PD;
    int32_t refX = ReadAffineReference(REG_ADDR_BG2X);
    int32_t refY = ReadAffineReference(REG_ADDR_BG2Y);

    if (control & BGCNT_MOSAIC)
        lineNum = applyBGVerticalMosaicEffect(lineNum);

    refX += pb * lineNum;
    refY += pd * lineNum;
    for (int x = 0; x < DISPLAY_WIDTH; x++)
    {
        int screenX = (control & BGCNT_MOSAIC) ? applyBGHorizontalMosaicEffect(x) : x;
        int32_t texX = (refX + pa * screenX) >> 8;
        int32_t texY = (refY + pc * screenX) >> 8;

        if (control & BGCNT_WRAP)
        {
            texX &= size - 1;
            texY &= size - 1;
        }
        else if (texX < 0 || texY < 0 || texX >= (int32_t)size || texY >= (int32_t)size)
        {
            continue;
        }

        uint8_t tileNum = bgmap[(texY / 8) * mapWidth + texX / 8];
        uint8_t pixel = bgtiles[tileNum * 64 + (texY & 7) * 8 + (texX & 7)];

        if (pixel != 0)
            line[x] = pal[pixel] | 0x8000;
    }
}


static uint16_t alphaBlendColor(uint16_t targetA, uint16_t targetB)
{
    unsigned int eva = REG_BLDALPHA & 0x1F;
    unsigned int evb = (REG_BLDALPHA >> 8) & 0x1F;
    unsigned int r = ((getRedChannel(targetA) * eva) + (getRedChannel(targetB) * evb)) >> 4;
    unsigned int g = ((getGreenChannel(targetA) * eva) + (getGreenChannel(targetB) * evb)) >> 4;
    unsigned int b = ((getBlueChannel(targetA) * eva) + (getBlueChannel(targetB) * evb)) >> 4;

    if (r > 31) r = 31;
    if (g > 31) g = 31;
    if (b > 31) b = 31;

    return r | (g << 5) | (b << 10) | 0x8000;
}

static uint16_t alphaBrightnessIncrease(uint16_t targetA)
{
    unsigned int evy = (REG_BLDY & 0x1F);
    unsigned int r = getRedChannel(targetA) + (31 - getRedChannel(targetA)) * evy / 16;
    unsigned int g = getGreenChannel(targetA) + (31 - getGreenChannel(targetA)) * evy / 16;
    unsigned int b = getBlueChannel(targetA) + (31 - getBlueChannel(targetA)) * evy / 16;

    if (r > 31) r = 31;
    if (g > 31) g = 31;
    if (b > 31) b = 31;

    return r | (g << 5) | (b << 10) | 0x8000;
}

static uint16_t alphaBrightnessDecrease(uint16_t targetA)
{
    unsigned int evy = (REG_BLDY & 0x1F);
    unsigned int r = getRedChannel(targetA) - getRedChannel(targetA) * evy / 16;
    unsigned int g = getGreenChannel(targetA) - getGreenChannel(targetA) * evy / 16;
    unsigned int b = getBlueChannel(targetA) - getBlueChannel(targetA) * evy / 16;

    if (r > 31) r = 31;
    if (g > 31) g = 31;
    if (b > 31) b = 31;

    return r | (g << 5) | (b << 10) | 0x8000;
}

static bool alphaBlendSelectTargetB(struct scanlineData *scanline, uint16_t *colorOutput, char prnum, char prsub, int pixelpos, bool spriteBlendEnabled)
{
    for (unsigned int blndprnum = prnum; blndprnum <= 3; blndprnum++)
    {
        if (spriteBlendEnabled && getAlphaBit(scanline->spriteLayers[blndprnum][pixelpos]) == 1)
        {
            *colorOutput = scanline->spriteLayers[blndprnum][pixelpos];
            return true;
        }

        for (unsigned int blndprsub = prsub; blndprsub < (unsigned int)scanline->prioritySortedBgsCount[blndprnum]; blndprsub++)
        {
            char currLayer = scanline->prioritySortedBgs[blndprnum][blndprsub];
            if (getAlphaBit(scanline->layers[(int)currLayer][pixelpos]) == 1 && (REG_BLDCNT & (1 << (8 + currLayer))) && isbgEnabled(currLayer))
            {
                *colorOutput = scanline->layers[(int)currLayer][pixelpos];
                return true;
            }
            if (getAlphaBit(scanline->layers[(int)currLayer][pixelpos]) == 1 && isbgEnabled(currLayer) && prnum != (char)blndprnum)
            {
                return false;
            }
        }
        prsub = 0;
    }

    if (REG_BLDCNT & BLDCNT_TGT2_BD)
    {
        *colorOutput = *(uint16_t *)PLTT;
        return true;
    }
    return false;
}

// Compute, for every pixel of this scanline, which window region it falls in
// and therefore which layers that region allows. The engine's battle
// transitions and menu animations are built entirely out of this: a transition
// wipes by animating WIN0H/WIN0V so that BG0 (the effect layer) replaces the
// field only inside the window, and the menu screens dim everything except the
// cursor row by putting just that row inside WIN0.
//
// Window region priority is WIN0 > WIN1 > OBJWIN > outside. When neither WIN0
// nor WIN1 is enabled on the display control, masking is inactive and the
// ordinary DISPCNT layer enables apply everywhere.
static void ComputeWindowMask(struct scanlineData *scanline, int vcount)
{
    bool win0On = (REG_DISPCNT & DISPCNT_WIN0_ON) != 0;
    bool win1On = (REG_DISPCNT & DISPCNT_WIN1_ON) != 0;
    bool objWinOn = (REG_DISPCNT & DISPCNT_OBJWIN_ON) != 0;
    uint16_t win0H = REG_WIN0H;
    uint16_t win0V = REG_WIN0V;
    uint16_t win1H = REG_WIN1H;
    uint16_t win1V = REG_WIN1V;

    bool inWin0Row = false;
    bool inWin1Row = false;

    if (win0On)
    {
        int top = win0V >> 8;
        int bottom = win0V & 0xFF;

        inWin0Row = (vcount >= top && vcount < bottom);
    }

    if (win1On)
    {
        int top = win1V >> 8;
        int bottom = win1V & 0xFF;

        inWin1Row = (vcount >= top && vcount < bottom);
    }

    for (int x = 0; x < DISPLAY_WIDTH; x++)
    {
        uint8_t region;

        if (inWin0Row && x >= (win0H >> 8) && x < (win0H & 0xFF))
            region = WINREGION_WIN0;
        else if (inWin1Row && x >= (win1H >> 8) && x < (win1H & 0xFF))
            region = WINREGION_WIN1;
        else
            region = WINREGION_OUTSIDE;

        scanline->winRegion[x] = region;

        switch (region)
        {
        case WINREGION_WIN0:
            scanline->winMask[x] = REG_WININ & 0x3F;
            break;
        case WINREGION_WIN1:
            scanline->winMask[x] = (REG_WININ >> 8) & 0x3F;
            break;
        case WINREGION_OBJWIN:
            scanline->winMask[x] = (REG_WINOUT >> 8) & 0x3F;
            break;
        default:
            scanline->winMask[x] = REG_WINOUT & 0x3F;
            break;
        }
    }
}

static void DrawSprites(struct scanlineData *scanline, uint16_t vcount, bool windowsEnabled)
{
    void *objtiles = VRAM_ + 0x10000;
    unsigned int blendMode = (REG_BLDCNT >> 6) & 3;

    for (int i = 127; i >= 0; i--)
    {
        struct OamData *oam = &((struct OamData *)OAM_)[i];
        unsigned int width;
        unsigned int height;

        bool isAffine = oam->affineMode & 1;
        bool doubleSizeOrDisabled = (oam->affineMode >> 1) & 1;
        bool isSemiTransparent = (oam->objMode == 1);
        bool isObjWin = (oam->objMode == 2);

        if (!isAffine && doubleSizeOrDisabled)
            continue;

        if (oam->shape == 0)
        {
            width = (1 << oam->size) * 8;
            height = (1 << oam->size) * 8;
        }
        else if (oam->shape == 1)
        {
            width = spriteSizes[oam->size][1];
            height = spriteSizes[oam->size][0];
        }
        else if (oam->shape == 2)
        {
            width = spriteSizes[oam->size][0];
            height = spriteSizes[oam->size][1];
        }
        else
        {
            continue;
        }

        uint16_t *pixels = scanline->spriteLayers[oam->priority];

        int32_t x = oam->x;
        int32_t y = oam->y;

        if (x >= DISPLAY_WIDTH)
            x -= 512;
        if (y >= DISPLAY_HEIGHT)
            y -= 256;

        // Affine (rotated/scaled) sprites transform around their centre and, in
        // double-size mode, draw into a bounding box twice the sprite's size.
        // The engine rotates the Poké Ball this way while it is thrown, spins
        // battler sprites, and scales mons during the ball send-out; without
        // this the matrices are computed but never sampled (the ball flew
        // without spinning and battler growth/shrink did nothing).
        int32_t pa = 0x100, pb = 0, pc = 0, pd = 0x100;

        if (isAffine)
        {
            const struct OamMatrix *mat = &gOamMatrices[oam->matrixNum & 0x1F];

            pa = mat->a;
            pb = mat->b;
            pc = mat->c;
            pd = mat->d;
        }

        int half_width = width / 2;
        int half_height = height / 2;

        if (isAffine && (oam->affineMode >> 1) & 1)
        {
            // Double-size affine boxes span twice the object's dimensions.
            width *= 2;
            height *= 2;
            half_width = width / 2;
            half_height = height / 2;
        }

        x += half_width;
        y += half_height;

        // Sprites whose top is above the screen get a negative y here (OAM wraps 248 -> -8).
        // The scanline window must be compared signed, or those sprites are dropped
        // entirely instead of being clipped at the top edge.
        if ((int32_t)vcount >= y - half_height && (int32_t)vcount < y + half_height)
        {
            int local_y = (oam->mosaic == 1) ? applySpriteVerticalMosaicEffect(vcount) - y : (int)vcount - y;
            bool flipX = !isAffine && ((oam->matrixNum >> 3) & 1);
            bool flipY = !isAffine && ((oam->matrixNum >> 4) & 1);
            bool is8BPP = oam->bpp & 1;

            // Source-space span of the sprite's own pixels, used to reject
            // texels that a rotation would otherwise sample from outside it.
            const int src_width = (oam->shape == 0) ? (1 << oam->size) * 8
                                : (oam->shape == 1) ? (int)spriteSizes[oam->size][1]
                                : (int)spriteSizes[oam->size][0];
            const int src_height = (oam->shape == 0) ? (1 << oam->size) * 8
                                 : (oam->shape == 1) ? (int)spriteSizes[oam->size][0]
                                 : (int)spriteSizes[oam->size][1];

            for (int local_x = -half_width; local_x < half_width; local_x++)
            {
                uint8_t *tiledata = (uint8_t *)objtiles;
                uint16_t *palette = (uint16_t *)(PLTT + 0x200);

                int global_x = local_x + x;
                if (global_x < 0 || global_x >= DISPLAY_WIDTH)
                    continue;

                int tex_x;
                int tex_y;

                if (isAffine)
                {
                    // Inverse affine transform: the OAM matrix maps screen space
                    // back into the sprite's texture.
                    int32_t dx = local_x;
                    int32_t dy = local_y;

                    tex_x = (int)((pa * dx + pb * dy) >> 8) + (src_width / 2);
                    tex_y = (int)((pc * dx + pd * dy) >> 8) + (src_height / 2);
                }
                else
                {
                    tex_x = local_x + half_width;
                    tex_y = local_y + half_height;
                }

                if (tex_x >= src_width || tex_y >= src_height || tex_x < 0 || tex_y < 0)
                    continue;

                if (flipX)
                    tex_x = src_width - tex_x - 1;
                if (flipY)
                    tex_y = src_height - tex_y - 1;

                int tile_x = tex_x % 8;
                int tile_y = tex_y % 8;
                int block_x = tex_x / 8;
                int block_y = tex_y / 8;
                // Tile stride follows the object's own dimensions: an affine
                // double-size sprite stores its tiles as if undoubled.
                int block_offset = (block_y * (REG_DISPCNT & 0x40 ? (src_width / 8) : 16)) + block_x;
                uint16_t pixel = 0;

                if (!is8BPP)
                {
                    pixel = tiledata[(block_offset + oam->tileNum) * 32 + (tile_y * 4) + (tile_x / 2)];
                    if (tile_x & 1)
                        pixel >>= 4;
                    else
                        pixel &= 0xF;
                    palette += oam->paletteNum * 16;
                }
                else
                {
                    pixel = tiledata[(block_offset * 2 + oam->tileNum) * 32 + (tile_y * 8) + tile_x];
                }

                if (pixel != 0)
                {
                    uint16_t color = palette[pixel];

                    if (isObjWin)
                    {
                        // An OBJ-window sprite is not drawn; its opaque pixels
                        // mark the OBJ-window region, which only applies where
                        // no rectangular window already claimed the pixel.
                        if (scanline->winRegion[global_x] == WINREGION_OUTSIDE)
                        {
                            scanline->winRegion[global_x] = WINREGION_OBJWIN;
                            scanline->winMask[global_x] = (REG_WINOUT >> 8) & 0x3F;
                        }
                        continue;
                    }

                    // The region's OBJ bit must allow sprites here.
                    if (windowsEnabled && !(scanline->winMask[global_x] & WINMASK_OBJ))
                        continue;

                    bool winShouldBlendPixel = (!windowsEnabled || (scanline->winMask[global_x] & WINMASK_CLR));

                    if ((blendMode == 1 && (REG_BLDCNT & BLDCNT_TGT1_OBJ) && winShouldBlendPixel) || isSemiTransparent)
                    {
                        uint16_t targetA = color;
                        uint16_t targetB = 0;
                        if (alphaBlendSelectTargetB(scanline, &targetB, oam->priority, 0, global_x, false))
                            color = alphaBlendColor(targetA, targetB);
                    }
                    else if ((REG_BLDCNT & BLDCNT_TGT1_OBJ) && winShouldBlendPixel)
                    {
                        if (blendMode == 2)
                            color = alphaBrightnessIncrease(color);
                        else if (blendMode == 3)
                            color = alphaBrightnessDecrease(color);
                    }

                    pixels[global_x] = color | 0x8000;
                }
            }
        }
    }
}

void PPU_RenderScanline(uint16_t *pixels, int vcount)
{
    unsigned int mode = REG_DISPCNT & 3;
    struct scanlineData scanline;
    unsigned int blendMode = (REG_BLDCNT >> 6) & 3;
    // Masking is active only while WIN0 (or WIN1/OBJWIN) is switched on in
    // DISPCNT; otherwise the ordinary layer enables apply everywhere.
    bool windowsEnabled = (REG_DISPCNT & (DISPCNT_WIN0_ON | DISPCNT_WIN1_ON | DISPCNT_OBJWIN_ON)) != 0;

    memset(scanline.layers, 0, sizeof(scanline.layers));
    memset(scanline.winMask, 0, sizeof(scanline.winMask));
    memset(scanline.winRegion, 0, sizeof(scanline.winRegion));
    memset(scanline.spriteLayers, 0, sizeof(scanline.spriteLayers));
    memset(scanline.prioritySortedBgsCount, 0, sizeof(scanline.prioritySortedBgsCount));

    ComputeWindowMask(&scanline, vcount);

    // Backdrop fill. On the GBA the backdrop is itself a blend target
    // (BLDCNT_TGT1_BD), and the window's colour-effect bit gates whether that
    // darkening/lightening applies per pixel — this is how the menus dim the
    // screen everywhere except the cursor row.
    for (int x = 0; x < DISPLAY_WIDTH; x++)
    {
        uint16_t backdropColor = *(uint16_t *)PLTT;
        bool effectAllowed = !windowsEnabled || (scanline.winMask[x] & WINMASK_CLR);

        if ((REG_BLDCNT & BLDCNT_TGT1_BD) && effectAllowed)
        {
            if (blendMode == 2)
                backdropColor = alphaBrightnessIncrease(backdropColor);
            else if (blendMode == 3)
                backdropColor = alphaBrightnessDecrease(backdropColor);
        }

        pixels[x] = backdropColor;
    }

    for (int bgnum = 0; bgnum < 4; bgnum++)
    {
        uint16_t bgcnt = *(uint16_t *)(REG_ADDR_BG0CNT + bgnum * 2);
        scanline.bgcnts[bgnum] = bgcnt;
        uint16_t priority = bgcnt & 3;
        scanline.bgtoprio[bgnum] = priority;

        char priorityCount = scanline.prioritySortedBgsCount[priority];
        scanline.prioritySortedBgs[priority][(int)priorityCount] = bgnum;
        scanline.prioritySortedBgsCount[priority]++;
    }

    if (mode == 0)
    {
        for (int bgnum = 3; bgnum >= 0; bgnum--)
        {
            if (isbgEnabled(bgnum))
            {
                uint16_t bghoffs = *(uint16_t *)(REG_ADDR_BG0HOFS + bgnum * 4);
                uint16_t bgvoffs = *(uint16_t *)(REG_ADDR_BG0VOFS + bgnum * 4);
                RenderBGScanline(bgnum, scanline.bgcnts[bgnum], bghoffs, bgvoffs, vcount, scanline.layers[bgnum]);
            }
        }
    }
    else if (mode == 1)
    {
        for (int bgnum = 1; bgnum >= 0; bgnum--)
        {
            if (isbgEnabled(bgnum))
            {
                uint16_t bghoffs = *(uint16_t *)(REG_ADDR_BG0HOFS + bgnum * 4);
                uint16_t bgvoffs = *(uint16_t *)(REG_ADDR_BG0VOFS + bgnum * 4);
                RenderBGScanline(bgnum, scanline.bgcnts[bgnum], bghoffs, bgvoffs, vcount, scanline.layers[bgnum]);
            }
        }
        if (isbgEnabled(2))
            RenderAffineBGScanline(scanline.bgcnts[2], vcount, scanline.layers[2]);
    }

    if (REG_DISPCNT & DISPCNT_OBJ_ON)
        DrawSprites(&scanline, vcount, windowsEnabled);

    for (int prnum = 3; prnum >= 0; prnum--)
    {
        for (char prsub = scanline.prioritySortedBgsCount[prnum] - 1; prsub >= 0; prsub--)
        {
            char bgnum = scanline.prioritySortedBgs[prnum][(int)prsub];
            if (isbgEnabled(bgnum))
            {
                uint16_t *src = scanline.layers[(int)bgnum];
                for (int xpos = 0; xpos < DISPLAY_WIDTH; xpos++)
                {
                    uint16_t color = src[xpos];
                    if (!getAlphaBit(color))
                        continue;

                    if (windowsEnabled && !(scanline.winMask[xpos] & (1 << bgnum)))
                        continue;

                    if (blendMode != 0 && (REG_BLDCNT & (1 << bgnum)))
                    {
                        uint16_t targetA = color;
                        uint16_t targetB = 0;
                        switch (blendMode)
                        {
                            case 1:
                                if (windowsEnabled && !(scanline.winMask[xpos] & WINMASK_CLR))
                                    break;
                                if (alphaBlendSelectTargetB(&scanline, &targetB, prnum, prsub + 1, xpos, (REG_BLDCNT & BLDCNT_TGT2_OBJ) ? true : false))
                                    color = alphaBlendColor(targetA, targetB);
                                break;
                            case 2:
                                if (!windowsEnabled || (scanline.winMask[xpos] & WINMASK_CLR))
                                    color = alphaBrightnessIncrease(targetA);
                                break;
                            case 3:
                                if (!windowsEnabled || (scanline.winMask[xpos] & WINMASK_CLR))
                                    color = alphaBrightnessDecrease(targetA);
                                break;
                        }
                    }
                    pixels[xpos] = color;
                }
            }
        }

        uint16_t *src = scanline.spriteLayers[prnum];
        for (int xpos = 0; xpos < DISPLAY_WIDTH; xpos++)
        {
            if (getAlphaBit(src[xpos]))
                pixels[xpos] = src[xpos];
        }
    }
}

void PPU_Init(void)
{
}

void PPU_RenderFrame(uint16_t *framebuffer)
{
    for (int i = 0; i < DISPLAY_HEIGHT; i++)
    {
        REG_VCOUNT = i;

        uint16_t *scanline = &framebuffer[i * DISPLAY_WIDTH];

        PPU_RenderScanline(scanline, i);

        // H-Blank work happens after the scanline is drawn and takes effect on
        // the NEXT one. On the GBA the VBlank callback arms DMA0 with the
        // per-scanline buffer and writes the first line's register by hand, so
        // the first transfer here must consume entry 1, not entry 0 — which is
        // why the engine seeds dmaSrcBuffers to buffer + 1.
        //
        // REG_IE gates the callback exactly as the hardware interrupt would:
        // scenes that never call EnableInterrupts(INTR_FLAG_HBLANK) keep their
        // callback dormant.
        Platform_RunHBlankDma();
        if ((REG_IE & INTR_FLAG_HBLANK) && gMain.hblankCallback)
            gMain.hblankCallback();
    }

    for (int i = DISPLAY_HEIGHT; i < 228; i++)
    {
        REG_VCOUNT = i;
    }
}
