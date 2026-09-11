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
4. **Software GBA PPU:** The GBA engine does not write to a linear framebuffer; it writes tiles, tilemaps, and OAM structures. A software PPU (`src/platform/ppu.c`) composites Mode 0/1 background layers and 128 OAM sprites into a 240×160 15-bit BGR555 texture displayed via SDL2 at the GBA's approximately 59.7275 FPS.

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
  - Top-left FPS overlay (development aid): `UpdateFpsCounter()` samples presented frames over 0.5 s windows and `DrawFpsOverlay()` draws a 5×7 bitmap font in the renderer's logical 240×160 space (translucent backing bar, then white glyph pixels as 1×1 rects). It draws after the GBA texture and never touches `sFramebuffer`, so `Platform_SaveScreenshot` captures stay byte-identical and overlay-free. Verified live: window shows `FPS 60` (paced 59.7275 Hz) in the bedroom.
  - The overlay is entirely behind `#ifdef FPS_OVERLAY` (state, glyph table, `UpdateFpsCounter()`/`DrawFpsOverlay()` calls). **Production removal: build with `FPS_OVERLAY=0`** — see §5. Nothing to delete from `src/platform/sdl2.c`; leave the guard in place so it stays available for dev builds.
  - Display-independent GBA frame pacing: `WaitForFrameDeadline()` in `src/platform/sdl2.c` runs before rendering in `Platform_RenderAndPresent()`. Interactive play, boot automation, and the standalone PPU test share this clock.
- [x] **CPU Game Engine Linked (`src/main.c` & Subsystems):**
  - `src/main.c` has its 32-bit assembly guarded and `WaitForVBlank()` hooked into the platform tick (`Platform_UpdateInput()`, `VBlankIntr()`, `Platform_RenderAndPresent()`).
  - Core subsystems linked: `gpu_regs.c`, `palette.c`, `task.c`, `sprite.c`, `malloc.c`, `scanline_effect.c`, `dma3_manager.c`, `bg.c`, `random.c`, `trig.c`, `decompress.c`, `blend_palette.c`.
  - Hardware stubs for sound (M4A) and link communications in `src/platform/stubs.c`.
- [x] **Title Screen Scene (`src/title_screen.c`):**
  - Charizard box art sprite, FireRed title logo, copyright bar, "PRESS START", and animated flame particles fully rendered and animating at the GBA frame rate in SDL2.
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
  - Boot automation at frame 9200 successfully executes `CB2_NewGame`, initializes save data, warps to `MAP_PALLET_TOWN_PLAYERS_HOUSE_2F`, draws the complete bedroom map view (bed, rug, TV/NES, PC, desk, stairs), spawns the player avatar facing North, and enters `CB2_Overworld` running at the GBA frame rate in SDL2.
- [x] **Dev Save Fast Boot (`--skip-intro`, `src/platform/dev_boot.c`):**
  - `Platform_DevBootNewGame()` runs the title screen's save initialization (`SeedRngAndSetTrainerId` → `SetSaveBlocksPointers` → `ResetMenuAndMonGlobals` → `Save_ResetSaveCounters` → `LoadGameSave` → `Sav2_ClearSetDefault`) on a fresh save, writes the naming screens' first choices (player `RED` male, rival `GREEN`), and hands off to `CB2_NewGame` — the same entry point Oak's farewell dialogue uses. Skips title screen, main menu, controls guide, Oak's speech, and both naming screens.
  - Names come from the charmap-encoded `gNameChoice_*` tables (`src/platform/new_game_intro_text.c`), never plain ASCII, so the save data renders and terminates correctly.
  - Hooked in `AgbMain()` after `SetDefaultFontsPointer()` and guarded by `#ifdef PORTABLE`; the flag is parsed in `src/platform/main.c` and declared in `include/platform/platform.h`.
  - `--boot-test`'s intro key sequence is gated on `!gPlatformSkipIntro`; dev runs get a compact sequence instead (UP at frames 60–65, A at 90–95) that replays the NES interaction the full boot test only reaches at frame 6280.
- [x] **Pallet Town Exterior, Front-Door Warp & Route Connections (`src/field_door.c`, `src/field_effect.c`, `src/wild_encounter.c`, `tools/gen_map_data.py`):**
  - Walking out of the front door works in both directions (1F ↔ Pallet Town) with the real door open/close animation, and Pallet Town renders its tilesets, NPCs, signs, and weather.
  - Outdoor maps now carry generated `struct MapConnections`, so the neighbouring map fills `VMap` at the border and the player can walk across it (Pallet Town ↔ Route 1 / Route 21).
  - Fixing this exposed four defects on the way out — door-animation pointer truncation, coord-event dummy-script livelock, field-effect stub table overrun, and a wild-encounter lock with no battle engine. See §3 fixes 26–29.
- [x] **Professor Oak's Lab Interactions (`tools/gen_map_data.py`, `src/data/maps_data.h`, `src/platform/main.c`):**
  - All ten object events and four background events in `MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB` were `sDummyScript`; pressing A on an aide, the rival, a starter ball, a dex unit, a PC or a sign did nothing.
  - The three aides, the rival, the two dex units, the two computer terminals, the two signs and the three starter balls now run their real scripts (see §6 item 3). Oak's own dialogue is deliberately left stubbed — he is hidden until the unported starter scene, and his closure needs the party/item-give engine.
  - `tools/gen_map_data.py` gained `copyvar`, `waitstate`, `removeobject`, `showmonpic`, `hidemonpic`, `givemon`, `bufferspeciesname`, `MSGBOX_YESNO` and the `YES`/`NO` constants; it now treats text defined inline in `data/event_scripts.s` as a text source and no longer reads an `.include` directive as a script command.
  - Verified with a temporary warp-stand-turn-press probe: 11/11 reachable targets opened their dialogue (`gStringVar4` charmap-correct, field locked while the box waited), and a PPU capture shows the aide's "I study POKéMON as PROF. OAK's AIDE." box.
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
21. **Script command-table end must be a derived pointer, never a separate array.** The interpreter bounds-checks every opcode with `cmdFunc >= ctx->cmdTableEnd`. `gScriptCmdTableEnd` was a separate 1-element `ScrCmdFunc[]` array; the macOS linker placed it ~500 KB BEFORE `gScriptCmdTable`, so every command failed the bounds check, `ScriptContext_RunScript()` shut down instantly, and all field interactions (A-press on NPCs/signposts) silently did nothing — movement still worked because it bypasses the interpreter. It is now `const ScrCmdFunc *const gScriptCmdTableEnd = gScriptCmdTable + ARRAY_COUNT(gScriptCmdTable)` in `src/data/script_cmd_table.h`. `ScriptContext_SetupScript`/`RunScriptImmediately` must pass `gScriptCmdTableEnd` as a VALUE (no `&`).
22. **`Makefile.native` has no header dependency tracking (no `.d` files).** Changing a header included by an ENGINE_SRCS file does NOT recompile that object — `make` silently links stale code. This bit the script-table fix: `src/data/script_cmd_table.h` changed but `scrcmd.o` didn't rebuild. After editing any header included by a built source, `touch` the including `.c` file or `make -B`. A dependency-file rule is future work.
23. **The linker requires pointer relocations to be aligned.** Emitting `const void *` fields inside `__attribute__((packed))` structs (or byte-spilled `(u8)((uintptr_t)&sym >> n)` arrays) fails with `ld: pointer not aligned`. Native scripts therefore store pointer references as u32 INDICES into the aligned `gNativeScriptPtrs[]` table generated by `tools/gen_map_data.py`; `ScriptReadPtr()` in `src/script.c` resolves `gNativeScriptPtrs[ScriptReadWord(ctx)]`. Opcodes carrying pointers must call `ScriptReadPtr`, never `ScriptReadWord`.
24. **Pointer-typed operand ABI for native scripts.** `ScriptContext.data` is `uintptr_t[4]` on the host. Commands whose operands are addresses (`goto`/`call`/`goto_if*`/`call_if*`/`loadword` with labels, `applymovement`, `msgbox`, `special` table lookups) read host-width pointers via `ScriptReadPtr`; numeric/virtual operands (`vgoto`, `vmessage`, plain `.2byte`) stay `ScriptReadHalfword`/`ScriptReadWord`.
25. **macOS/SDL window-repaint quirk.** Occasionally a freshly launched window shows a stale mid-fade frame (a gray FireRed logo) even though the engine loop is healthy — confirmed via `sample`: PPU rendering, texture upload, and frame pacing all tick while the window shows the stale frame. Clicking/focusing the window redraws it. Not an engine bug; do not chase it in PPU/renderer code.
26. **Host pointers cannot be split across 16-bit task-data slots.** `SetWordTaskArg()` stores two 16-bit slots, so `SetWordTaskArg(taskId, N, (uintptr_t)ptr)` silently truncates on 64-bit. `Task_AnimateDoor` (`src/field_door.c`) packed `struct DoorGraphics *` and `struct DoorAnimFrame *` into two slots each and rebuilt a 32-bit pointer, so the FIRST door animation (stepping out of the front door) dereferenced garbage and crashed. Both operands are now stored whole with `memcpy` (`tFramesPtr`/`tGfxPtr`, four slots each on the host). The same trap existed in `src/new_menu_helpers.c` (`DecompressAndLoadBgGfxUsingHeap*` → `TaskFreeBufAfterCopyingTileDataToVram`), which freed a truncated buffer during the map transition; that path now uses the new `SetPointerTaskArg()`/`GetPointerTaskArg()` (`include/task.h`, `src/task.c`). Other `SetWordTaskArg(..., (uintptr_t)…)` sites remain in files that are not linked yet (`easy_chat_2.c`, `intro.c`, `pokemon_jump.c`, `pokemon_special_anim*.c`, `shop.c`) and in `src/fldeff_flash.c` (map preview, excluded under `PORTABLE`). Fix them the same way before linking those features.
27. **A coord event must never fall back to `sDummyScript`.** An inline `end` script is NOT inert for an automatic proximity trigger: `ProcessPlayerFieldInput()` → `TryStartStepBasedScript()` re-runs it every frame the player stands on the tile, `DoCB1_Overworld()` re-locks the field each time, and the player can never start another step — a permanent freeze. This is what stopped the player dead on Pallet Town's `(12,1)`/`(13,1)` Oak trigger. `tools/gen_map_data.py` now emits `.script = NULL` for any coord event whose script is not compiled natively; `TryRunCoordEventScript()` handles NULL (runs the weather trigger and returns) so the tile is simply walkable. Object and BG events keep `sDummyScript` — an A-press that does nothing is harmless there.
28. **Unported field effects must be no-ops, and the stub table must be full-size.** `FieldEffectStart()` indexes `gFieldEffectScriptPointers[fldeff]` and then calls `sFldEffScrcmdTable[*script]`. The stub table was a single `{ NULL }` entry, so the tall-grass ground effect on the first step into Route 1 read out of bounds and jumped through a garbage command pointer. The stub is now sized `[FLDEFF_COUNT]` (`include/constants/field_effects.h`, `src/platform/overworld_stubs.c`) and `FieldEffectStart()` returns 0 for a NULL script WITHOUT adding the id to the active list (otherwise waiters would hang forever). Field effects do not render yet — `src/field_effect_helpers.c` is not linked and the real script data (`data/field_effect_scripts.s`) stores 32-bit pointers that cannot be read on the host as-is.
29. **Wild encounters must not start while the battle engine is stubbed out.** `CheckStandardWildEncounter()` → `StandardWildEncounter()` → `StartWildBattle()` (a no-op stub) still returned TRUE, so `DoCB1_Overworld()` locked the field expecting a battle to unlock it later — walking into Route 1's tall grass froze the game with `locked=1, scriptRunning=0`. `StandardWildEncounter()` returns FALSE under `PORTABLE` (the battle engine is not linked); `StartWildBattle()`/`StartRoamerBattle()` remain stubs.
30. **Outdoor maps need generated `MapConnections`.** `MapHeader.connections` was hardcoded NULL, so `gMapConnectionFlags` stayed all-false, `GetMapBorderIdAt()` answered `CONNECTION_INVALID` in every direction (walling the player inside Pallet Town) and `InitBackupMapLayoutConnections()` had nothing to fill the neighbouring map into `VMap`. `tools/gen_map_data.py` now emits `struct MapConnection`/`struct MapConnections` from each `map.json` `connections` array (`up/down/left/right` → `CONNECTION_NORTH/SOUTH/WEST/EAST`, with `offset`, `MAP_GROUP()`/`MAP_NUM()`), which both draws the neighbour across the border and lets `CameraMove()` hand off via `LoadMapFromCameraTransition()`.
31. **Sprites whose top is above the screen must be clipped, not dropped.** `DrawSprites()` (`src/platform/ppu.c`) converted a wrapped OAM y (`y >= DISPLAY_HEIGHT` → `y -= 256`, i.e. 248 → −8) and then compared it against the scanline as `vcount >= (unsigned int)(y - half_height)`, so any sprite with a negative top failed the test and was skipped entirely instead of having its visible rows drawn. Overworld sprites use `coordOffsetEnabled` (map-space `sprite->y` plus `gSpriteCoordOffsetY` at OAM build time), so walking south in `PalletTown_PlayersHouse_1F` gives Mom at `(8,4)` an OAM y of 248 the moment the player reaches the room's south-most walkable row `(x,8)` — `CameraUpdate()` keeps the player ~4.5 tiles down the screen and pans 16 px per tile, and the room is only 10 tiles tall. She vanished for one row and returned one step north; the engine was not hiding her (`invisible=0`, `offScreen=0`, `oam.y=248`). The window check is now signed (`(int32_t)vcount >= y - half_height && (int32_t)vcount < y + half_height`); sprites fully above the screen (OAM y ≥ 256 − height) still stay hidden. Verified with a scripted `--skip-intro` walk (bedroom → 2F stairwell `(10,2)`, hold west → 1F → down to `(10,8)`): the pre-fix capture has no Mom, the post-fix capture draws her clipped at the top edge.
32. **The native `gSpecials` table must stay index-aligned with `data/specials.inc`.** `tools/gen_map_data.py` encodes a `special` as the opcode plus the special's *position* in the real specials table (`data/specials.inc`), and `ScrCmd_special`/`ScrCmd_specialvar` index `gSpecials` with it. The stub table in `src/platform/overworld_stubs.c` used to hold one entry (`HealPlayerParty`, index 0 by luck), so any other special was out of range and `AGB_ASSERT_EX` → `abort()` (fix the player sees: reading Oak's lab sign killed the game). Entries are now designated initializers keyed by the real index (`[0]`, `[368]`, `[369]`, `[371]`), unported slots stay `NULL`, and the `PORTABLE` dispatch prints `[Script] special N is not ported` instead of calling through `NULL` or aborting. Adding a `special` to a compiled scene means adding its entry here; if `data/specials.inc` ever changes, the generator re-emits a different index and the table reports the miss loudly rather than jumping into a neighbouring function.
33. **FRLG reads signs by walking into them, and walking away must close the box.** FireRed/LeafGreen (unlike Ruby/Sapphire/Emerald) open a signpost's text when the player holds the direction he is facing into the sign — `ProcessPlayerFieldInput()` checks held input (`heldDirection`/`checkStandardWildEncounter` → `TrySetUpWalkIntoSignpostScript`) *before* it checks `pressedAButton`. That path was already correct here. What was missing was the dismissal: `FieldInput_HandleCancelSignpost()` pushes a direction *away* from the sign, calls `ScriptContext_SetupScript(EventScript_CancelMessageBox)`, and locks the field, but `EventScript_CancelMessageBox` was one of the dummy `{ 0x02 }` scripts in `src/platform/overworld_stubs.c`, so the script did nothing and the signpost frame stayed drawn over the world for ~65 frames of walking. The real 3-byte script (`special DoPicboxCancel`, `release`, `end`) is now compiled from `data/event_scripts.s` (added to `NATIVE_SCRIPT_ROOTS`) and special 346 (`DoPicboxCancel`) is a native entry in `gSpecials` — it mirrors `src/field_specials.c` minus its `PicboxCancel()` call, since `src/script_menu.c` (`ScriptMenu_ShowPokemonPic`) is not linked and that window can never be open. Verified under `--skip-intro` with a temporary warp + input probe: with the stub, a held-into sign at frame 245 shows `mb_hidden=0 mb_type=2 signpost=1` and the box survives until frame 320 as the player walks away; with the real script the box is hidden the same frame the cancel script runs (`mb_hidden=1 mb_type=0`) and the PPU capture shows no box. Both the held-direction walk-into and the A-press paths open the box in the real signpost frame (`LoadSignpostWindowFrameGfx`). `TestOverworldInteractions()` (`src/platform/main.c`, runs on every launch) now also asserts the cancel script's opcode sequence, so a reintroduced dummy stub fails loudly instead of silently leaving the box on screen.
34. **Object-event sprite sheets need `spritesheet_rules.mk`, not plain gbagfx conversion.** `src/data/object_events/object_event_graphics.h` bakes every overworld sprite in with `INCBIN`, and `overworld_frame(ptr, 2, 4, frame)` addresses frame *i* at byte offset `i*256` assuming GBA column-major tiles *inside* each frame. `gbagfx` only emits that layout when given pret's per-sprite `-mwidth/-mheight` flags; a plain conversion emits row-major tiles and the frames land in the wrong places. `Makefile.native` had explicit flag rules for only three sprites (`red_normal`, `green_normal`, `mom`), so `woman_1`, `fat_man` and `prof_oak` — and 126 others — were row-major and their first frames decoded to all-transparent tiles. Symptom: Pallet Town's sign lady and fat man were invisible while the player and Mom rendered fine; a runtime probe showed `tilesum=0` in OAM VRAM for the NPCs (vs 16609 for the player), and `srcsum=0` on the copy request proved the *source* frame was blank rather than the DMA path. Auditing all 157 object-event sprites against their canonical rules found 129 mismatches. `Makefile.native` now does `include spritesheet_rules.mk` (plus a plain `%.4bpp: %.png` fallback for the ~25 sprites pret gives no rule) and `assets` depends on `$(OBJ_EVENT_PICS)`, so tile order comes from pret's own table and cannot drift. `spritesheet_rules.mk` defines concrete targets, so the Makefile pins `.DEFAULT_GOAL := all`. After regeneration all 157 sprites byte-match canonical conversion. Four assets (`birth_island_stone`, `seagallop`, `ss_anne`, `old_man_lying_down`) still have a blank frame 0 — that is correct; their source PNGs are transparent there.

### Native Frame Timing
- **The game clock is not the display clock.** The original native loop relied on `SDL_RENDERER_PRESENTVSYNC` without an independent timer; that allowed high-refresh presentation to accelerate frame-based gameplay, while the fallback renderer did not explicitly request VSync. This timing gap was found after movement appeared too fast. The pre-fix runtime frame rate was not measured.
- **Target the hardware frame period:** 280,896 CPU cycles per frame at 16,777,216 Hz, or approximately 59.7275 Hz (16.7427 ms per tick). Do not change movement speeds, animation counters, or emulate CPU clock speed to compensate for host performance.
- **One pacing owner:** `WaitForFrameDeadline()` uses `SDL_GetPerformanceCounter()` / `SDL_GetPerformanceFrequency()` and absolute deadlines. Time spent in the engine and renderer counts toward the next deadline; `SDL_Delay()` sleeps rather than busy-spins. Do not add a fixed sleep after every frame or a second limiter in `WaitForVBlank()`, the BIOS shim, or the standalone test.
- **VSync is deliberately disabled** in renderer creation so monitor refresh does not set engine speed. Do not re-enable it as a replacement for the game clock; any future presentation changes must preserve independent simulation timing.
- **Stalls do not accumulate catch-up ticks.** When the next deadline is already past, the clock resynchronizes to one frame after the current time. Sustained overload slows the game rather than skipping engine updates.
- **Verification:** Metal and software renderers were exercised with 4 ms of simulated engine work per frame. For 300 frames, elapsed times were 5.022 s and 5.049 s respectively (target 5.023 s). Both recovered from a 500 ms stall without a catch-up burst. A real 9,300-frame engine boot completed in 155.92 s (frame-time target 155.71 s plus startup), with bedroom spawn confirmed in the capture. The standalone 120-frame PPU test also passed. Temporary timing probes were removed.

### Debugging Tips
- **Boot tests now run at game speed:** allow roughly `N / 59.7275` seconds plus startup for `--boot-test N`; 9,300 frames take about 156 seconds. Use a timeout above that duration. `--test` renders 120 paced frames in about two seconds; its old extra `SDL_Delay(16)` was removed.
- Boot test with N frames: `./firered-native --boot-test N` (saves `engine_boot_output.bmp` at frame N). The boot test auto-presses: START at frames 5–10 (skip intro fade), 30–35 (enter game from Title Screen), DOWN at 230–231 + A at 240–245 (select NEW GAME), then A at 320–325 / 360–365 / 400–405 / 520–525 / 560–565 (advance Controls Guide → Pikachu intro → Oak speech). Starting at frame 620 it also presses A for 6 frames every 90 frames. This periodic input advances Oak's dialogue, gender selection, player naming, rival naming (frames 6500–7200), farewell speech (frame 8600), hands off to `CB2_NewGame` by frame ~8900–9000, and spawns into the player's bedroom in Pallet Town (`CB2_Overworld`) by frame 9200.
- Fast overworld iteration: `./firered-native --skip-intro --boot-test 200` reaches the bedroom (fresh save, `RED`/`GREEN`) in ~3.5 s instead of ~156 s for the 9,300-frame full boot. Byte-identical captures across runs (RNG is seeded from a host-zeroed timer).
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
│   │   ├── dev_boot.c         # --skip-intro: fresh save straight into the bedroom
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

### Release build — no development FPS overlay:
```bash
rm -f build/native/platform/sdl2.o && make -f Makefile.native FPS_OVERLAY=0
```
`FPS_OVERLAY` defaults to `1` (`Makefile.native` line ~7; it appends `-DFPS_OVERLAY` to `CFLAGS`). `src/platform/sdl2.c` guards the FPS counters, glyph table, and both draw calls with `#ifdef FPS_OVERLAY`, so a `FPS_OVERLAY=0` binary contains no overlay code. **The stale `sdl2.o` must be deleted first** (or `touch src/platform/sdl2.c`): object files do not depend on `CFLAGS`, so `make` would otherwise keep the previously compiled overlay (same trap as fix #22). `-B` also works but rebuilds everything, assets included.

Verified: with `FPS_OVERLAY=0` the compile line carries no `-DFPS_OVERLAY`, `nm build/native/platform/sdl2.o` reports zero `fps` symbols, and the running window shows an overlay-free bedroom; the default build shows `FPS 60`.

### Run PPU standalone test (120 frames with test pattern & sprite):
```bash
./firered-native --test
```

### Run Engine boot test (60 frames through AgbMain):
```bash
./firered-native --boot-test
```

### Dev save — skip the intro and spawn in the bedroom:
```bash
./firered-native --skip-intro              # fresh save, bedroom, interactive
./firered-native --skip-intro --boot-test 200   # same, screenshot at frame 200
```
`--skip-intro` builds a fresh save (player `RED`, male, rival `GREEN`) and drops straight into `PalletTown_PlayersHouse_2F`, bypassing the title screen, main menu, controls guide, Oak's speech, and both naming screens. Use it for overworld iteration; use plain `--boot-test N` when the intro flow itself is under test. Flags may appear in any order.

### Run Engine interactively:
```bash
./firered-native
```

---

## 6. Overworld Warp Status & Next Steps

The engine spawns into `PalletTown_PlayersHouse_2F`, unfreezes objects, unlocks player controls, and accepts D-pad movement cleanly.
1. **Downstairs Stair Warp (2F ↔ 1F): Complete**
   - `Makefile.native` links the real `src/fldeff_flash.c` map-transition callbacks and cave-transition assets. The empty `CB2_DoChangeMap` stub previously stranded the engine after fade-out instead of invoking `gMain.savedCallback` to load the destination.
   - A 10,800-frame native smoke run with temporary D-pad input verified arrival on 1F, completed fade, unlocked controls, walking away from the stairs, and returning to 2F. A PPU capture verified the downstairs room and Mom.
   - Party-menu Flash and map-preview screens remain excluded under `PORTABLE`; normal map loading and cave fade transitions use the real engine code. Cave transitions were not exercised by the stair smoke.
   - **Mom dialogue is real and interactive (completed):** `tools/gen_map_data.py` now compiles a script's full dependency closure from the ORIGINAL pret sources (`data/maps/*/scripts.inc`, `text.inc`, `data/scripts/**/*.inc`, `data/event_scripts.s`) into native bytecode inside `src/data/maps_data.h`. Unsupported commands fail loudly (stderr report) and fall back to `sDummyScript`. Mom's male/female/heal branches, movement, and `EventScript_OutOfCenterPartyHeal` all compile from source.
   - **Overworld object and TV interactions (completed):**
     - **1F TV (`PalletTown_PlayersHouse_1F_EventScript_TV`):** Interacting facing North plays gender-specific movie dialogue ("Four boys on railroad tracks..." / "Girl in pigtails on a brick road..."); interacting from sides displays "Oops, wrong side...".
     - **2F NES (`PalletTown_PlayersHouse_2F_EventScript_NES`):** Interacting facing North from (6, 6) towards (6, 5) plays "{PLAYER} played with the NES. ...Okay! It's time to go!".
     - **2F Signpost (`PalletTown_PlayersHouse_2F_EventScript_Sign`):** Posted notice sign at (11, 1) displays "It's a posted notice... If you're confused, ask for HELP! Press the L or R Button!".
     - **House Metatile Flavor Scripts:** Linked real implementations for `EventScript_Bookshelf` ("It's crammed full of POKéMON books"), `EventScript_Dresser` ("It's a nicely made dresser"), `EventScript_Cabinet` ("Dishes and plates are neatly lined up"), `EventScript_Kitchen` ("It smells delicious! Somebody's been cooking here"), and `EventScript_PlayerFacingTVScreen` ("There's a POKéMON on TV!"). Removed dummy stubs from `overworld_stubs.c` and updated `tools/gen_map_data.py` to collect `data/text/*.inc` sources and emit non-static symbols for global event scripts.
     - **2F PC:** The 2F PC script (`PalletTown_PlayersHouse_2F_EventScript_PC`) remains stubbed pending item storage / `player_pc.c` menu linkage.
   - **Interaction path for future scenes:** A-press → `FieldGetPlayerInput()` (gates: avatar `T_NOT_MOVING`/`T_TILE_CENTER`, speed != `PLAYER_SPEED_FASTEST`, no `PLAYER_AVATAR_FLAG_FORCED`) → `ProcessPlayerFieldInput()` → `GetInteractedObjectEventScript()`/`GetInteractedBackgroundEventScript()` → `ScriptContext_SetupScript()`. If an interaction silently does nothing but movement still works, suspect the script interpreter (see fix #21) or a stub script (`sDummyScript` = instant end).
   - **Object-event sprite sheets:** `Makefile.native` now builds every `graphics/object_events/pics/**/<name>.png` through pret's own `spritesheet_rules.mk` (see fix #34), so the `-mwidth/-mheight` tile order is correct automatically and `assets` regenerates them all. Do NOT hand-write gbagfx lines for new object-event graphics; add the sprite to `spritesheet_rules.mk` if pret has a rule for it. Palettes still need a `.pal → .gbapal` rule.
   - **Stub-vs-real scripts:** `src/platform/overworld_stubs.c` still defines dummy event scripts for every non-linked scene (`sDummyScript` = `{0x02}` instant-end). Interacting with TV/PC currently "succeeds" then instantly ends (no visible effect). When wiring a real scene, delete the stub definition and add the source (see also fix #8). `gStdScripts`/`gStdScriptsEnd` remain stubbed — Mom's default msgbox path does not need them.
   - **Specials:** `gSpecials` in `overworld_stubs.c` is native and `gSpecialsEnd` a one-past pointer; index 0 (`NativeSpecial_HealPlayerParty`) calls the real `HealPlayerParty()` from `src/script_pokemon_util.c`. Party-selection/egg-giving functions (`ScriptGiveMon`, `ScriptGiveEgg`, `CreateScriptedWildMon`, `ScriptSetMonMoveSlot`, `ReducePlayerPartyToThree`, `ChooseHalfPartyForBattle`) are `#ifndef PORTABLE`-guarded in `script_pokemon_util.c` with stubs retained in `overworld_stubs.c`.

2. **Pallet Town Exterior & Front-Door Warp (1F ↔ Outdoors): Complete**
   - Exit is the south arrow warp (`MB_SOUTH_ARROW_WARP` = `0x65`) on 1F's `(4,8)`/`(5,8)`; walking DOWN while standing on it (facing South) warps to `MAP_PALLET_TOWN` warp 0. Re-entering the house is the reverse: step onto Pallet Town's door tile `(6,7)` (`MB_WARP_DOOR`). BOTH directions play the real door animation (`Task_ExitDoor` → `FieldSetDoorOpened`/`FieldAnimateDoorOpen`/`FieldAnimateDoorClose`).
   - The `SIGSEGV` on stepping out the front door was the door-animation task storing 64-bit pointers in 16-bit task slots — see fix #26.
   - Pallet Town (`MAP_PALLET_TOWN`, map **3/0**, 24×20) renders its primary/secondary tilesets, weather, signs, and object events (sign lady at `(3,10)`, fat man at `(13,17)`, Oak at `(10,8)` hidden by `FLAG_HIDE_OAK_IN_PALLET_TOWN`). Its two connections (north `MAP_ROUTE1`, south `MAP_ROUTE21_NORTH`) are generated — see fix #30.
   - Verified with a scripted `--skip-intro` run (input injected through `REG_KEYINPUT` in `WaitForVBlank`, same path as SDL input): bedroom → stairs → 1F → front door → out to Pallet Town → north across the border into Route 1 (map **3/19**, 24×40) → south back into Pallet Town → back in through the front door to 1F. No crash; controls unlocked at every stage (asserted via `ArePlayerFieldControlsLocked()`/`ScriptContext_IsEnabled()` dumps); PPU captures checked for the exterior and Route 1. The temporary probe and scripted input were removed afterwards.
   - **Still stubbed / deliberately disabled while walking outside:**
     - **Oak's interception trigger is INERT.** Pallet Town's coord events at `(12,1)`/`(13,1)` (`PalletTown_EventScript_OakTriggerLeft/Right`, `VAR_MAP_SCENE_PALLET_TOWN_OAK == 0`) are generated with `.script = NULL`, so the player can walk to Route 1 without a Pokémon. `famechecker`, `textcolor`, `delay`, `playse` and the movement mnemonics compile now; what still blocks the cutscene is `playbgm`, `addobject`, `opendoor`/`waitdooranim`/`closedoor`, `warp`/`waitstate`, `specialvar`/`copyvar`/`buffernumberstring`, `removeobject`, `setworldmapflag` and the map-script table (see item 3). Coord events MUST stay NULL rather than `sDummyScript` (fix #27).
     - **Wild encounters are DISABLED.** `StandardWildEncounter()` returns FALSE under `PORTABLE` because the battle engine is not linked (fix #29); tall grass has no rustle and no battles, but walking through it no longer freezes the field.
     - **Field effects do not render.** `gFieldEffectScriptPointers` is a full-size table of NULLs and `FieldEffectStart()` is a no-op for them (fix #28): no tall-grass rustle, footprints, dust, splashes, exclamation marks, etc.
   - **Pallet Town exterior interactions (completed):** every A-press target in the town now runs its real script.
     - **Signs:** `PalletTown_EventScript_PlayersHouseSign` ("{PLAYER}'s house"), `_RivalsHouseSign` ("{RIVAL}'s house"), `_TownSign` ("PALLET TOWN …"), `_TrainerTips` ("TRAINER TIPS …"), `_OaksLabSign` ("OAK POKéMON RESEARCH LAB").
     - **NPCs:** `PalletTown_EventScript_SignLady` (full original script: `lock`, `SIGN_LADY_READY`/`FLAG_TEMP_2` branches, `playse SE_PIN`, three `applymovement`s, `copyobjectxytoperm`) and `_FatMan` ("Technology is amazing! …").
     - **Generator (`tools/gen_map_data.py`) support added for this scene:** script commands `playse`, `copyobjectxytoperm`, `famechecker`, `delay`, `textcolor`, `signmsg`, `normalmsg`; `.equ` symbols from map `scripts.inc` (e.g. `SIGN_LADY_READY` → `VAR_TEMP_2`); movement labels declared next to their map scripts (not only in `data/scripts/movement.inc`); and movement mnemonics resolved through `asm/macros/movement.inc` to their `MOVEMENT_ACTION_*` constant instead of a hardcoded table.
     - **Two generator defects fixed on the way:** constant resolution stripped every `U` from an expression, so `MAX_TRAINERS_COUNT` became `MAX_TRAINERS_CONT` and *every* constant derived from `SYS_FLAGS` (`TRAINER_FLAGS_END`, all system flags, `FLAG_OPENED_START_MENU`, …) failed to resolve — only C integer suffixes are stripped now; and each `NATIVE_SCRIPT_ROOTS` entry is compiled independently, so an unsupported command reports the scene on stderr and leaves it on its dummy-script fallback instead of aborting the whole generator or wiring a half-compiled script.
     - **Native specials are index-aligned** with `data/specials.inc` (see fix #32). `famechecker` reaches `NativeSpecial_SetFlavorTextFlagFromSpecialVars` (index 371), which mirrors the fame checker save-data bookkeeping in `src/fame_checker.c`; `SetWalkingIntoSignVars` (368) and `DisableMsgBoxWalkaway` (369) call the real functions in `src/script.c`.
     - **Verified** with a temporary warp-stand-turn-press probe under `--skip-intro`: 7/7 targets rendered their own text into `gStringVar4` (asserted against the expected charmap bytes) with `gSpecialVar_LastTalked` 1/2 for the two NPCs, and PPU captures show each sign/NPC dialogue on screen. A one-frame direction press turns the player without stepping; holding the key long enough makes him walk onto the tile instead.
     - **Signs match FRLG's walk-into behavior** (see fix #33): holding the faced direction into a sign opens its text automatically — this is genuine FireRed/LeafGreen behavior, not a port bug, and Ruby/Sapphire/Emerald do NOT have it. A-press works as well. Both go through `TrySetUpWalkIntoSignpostScript`/`GetInteractedBackgroundEventScript`, which call `MsgSetSignpost()`, so the box draws with the signpost frame. Pushing a direction away closes the box via the real `EventScript_CancelMessageBox`; before fix #33 that symbol was a dummy and the box frame lingered over the world while the player walked off.
3. **Professor Oak's Lab interactions: Complete**
   - **Every reachable A-press target in `MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB` runs its real script.** Previously all ten object events and the four background events carried `sDummyScript`, so pressing A on an aide, the rival, a starter ball, a PC or a sign did nothing at all.
   - **NPCs:** `PalletTown_ProfessorOaksLab_EventScript_Aide1`/`Aide2`/`Aide3` ("I study POKéMON as PROF. OAK's AIDE." with the `FLAG_SYS_GAME_CLEAR` fame-checker variant) and `_Rival` (the `VAR_MAP_SCENE_PALLET_TOWN_PROFESSOR_OAKS_LAB` 3/2/0 branches).
   - **Objects:** the three item balls (`_BulbasaurBall`, `_SquirtleBall`, `_CharmanderBall`) and the two dex units (`_Pokedex`, "It's like an encyclopedia, but the pages are blank.").
   - **Background events:** the two computer terminals (`_Computer`, "There's an e-mail message here. …") and the two signs (`_LeftSign` "PRESS START…", `_RightSign` "SAVE…" with its `VAR >= 6` alternate text).
   - **Oak (localId 4) is deliberately left on the dummy.** He is hidden by `FLAG_HIDE_OAK_IN_HIS_LAB`, which `EventScript_ResetAllMapFlags` sets and only the unported starter scene clears, and `_EventScript_ProfOak`'s dependency closure pulls the whole dex/starter-give scene (`giveitem_msg`, `checkitem`, `removeitem`, `bufferitemnameplural`, `gStdScripts` bag messages, pokedex ratings) that the native port has no engine for. The three starter balls DO compile that closure, but `VAR_MAP_SCENE_PALLET_TOWN_PROFESSOR_OAKS_LAB` only ever reaches 2 from the lab's own (uncompiled) map scripts, so their reachable behavior is the pre-starter "Those are POKé BALLS" line.
   - **Generator (`tools/gen_map_data.py`) support added for this scene:** script commands `copyvar`, `waitstate`, `removeobject`, `showmonpic`, `hidemonpic`, `givemon`, `bufferspeciesname`, `MSGBOX_YESNO` (inlined as `message`/`waitmessage`/`yesnobox 20, 8` so it does not depend on the unemitted `gStdScripts` table) and the `YES`/`NO` comparison constants; shared text defined inline in `data/event_scripts.s` (e.g. `Text_GiveNicknameToThisMon`) is now a text source; and `parse_labels()` no longer treats an `.include` directive as a script command of whatever label precedes it.
   - **Smoke test:** `TestOverworldInteractions()` (`src/platform/main.c`) asserts all 10 lab object events and 4 BG events point at their real scripts (Oak's stays `0x02`), that the NPCs/balls begin with `lock`, that the BG events begin with `lockall`, and that the first aide's message operand resolves through `gNativeScriptPtrs`.
   - **Verified** with a temporary warp-stand-turn-press probe under `--skip-intro`: 11/11 targets opened their own message (`gSpecialVar_LastTalked` 1/2/3/5/7/8/9 and charmap-correct `gStringVar4` bytes; the computer/PC and sign targets do not set `LastTalked`), each left `ScriptContext_IsEnabled()`/`ArePlayerFieldControlsLocked()` true while its box waited for a button press, and a PPU capture of the Aide1 box reads "I study POKéMON as PROF. OAK's AIDE.". The probe and injected input were removed afterwards.
4. **Overworld Scripts & Interaction — next:**
   - **Oak's Pallet Town interception cutscene** (the scene that normally plays when you try to leave town without a Pokémon): compile `PalletTown_EventScript_OakTrigger{Left,Right}` + closure natively. `famechecker`, `textcolor`, `delay`, `playse` and the movement mnemonics are supported now; what still blocks it is `playbgm`, `addobject`, `opendoor`/`waitdooranim`/`closedoor`, `warp`/`waitstate`, `setobjectxyperm`/`setobjectmovementtype`, `specialvar`/`buffernumberstring`, and `setworldmapflag`, plus the map-script tables (`map_script`/`map_script_2` — `MapHeader.mapScripts` is still `sEmptyMapScripts`). Then flip those coord events from NULL to the real script.
   - **Oak's Lab starter scene & Oak's own dialogue** (items 3's deliberate gap): needs the lab map scripts (`map_script`/`map_script_2`), the party/item-give path (`ScriptGiveMon` is `#ifndef PORTABLE`-guarded), the `gStdScripts` bag-message tables, and a working `yesnobox` (`ScriptMenu_YesNo`).
   - **2F PC menu linkage** (`player_pc.c`, item storage).
   - **Field effects** (start with tall grass): link `src/field_effect_helpers.c` and give `FieldEffectScript_ReadWord()` a PORTABLE index-table path (`gNativeFieldEffectPtrs[]`, like `gNativeScriptPtrs`) so the `data/field_effect_scripts.s` operands survive 64-bit.

## 7. Git & Commit Guidelines

- **One commit per modified file:** The project owner requests individual commits per modified file rather than omnibus commits.
- **Keep build artifacts ignored:** Do not commit `firered-native`, `*.bmp`, `*.png`, or IDE-specific `compile_flags.txt`. These are ignored in `.gitignore`.
