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
const u8 EventScript_FieldPoison[] = { 0x02 };
const u8 EventScript_Food[] = { 0x02 };
const u8 EventScript_HiddenItemScript[] = { 0x02 };
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
const u8 gPokedexEntries[] = { 0 };
u8 gMaxFlashLevel = 0;
u32 gOverworldBackgroundLayerFlags = 0;
struct Link gLink = {0};
u16 gLinkPartnersHeldKeys[6] = {0};
struct RfuManager gRfu = {0};
const u8 mus_victory_gym_leader[] = {0};
struct FieldInput gQuestLogFieldInput = {0};

/* START menu entries whose scenes are not linked yet, plus the SAVE dialog's
 * flash-write path (src/save.c needs the GBA linker-script layout constants and
 * src/quest_log.c is mostly stub). These report the gap on stderr instead of
 * running. gSaveAttemptStatus stays a real writable variable so the save
 * result check can read it. */
u16 gSaveAttemptStatus = SAVE_STATUS_ERROR;

void CB2_OpenPokedexFromStartMenu(void) { printf("[Menu] POKéDEX scene is not ported\n"); }
void CB2_ReturnToPokeStorage(void) { printf("[Menu] POKéMON storage scene is not ported\n"); }
void SaveQuestLogData(void) {}
void SetUsingUnionRoomStartMenu(void) {}
void Pokedude_InitTMCase(void) {}
void RecordItemTransaction(u16 itemId, u16 quantity, u8 logEventId) { (void)itemId; (void)quantity; (void)logEventId; }
bool8 WriteSaveBlock2(void) { printf("[Menu] saving is not ported\n"); return FALSE; }
bool8 WriteSaveBlock1Sector(void) { printf("[Menu] saving is not ported\n"); return FALSE; }
void Task_LinkFullSave(u8 taskId) { (void)taskId; printf("[Menu] saving is not ported\n"); }

/* Function stubs */
void BerryPouch_SetExitCallback(void *cb) {}
void BerryPouch_StartFadeToExitCallback(u8 taskId) {}
bool8 CheckForTrainersWantingBattle(void) { return FALSE; }
void ClearLinkCallback_2(void) {}
/* src/field_specials.c: CountDigits. Declared s32 in include/field_specials.h
 * and used by linked src/scrcmd.c and src/overworld.c. */
s32 CountDigits(s32 number)
{
    if (number / 10 == 0)
        return 1;
    else if (number / 100 == 0)
        return 2;
    else if (number / 1000 == 0)
        return 3;
    else if (number / 10000 == 0)
        return 4;
    else if (number / 100000 == 0)
        return 5;
    else if (number / 1000000 == 0)
        return 6;
    else if (number / 10000000 == 0)
        return 7;
    else if (number / 100000000 == 0)
        return 8;
    else
        return 1;
}
void CreateTask_ReestablishCableClubLink(void) {}
s8 DexScreen_GetSetPokedexFlag(u16 nationalNum, u8 caseId) { return 0; }
void DismissMapNamePopup(void) {}
void DisplayItemMessageInBerryPouch(u8 taskId, u8 fontId, const u8 *str, void *cb) {}
void DoCurrentWeather(void) {}
void DoOutwardBarnDoorWipe(void) {}
void DoPoisonFieldEffect(void) {}
void FadeOutAndFadeInNewMapMusic(u16 song, u8 speed) {}
void FadeOutAndPlayNewMapMusic(u16 song, u8 speed) {}
void FieldCB_RushInjuredPokemonToCenter(void) {}
u32 GetBerryPowder(void) { return 0; }
u32 GetCoins(void) { return 0; }
u16 GetCurrentMapMusic(void) { return 0; }
u16 GetHealLocation(u8 index) { return 0; }
/* src/field_specials.c: GetHiddenItemAttr. Called by linked
 * src/field_control_avatar.c when the player faces a hidden-item bg event, so
 * the old always-0 stub made every hidden item resolve to item 0 / flag 1000.
 * The bit layout lives in include/global.fieldmap.h. */
u16 GetHiddenItemAttr(u32 hiddenItem, u8 attr)
{
    if (attr == HIDDEN_ITEM_ITEM)
        return GET_HIDDEN_ITEM_ITEM(hiddenItem);
    else if (attr == HIDDEN_ITEM_FLAG)
        return GET_HIDDEN_ITEM_FLAG(hiddenItem) + FLAG_HIDDEN_ITEMS_START;
    else if (attr == HIDDEN_ITEM_QUANTITY)
        return GET_HIDDEN_ITEM_QUANTITY(hiddenItem);
    else if (attr == HIDDEN_ITEM_UNDERFOOT)
        return GET_HIDDEN_ITEM_UNDERFOOT(hiddenItem);
    else
        return 1;
}
u32 GetLinkRecvQueueLength(void) { return 0; }
u8 GetQuestLogStartType(void) { return 0; }
const u8 *GetSeeingLinkPlayerCardMsg(u8 id) { return NULL; }
void IncrementBirthIslandRockStepCount(void) {}
void IncrementResortGorgeousStepCounter(void) {}
void InitBerryPouch(u8 type, void *cb) {}
void InitSecondaryTilesetAnimation(void) {}
void InitTMCase(u8 type, void *cb, bool8 a) {}
void InitTeachyTvController(void) {}
void InitTilesetAnim_CeladonCity(void) {}
void InitTilesetAnim_CeladonGym(void) {}
void InitTilesetAnim_General(void) {}
void InitTilesetAnim_MtEmber(void) {}
void InitTilesetAnim_SilphCo(void) {}
void InitTilesetAnim_VermilionGym(void) {}
void InitTilesetAnimations(void) {}
bool8 IsEscalatorMoving(void) { return FALSE; }
bool32 IsRfuRecvQueueEmpty(void) { return TRUE; }
bool32 IsSendingKeysToLink(void) { return FALSE; }
bool8 IsSpecialSEPlaying(void) { return FALSE; }
void ItemUseOnFieldCB_Itemfinder(u8 taskId) {}
void LinkRfu_FatalError(void) {}
void MovementAction_RevealTrainer_RunTrainerSeeFuncList(struct ObjectEvent *obj, struct Sprite *sprite) {}
void PlayCry_NormalNoDucking(u16 species, s8 pan, u8 volume, u8 priority) {}
void PlayFanfareByFanfareNum(u8 num) {}
void QL_AfterRecordFishActionSuccessful(void) {}
void QL_CopySaveState(void) {}
u8 QL_GetPlaybackState(void) { return 0; }
void QL_HandleInput(void) {}
void QL_InitSceneObjectsAndActions(void) {}
void QL_RecordFieldInput(void *rec) {}
void QL_ResetDefeatedWildMonRecord(void) {}
void QL_ResetPartyAndPC(void) {}
void QL_RestoreMapLayoutId(void) {}
void QL_TryRunActions(void) {}
void QL_TryStopSurfing(void) {}
void QL_UpdateObject(struct Sprite *sprite) {}
void QuestLogCallUpdatePlayerSprite(void) {}
void *QuestLogGetFlagOrVarPtr(bool8 isFlag, u16 idx) { return NULL; }
void QuestLogRecordNPCStep(u8 a, u8 b, u8 c, u8 d) {}
void QuestLogRecordNPCStepWithDuration(u8 a, u8 b, u8 c, u8 d, u8 e) {}
void QuestLogRecordPlayerAvatarGfxTransitionWithDuration(u8 a, u8 b) {}
void QuestLogRecordPlayerStep(u8 a) {}
void QuestLogRecordPlayerStepWithDuration(u8 a, u8 b) {}
bool8 QuestLogScenePlaybackIsEnding(void) { return TRUE; }
void QuestLogTryRecordPlayerAvatarGfxTransition(u8 a) {}
void QuestLog_AdvancePlayhead_(void) {}
void QuestLog_BackUpPalette(u16 a, u16 b) {}
void QuestLog_CheckDepartingIndoorsMap(void) {}
void QuestLog_CutRecording(void) {}
void QuestLog_DrawPreviouslyOnQuestHeaderIfInPlaybackMode(void) {}
void QuestLog_InitPalettesBackup(void) {}
void QuestLog_OnEscalatorWarp(u8 a) {}
void QuestLog_TryRecordDepartedLocation(void) {}
void ReadMail(struct Mail *mail, void *cb, bool8 a) {}
void ResetContextNpcTextColor(void) {}
void ResetCyclingRoadChallengeData(void) {}
void ResumePausedWeather(void) {}
/* src/field_specials.c: RunMassageCooldownStepCounter. Daisy offers to groom a
 * mon only once VAR_MASSAGE_COOLDOWN_STEP_COUNTER reaches 500, and
 * DaisyMassageServices resets it. A no-op counter would gate her on a value
 * that never moves, so the real increment lives here (called from
 * ProcessPlayerFieldInput on every step). */
void RunMassageCooldownStepCounter(void)
{
    u16 count = VarGet(VAR_MASSAGE_COOLDOWN_STEP_COUNTER);
    if (count < 500)
        VarSet(VAR_MASSAGE_COOLDOWN_STEP_COUNTER, count + 1);
}
void RunQuestLogCB(void) {}
void SetBerryPowder(u32 *powder, u32 amount) {}
void SetHelpContextForMap(void) {}
void SetSavedWeatherFromCurrMapHeader(void) {}
void SetWhiteoutRespawnWarpAndHealerNpc(struct WarpData *warp) {}
bool8 ShouldEggHatch(void) { return FALSE; }
void ShowMapNamePopup(bool8 a) {}
void StartEscalator(bool8 a) {}
void StartSendingKeysToLink(void) {}
void StopEscalator(void) {}
void StopMapMusic(void) {}
void StopPokemonLeagueLightingEffectTask(void) {}
void Task_BarnDoorWipe(u8 taskId) {}
void Task_BerryPouch_DestroyDialogueWindowAndRefreshListMenu(u8 taskId) {}
void TransferTilesetAnimsBuffer(void) {}
void UpdateTilesetAnimations(void) {}
void UseFameChecker(MainCallback savedCallback) { (void)savedCallback; }
/* src/field_specials.c: UsedPokemonCenterWarp. Called by linked
 * src/overworld.c when resolving a whiteout, to decide whether the player's
 * last warp was a Pokémon Center (respawn there) or an ordinary map warp.
 * The old always-false stub made every whiteout respawn at the current map. */
const u16 sPokeCenter1FMaps[] = {
    MAP_VIRIDIAN_CITY_POKEMON_CENTER_1F,
    MAP_PEWTER_CITY_POKEMON_CENTER_1F,
    MAP_CERULEAN_CITY_POKEMON_CENTER_1F,
    MAP_LAVENDER_TOWN_POKEMON_CENTER_1F,
    MAP_VERMILION_CITY_POKEMON_CENTER_1F,
    MAP_CELADON_CITY_POKEMON_CENTER_1F,
    MAP_FUCHSIA_CITY_POKEMON_CENTER_1F,
    MAP_CINNABAR_ISLAND_POKEMON_CENTER_1F,
    MAP_INDIGO_PLATEAU_POKEMON_CENTER_1F,
    MAP_SAFFRON_CITY_POKEMON_CENTER_1F,
    MAP_ROUTE4_POKEMON_CENTER_1F,
    MAP_ROUTE10_POKEMON_CENTER_1F,
    MAP_ONE_ISLAND_POKEMON_CENTER_1F,
    MAP_TWO_ISLAND_POKEMON_CENTER_1F,
    MAP_THREE_ISLAND_POKEMON_CENTER_1F,
    MAP_FOUR_ISLAND_POKEMON_CENTER_1F,
    MAP_FIVE_ISLAND_POKEMON_CENTER_1F,
    MAP_SEVEN_ISLAND_POKEMON_CENTER_1F,
    MAP_SIX_ISLAND_POKEMON_CENTER_1F,
    MAP_UNION_ROOM,
    MAP_UNDEFINED
};

bool8 UsedPokemonCenterWarp(void)
{
    s32 i;
    u16 mapno = (gLastUsedWarp.mapGroup << 8) + gLastUsedWarp.mapNum;

    for (i = 0; sPokeCenter1FMaps[i] != MAP_UNDEFINED; i++)
    {
        if (sPokeCenter1FMaps[i] == mapno)
            return TRUE;
    }
    return FALSE;
}
void WaitFanfare(bool8 a) {}
void WriteFlashScanlineEffectBuffer(u8 a) {}

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

extern u8 gQuestLogState;
// src/prof_pc.c has no header; the GBA build declares these through
// data/specials.inc's def_special macro.
u16 GetPokedexCount(void);
void GetProfOaksRatingMessage(void);

/* The quest log recorder is not linked. PokedexRating_EventScript_RateInPerson
 * opens with `goto_if_questlog EventScript_ReleaseEnd` and
 * `special QuestLog_CutRecording`, so the pair must exist and report "not
 * recording" (QL_STATE 0) to let the rating run. */
static u16 NativeSpecial_GetQuestLogState(void)
{
    gSpecialVar_Result = gQuestLogState;
    return 0;
}

static u16 NativeSpecial_QuestLog_CutRecording(void)
{
    gQuestLogState = 0;
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
    [158] = NativeSpecial_ChangePokemonNickname,
    [159] = NativeSpecial_ChoosePartyMon,
    [212] = NativeSpecial_GetPokedexCount,
    [213] = NativeSpecial_GetProfOaksRatingMessage,
    [230] = NativeSpecial_GetLeadMonFriendship,
    [327] = NativeSpecial_GetPartyMonSpecies,
    [346] = NativeSpecial_DoPicboxCancel,
    [367] = NativeSpecial_EnableNationalPokedex,
    [368] = NativeSpecial_SetWalkingIntoSignVars,
    [369] = NativeSpecial_DisableMsgBoxWalkaway,
    [371] = NativeSpecial_SetFlavorTextFlagFromSpecialVars,
    [372] = NativeSpecial_UpdatePickStateFromSpecialVar8005,
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

void AddCoins(u16 coins) { (void)coins; }
void RemoveCoins(u16 coins) { (void)coins; }
void ShowCoinsWindow(u16 x, u16 y) { (void)x; (void)y; }
void HideCoinsWindow(void) {}
void PrintCoinsString(u16 coins) { (void)coins; }
void PlaySlotMachine(u8 a, void *cb) { (void)a; (void)cb; }
void AnimateFlash(u8 a) { (void)a; }
void CreateDecorationShop1Menu(void) {}
void CreateDecorationShop2Menu(void) {}
void CreatePokemartMenu(const u16 *items) { (void)items; }
void CreateScriptedWildMon(u16 species, u8 level, u16 item) { (void)species; (void)level; (void)item; }
void FadeOutBGMTemporarily(u8 a) { (void)a; }
bool8 IsBGMPausedOrStopped(void) { return FALSE; }
/* src/field_specials.c: GetLeadMonIndex. The first party slot holding a real
 * mon (not empty, not an egg). Returning 0 unconditionally would report an
 * empty slot's friendship to Daisy's rating branch and to
 * ScrCmd_bufferleadmonspeciesname. */
u8 GetLeadMonIndex(void)
{
    u8 partyCount = CalculatePlayerPartyCount();
    u8 i;

    for (i = 0; i < partyCount; i++)
    {
        u16 speciesOrEgg = GetMonData(&gPlayerParty[i], MON_DATA_SPECIES_OR_EGG, NULL);
        if (speciesOrEgg != SPECIES_EGG && speciesOrEgg != SPECIES_NONE)
            return i;
    }
    return 0;
}
void PlayCry_Script(u16 species, u8 a) { (void)species; (void)a; }
bool8 QL_AvoidDisplay(void (*callback)(void)) { (void)callback; return FALSE; }
u8 ScriptGiveEgg(u16 species) { (void)species; return 0; }
void ScriptSetMonMoveSlot(u8 partyIdx, u16 move, u8 slot) { (void)partyIdx; (void)move; (void)slot; }
void SetMysteryEventScriptStatus(u8 status) { (void)status; }
void SetSavedWeather(u16 weather) { (void)weather; }
