# AGENTS.md — Pokémon FireRed Native Apple Silicon Port

## 1. Project Overview
This repository is based on **pokefirered** (the pret decompilation of *Pokémon FireRed* for the Game Boy Advance).
- **Core Architecture:** 32-bit GBA C engine originally targeting the ARM7TDMI processor.
- **Primary Mission:** Port the game to run **natively as a 64-bit Mach-O ARM64 executable on macOS (Apple Silicon)** using **SDL2**, without requiring an external GBA emulator or GBA cross-compilation toolchains.

---

## 2. Key Technical Decisions & Constraints

### The 64-Bit Apple Silicon Reality
1. **No 32-bit binaries:** macOS on Apple Silicon (arm64) does not support 32-bit binaries or 32-bit ARM assembly (`armv4t`/`arm7tdmi`). Any inline assembly or `.s` files targeting GBA registers/instructions must be guarded with `#ifndef PORTABLE` or replaced with portable C.
2. **Memory Virtualization:** GBA physical addresses (`0x04000000` I/O registers, `0x05000000` Palette RAM, `0x06000000` VRAM, `0x07000000` OAM, `0x0E000000` Flash) cause segfaults if dereferenced directly on macOS. They are abstracted via `#ifdef PORTABLE` to point to host-allocated global buffers (`REG_BASE`, `PLTT_`, `VRAM_`, `OAM_`, `FLASH_BASE_`) in `src/platform/system.c`.
3. **No DMA pointer truncation:** GBA register-based DMA writes (`DmaSet`) cast source and destination addresses to 32-bit integers. On 64-bit systems, this truncates 64-bit pointers. In `include/gba/macro.h`, `DMA_COPY` and `DMA_FILL` are implemented as synchronous memory copy/fill functions under `PORTABLE`.
4. **Software GBA PPU:** The GBA engine does not write to a linear framebuffer; it writes tiles, tilemaps, and OAM structures. A software PPU (`src/platform/ppu.c`) composites Mode 0/1 background layers and 128 OAM sprites into a 240×160 15-bit BGR555 texture displayed via SDL2 at 60 FPS.

---

## 3. Current Implementation Status

### Completed Milestones
- [x] **Native Asset Compilers:** `tools/gbagfx`, `tools/scaninc`, `tools/preproc`, etc. compile natively for ARM64 using Apple Clang and Homebrew `libpng`/`pkgconf`.
- [x] **Hardware Abstraction Layer (HAL):**
  - `include/gba/defines.h`: Host buffer mappings for `PLTT`, `VRAM`, `OAM`.
  - `include/gba/io_reg.h`: Array-backed `REG_BASE`.
  - `include/gba/macro.h`: 64-bit safe synchronous DMA macros.
  - `include/gba/flash_internal.h`: Re-routed `FLASH_BASE_` buffer.
- [x] **Portable GBA BIOS (`src/platform/bios.c`):**
  - `CpuSet` and `CpuFastSet` (16-bit and 32-bit memory fill and copy)
  - `LZ77UnCompWram` and `LZ77UnCompVram` (GBA LZ77 decompression)
  - `RLUnCompWram` and `RLUnCompVram` (RLE decompression)
  - `Div`, `Sqrt`, `ArcTan2`
  - `BgAffineSet`, `ObjAffineSet`
  - `RegisterRamReset`, `VBlankIntrWait`
- [x] **Software PPU Compositor (`src/platform/ppu.c`, `include/platform/ppu.h`):**
  - Scanline compositor for Mode 0 text backgrounds ($32\times32$, $64\times32$, $32\times64$, $64\times64$ tilemaps with 4bpp/8bpp tiles, flipping, and scrolling).
  - 128 OAM sprite compositor with transparency, priority sorting, and 4bpp/8bpp palettes.
  - Alpha blending (`REG_BLDCNT`, `REG_BLDALPHA`) and brightness increase/decrease (`REG_BLDY`).
- [x] **SDL2 Platform Host Runner (`src/platform/sdl2.c`, `include/platform/platform.h`):**
  - Resizable 3× scaled window ($720\times480$) with high-DPI support.
  - Keyboard mapping (WASD/Arrows, Z/X/J/K, Enter, Tab/Backspace) mapped to active-low `REG_KEYINPUT`.
  - Screenshot capture functionality (`Platform_SaveScreenshot`).
- [x] **CPU Game Engine Linked (`src/main.c` & Subsystems):**
  - `src/main.c` has its 32-bit assembly guarded and `WaitForVBlank()` hooked into the platform tick (`Platform_UpdateInput()`, `VBlankIntr()`, `Platform_RenderAndPresent()`).
  - Core subsystems linked: `gpu_regs.c`, `palette.c`, `task.c`, `sprite.c`, `malloc.c`, `scanline_effect.c`, `dma3_manager.c`, `bg.c`, `random.c`, `trig.c`, `decompress.c`, `blend_palette.c`.
  - Hardware stubs for sound (M4A) and link communications in `src/platform/stubs.c`.
  - Verified with `--boot-test`: `AgbMain()` initializes, runs 60 frames, and executes `CB2_InitCopyrightScreenAfterBootup`.

---

## 4. Key Files & Structure

```text
openfirered/
├── Makefile.native            # Primary build script for native macOS ARM64 binary
├── include/
│   ├── gba/                   # GBA system headers (defines.h, io_reg.h, macro.h, etc.)
│   └── platform/
│       ├── platform.h         # Platform runner interface & memory declarations
│       └── ppu.h              # Software PPU interface
├── src/
│   ├── main.c                 # GBA main loop & AgbMain() entry point
│   ├── platform/
│   │   ├── system.c           # Memory buffer allocations (REG_BASE, VRAM_, PLTT_, etc.)
│   │   ├── bios.c             # Portable C implementations of GBA BIOS SWI routines
│   │   ├── ppu.c              # Software scanline rasterizer (BGs, sprites, blending)
│   │   ├── sdl2.c             # SDL2 windowing, texture streaming, and input handling
│   │   ├── stubs.c            # Clean stubs for M4A audio, wireless RFU, link serial
│   │   └── main.c             # Native entry point (main) calling Platform_Init & AgbMain
│   └── [engine subsystems]    # gpu_regs.c, palette.c, task.c, sprite.c, malloc.c, etc.
```

---

## 5. Build & Run Instructions

Prerequisites (installed via Homebrew):
```bash
brew install sdl2 libpng pkgconf
```

### Build native binary:
```bash
make -f Makefile.native
```

### Run PPU standalone test (120 frames with test pattern & sprite):
```bash
./firered-native --test
```

### Run Engine boot test (60 frames through AgbMain):
```bash
./firered-native --boot-test
```

### Run Engine interactively:
```bash
./firered-native
```

---

## 6. Immediate Next Steps: Reaching the Title Screen

The next milestone is displaying the actual **Title Screen** (`src/title_screen.c`):
1. **Asset Generation:** Ensure title screen graphic files (Charizard sprite, logo, "PRESS START", background tilesets) are generated from `graphics/title_screen/` using `tools/gbagfx`.
2. **Intro Sequence (`src/intro.c`):**
   - Replace the weak `CB2_InitCopyrightScreenAfterBootup` stub in `src/platform/stubs.c` by compiling `src/intro.c`.
   - The boot sequence is:
     `CB2_InitCopyrightScreenAfterBootup` $\to$ `CB2_SetUpIntro` $\to$ `CB2_InitTitleScreen`.
3. **Title Screen Scene (`src/title_screen.c`):**
   - Add `src/title_screen.c` and its dependencies (`src/text.c`, `src/window.c`, `src/string_util.c`) to `ENGINE_SRCS` in `Makefile.native`.
   - Boot `./firered-native` and confirm Charizard, the logo, and the title screen animate at 60 FPS in the SDL2 window.

---

## 7. Git & Commit Guidelines

- **One commit per modified file:** The project owner requests individual commits per modified file rather than omnibus commits.
- **Keep build artifacts ignored:** Do not commit `firered-native`, `*.bmp`, `*.png`, or IDE-specific `compile_flags.txt`. These are ignored in `.gitignore`.
