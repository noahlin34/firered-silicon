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
  - Mode 1 affine BG2 compositor renders Oak's scrolling/zooming backdrop and trainer portraits; verified through Oak and the selected player portrait.
- [x] **SDL2 Platform Host Runner (`src/platform/sdl2.c`, `include/platform/platform.h`):**
  - Resizable 3× scaled window ($720\times480$) with high-DPI support.
  - Keyboard mapping (WASD/Arrows, Z/X/J/K, Enter, Tab/Backspace) mapped to active-low `REG_KEYINPUT`.
  - Screenshot capture functionality (`Platform_SaveScreenshot`).
- [x] **CPU Game Engine Linked (`src/main.c` & Subsystems):**
  - `src/main.c` has its 32-bit assembly guarded and `WaitForVBlank()` hooked into the platform tick (`Platform_UpdateInput()`, `VBlankIntr()`, `Platform_RenderAndPresent()`).
  - Core subsystems linked: `gpu_regs.c`, `palette.c`, `task.c`, `sprite.c`, `malloc.c`, `scanline_effect.c`, `dma3_manager.c`, `bg.c`, `random.c`, `trig.c`, `decompress.c`, `blend_palette.c`.
  - Hardware stubs for sound (M4A) and link communications in `src/platform/stubs.c`.
- [x] **Title Screen Scene (`src/title_screen.c`):**
  - Charizard box art sprite, FireRed title logo, copyright bar, "PRESS START", and animated flame particles fully rendered and animating at 60 FPS in SDL2.
  - Native `INCBIN` asset pipeline using `tools/gbagfx` and `tools/preproc/preproc`.
- [x] **Main Menu Scene (`src/main_menu.c`):**
  - Pressing START/A on the Title Screen transitions through `SetTitleScreenScene_Cry` into `CB2_InitMainMenu`.
  - Renders "CONTINUE" (PLAYER / TIME / BADGES stats) and "NEW GAME" windows with user-frame tiles.
- [x] **Oak's Speech Scene (`src/oak_speech.c`):**
  - Pressing DOWN+A on the Main Menu selects NEW GAME, transitioning through `StartNewGameScene` → `Task_NewGameScene` → Controls Guide (3 pages) → Pikachu intro → Oak's speech.
  - Professor Oak's speech text renders with dialog windows; controls guide pages and Pikachu intro fully working.
  - Battle engine NOT linked — `src/platform/battle_engine_stubs.c` provides blank data tables (`BattleScript_*`, battle EWRAM), loud-failure tripwires for battle-only entry points, and temporary substitutes required by non-battle scenes. Real battle engine files (`battle_util.c`, `battle_controllers.c`, `battle_script_commands.c`, etc.) are intentionally EXCLUDED from the build: they depend on assembly battle scripts (`data/battle_scripts_1.s`) that cannot be assembled for the host.
  - Asset pipeline: all `graphics/pokemon/*`, `graphics/trainers/*`, `graphics/items/*`, `graphics/interface/*`, `graphics/battle_*` INCBIN assets regenerated via `gbagfx`; split sprite sheets (`_0.png.._N.png`) must be combined vertically with width padding to 64px before 4bpp conversion.
  - Mode 1 BG0/BG1 text and affine BG2 rendering are working. Oak, Nidoran F, and the selected player portrait are visually verified in boot-test captures.
- [x] **Player Naming Flow (`src/naming_screen.c`):**
  - Boot automation reaches gender selection, enters the player naming screen, accepts a player name, and returns to Oak's speech without crashing.
  - Naming-screen teardown now disables VBlank before freeing `sNamingScreen`; `CB2_NamingScreen` stops the same frame when its task frees that state.
  - Naming keyboard letters and punctuation render from the real charmap-encoded `src/keyboard_text.c`; the blank battle-stub definitions were removed.
- [x] **Rival Naming & Oak's Speech Exit Flow (`src/naming_screen.c`, `src/oak_speech.c`):**
  - Oak's post-player-name dialogue enters the rival naming screen, inputs the rival name, and returns to Oak's speech.
  - Player and rival naming icons render cleanly using fully initialized local sprite templates.
  - Oak confirms the rival name, delivers his farewell dialogue, and triggers the exit animation (`Task_OakSpeech_ShrinkPlayerPic` affine Mode 1 BG2 shrink and fade-to-black).
  - Oak's speech resources are freed and cleanly hand off execution to `CB2_NewGame` (`[Engine] Successfully completed Oak's speech and reached CB2_NewGame!`).
- [x] **Overworld Map Engine & Player Spawn (`src/overworld.c`, `src/new_game.c`, `src/fieldmap.c`, `src/maps.c`):**
  - Replaced `CB2_NewGame` stub with real engine initialization (`NewGameInitData()`).
  - Implemented 64-bit safe C map data generator (`tools/gen_map_data.py`) producing native 64-bit struct layouts (`src/data/layouts_data.h`) and map headers (`src/data/maps_data.h`), avoiding 32-bit GBA assembly pointer truncation.
  - Linked real field engine subsystems: `fieldmap.c`, `field_camera.c`, `field_player_avatar.c`, `event_object_movement.c`, `field_control_avatar.c`, `field_tasks.c`, `field_weather.c`, `field_weather_effects.c`, `field_fadetransition.c`, `metatile_behavior.c`, `bike.c`, `wild_encounter.c`, `script.c`, `scrcmd.c`, `tilesets.c`, `field_door.c`, `field_effect.c`, `maps.c`.
  - Converted all 67 game tilesets to `4bpp.lz` and 1,072 tileset palettes to `.gbapal`.
  - Fixed saveblock pointer ASLR under `PORTABLE` in `src/load_save.c`.
  - Boot automation at frame 9200 successfully executes `CB2_NewGame`, initializes save data, warps to `MAP_PALLET_TOWN_PLAYERS_HOUSE_2F`, draws the complete bedroom map view (bed, rug, TV/NES, PC, desk, stairs), spawns the player avatar facing North, and enters `CB2_Overworld` running at 60 FPS in SDL2.
> **Note on Intro Sequence:** The opening intro sequence (`src/intro.c` — Copyright screen, GameFreak star shooting animation, and Nidorino vs. Gengar battle) is temporarily bypassed in `src/main.c` (`InitMainCallbacks()` boots directly to `CB2_InitTitleScreen`). This was done deliberately to expedite reaching gameplay, and the intro cinematic will be linked back in at a later point in time.

### Critical 64-Bit Portability Fixes (Learned the Hard Way)
These bugs are subtle and WILL recur if new engine files are linked. Understand them before adding `ENGINE_SRCS`:

1. **GBA address-range checks break on host pointers.** Any engine code comparing a pointer against GBA constants (`IWRAM_END`, `EWRAM_START`, etc.) is wrong on 64-bit macOS — host pointers always compare "greater". Guard with `#ifdef PORTABLE` and check for `NULL` instead. Example: `IsTileMapOutsideWram` in `src/bg.c`.
2. **GameFreak string literals MUST go through `tools/preproc`.** The engine uses a custom charmap (`charmap.txt`), not ASCII. Strings are encoded bytes terminated by `EOS` (0xFF). If `_(x)` expands to `(x)` (plain ASCII), strings never terminate and `StringAppend`/text rendering walk off buffers and hang. All sources containing `_("...")` or `INCBIN_*` must be listed in `PREPROC_SRCS` in `Makefile.native` (piped through `clang -E | preproc | clang`), NOT plain `CFLAGS` compilation. Affected so far: `title_screen.c`, `sprite.c`, `text.c`, `text_window.c`, `text_window_graphics.c`, `strings.c`, `string_util.c`, `main_menu.c`, `new_menu_helpers.c`, `oak_speech.c`, `naming_screen.c`, `pokemon.c`, `battle_message.c`, `keyboard_text.c`, `util.c`, `battle_main.c`, `pokemon_icon.c`, `pokemon_storage_system_data.c`, `graphics.c`, `data.c`.
3. **DMA3 must be synchronous in PORTABLE mode.** `RequestDma3Copy`/`RequestDma3Fill` in `src/dma3_manager.c` perform instant `memcpy` and return immediately; `WaitDma3Request` always reports idle. The GBA queue-based path is guarded with `#else`.
4. **REG_VCOUNT must be 160 before `VBlankIntr()`.** `ProcessDma3Requests` and other engine code check `REG_VCOUNT > 224` to detect end-of-VBlank. `PPU_RenderFrame` leaves VCOUNT at 227; `src/main.c` `WaitForVBlank()` resets it to 160 each frame.
5. **`gFonts` must be initialized before any text is printed.** `SetDefaultFontsPointer()` now lives in the real `src/new_menu_helpers.c` (called via `InitTextBoxGfxAndPrinters`). If text prints with `gFonts == NULL`, `AddTextPrinter` silently no-ops or downstream code hangs.
6. **Save file status defaults to `SAVE_STATUS_EMPTY` (0)** in `src/platform/stubs.c` (`gSaveFileStatus`) so pressing START on the title screen starts a fresh game directly into character creation. It was previously hardcoded to 1 (`SAVE_STATUS_OK`) during early main menu porting to test the CONTINUE window, which caused interactive runs to select CONTINUE and hang on unlinked save-loading stubs.
7. **Software PPU video MODE 1.** Oak's Speech calls `InitBgsFromTemplates(1, ...)`: BG0/BG1 are text layers and BG2 is affine. `PPU_RenderScanline` has separate Mode 1 text and affine paths. Oak, the selected player portrait, and the rival naming icon are visually verified; the player-shrink transition remains to be checked after completing rival naming.
8. **Duplicate symbols when linking real engine files.** `src/platform/stubs.c` previously defined functions/data (`SetDefaultFontsPointer`, `DrawSpindaSpots`, `gMonFrontPicTable`, `gBattle_BG0_X`, etc.) now provided by linked engine files (`new_menu_helpers.c`, `pokemon.c`, `graphics.c`). When linking a real implementation, DELETE the stub version — never the reverse.
9. **Split sprite-sheet PNGs** (`name_0.png`, `name_1.png`, …) in `graphics/battle_anims/sprites/` must be combined into ONE png (vertically stacked, padded to width 64) before `gbagfx` 4bpp conversion; `gbagfx` cannot read them individually.
10. **`gbagfx` palette sources:** `INCBIN_U16("...gbapal")` sources are `.pal` (JASC) files, not PNGs. Convert `.pal → .gbapal → .gbapal.lz`.
11. **Naming-screen teardown spans the rest of the current frame.** `MainState_Exit` runs inside `RunTasks()`, changes the main callback, and frees `sNamingScreen`, but the old `CB2_NamingScreen` frame would otherwise continue into sprite animation and VBlank. Disable VBlank before freeing the state and return from `CB2_NamingScreen` immediately after `RunTasks()` when `sNamingScreen == NULL`.
12. **Stubbed sprite factories are unsafe for real scenes.** Returning 0 from `CreateObjectGraphicsSprite` is not a harmless no-op: callers mutate `gSprites[spriteId]`, corrupting whichever real sprite owns slot 0. Returning `MAX_SPRITES` is also unsafe when callers index without checking. Naming screens now create their icons from local, fully initialized templates; future callers must do the same or link the real object-event graphics path.
13. **`RegisterRamReset(RESET_REGS)` must restore affine identity matrices.** Clearing `REG_BASE` to zero collapses affine BG2/BG3 to one source pixel, hiding Oak and the player/rival portraits even though their tiles and maps are valid. After clearing registers, initialize `REG_BG2PA`, `REG_BG2PD`, `REG_BG3PA`, and `REG_BG3PD` to `0x100`.
14. **Real text tables must replace blank stubs completely.** The naming keyboard was blank because `battle_engine_stubs.c` supplied one-byte zero-filled `gText_NamingScreenKeyboard_*` arrays while `src/keyboard_text.c` was not linked. Add text-bearing sources to `PREPROC_SRCS` and delete every duplicate stub definition.
15. **Map headers, layouts, and script tables must use 64-bit host pointers.** GBA assembly `.4byte` pointers truncate 64-bit pointers on macOS ARM64. `tools/gen_map_data.py` compiles map layouts, headers, and event structures directly as native C structs in `src/maps.c`, where pointers are natively 64-bit (`sizeof(void *) == 8`).
16. **SaveBlock ASLR offset must be zero in PORTABLE mode.** GBA `SetSaveBlocksPointers` randomizes offsets up to 128 bytes assuming EWRAM padding. On macOS, save blocks are exact C struct buffers; adding an offset walks past buffer boundaries.
17. **Script command table (`gScriptCmdTable`) requires native function pointer sizing.** Script command dispatch indexes function pointers; on 64-bit architectures, this table must contain 8-byte pointers generated in C (`src/data/script_cmd_table.h`).
18. **`ForestMapPreviewScreenIsRunning` must return TRUE when idle.** In pret decompilation, this function returns whether the preview screen is *not* active. Returning FALSE keeps `FieldFadeTransitionBackgroundEffectIsFinished()` permanently false, preventing `Task_ExitNonDoor` from unfreezing objects and unlocking player controls upon warp entry.
19. **Map script headers must be terminated by 0x00.** Bytecode scripts terminate with `0x02` (`end`), but map header `.mapScripts` lists entries terminated by `0x00`. Using `0x02` causes `MapHeaderGetScriptTable` to overrun the array on frame-script checks.
20. **`src/menu.c` handles dialogue choice and cursor input.** Menu selection (`Menu_ProcessInput`, `Menu_InitCursor`, `Menu_MoveCursor`, `DrawStdFrameWithCustomTileAndPalette`, `CreateYesNoMenu`) was previously stubbed out in `battle_engine_stubs.c` returning 0, causing dialogue selection menus (such as Professor Oak's rival name choice box) to immediately choose option 0 (`NEW NAME`) and skip user interaction. `src/menu.c` is linked in `Makefile.native` with all duplicate stubs removed.

### Debugging Tips
- Boot test with N frames: `./firered-native --boot-test N` (saves `engine_boot_output.bmp` at frame N). The boot test auto-presses: START at frames 5–10 (skip intro fade), 30–35 (enter game from Title Screen), DOWN at 230–231 + A at 240–245 (select NEW GAME), then A at 320–325 / 360–365 / 400–405 / 520–525 / 560–565 (advance Controls Guide → Pikachu intro → Oak speech). Starting at frame 620 it also presses A for 6 frames every 90 frames. This periodic input advances Oak's dialogue, gender selection, player naming, rival naming (frames 6500–7200), farewell speech (frame 8600), hands off to `CB2_NewGame` by frame ~8900–9000, and spawns into the player's bedroom in Pallet Town (`CB2_Overworld`) by frame 9200.
- Missing-symbol workflow: `make -f Makefile.native` then extract `grep -oE '"_[A-Za-z0-9_]+"'` from linker output; find real definitions with `grep -rln "SymbolName" src/*.c`; add that file to ENGINE_SRCS (or PREPROC_SRCS if it has `INCBIN`/`_()`) and DELETE the stub version from `stubs.c`. Symbols from the battle engine go into `src/platform/battle_engine_stubs.c` instead (generated from header prototypes).
- A `SIGSEGV`/`SIGBUS` backtrace handler is installed in `src/platform/main.c` (`CrashHandler`) — crashes print a symbolized stack trace.
- `compile_flags.txt` at repo root configures clangd with `-DPORTABLE -DMODERN=1 -DFIRERED`; keep it in sync with `Makefile.native` CFLAGS.
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
│   ├── main_menu.c            # Main Menu scene (CONTINUE / NEW GAME)
│   ├── title_screen.c         # Title screen scene
│   ├── oak_speech.c           # Oak's Speech scene (NEW GAME flow)
│   ├── naming_screen.c        # Player and rival naming flows fully completed
│   ├── pokemon.c              # Pokemon data, MonSpritesGfxManager, Spinda spots
│   ├── platform/
│   │   ├── system.c           # Memory buffer allocations (REG_BASE, VRAM_, PLTT_, gHeap, SaveBlocks)
│   │   ├── bios.c             # Portable C implementations of GBA BIOS SWI routines
│   │   ├── ppu.c              # Software scanline rasterizer (Mode 0 text BGs; Mode 1 text BGs + affine BG2; sprites and blending)
│   │   ├── sdl2.c             # SDL2 windowing, texture streaming, and input handling
│   │   ├── stubs.c            # M4A audio, link, save status, and remaining scene-transition stubs
│   │   ├── battle_engine_stubs.c  # Battle data/entry-point substitutes, tripwires, and temporary non-battle scene dependencies
│   │   ├── new_game_intro_text.c  # Charmap-encoded text tables from data/text/new_game_intro.inc
│   │   └── main.c             # Native entry point (main) with crash backtrace handler
│   └── [engine subsystems]    # gpu_regs.c, palette.c, task.c, sprite.c, malloc.c, text.c, etc.
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

## 6. Immediate Next Steps: Downstairs Warp & Pallet Town Exterior

The engine spawns into `PalletTown_PlayersHouse_2F`, unfreezes objects, unlocks player controls, and accepts D-pad movement cleanly. Remaining work:
1. **Downstairs Stair Warp (2F → 1F):**
   - Handle stair warp execution when stepping on the upper-right staircase tile (x=10, y=2) to warp to `MAP_PALLET_TOWN_PLAYERS_HOUSE_1F`.
   - Resolve `DoStairWarp` / `Task_ExitStairs` flow and render 1F map view, metatiles, and Mom NPC object event.
2. **Pallet Town Exterior & Door Warp (1F → Outdoors):**
   - Support door exit from 1F into Pallet Town exterior (`FieldAnimateDoorClose` / `FieldAnimateDoorOpen`).
   - Render Pallet Town outdoor map, connecting boundaries, and outdoor NPC object events.
3. **Overworld Scripts & Interaction:**
   - Link signpost, PC, Mom dialogue, and Oak's tall-grass trigger script.

## 7. Git & Commit Guidelines

- **One commit per modified file:** The project owner requests individual commits per modified file rather than omnibus commits.
- **Keep build artifacts ignored:** Do not commit `firered-native`, `*.bmp`, `*.png`, or IDE-specific `compile_flags.txt`. These are ignored in `.gitignore`.
