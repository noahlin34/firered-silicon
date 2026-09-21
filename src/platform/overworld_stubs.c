#include <stdio.h>

#include "global.h"
#include "task.h"
#include "sprite.h"
#include "field_effect.h"
#include "event_scripts.h"
#include "script_pokemon_util.h"
#include "constants/event_objects.h"
#include "link.h"
#include "link_rfu.h"
#include "overworld.h"
#include "field_control_avatar.h"
#include "fame_checker.h"
#include "event_data.h"
#include "script.h"
#include "text.h"
#include "pokemon.h"
#include "naming_screen.h"
#include "pokedex.h"
#include "save.h"
#include "save_location.h"
#include "field_message_box.h"
#include "battle_setup.h"
#include "field_specials.h"
#include "quest_log.h"
#include "cable_club.h"
#include "sound.h"
#include "teachy_tv.h"
#include "wild_encounter.h"
#include "slot_machine.h"
#include "mystery_event_script.h"
#include "pokedex_screen.h"
#include "trainer_see.h"
#include "shop.h"
#include "berry_pouch.h"
#include "tm_case.h"
#include "map_name_popup.h"
#include "field_poison.h"
#include "coins.h"
#include "heal_location.h"
#include "mail.h"
#include "field_weather.h"
#include "string_util.h"
#include "palette.h"
#include "party_menu.h"
#include "constants/flags.h"
#include "constants/vars.h"
#include "constants/items.h"
#include "constants/maps.h"
#include "constants/pokemon.h"
#include "constants/rgb.h"

static const u8 sDummyScript[] = { 0x02 };

/* START menu's SAVE dialog text. src/start_menu.c is built from plain C, so its
 * `_("...")` strings are not compiled by tools/preproc; these externs make
 * tools/gen_map_data.py's collect_engine_text_references emit them into the
 * generated text bank with external linkage (same mechanism as src/prof_pc.c's
 * PokedexRating_Text_*). The SAVE action itself reports the gap — see below. */
extern const u8 gText_WouldYouLikeToSaveTheGame[];
extern const u8 gText_AlreadySaveFile_WouldLikeToOverwrite[];
extern const u8 gText_SavingDontTurnOffThePower[];
extern const u8 gText_PlayerSavedTheGame[];
extern const u8 gText_DifferentGameFile[];
extern const u8 gText_SaveError_PleaseExchangeBackupMemory[];

/* Dummy Event Scripts */
const u8 BattleColosseum_2P_EventScript_PlayerSpot0[] = { 0x02 };
const u8 BattleColosseum_2P_EventScript_PlayerSpot1[] = { 0x02 };
const u8 BattleColosseum_4P_EventScript_PlayerSpot0[] = { 0x02 };
const u8 BattleColosseum_4P_EventScript_PlayerSpot1[] = { 0x02 };
const u8 BattleColosseum_4P_EventScript_PlayerSpot2[] = { 0x02 };
const u8 BattleColosseum_4P_EventScript_PlayerSpot3[] = { 0x02 };
const u8 CableClub_EventScript_DoLinkRoomExit[] = { 0x02 };
const u8 CableClub_EventScript_ReadTrainerCard[] = { 0x02 };
const u8 CableClub_EventScript_ReadTrainerCardColored[] = { 0x02 };
const u8 CableClub_EventScript_ShowBattleRecords[] = { 0x02 };
const u8 CableClub_EventScript_ShowWirelessCommunicationScreen[] = { 0x02 };
const u8 CableClub_EventScript_TooBusyToNotice[] = { 0x02 };
const u8 EventScript_AdvertisingPoster[] = { 0x02 };
const u8 EventScript_BeautifulSkyWindow[] = { 0x02 };
const u8 EventScript_BlinkingLights[] = { 0x02 };
const u8 EventScript_Blueprints[] = { 0x02 };
const u8 EventScript_Burglary[] = { 0x02 };
const u8 EventScript_CantUseWaterfall[] = { 0x02 };
const u8 EventScript_Computer[] = { 0x02 };
const u8 EventScript_Cup[] = { 0x02 };
const u8 EventScript_CurrentTooFast[] = { 0x02 };
const u8 EventScript_DoFallWarp[] = { 0x02 };
const u8 EventScript_EggHatch[] = { 0x02 };
const u8 EventScript_Food[] = { 0x02 };
const u8 EventScript_ImpressiveMachine[] = { 0x02 };
const u8 EventScript_Indigo_HighestAuthority[] = { 0x02 };
const u8 EventScript_Indigo_UltimateGoal[] = { 0x02 };
const u8 EventScript_NeatlyLinedUpTools[] = { 0x02 };
const u8 EventScript_PC[] = { 0x02 };
const u8 EventScript_Painting[] = { 0x02 };
const u8 EventScript_PokeMartShelf[] = { 0x02 };
const u8 EventScript_PokecenterSign[] = { 0x02 };
const u8 EventScript_PokemartSign[] = { 0x02 };
const u8 EventScript_PolishedWindow[] = { 0x02 };
const u8 EventScript_PowerPlantMachine[] = { 0x02 };
const u8 EventScript_Questionnaire[] = { 0x02 };
const u8 EventScript_RepelWoreOff[] = { 0x02 };
const u8 EventScript_ResetEliteFourEnd[] = { 0x02 };
const u8 EventScript_Snacks[] = { 0x02 };
const u8 EventScript_TastyFood[] = { 0x02 };
const u8 EventScript_Telephone[] = { 0x02 };
const u8 EventScript_TestSignpostMsg[] = { 0x02 };
const u8 EventScript_TrashBin[] = { 0x02 };
const u8 EventScript_UseSurf[] = { 0x02 };
const u8 EventScript_VideoGame[] = { 0x02 };
const u8 EventScript_VsSeekerChargingDone[] = { 0x02 };
const u8 EventScript_WallTownMap[] = { 0x02 };
const u8 EventScript_Waterfall[] = { 0x02 };
const u8 PalletTown_PlayersHouse_2F_EventScript_PC[] = { 0x02 };
const u8 RecordCorner_EventScript_Spot0[] = { 0x02 };
const u8 RecordCorner_EventScript_Spot1[] = { 0x02 };
const u8 RecordCorner_EventScript_Spot2[] = { 0x02 };
const u8 RecordCorner_EventScript_Spot3[] = { 0x02 };
const u8 TradeCenter_ConfirmLeaveRoom[] = { 0x02 };
const u8 TradeCenter_EventScript_Chair0[] = { 0x02 };
const u8 TradeCenter_EventScript_Chair1[] = { 0x02 };
const u8 TradeCenter_TerminateLink[] = { 0x02 };
const u8 TrainerTower_EventScript_ShowTime[] = { 0x02 };

/* Data stubs */
/* gFieldEffectScriptPointers is now the real generated table in
 * src/data/field_effects/ptr_table.c (see tools/gen_field_effect_data.py);
 * the all-NULL stub that used to live here is deleted (fix #8). */
struct Link gLink = {0};
u16 gLinkPartnersHeldKeys[6] = {0};
struct RfuManager gRfu = {0};
const u8 mus_victory_gym_leader[] = {0};

/* START menu entries whose scenes are not linked yet, plus the SAVE dialog's
 * flash-write path (src/save.c needs the GBA linker-script layout constants and
 * src/quest_log.c is mostly stub). These report the gap on stderr instead of
 * running. gSaveAttemptStatus stays a real writable variable so the save
 * result check can read it. */
u16 gSaveAttemptStatus = SAVE_STATUS_ERROR;

/* CB2_OpenPokedexFromStartMenu is the real src/pokedex_screen.c, which is
 * linked (PREPROC_SRCS — it INCBINs its tiles and carries _() strings), so the
 * stub that used to print "[Menu] POKéDEX scene is not ported" is deleted. */
void CB2_ReturnToPokeStorage(void) { printf("[Menu] POKéMON storage scene is not ported\n"); }
void SetUsingUnionRoomStartMenu(void) {}
/* RecordItemTransaction is the real src/shop.c (linked); the stub that made
 * every purchase a no-op for the quest log is deleted (fix #8). */
bool8 WriteSaveBlock2(void) { printf("[Menu] saving is not ported\n"); return FALSE; }
bool8 WriteSaveBlock1Sector(void) { printf("[Menu] saving is not ported\n"); return FALSE; }
void Task_LinkFullSave(u8 taskId) { (void)taskId; printf("[Menu] saving is not ported\n"); }

/* Function stubs */
void ClearLinkCallback_2(void) {}
/* src/field_specials.c: CountDigits. Declared s32 in include/field_specials.h
 * and used by linked src/scrcmd.c and src/overworld.c. */
/* src/cable_club.c is not linked (the whole link/trade stack is out), so this
 * returns the "no task" id the header's type promises. Signature from
 * include/cable_club.h: u8, not void. */
u8 CreateTask_ReestablishCableClubLink(void) { return 0; }
/* DexScreen_GetSetPokedexFlag is the real body in src/pokedex_screen.c, which
 * is linked; the mirror that used to live here (initially an always-0 stub, then
 * a corrected copy) is deleted (fix #8). */
u32 GetBerryPowder(void) { return 0; }
/* src/field_specials.c: GetHiddenItemAttr. Called by linked
 * src/field_control_avatar.c when the player faces a hidden-item bg event, so
 * the old always-0 stub made every hidden item resolve to item 0 / flag 1000.
 * The bit layout lives in include/global.fieldmap.h. */
u32 GetLinkRecvQueueLength(void) { return 0; }
u32 GetSeeingLinkPlayerCardMsg(u8 id) { (void)id; return 0; }
void InitTeachyTvController(u8 mode, void (*cb)(void)) { (void)mode; (void)cb; }
bool8 IsEscalatorMoving(void) { return FALSE; }
bool32 IsRfuRecvQueueEmpty(void) { return TRUE; }
bool32 IsSendingKeysToLink(void) { return FALSE; }
void LinkRfu_FatalError(void) {}
/* src/field_specials.c: RunMassageCooldownStepCounter. Daisy offers to groom a
 * mon only once VAR_MASSAGE_COOLDOWN_STEP_COUNTER reaches 500, and
 * DaisyMassageServices resets it. A no-op counter would gate her on a value
 * that never moves, so the real increment lives here (called from
 * ProcessPlayerFieldInput on every step). */
void SetBerryPowder(u32 *powder, u32 amount) {}
void SetHelpContextForMap(void) {}
bool8 ShouldEggHatch(void) { return FALSE; }
void StartEscalator(bool8 a) {}
void StartSendingKeysToLink(void) {}
void StopEscalator(void) {}
void UseFameChecker(MainCallback savedCallback) { (void)savedCallback; }
/* Specials and Scrcmd Stubs */
static u16 NativeSpecial_HealPlayerParty(void)
{
    HealPlayerParty();
    return 0;
}

static u16 NativeSpecial_SetWalkingIntoSignVars(void)
{
    SetWalkingIntoSignVars();
    return 0;
}

static u16 NativeSpecial_DisableMsgBoxWalkaway(void)
{
    DisableMsgBoxWalkaway();
    return 0;
}

/* Mirrors DoPicboxCancel (src/field_specials.c). EventScript_CancelMessageBox
 * runs it before `release` to cancel whatever printer/window the message box
 * left behind. The mon-pic window it also closes (PicboxCancel, src/script_menu.c)
 * can never be open here: ScriptMenu_ShowPokemonPic is not linked. */
static u16 NativeSpecial_DoPicboxCancel(void)
{
    u8 t = EOS;
    AddTextPrinterParameterized(0, FONT_NORMAL, &t, 0, 1, 0, NULL);
    return 0;
}

/* The fame checker UI is not ported, but the save data it edits is plain
 * state. Both functions mirror src/fame_checker.c so that a save started in
 * the native port carries the same fame checker bookkeeping. */
static void NativeUpdatePickStateFromSpecialVar8005(void)
{
    if (gSpecialVar_0x8004 < NUM_FAMECHECKER_PERSONS && gSpecialVar_0x8005 < 3)
    {
        if (gSpecialVar_0x8005 == FCPICKSTATE_NO_DRAW)
            return;
        if (gSpecialVar_0x8005 == FCPICKSTATE_SILHOUETTE
            && gSaveBlock1Ptr->fameChecker[gSpecialVar_0x8004].pickState == FCPICKSTATE_COLORED)
            return;
        gSaveBlock1Ptr->fameChecker[gSpecialVar_0x8004].pickState = gSpecialVar_0x8005;
    }
}

static u16 NativeSpecial_SetFlavorTextFlagFromSpecialVars(void)
{
    if (gSpecialVar_0x8004 < NUM_FAMECHECKER_PERSONS && gSpecialVar_0x8005 < 6)
    {
        gSaveBlock1Ptr->fameChecker[gSpecialVar_0x8004].flavorTextFlags |= (1 << gSpecialVar_0x8005);
        gSpecialVar_0x8005 = FCPICKSTATE_SILHOUETTE;
        NativeUpdatePickStateFromSpecialVar8005();
    }
    return 0;
}

static u16 NativeSpecial_UpdatePickStateFromSpecialVar8005(void)
{
    NativeUpdatePickStateFromSpecialVar8005();
    return 0;
}

/* Mirrors ChangePokemonNickname (src/field_specials.c). Oak's Lab starter
 * script offers the player a nickname after `givemon` (special 158,
 * data/specials.inc); DoNamingScreen and the party accessors are linked, so a
 * local copy avoids pulling all of field_specials.c into the host build. */
static void NativeChangePokemonNickname_CB(void)
{
    SetMonData(&gPlayerParty[gSpecialVar_0x8004], MON_DATA_NICKNAME, gStringVar2);
    CB2_ReturnToFieldContinueScriptPlayMapMusic();
}

static u16 NativeSpecial_ChangePokemonNickname(void)
{
    u16 species;
    u8 gender;
    u32 personality;

    GetMonData(&gPlayerParty[gSpecialVar_0x8004], MON_DATA_NICKNAME, gStringVar3);
    GetMonData(&gPlayerParty[gSpecialVar_0x8004], MON_DATA_NICKNAME, gStringVar2);
    species = GetMonData(&gPlayerParty[gSpecialVar_0x8004], MON_DATA_SPECIES, NULL);
    gender = GetMonGender(&gPlayerParty[gSpecialVar_0x8004]);
    personality = GetMonData(&gPlayerParty[gSpecialVar_0x8004], MON_DATA_PERSONALITY, NULL);
    DoNamingScreen(NAMING_SCREEN_NICKNAME, gStringVar2, species, gender, personality, NativeChangePokemonNickname_CB);
    return 0;
}

// src/prof_pc.c has no header; the GBA build declares these through
// data/specials.inc's def_special macro.
u16 GetPokedexCount(void);
void GetProfOaksRatingMessage(void);

/* The quest log recorder is now linked (src/quest_log.c), so these two
 * specials call the real functions. PokedexRating_EventScript_RateInPerson
 * opens with `goto_if_questlog EventScript_ReleaseEnd` and
 * `special QuestLog_CutRecording`, and the rating runs only while
 * QL_GetPlaybackState() reports not-recording. */
static u16 NativeSpecial_GetQuestLogState(void)
{
    gSpecialVar_Result = QL_GetPlaybackState();
    return 0;
}

static u16 NativeSpecial_QuestLog_CutRecording(void)
{
    QuestLog_CutRecording();
    return 0;
}

/* The help system UI is not ported, but its context bookkeeping is plain
 * state: src/help_system.c keeps a context id plus a backup. Mirrors
 * Script_SetHelpContext / BackupHelpContext / RestoreHelpContext so
 * PokedexRating_EventScript_Rate's save/restore trio is a real round trip
 * rather than a loud miss (the backup is local since help_system.c is not
 * linked). */
static u16 sHelpSystemContextId;
static u16 sHelpSystemContextIdBackup;

static u16 NativeSpecial_Script_SetHelpContext(void)
{
    sHelpSystemContextId = gSpecialVar_0x8004;
    return 0;
}

static u16 NativeSpecial_BackupHelpContext(void)
{
    sHelpSystemContextIdBackup = sHelpSystemContextId;
    return 0;
}

static u16 NativeSpecial_RestoreHelpContext(void)
{
    sHelpSystemContextId = sHelpSystemContextIdBackup;
    return 0;
}

/* Real functions, wrapped: the special table stores u16 (*)(void) but the
 * engine declares these as void. The implementations live in linked files
 * (src/prof_pc.c, src/pokedex.c, src/event_data.c) so no logic is duplicated
 * here. */
static u16 NativeSpecial_GetPokedexCount(void)
{
    return GetPokedexCount();
}

static u16 NativeSpecial_GetProfOaksRatingMessage(void)
{
    GetProfOaksRatingMessage();
    return 0;
}

static u16 NativeSpecial_HasAllMons(void)
{
    return HasAllMons();
}

static u16 NativeSpecial_EnableNationalPokedex(void)
{
    EnableNationalPokedex();
    return 0;
}

static u16 NativeSpecial_SetUnlockedPokedexFlags(void)
{
    SetUnlockedPokedexFlags();
    return 0;
}

/* Real functions, wrapped: the special table stores u16 (*)(void) while
 * battle_setup.c declares these as void or a different return width. The
 * implementations are in linked src/battle_setup.c. */
static u16 NativeSpecial_PlayTrainerEncounterMusic(void)
{
    PlayTrainerEncounterMusic();
    return 0;
}

static u16 NativeSpecial_SetBattledTrainerFlag(void)
{
    SetBattledTrainerFlag();
    return 0;
}

/* Daisy's house (MAP_PALLET_TOWN_RIVALS_HOUSE). The five specials her script
 * closure calls are the last reason src/field_specials.c is not linked: it also
 * carries the diploma, Elite Four lighting and Sevii rock paths, which pull in
 * src/diploma.c, src/help_system.c and src/region_map.c. These mirror the real
 * bodies (src/field_specials.c, src/party_menu_specials.c) against linked
 * accessors so no logic is duplicated and no new file joins the link. */

/* field_specials.c: GetLeadMonFriendship. Buckets the lead mon's friendship
 * for the post-game friendship-rating branch (case 0..6). */
static u16 NativeSpecial_GetLeadMonFriendship(void)
{
    struct Pokemon *mon = &gPlayerParty[GetLeadMonIndex()];
    u32 friendship = GetMonData(mon, MON_DATA_FRIENDSHIP, NULL);

    if (friendship == 255)
        return 6;
    else if (friendship >= 200)
        return 5;
    else if (friendship >= 150)
        return 4;
    else if (friendship >= 100)
        return 3;
    else if (friendship >= 50)
        return 2;
    else if (friendship > 0)
        return 1;
    else
        return 0;
}

/* field_specials.c: GetPartyMonSpecies, used to reject eggs before grooming. */
static u16 NativeSpecial_GetPartyMonSpecies(void)
{
    return GetMonData(&gPlayerParty[gSpecialVar_0x8004], MON_DATA_SPECIES_OR_EGG, NULL);
}

/* field_specials.c: DaisyMassageServices. The friendship gain is real
 * (src/pokemon.c); the cooldown var is what gates the offer to every 500th
 * step, so it must be reset here or Daisy would keep offering. */
static u16 NativeSpecial_DaisyMassageServices(void)
{
    AdjustFriendship(&gPlayerParty[gSpecialVar_0x8004], FRIENDSHIP_EVENT_MASSAGE);
    VarSet(VAR_MASSAGE_COOLDOWN_STEP_COUNTER, 0);
    return 0;
}

/* field_specials.c: BufferMonNickname. */
static u16 NativeSpecial_BufferMonNickname(void)
{
    GetMonData(&gPlayerParty[gSpecialVar_0x8004], MON_DATA_NICKNAME, gStringVar1);
    StringGet_Nickname(gStringVar1);
    return 0;
}

/* field_specials.c: SetHiddenItemFlag. Sets the flag whose id the hidden-item
 * pickup script left in VAR_0x8004, so a picked-up item cannot be collected
 * again. Without this entry ScrCmd_special reports "special 150 is not ported"
 * and the flag stays clear, making every hidden item infinitely re-pickable.
 * The index (150) comes from data/specials.inc, verified with the generator's
 * own collect_specials(), not from the order in this table. */
void SetHiddenItemFlag(void);

static u16 NativeSpecial_SetHiddenItemFlag(void)
{
    SetHiddenItemFlag();
    return 0;
}

/* src/field_poison.c: TryFieldPoisonWhiteOut, index 199. EventScript_FieldPoison
 * runs this and then tests VAR_RESULT: TRUE means a mon fainted from poison and
 * the script continues into the whiteout, FALSE just releases the field. The
 * real body creates the whiteout task and stops the script context (the task
 * re-enables it), so the return value is left at 0 and the variable is what the
 * task sets. */
void TryFieldPoisonWhiteOut(void);

static u16 NativeSpecial_TryFieldPoisonWhiteOut(void)
{
    TryFieldPoisonWhiteOut();
    return 0;
}

/* The whiteout chain's other three specials. EventScript_FieldWhiteOutFade is
 * `special Script_FadeOutMapMusic` / `waitstate` / `fadescreen` /
 * `special SetCB2WhiteOut` / `waitstate` / `end`, and ScrCmd_waitstate is
 * ScriptContext_Stop() with nothing to restart it (src/scrcmd.c:155). Special
 * 332's real body is what creates Task_EnableScriptAfterMusicFade, the task
 * that calls ScriptContext_Enable() once the BGM fade completes, so leaving it
 * NULL printed "[Script] special 332 is not ported" and locked the field
 * permanently -- reached by simply fainting from poison on a step.
 *
 * All three implementations are already linked: Script_FadeOutMapMusic is
 * src/field_screen_effect.c:214, OverworldWhiteOutGetMoneyLoss is
 * src/overworld.c:271 (declared in overworld.h) and CB2_WhiteOut is
 * src/overworld.c:1545. SetCB2WhiteOut only lives in the unlinked
 * src/post_battle_event_funcs.c, and its whole body is the SetMainCallback2
 * below, so it is mirrored here. The fade does terminate: FadeOutBody
 * (src/m4a.c:716) sets MUSICPLAYER_STATUS_PAUSE at fade end, which makes
 * IsBGMStopped() TRUE, so MapMusicMain -- called every frame from AgbMain --
 * advances sMapMusicState 5 -> 0 and the unlock task fires. (Verified rather
 * than assumed: without that the task would spin forever and the lock would
 * persist.) */
void Script_FadeOutMapMusic(void);
void OverworldWhiteOutGetMoneyLoss(void);
void CB2_WhiteOut(void);

static u16 NativeSpecial_Script_FadeOutMapMusic(void)
{
    Script_FadeOutMapMusic();
    return 0;
}

static u16 NativeSpecial_OverworldWhiteOutGetMoneyLoss(void)
{
    OverworldWhiteOutGetMoneyLoss();
    return 0;
}

static u16 NativeSpecial_SetCB2WhiteOut(void)
{
    SetMainCallback2(CB2_WhiteOut);
    return 0;
}
/* src/wild_encounter.c: RockSmashWildEncounter. EventScript_UseRockSmash runs
 * it after a rock breaks, and branches on VAR_RESULT to decide whether a wild
 * mon appeared (the script then waits for the battle before releasing the
 * field). src/wild_encounter.c is linked, so this is the real body. */
static u16 NativeSpecial_RockSmashWildEncounter(void)
{
    RockSmashWildEncounter();
    return 0;
}


/* src/party_menu_specials.c: ChoosePartyMon. Starts the party menu in
 * CHOOSE_SINGLE_MON mode and stops the field until the player picks a slot;
 * HandleChooseMonSelection writes the chosen index to VAR_0x8004 and
 * Task_PartyMenuWaitForFade (src/party_menu.c) re-enables the script context. */
static void Task_ChoosePartyMonSpecials(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        gPaletteFade.bufferTransferDisabled = TRUE;
        ChoosePartyMonByMenuType((u8)gTasks[taskId].data[0]);
        DestroyTask(taskId);
    }
}

static u16 NativeSpecial_ChoosePartyMon(void)
{
    u8 taskId;

    LockPlayerFieldControls();
    taskId = CreateTask(Task_ChoosePartyMonSpecials, 10);
    gTasks[taskId].data[0] = PARTY_MENU_TYPE_CHOOSE_SINGLE_MON;
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 0x10, RGB_BLACK);
    return 0;
}

/* Index-aligned with data/specials.inc: the u16 operands emitted by
 * tools/gen_map_data.py are positions in that table. Specials that are not
 * ported keep a NULL entry, which ScrCmd_special reports instead of calling
 * through a NULL pointer. */
u16 (*const gSpecials[])(void) = {
    [0]   = NativeSpecial_HealPlayerParty,
    [56]  = NativeSpecial_PlayTrainerEncounterMusic,
    [124] = NativeSpecial_BufferMonNickname,
    [150] = NativeSpecial_SetHiddenItemFlag,
    [158] = NativeSpecial_ChangePokemonNickname,
    [159] = NativeSpecial_ChoosePartyMon,
    [171] = NativeSpecial_RockSmashWildEncounter,
    [199] = NativeSpecial_TryFieldPoisonWhiteOut,
    [200] = NativeSpecial_SetCB2WhiteOut,
    [212] = NativeSpecial_GetPokedexCount,
    [213] = NativeSpecial_GetProfOaksRatingMessage,
    [230] = NativeSpecial_GetLeadMonFriendship,
    [327] = NativeSpecial_GetPartyMonSpecies,
    [332] = NativeSpecial_Script_FadeOutMapMusic,
    [346] = NativeSpecial_DoPicboxCancel,
    [367] = NativeSpecial_EnableNationalPokedex,
    [368] = NativeSpecial_SetWalkingIntoSignVars,
    [369] = NativeSpecial_DisableMsgBoxWalkaway,
    [371] = NativeSpecial_SetFlavorTextFlagFromSpecialVars,
    [372] = NativeSpecial_UpdatePickStateFromSpecialVar8005,
    [373] = NativeSpecial_OverworldWhiteOutGetMoneyLoss,
    [381] = NativeSpecial_Script_SetHelpContext,
    [382] = NativeSpecial_BackupHelpContext,
    [383] = NativeSpecial_RestoreHelpContext,
    [385] = NativeSpecial_SetUnlockedPokedexFlags,
    [391] = NativeSpecial_GetQuestLogState,
    [392] = NativeSpecial_QuestLog_CutRecording,
    [399] = NativeSpecial_SetBattledTrainerFlag,
    [407] = NativeSpecial_DaisyMassageServices,
};
u16 (*const *gSpecialsEnd)(void) = gSpecials + ARRAY_COUNT(gSpecials);
const u8 *const gStdScripts[] = { NULL };
const u8 *const gStdScriptsEnd[] = { NULL };

void PlaySlotMachine(u16 a, void (*cb)(void)) { (void)a; (void)cb; }
/* CreatePokemartMenu / CreateDecorationShop1Menu / CreateDecorationShop2Menu
 * and RecordItemTransaction are the real src/shop.c (linked via PREPROC_SRCS —
 * it carries _() strings and the shop_menu INCBINs live in src/graphics.c), so
 * the four stubs that used to no-op the Mart clerk are deleted. */
void CreateScriptedWildMon(u16 species, u8 level, u16 item) { (void)species; (void)level; (void)item; }
/* src/field_specials.c: GetLeadMonIndex. The first party slot holding a real
 * mon (not empty, not an egg). Returning 0 unconditionally would report an
 * empty slot's friendship to Daisy's rating branch and to
 * ScrCmd_bufferleadmonspeciesname. */
u8 ScriptGiveEgg(u16 species) { (void)species; return 0; }
void ScriptSetMonMoveSlot(u8 partyIdx, u16 move, u8 slot) { (void)partyIdx; (void)move; (void)slot; }
void SetMysteryEventScriptStatus(u32 status) { (void)status; }
