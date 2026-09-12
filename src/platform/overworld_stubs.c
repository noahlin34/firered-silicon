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
#include "save_location.h"
#include "field_message_box.h"
#include "battle_setup.h"
#include "constants/flags.h"
#include "constants/vars.h"
#include "constants/items.h"

static const u8 sDummyScript[] = { 0x02 };

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

/* Function stubs */
void Bag_BeginCloseWin0Animation(void) {}
void BerryPouch_SetExitCallback(void *cb) {}
void BerryPouch_StartFadeToExitCallback(u8 taskId) {}
void CB2_BagMenuFromBattle(void) {}
void CB2_BagMenuFromStartMenu(void) {}
bool8 CheckForTrainersWantingBattle(void) { return FALSE; }
void ClearLinkCallback_2(void) {}
u8 CountDigits(u32 number) { return 1; }
void CreateTask_ReestablishCableClubLink(void) {}
s8 DexScreen_GetSetPokedexFlag(u16 nationalNum, u8 caseId) { return 0; }
void DismissMapNamePopup(void) {}
void DisplayItemMessageInBag(u8 taskId, u8 fontId, const u8 *str, void *cb) {}
void DisplayItemMessageInBerryPouch(u8 taskId, u8 fontId, const u8 *str, void *cb) {}
void DoCurrentWeather(void) {}
void DoOutwardBarnDoorWipe(void) {}
void DoPoisonFieldEffect(void) {}
void FadeOutAndFadeInNewMapMusic(u16 song, u8 speed) {}
void FadeOutAndPlayNewMapMusic(u16 song, u8 speed) {}
void FieldCB_RushInjuredPokemonToCenter(void) {}
bool32 ForestMapPreviewScreenIsRunning(void) { return TRUE; }
u32 GetBerryPowder(void) { return 0; }
u32 GetCoins(void) { return 0; }
u16 GetCurrentMapMusic(void) { return 0; }
u16 GetHealLocation(u8 index) { return 0; }
u8 GetHiddenItemAttr(u16 hiddenItemId, u8 attr) { return 0; }
u32 GetLinkRecvQueueLength(void) { return 0; }
u8 GetQuestLogStartType(void) { return 0; }
const u8 *GetSeeingLinkPlayerCardMsg(u8 id) { return NULL; }
void IncrementBirthIslandRockStepCount(void) {}
void IncrementResortGorgeousStepCounter(void) {}
void InitBerryPouch(u8 type, void *cb) {}
void InitRegionMapWithExitCB(u8 type, void *cb) {}
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
void ItemMenu_SetExitCallback(void *cb) {}
void ItemMenu_StartFadeToExitCallback(u8 taskId) {}
void ItemUseOnFieldCB_Itemfinder(u8 taskId) {}
void LinkRfu_FatalError(void) {}
bool8 MapHasPreviewScreen(u8 mapSec, u8 type) { return FALSE; }
bool8 MapHasPreviewScreen_HandleQLState2(u8 mapSec, u8 type) { return FALSE; }
void MapPreview_LoadGfx(u8 mapSec) {}
void MapPreview_StartForestTransition(u8 mapSec) {}
void MovementAction_RevealTrainer_RunTrainerSeeFuncList(struct ObjectEvent *obj, struct Sprite *sprite) {}
void PlayCry_NormalNoDucking(u16 species, s8 pan, u8 volume, u8 priority) {}
void PlayFanfareByFanfareNum(u8 num) {}
void PocketCalculateInitialCursorPosAndItemsAbove(void) {}
void Pocket_CalculateNItemsAndMaxShowed(u8 pocket) {}
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
void RunMassageCooldownStepCounter(void) {}
void RunQuestLogCB(void) {}
void SetBerryPowder(u32 *powder, u32 amount) {}
void SetHelpContextForMap(void) {}
void SetSavedWeatherFromCurrMapHeader(void) {}
void SetUpReturnToStartMenu(void) {}
void SetWhiteoutRespawnWarpAndHealerNpc(struct WarpData *warp) {}
bool8 ShouldEggHatch(void) { return FALSE; }
void ShowMapNamePopup(bool8 a) {}
void ShowStartMenu(void) {}
void StartEscalator(bool8 a) {}
void StartSendingKeysToLink(void) {}
void StopEscalator(void) {}
void StopMapMusic(void) {}
void StopPokemonLeagueLightingEffectTask(void) {}
void Task_BarnDoorWipe(u8 taskId) {}
void Task_BerryPouch_DestroyDialogueWindowAndRefreshListMenu(u8 taskId) {}
void Task_ReturnToBagFromContextMenu(u8 taskId) {}
void Task_StartMenuHandleInput(u8 taskId) {}
void TransferTilesetAnimsBuffer(void) {}
void UpdateTilesetAnimations(void) {}
void UseFameChecker(MainCallback savedCallback) { (void)savedCallback; }
void UseRegisteredKeyItemOnField(void) {}
void UsedPokemonCenterWarp(void) {}
bool8 ValidateSavedWonderCard(void) { return FALSE; }
void WaitFanfare(bool8 a) {}
void WonderNews_IncrementStepCounter(void) {}
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

/* Index-aligned with data/specials.inc: the u16 operands emitted by
 * tools/gen_map_data.py are positions in that table. Specials that are not
 * ported keep a NULL entry, which ScrCmd_special reports instead of calling
 * through a NULL pointer. */
u16 (*const gSpecials[])(void) = {
    [0]   = NativeSpecial_HealPlayerParty,
    [56]  = NativeSpecial_PlayTrainerEncounterMusic,
    [158] = NativeSpecial_ChangePokemonNickname,
    [212] = NativeSpecial_GetPokedexCount,
    [213] = NativeSpecial_GetProfOaksRatingMessage,
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
u8 GetLeadMonIndex(void) { return 0; }
void MapPreview_SetFlag(u16 a) { (void)a; }
void PlayCry_Script(u16 species, u8 a) { (void)species; (void)a; }
bool8 QL_AvoidDisplay(void (*callback)(void)) { (void)callback; return FALSE; }
u8 ScriptGiveEgg(u16 species) { (void)species; return 0; }
void ScriptSetMonMoveSlot(u8 partyIdx, u16 move, u8 slot) { (void)partyIdx; (void)move; (void)slot; }
void SetMysteryEventScriptStatus(u8 status) { (void)status; }
void SetSavedWeather(u16 weather) { (void)weather; }
