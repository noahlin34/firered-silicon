Headline

┌─────────────────────────────────┬─────────────────────┐
│                                 │ count               │
├─────────────────────────────────┼─────────────────────┤
│ src/*.c total                   │ 285                 │
├─────────────────────────────────┼─────────────────────┤
│ linked (ENGINE_SRCS 65 /        │ 181                 │
│ PREPROC_SRCS 119 /              │                     │
│ PLATFORM_SRCS 14)               │                     │
├─────────────────────────────────┼─────────────────────┤
│ unlinked                        │ 104                 │
├─────────────────────────────────┼─────────────────────┤
│ stub TUs (overworld_stubs,      │ 438 defined         │
│ battle_engine_stubs,            │ 294 are             │
│ battle_peripheral_stubs, stubs) │ the sole definition │
│                                 │ (load-bearing); the │
│                                 │ other 144 collide   │
│                                 │ with another        │
│                                 │ definition and lose │
│                                 │ (127 to a generated │
│                                 │ table, 17 to a      │
│                                 │ linked source)      │
└─────────────────────────────────┴─────────────────────┘

 Landed since this inventory was written:

 - Poké Mart shop. src/shop.c + src/buy_menu_helpers.c are
   linked via PREPROC_SRCS (both carry _() strings; the
   shop_menu INCBINs live in the already-linked src/graphics.c,
   and graphics/shop_menu/* converts through the generic
   %.4bpp/%.gbapal/%.lz chain, so no new asset rules). The 4
   stub definitions (CreatePokemartMenu,
   CreateDecorationShop1Menu, CreateDecorationShop2Menu,
   RecordItemTransaction) are deleted and the three "[Script]
   pokemart is not ported" guards are gone from src/scrcmd.c,
   so the Mart clerk sells. One 64-bit defect surfaced (fix
   #83): the buy menu carried its MainCallback through
   SetWordTaskArg, which keeps 32 bits — now
   SetPointerTaskArg/GetPointerTaskArg at data[8]. Covered by
   tests/shop.c (7 tests; the engine ones boot the
   fixture:dex save). See AGENTS.md item 8c.

- Item/TM animation scene. src/pokemon_special_anim_scene.c is
  linked via PREPROC_SRCS (10 INCBINs) and all 27 stubs it
  supersedes are deleted from battle_peripheral_stubs.c, so the
  item-use, TM/HM and level-up animations draw instead of only
  advancing state. One 64-bit defect surfaced (fix #85): four
  sprite pointers were stored with SetWordTaskArg, which keeps
  32 bits — gSprites is at 0x1_008d1ec8, so the truncated read
  faulted in Task_ZoomAnim. Now a task-keyed side table
  (sTaskSprites[NUM_TASKS]). Covered by tests/item_anim.c's
  fourth test, which asserts the pixels: pre-fix the scene
  painted 350 px and created 0 sprites; post-fix >= 10 live
  sprites. See AGENTS.md item 7.4 and fix #85.

- Evolution scene. src/evolution_scene.c +
  src/evolution_graphics.c are linked via PREPROC_SRCS (both
  INCBIN assets; evolution_scene.c carries 5 _() strings).
  BeginEvolutionScene, EvolutionScene and gCB2_AfterEvolution
  are deleted from battle_engine_stubs.c. Both entry points
  were empty stubs, so a mon that levelled into its evolution
  level silently stayed unevolved and the chance was consumed
  (the battle path clears its bit before asking) — a Rare
  Candy did the same. The trade-evolution half is excluded
  with #ifndef PORTABLE rather than stubbed: removing the
  guard leaves exactly 5 undefined symbols (measured by
  compiling the file with the guard stripped and diffing
  against the linked objects) — LoadTradeAnimGfx,
  LinkTradeDrawWindow, InitTradeSequenceBgGpuRegs,
  DrawTextOnTradeWindow, gTradeEvolutionSceneYesNoWindowTemplate
  — all defined in the unlinked src/trade_scene.c, and a
  WindowTemplate stub would be fix #57's wrong-typed-data
  trap. Covered by
  tests/evolution.c (3 tests on fixture:lab, one of which
  drives the real BAG path: Rare Candy at Lv15 → Lv16 →
  Ivysaur).

- src/bg_regs.c. It is linked (ENGINE_SRCS) and the
  gOverworldBackgroundLayerFlags stub that was at
  overworld_stubs.c:131 is deleted, so the overworld's BLDCNT
  target-2 mask is the real const u16[4] rather than a 4-byte
  zero.

 Tier A — linkable now, unblocks a player-visible feature

 Each verified by compiling through the real pipeline and
 resolving externals against the current binary; collides
 = stub definitions that must be deleted (fix #8), new ext
 = symbols nothing defines yet.

 Counts re-measured after the evolution/bg_regs links landed:
 the four stub TUs now define 438 symbols, 294 of them
 sole-definition/load-bearing. The four deleted stubs
 (BeginEvolutionScene, EvolutionScene, gCB2_AfterEvolution,
 gOverworldBackgroundLayerFlags) were all sole definitions, so
 both numbers fell by 4; the 144 collisions are unchanged and
 all sit in battle_engine_stubs.c.

 ┌──────────────┬──────────────┬───────────┬─────────────┐
 │ Feature      │ files        │ new ext   │ collides    │
 ├──────────────┼──────────────┼───────────┼─────────────┤
 │ Trainer      │ trainer_see. │ 0         │ 2           │
 │ sight — NPC  │ c            │           │ (CheckForTr │
 │ trainers     │              │           │ ainersWanti │
 │ spot and     │              │           │ ngBattle,   │
 │ engage you   │              │           │ MovementAct │
 │              │              │           │ ion_RevealT │
 │              │              │           │ rainer_RunT │
 │              │              │           │ rainerSeeFu │
 │              │              │           │ ncList),    │
 │              │              │           │ both in     │
 │              │              │           │ overworld_  │
 │              │              │           │ stubs.c.    │
 │              │              │           │ NOT worth   │
 │              │              │           │ doing first:│
 │              │              │           │ 432 sight-  │
 │              │              │           │ capable     │
 │              │              │           │ object      │
 │              │              │           │ events      │
 │              │              │           │ game-wide,  │
 │              │              │           │ but 0 of    │
 │              │              │           │ them are in │
 │              │              │           │ the 6 maps  │
 │              │              │           │ whose       │
 │              │              │           │ scripts     │
 │              │              │           │ compile, so │
 │              │              │           │ nothing     │
 │              │              │           │ changes     │
 │              │              │           │ today.      │
 ├──────────────┼──────────────┼───────────┼─────────────┤
 │ Field moves  │ fldeff_{cut, │ 4 script  │ 7           │
 │ (Cut/Dig/Roc │ dig,rocksmas │ labels +  │ SetUpFiel   │
 │ kSmash/Stren │ h,strength,t │ 7         │ dMove_*     │
 │ gth/Teleport │ eleport,swee │ SetUpFiel │ stubs to    │
 │ /SweetScent/ │ tscent,softb │ dMove_*   │ delete;     │
 │ Softboiled)  │ oiled}.c     │ stubs and │ Flash is    │
 │              │              │ 8 of 13   │ already     │
 │              │              │ NULL      │ shadowed    │
 │              │              │ field     │ by the      │
 │              │              │ effects   │ linked      │
 │              │              │ become    │ fldeff_     │
 │              │              │ live      │ flash.c     │
 ├──────────────┼──────────────┼───────────┼─────────────┤
 │ Item/TM      │ pokemon_spec │ 0         │ 27 PSA_*    │
 │ animation    │ ial_anim_sce │           │ stubs       │
 │ scene        │ ne.c         │           │ deleted     │
 │ (Potion/Rare │              │           │ — DONE      │
 │ Candy/TM     │              │           │ (see the    │
 │ visuals)     │              │           │ Landed list)│
 ├──────────────┼──────────────┼───────────┼─────────────┤
 │ Player PC /  │ player_pc.c, │ EventScri │ 3 +         │
 │ item PC /    │ item_pc.c,   │ pt_..._Sh │ ItemPc_*/Ma │
 │ mailbox (2F  │ mailbox_pc.c │ utDownPC  │ ilboxPC_*   │
 │ PC menu)     │ ,            │           │ stubs       │
 │              │ pc_screen_ef │           │             │
 │              │ fect.c       │           │             │
 ├──────────────┼──────────────┼───────────┼─────────────┤
 │ Hall of Fame │ hall_of_fame │ CB2_DoHal │ 0           │
 │              │ .c,          │ lOfFameSc │             │
 │              │ hof_pc.c,    │ reen,     │             │
 │              │ post_battle_ │ gHasHallO │             │
 │              │ event_funcs. │ fFameReco │             │
 │              │ c            │ rds,      │             │
 │              │              │ gGameCont │             │
 │              │              │ inueCallb │             │
 │              │              │ ack       │             │
 ├──────────────┼──────────────┼───────────┼─────────────┤
 │ Daycare      │ daycare.c    │ CompactPa │ 1           │
 │              │              │ rtySlots  │             │
 │              │              │ + 3 text  │             │
 │              │              │ labels    │             │
 ├──────────────┼──────────────┼───────────┼─────────────┤
 │ Game Corner  │ slot_machine │ 0         │ 1           │
 │ slot machine │ .c           │           │ (PlaySlotMa │
 │              │              │           │ chine)      │
 ├──────────────┼──────────────┼───────────┼─────────────┤
 │ Credits      │ credits.c    │ 0         │ 0           │
 ├──────────────┼──────────────┼───────────┼─────────────┤
 │ Fame checker │ fame_checker │ 320 (its  │ 2           │
 │ UI           │ .c           │ generated │             │
 │              │              │ flavor-te │             │
 │              │              │ xt        │             │
 │              │              │ tables)   │             │
 ├──────────────┼──────────────┼───────────┼─────────────┤
 │ Help system  │ help_system. │ 368       │ 15          │
 │ (L/R HELP    │ c,           │ Help_Text │             │
 │ windows)     │ help_message │ _*        │             │
 │              │ .c,          │ (text-ban │             │
 │              │ help_system_ │ k wiring) │             │
 │              │ util.c       │           │             │
 └──────────────┴──────────────┴───────────┴─────────────┘

 Tier B — needs a whole subsystem, no incremental path

 - Save persistence.
   TrySavingData/WriteSaveBlock2/WriteSaveBlock1Sector are
   printf("[Menu] saving is not ported") stubs in
   overworld_stubs.c:150-152;
   LoadGameSave/Save_ResetSaveCounters are no-ops in
   stubs.c. Nothing is written to disk. src/save.c is the
   real implementation and cannot compile: STATIC_ASSERT
   at line 77 fails — host pointers widen SaveBlock1 to
   16056 bytes vs the 15872 budget (184 over:
   ObjectEventTemplate.script ×64, QuestLogScene, Mail.…).
   Needs a PORTABLE allowance on the assert plus a host
   sector layout, then a save-file backend.
 - PC storage system + Union Room + trade + link
   (pokemon_storage_system_*.c 6 files,
   link.c/link_rfu_*/librfu_*/cable_club.c, union_room*.c,
   trade*.c, ereader_*). ~40 of the 104 files share one
   cause: no GBA link hardware / RFU. link.c alone
   collides with 32 stubs.
 - Intro cinematic (intro.c): needs multiboot.c, which is
   not host-buildable (GameCube multiboot symbols).
 - Berry crush / Dodrio / Pokémon Jump / minigames: all
   link-dependent.
 - Unbuildable under the current preproc/compiler even
   after linking: cereader_tool.c (typeof), isagbprn.c,
   librfu_intr.c, multiboot.c, slot_machine.c (builds OK
   via preproc), rom_header_gf.c (Mach-O section
   attribute).

 Tier C — structural gaps that gate whole classes of
 content

 1. Script generator command coverage.
    tools/gen_map_data.py supports 105 commands + 171
    movement macros; map scripts use 99 unsupported
    commands over 1547 sites — dominated by
    trainerbattle_single (412), trainerbattle_rematch
    (208), finditem (168), multichoice (59),
    playmoncry/waitmoncry (40 each), setrespawn (23),
    setdynamicwarp (20), braillemessage* (39). Unlisted
    roots fail the generator loudly, so this is a
    prerequisite for ordinary trainer battles everywhere.
 2. Map coverage. Of 425 maps, 419 are on
    sEmptyMapScripts; of 2395 event .script slots only 59
    are real (2118 sDummyScript, 218 NULL). Only 6 maps
    have real headers. Each new area is a
    NATIVE_SCRIPT_ROOTS + command-support task.
 3. gSpecials registry. 28 of 444 specials registered. 2
    specials called by already-compiled scripts are
    unregistered (the two trade ones below):
   - Whiteout. EventScript_FieldPoison →
     EventScript_FieldWhiteOut → FieldWhiteOutFade are
     compiled and reachable (field_control_avatar.c:667),
     but specials 200 SetCB2WhiteOut, 332
     Script_FadeOutMapMusic, 373
     OverworldWhiteOutGetMoneyLoss are NULL. 373/332 print
     [Script] special N is not ported; the following
     waitstate does ScriptContext_Stop() and the task that
     would ScriptContext_Enable()
     (Task_EnableScriptAfterMusicFade) is never created →
     field locked forever. Faint-from-poison on a step
     reproduces it. (Battle-loss whiteout bypasses this —
     battle_setup.c calls CB2_WhiteOut directly.)
   - ~~Whiteout.~~ **Fixed.** Specials 200 (SetCB2WhiteOut), 332
     (Script_FadeOutMapMusic) and 373 (OverworldWhiteOutGetMoneyLoss)
     are registered, so the poison-faint whiteout no longer locks the
     field. All three implementations were already linked
     (src/field_screen_effect.c:214, src/overworld.c:271 and :1545;
     SetCB2WhiteOut only lived in unlinked post_battle_event_funcs.c
     and its whole body is the SetMainCallback2 the wrapper mirrors).
     Pinned by tests/whiteout.c: both engine tests fail pre-fix with
     "[Script] special 332 is not ported" and the field locked forever.
   - 253/254 (CreateInGameTradePokemon/DoInGameTradeScene)
     unreachable until trade_scene.c links: EventScript_DoInGameTrade
     is not in NATIVE_SCRIPT_ROOTS, and the generated data's only
     occurrence of those two specials is inside it.
  (Verified against the generated bytecode rather than the pret
  sources: special = opcode 0x25 + a halfword, and the three
  whiteout specials appear exactly once each — 0x25,c8,00 in
  EventScript_FieldWhiteOutFade, 0x25,4c,01 in the same label,
  0x25,75,01 in EventScript_FieldWhiteOutHasMoney. ScrCmd_waitstate
  is `ScriptContext_Stop(); return TRUE;` and nothing in the
  compiled whiteout chain re-enables it, so the lock is real.
  A byte scan also flags `0x25,01,00` inside
  PalletTown_ProfessorOaksLab_EventScript_Rival, but that is an
  overlapping operand, not a `special`: the rival script has no
  `special` statement. Special ids found that way are only
  trustworthy when confirmed against the source.)
4. ~~src/bg_regs.c is unlinked~~ **Done** — it is linked and
   the stub is deleted, so gOverworldBackgroundLayerFlags is
   the real const u16[4] and the overworld's BLDCNT target-2
   mask is correct.
 5. gStdScripts is { NULL } (overworld_stubs.c:520), so
    ScrCmd_gotostd/callstd jump to NULL. The generator
    dodges this by inlining msgbox expansions; any script
    using std/goto_std needs the table emitted from
    data/event_scripts.s:77.
 6. 144 stub definitions in battle_engine_stubs.c are
    C-common and lose to another definition — 127 to the
    generated src/data/battle/ptr_table.c (BattleScript_*,
    gBattleAnimArgs, gBattlePartyCurrentOrder, …) and 17 to a
    linked hand-written source (battle_anim, battle_message,
    battle_setup, battle_bg, party_menu, battle_anim_mons,
    safari_zone, battle_controller_pokedude). Harmless but
    weight; delete when convenient.

 Suggested order

1. ~~bg_regs.c + delete its stub~~ **Done** (one-line fix for
   a real overworld blend defect).
 2. ~~Register specials 200/332/373~~ **Done** — closes the
    poison-whiteout soft-lock; three gSpecials entries, no new
    file (fix #84). Pinned by tests/whiteout.c.
 3. trainer_see.c + fldeff_* field moves (0 new externals
    each). trainer_see makes 432 trainer events real, but 0 of
    them are in the compiled maps — see the Tier A note — and
    the fldeff_* files close 8 of the 13 NULL effects.
 4. ~~pokemon_special_anim_scene.c~~ **Done** — 27 stubs out,
    item/TM animations visible. Its sprite pointers were stored
    with SetWordTaskArg at tOff_MonSprite 6 / tOff_ItemSprite
    {4,9} (32 bits), which faulted on the first dereference;
    fixed with a task-keyed side table (fix #85). Pinned by
    tests/item_anim.c's pixel test.
 5. Generator:
    trainerbattle_single/_rematch/_no_intro/_double +
    finditem/multichoice → then ordinary trainer battles
    and Route/area scripts.
 6. save.c host sector layout + backend (touches the
    STATIC_ASSERT, so it's a design task, not a link
    task).
