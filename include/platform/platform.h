#ifndef GUARD_PLATFORM_H
#define GUARD_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define GBA_SCREEN_WIDTH  240
#define GBA_SCREEN_HEIGHT 160

// Portable DMA model. On the GBA, a DMA channel started with DMA_START_HBLANK
// copies its source into a video register once per scanline; that is how the
// engine drives per-scanline effects (battle-transition swirls, the battle
// intro slide, the underground flash). With no real DMA hardware, DmaSet
// records the channel here and the software PPU replays one transfer per
// rendered scanline.
void PortableDmaSet(unsigned int dmaNum, const void *src, void *dest, uint32_t control);
void PortableDmaStop(unsigned int dmaNum);
void Platform_RunHBlankDma(void);

// GBA Hardware Memory Buffers
extern uint8_t REG_BASE[0x400];
extern uint8_t PLTT_[0x400];
extern uint8_t VRAM_[0x18000];
extern uint8_t OAM_[0x400];
extern uint8_t FLASH_BASE_[131072];

extern uint16_t INTR_CHECK;
extern void *INTR_VECTOR;
extern struct SoundInfo *SOUND_INFO_PTR;

// Dev boot (--skip-intro): start a fresh save directly in the player's bedroom.
extern bool gPlatformSkipIntro;
void Platform_DevBootNewGame(void);

// Dev boot (--post-rival): same fresh save, but with the starter already
// received and the first rival battle already won, standing in Oak's Lab.
extern bool gPlatformSkipStory;

void Platform_DevBootApplyStoryProgress(void);

// Dev boot (--post-parcel): one scene further still -- Oak's parcel has been
// delivered, so the player has the Pokédex, five Poké Balls and lab scene 6.
// This is the state the START menu's POKéDEX entry needs (its sanity check
// refuses to open an empty dex).
extern bool gPlatformDexObtained;

void Platform_DevBootApplyDexProgress(void);

// Developer panel (--dev-panel): a separate, resizable SDL2 window listing every
// map the engine can load, so a warp destination can be picked by name -- by
// click or by keyboard. Toggle with F3.
// Platform_DevPanelInit() builds the destination table and must run before
// AgbMain starts, while the engine is stopped.
extern bool gPlatformDevPanelEnabled;
// --dev-panel-keys: comma-separated tokens, played one per frame (e.g.
// "TAB,O,A,ENTER,DOWN*3,ENTER", "WAIT*120,ENTER", or "RESIZE:700:820,MOVE:100:150,
// CLICK:100:150"). Key names: UP DOWN LEFT RIGHT PGUP PGDN HOME END ENTER TAB ESC
// BACKSPACE F3 SPACE WAIT. Mouse/resize commands use `:` between their arguments:
// CLICK:x:y, DBLCLICK:x:y, MOVE:x:y, WHEEL:n, RESIZE:w:h. `*N` repeats a token,
// and a single character stands for itself. Headless only: the panel window's own
// events arrive through SDL.
extern const char *gPlatformDevPanelKeySequence;
void Platform_DevPanelUpdate(void);
void Platform_DevPanelInit(void);
bool Platform_DevPanelHandleEvent(void *sdlEvent);
bool Platform_DevPanelToggleKey(int key);
// Writes the panel's own pixels to a BMP and reports its widget state to stdout;
// development aid, pairs with Platform_SaveScreenshot.
void Platform_DevPanelSaveScreenshot(const char *filename);

// Platform Host Functions
int  Platform_Init(int argc, char **argv);
void Platform_MainLoop(void);
void Platform_UpdateInput(void);
void Platform_PresentFrame(const uint16_t *framebuffer);
void Platform_RenderAndPresent(void);
void Platform_SaveScreenshot(const char *filename);
void Platform_Cleanup(void);
// Closes the developer panel's window; called from Platform_Cleanup so both
// windows own their own teardown.
void Platform_DevPanelShutdown(void);

#endif // GUARD_PLATFORM_H
