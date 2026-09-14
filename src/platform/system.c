#include "global.h"
#include "platform/platform.h"
#include "load_save.h"
// Simulated GBA hardware memory spaces
uint8_t REG_BASE[0x400] __attribute__((aligned(4))) = {0};
uint8_t PLTT_[0x400] __attribute__((aligned(4))) = {0};
uint8_t VRAM_[0x18000] __attribute__((aligned(4))) = {0};
uint8_t OAM_[0x400] __attribute__((aligned(4))) = {0};
uint8_t FLASH_BASE_[131072] __attribute__((aligned(4))) = {0};

uint16_t INTR_CHECK = 0;
void *INTR_VECTOR = NULL;
struct SoundInfo *SOUND_INFO_PTR = NULL;

// Game Engine Heap
uint8_t gHeap[0x1C000] __attribute__((aligned(4))) = {0};

//---------------------------------------------------------------------------
// Portable DMA model
//
// The GBA's DMA channels are real hardware: a channel armed with
// DMA_START_HBLANK performs one transfer at the start of every scanline, and
// DMA_DEST_RELOAD rewinds its destination afterwards. The engine relies on
// this for per-scanline effects — the battle-transition swirl/ripple/wave feed
// REG_WIN0H (or REG_BLDY / REG_BG3HOFS), the battle intro slides BG0, and the
// underground flash dims the screen.
//
// The port has no DMA hardware, and DmaSet used to compile to nothing, so
// those effects never touched a register and their transitions rendered as a
// plain fade. The channel state is recorded here instead and the software PPU
// replays one transfer per rendered scanline via Platform_RunHBlankDma().
// Immediate transfers (DMA_START_NOW) still execute inline, as before.
//---------------------------------------------------------------------------

struct PortableDmaChannel
{
    const uint8_t *src;
    uint8_t *dest;
    uint32_t control;
    bool active;
};

static struct PortableDmaChannel sDmaChannel[4];

void PortableDmaSet(unsigned int dmaNum, const void *src, void *dest, uint32_t control)
{
    struct PortableDmaChannel *ch;

    if (dmaNum >= 4)
        return;

    ch = &sDmaChannel[dmaNum];
    ch->control = control;
    ch->src = (const uint8_t *)src;
    ch->dest = (uint8_t *)dest;

    // Bit 15 of the high halfword is DMA_ENABLE; the start-timing bits decide
    // whether this runs inline or once per scanline.
    ch->active = (control & (DMA_ENABLE << 16))
              && ((control & (DMA_START_MASK << 16)) == (DMA_START_HBLANK << 16));
}

void PortableDmaStop(unsigned int dmaNum)
{
    if (dmaNum >= 4)
        return;

    sDmaChannel[dmaNum].active = false;
}

// Replay one transfer for each active HBlank-timed DMA channel. Called once
// per rendered scanline by the PPU.
//
// These are all DMA0 transfers, and DMA0 ignores the repeat/reload bits: the
// channel copies `count` units at the start of every scanline, the source
// keeps advancing from where the previous scanline left it, and the
// destination stays put. That is exactly what the engine expects — each
// scanline consumes the next entry of gScanlineEffectRegBuffers and writes it
// to one video register, producing the swirl/ripple/wave. The engine re-arms
// the channel from the buffer start every VBlank, which is what resets the
// source.
void Platform_RunHBlankDma(void)
{
    for (unsigned int i = 0; i < 4; i++)
    {
        struct PortableDmaChannel *ch = &sDmaChannel[i];
        uint32_t unit;
        uint32_t count;

        if (!ch->active)
            continue;

        unit = (ch->control & (DMA_32BIT << 16)) ? 4 : 2;
        count = ch->control & 0xFFFF;

        for (uint32_t n = 0; n < count; n++)
        {
            if (unit == 4)
                *(uint32_t *)ch->dest = *(const uint32_t *)ch->src;
            else
                *(uint16_t *)ch->dest = *(const uint16_t *)ch->src;

            switch ((ch->control >> 7) & 3)
            {
            case 1: ch->src -= unit; break;
            case 2: break;
            default: ch->src += unit; break;
            }

            switch ((ch->control >> 5) & 3)
            {
            case 1: ch->dest -= unit; break;
            case 2: break;
            default: ch->dest += unit; break;
            }
        }
    }
}

