#ifndef GUARD_PLATFORM_PPU_H
#define GUARD_PLATFORM_PPU_H

#include <stdint.h>
#include <stdbool.h>

void PPU_Init(void);
void PPU_RenderFrame(uint16_t *framebuffer);
void PPU_RenderScanline(uint16_t *scanline, int lineNum);

#endif // GUARD_PLATFORM_PPU_H
