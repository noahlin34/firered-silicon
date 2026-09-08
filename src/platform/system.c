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

