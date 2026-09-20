 Headline

 ┌─────────────────────────────────┬─────────────────────┐
 │                                 │ count               │
 ├─────────────────────────────────┼─────────────────────┤
 │ src/*.c total                   │ 285                 │
 ├─────────────────────────────────┼─────────────────────┤
 │ linked (ENGINE_SRCS 64 /        │ 178                 │
 │ PREPROC_SRCS 117 /              │                     │
 │ PLATFORM_SRCS 14)               │                     │
 ├─────────────────────────────────┼─────────────────────┤
 │ unlinked                        │ 107                 │
 ├─────────────────────────────────┼─────────────────────┤
 │ stub TUs (overworld_stubs,      │ 442 defined         │
 │ battle_engine_stubs,            │ symbols; 298 are    │
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

 Tier A — linkable now, unblocks a player-visible feature

 Each verified by compiling through the real pipeline and
 resolving externals against the current binary; collides
 = stub definitions that must be deleted (fix #8), new ext
 = symbols nothing defines yet.

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
 │              │              │           │ ncList) —   │
 │              │              │           │ both        │
 │              │              │           │ currently   │
 │              │              │           │ inert, so   │
 │              │              │           │ 432         │
 │              │              │           │ TRAINER_TYP │
 │              │              │           │ E_NORMAL    │
 │              │              │           │ object      │
 │              │              │           │ events      │
 │              │              │           │ can't see   │
 │              │              │           │ you         │
 ├──────────────┼──────────────┼───────────┼─────────────┤
 │ Field moves  │ fldeff_{cut, │ 4 script  │ 8           │
 │ (Cut/Dig/Roc │ dig,rocksmas │ labels +  │             │
 │ kSmash/Stren │ h,strength,t │ 6         │             │
 │ gth/Teleport │ eleport,swee │ SetUpFiel │             │
 │ /SweetScent/ │ tscent,softb │ dMove_*   │             │
 │ Softboiled)  │ oiled}.c     │ stubs to  │             │
 │              │              │ delete;   │             │
 │              │              │ 10 of 13  │             │
 │              │              │ NULL      │             │
 │              │              │ field     │             │
 │              │              │ effects   │             │
 │              │              │ become    │             │
 │              │              │ live      │             │
 ├──────────────┼──────────────┼───────────┼─────────────┤
 │ Item/TM      │ pokemon_spec │ 0         │ 27 PSA_*    │
 │ animation    │ ial_anim_sce │           │ stubs       │
 │ scene        │ ne.c         │           │ deleted     │
 │ (Potion/Rare │              │           │             │
 │ Candy/TM     │              │           │             │
 │ visuals)     │              │           │             │
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
 │ Evolution    │ evolution_sc │ 6         │ 3           │
 │ scene        │ ene.c,       │ trade-sce │             │
 │              │ evolution_gr │ ne        │             │
 │              │ aphics.c     │ symbols   │             │
 │              │              │ (or link  │             │
 │              │              │ trade_sce │             │
 │              │              │ ne.c too) │             │
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
   trade*.c, ereader_*). ~40 of the 107 files share one
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
 3. gSpecials registry. 25 of 444 specials registered. 5
    specials called by already-compiled scripts are
    unregistered — and one is a live soft-lock:
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
   - 253/254 (CreateInGameTradePokemon/DoInGameTradeScene)
     unreachable until trade_scene.c links.
 4. src/bg_regs.c is unlinked, so
    gOverworldBackgroundLayerFlags is the stub u32 = 0
    (overworld_stubs.c:131) while the real definition is
    const u16[4]. overworld.c:2082 reads [1]|[2]|[3] off a
    4-byte zero — so the overworld's BLDCNT target-2 mask
    is 0 and BLDALPHA_BLEND(13,7) blends nothing. Link it
    and delete the stub (1 collision).
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

 1. bg_regs.c + delete its stub (one-line fix for a real
    overworld blend defect).
 2. Register specials 200/332/373 (+ SetCb2WhiteOut
    wrapper) — closes the poison-whiteout soft-lock; needs
    no new file.
 3. trainer_see.c + fldeff_* field moves (0 new externals
    each; makes 432 trainer events and all 13 NULL field
    effects real).
 4. pokemon_special_anim_scene.c (27 stubs out, item/TM
    animations visible).
 5. Generator:
    trainerbattle_single/_rematch/_no_intro/_double +
    finditem/multichoice → then ordinary trainer battles
    and Route/area scripts.
 6. save.c host sector layout + backend (touches the
    STATIC_ASSERT, so it's a design task, not a link
    task).
