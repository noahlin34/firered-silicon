#ifndef GUARD_PLATFORM_H
#define GUARD_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define GBA_SCREEN_WIDTH  240
#define GBA_SCREEN_HEIGHT 160

// GBA Hardware Memory Buffers
extern uint8_t REG_BASE[0x400];
extern uint8_t PLTT_[0x400];
extern uint8_t VRAM_[0x18000];
extern uint8_t OAM_[0x400];
extern uint8_t FLASH_BASE_[131072];

extern uint16_t INTR_CHECK;
extern void *INTR_VECTOR;
extern struct SoundInfo *SOUND_INFO_PTR;

// Platform Host Functions
int  Platform_Init(int argc, char **argv);
void Platform_MainLoop(void);
void Platform_UpdateInput(void);
void Platform_PresentFrame(const uint16_t *framebuffer);
void Platform_RenderAndPresent(void);
void Platform_SaveScreenshot(const char *filename);
void Platform_Cleanup(void);

#endif // GUARD_PLATFORM_H
