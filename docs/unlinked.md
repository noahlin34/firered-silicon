Headline

┌─────────────────────────────────┬─────────────────────┐
│                                 │ count               │
├─────────────────────────────────┼─────────────────────┤
│ src/*.c total                   │ 285                 │
├─────────────────────────────────┼─────────────────────┤
│ linked (ENGINE_SRCS 65 /        │ 207                 │
│ PREPROC_SRCS 128 /              │                     │
│ PLATFORM_SRCS 14)               │                     │
├─────────────────────────────────┼─────────────────────┤
│ unlinked                        │ 95                  │
├─────────────────────────────────┼─────────────────────┤
│ stub TUs (overworld_stubs,      │ 427 defined         │
│ battle_engine_stubs,            │ 283 are             │
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
- Field moves + trainer sight. The seven
  src/fldeff_{cut,dig,rocksmash,strength,teleport,sweetscent,
  softboiled}.c sources and src/trainer_see.c are linked via
  PREPROC_SRCS (the fldeff files carry _() strings, and
  trainer_see.c INCBINs graphics/misc/emoticons.4bpp). Before
  this, battle_peripheral_stubs.c served all eight
  SetUpFieldMove_* with `return FALSE` and overworld_stubs.c
  served CheckForTrainersWantingBattle with FALSE plus an
  empty MovementAction_RevealTrainer_RunTrainerSeeFuncList —
  so the party menu offered a field move and then reported
  "can't use that here", and no NPC trainer could spot the
  player. Eleven stub definitions deleted (SetUpFieldMove_Flash kept: flash
  is not a FLDEFF_USE_* effect and its real body is behind
  #ifndef PORTABLE in the already-linked src/fldeff_flash.c).
  All 13 previously-NULL field effects are now real, 0 NULL.
  Two 64-bit defects surfaced on linking, both live on first
  use rather than at link time: include/fldeff.h's
  FLDEFF_SET/CALL_FUNC_IN_DATA packed a function pointer into
  data[8..9] (32 bits; now SetPointerTaskArg/GetPointerTaskArg
  at index 8), and src/trainer_see.c packed an ObjectEvent *
  into tTrainerObjHi/tTrainerObjLo — gObjectEvents is at
  0x1_008da5d8, so the load faulted in RunTasks (now a
  task-keyed side table, leaving data[7] at its pret offset
  because the reveal task genuinely uses it). Generator:
  checkpartymove, bufferpartymonnick, buffermovename and
  setfieldeffectargument added (opcodes derived from
  src/data/script_cmd_table.h, not pret's source order);
  18 field-move script roots added; inline text from
  data/scripts/**/*.inc is now a text source, which is where
  Text_CutTreeDown lives; STR_VAR_1/2/3 map to 0/1/2 through
  one string_var_index helper. Special 171
  (RockSmashWildEncounter) registered — the real body is in
  the already-linked src/wild_encounter.c, and
  EventScript_UseRockSmash branches on its VAR_RESULT.
  RockSmashWildEncounter had no declaration anywhere; it was
  added to include/wild_encounter.h rather than hand-written at
  the call site (fix #57). Covered by tests/field_moves.c
  (6 tests) and tests/trainer_sight.c (3 tests). Both files
  fail on the historical defect, verified by reverting the
  link: SetUpFieldMove_Cut() returns FALSE, all 8 move effects
  read NULL, and both end-to-end CUT tests fail with assertions
  (not crashes — the callback is null-guarded), and the
  truncating trainer pointer SIGSEGVs inside RunTasks.

  CUT is now covered end to end, through the real party-menu
  path: START -> POKéMON -> the mon -> CUT -> the tree object
  leaves the map, and separately CUT's grass half rewrites the
  map's metatiles. The route is CB2_PartyMenuFromStartMenu ->
  PARTY_MENU_TYPE_FIELD -> SetPartyMonFieldSelectionActions,
  which builds [SUMMARY, <field moves in sFieldMoves order>,
  SWITCH, ITEM, CANCEL]; FIELD_MOVE_CUT is 1 and is only
  appended when the mon knows the move, so it is one DPAD_DOWN
  below the SUMMARY row. Three things worth keeping:
    - The menu path has NO Yes/No box. EventScript_FldEffCut
      (what FldEff_UseCutOnTree sets up) is lockall,
      dofieldeffect, waitstate, goto EventScript_CutTreeDown.
      Only EventScript_CutTree — the label the tree object
      carries for an A-press — asks "Would you like to CUT
      it?", because that path must run
      checkpartymove/bufferpartymonnick first. Pressing A for a
      box that does not exist was the first version's bug.
    - The grass assertion is a WHOLE-GRID diff, not a sample of
      the 3x3 the sweep covers. FldEff_CutGrass's origin is the
      player's DESTINATION (PlayerGetDestCoords) and this port
      has more than one coordinate space in play — after a
      double warp the avatar's object-event coords read (24,25)
      while gSaveBlock1Ptr->pos read (17,18), so a window
      derived in the test sampled tiles the sweep never
      touched. Counting differing tiles over the 24x40 map
      needs no coordinate reasoning. Warping twice also left
      that disagreement behind; the test now warps once.
    - A test that calls a callback the engine may not have
      installed must null-guard it, or the historical defect is
      reported as a SIGSEGV instead of a failed assertion.

  TEST COVERAGE GAP — trainer sight. No test can observe a
  trainer spotting the player, and the reason is reachability,
  not effort: 432 object events carry a trainer_type other than
  TRAINER_TYPE_NONE across 95 maps, and 0 of them are in the six
  maps whose mapScripts compile (PalletTown_ProfessorOaksLab,
  PalletTown_RivalsHouse, Route1, Route11_EastEntrance_2F,
  ViridianCity, ViridianCity_Mart). There is no NPC trainer in
  the build to point CheckForTrainersWantingBattle() at, so
  tests/trainer_sight.c can only assert that the function is
  linked, returns FALSE with nothing in sight, and that the
  reveal path survives dereferencing a whole pointer. Closing
  this needs an out-of-battle trainer object event in a compiled
  map, which needs trainerbattle_single in the generator
  (suggested item 5) — not a test-writing task.


 Tier A — linkable now, unblocks a player-visible feature

 Each verified by compiling through the real pipeline and
 resolving externals against the current binary; collides
 = stub definitions that must be deleted (fix #8), new ext
 = symbols nothing defines yet.

 Counts re-measured after the field-move/trainer-sight links
 landed: the four stub TUs now define 427 symbols, 283 of
 them sole-definition/load-bearing. The 11 definitions deleted
 with that link were all sole definitions, so both numbers fell
 by 11; the 144 collisions are unchanged and all sit in
 battle_engine_stubs.c.

 ┌──────────────┬──────────────┬───────────┬─────────────┐
 │ Feature      │ files        │ new ext   │ collides    │
 ├──────────────┼──────────────┼───────────┼─────────────┤
 │ Trainer      │ trainer_see. │ 0         │ 2 stubs     │
 │ sight — NPC  │ c            │           │ deleted —   │
 │ trainers     │              │           │ DONE (see   │
 │ spot and     │              │           │ the Landed  │
 │ engage you   │              │           │ list). Still│
 │              │              │           │ invisible:  │
 │              │              │           │ 432 sight-  │
 │              │              │           │ capable     │
 │              │              │           │ object      │
 │              │              │           │ events      │
 │              │              │           │ game-wide,  │
 │              │              │           │ 0 of them   │
 │              │              │           │ in the 6    │
 │              │              │           │ maps whose  │
 │              │              │           │ scripts     │
 │              │              │           │ compile, so │
 │              │              │           │ no NPC can  │
 │              │              │           │ spot you    │
 │              │              │           │ yet.        │
 ├──────────────┼──────────────┼───────────┼─────────────┤
 │ Field moves  │ fldeff_{cut, │ —         │ 7 SetUpFiel │
 │ (Cut/Dig/Roc │ dig,rocksmas │           │ dMove_*     │
 │ kSmash/Stren │ h,strength,t │           │ stubs       │
 │ gth/Teleport │ eleport,swee │           │ deleted     │
 │ /SweetScent/ │ tscent,softb │           │ —           │
 │ Softboiled)  │ oiled}.c     │           │ DONE (see   │
 │              │              │           │ the Landed  │
 │              │              │           │ list)       │
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
3. ~~trainer_see.c + fldeff_* field moves~~ **Done** — all
   eight sources linked, 11 stubs deleted, 13 NULL effects
   resolved to 0. trainer_see makes 432 trainer events real,
   but 0 of them are in the compiled maps — see the Tier A
   note — so the change is currently observable through the
   field moves and the five emote effects, not through an NPC
   spotting you. Two pointer-width defects fixed on the way
   (include/fldeff.h's callback macro, src/trainer_see.c's
   packed ObjectEvent *); pinned by tests/field_moves.c and
   tests/trainer_sight.c, both of which fail on the pre-link
   tree.
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
