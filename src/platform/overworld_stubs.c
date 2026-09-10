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
const u8 EventScript_CancelMessageBox[] = { 0x02 };
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
const u8 *const gFieldEffectScriptPointers[] = { NULL };
const u8 gPokedexEntries[] = { 0 };
u8 gMaxFlashLevel = 0;
u32 gOverworldBackgroundLayerFlags = 0;
struct Link gLink = {0};
u16 gLinkPartnersHeldKeys[6] = {0};
struct RfuManager gRfu = {0};
u8 gItemUseCB = 0;
const struct OamData gOamData_AffineOff_ObjNormal_8x8 = {0};
const u8 mus_victory_gym_leader[] = {0};
struct FieldInput gQuestLogFieldInput = {0};

/* Function stubs */
void Bag_BeginCloseWin0Animation(void) {}
void BerryPouch_SetExitCallback(void *cb) {}
void BerryPouch_StartFadeToExitCallback(u8 taskId) {}
void CB2_BagMenuFromBattle(void) {}
void CB2_BagMenuFromStartMenu(void) {}
void CB2_ShowPartyMenuForItemUse(void) {}
bool8 CheckForTrainersWantingBattle(void) { return FALSE; }
void ClearLinkCallback_2(void) {}
u8 CountDigits(u32 number) { return 1; }
void CreateMonPicSprite_HandleDeoxys(u16 species, u32 personality, u16 x, u16 y, u8 priority) {}
void CreateTask_ReestablishCableClubLink(void) {}
void CreateWarpArrowSprite(void) {}
s8 DexScreen_GetSetPokedexFlag(u16 nationalNum, u8 caseId) { return 0; }
void DismissMapNamePopup(void) {}
void DisplayItemMessageInBag(u8 taskId, u8 fontId, const u8 *str, void *cb) {}
void DisplayItemMessageInBerryPouch(u8 taskId, u8 fontId, const u8 *str, void *cb) {}
void DoCurrentWeather(void) {}
void DoOutwardBarnDoorWipe(void) {}
void DoPoisonFieldEffect(void) {}
void EnterPartyFromItemMenuInBattle(u8 taskId) {}
void FadeOutAndFadeInNewMapMusic(u16 song, u8 speed) {}
void FadeOutAndPlayNewMapMusic(u16 song, u8 speed) {}
void FieldCB_RushInjuredPokemonToCenter(void) {}
u8 FindTallGrassFieldEffectSpriteId(u8 a) { return 0; }
bool32 ForestMapPreviewScreenIsRunning(void) { return TRUE; }
void FreeAndDestroyMonPicSprite(u8 spriteId) {}
u32 GetBerryPowder(void) { return 0; }
u32 GetCoins(void) { return 0; }
u16 GetCurrentMapMusic(void) { return 0; }
u8 GetCursorSelectionMonId(void) { return 0; }
u16 GetHealLocation(u8 index) { return 0; }
u8 GetHiddenItemAttr(u16 hiddenItemId, u8 attr) { return 0; }
u8 GetItemEffectType(u16 item) { return 0; }
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
void ItemUseCB_EvolutionStone(u8 taskId) {}
void ItemUseCB_Medicine(u8 taskId) {}
void ItemUseCB_MedicineStep(u8 taskId) {}
void ItemUseCB_PPUp(u8 taskId) {}
void ItemUseCB_RareCandy(u8 taskId) {}
void ItemUseCB_SacredAsh(u8 taskId) {}
void ItemUseCB_TryRestorePP(u8 taskId) {}
void ItemUseOnFieldCB_Itemfinder(u8 taskId) {}
void LinkRfu_FatalError(void) {}
bool8 MapHasPreviewScreen(u8 mapSec, u8 type) { return FALSE; }
bool8 MapHasPreviewScreen_HandleQLState2(u8 mapSec, u8 type) { return FALSE; }
void MapPreview_LoadGfx(u8 mapSec) {}
void MapPreview_StartForestTransition(u8 mapSec) {}
void MapResetTrainerRematches(u16 grp, u16 num) {}
bool8 MonKnowsMove(struct Pokemon *mon, u16 move) { return FALSE; }
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
bool8 QuestLog_ShouldEndSceneOnMapChange(void) { return FALSE; }
void QuestLog_TryRecordDepartedLocation(void) {}
void ReadMail(struct Mail *mail, void *cb, bool8 a) {}
void ResetAllPicSprites(void) {}
void ResetContextNpcTextColor(void) {}
void ResetCyclingRoadChallengeData(void) {}
void ResetSafariZoneFlag(void) {}
void ResumePausedWeather(void) {}
void RunMassageCooldownStepCounter(void) {}
void RunQuestLogCB(void) {}
bool8 SafariZoneTakeStep(void) { return FALSE; }
void SetBerryPowder(u32 *powder, u32 amount) {}
void SetHelpContextForMap(void) {}
void SetQuestLogEvent_Arrived(void) {}
void SetSavedWeatherFromCurrMapHeader(void) {}
void SetSpriteInvisible(u8 spriteId) {}
void SetSurfBlob_BobState(u8 a, u8 b) {}
void SetSurfBlob_DontSyncAnim(u8 a, bool8 b) {}
void SetSurfBlob_PlayerOffset(u8 a, s16 b, s16 c) {}
void SetUpReflection(struct ObjectEvent *obj, struct Sprite *sprite, bool8 a) {}
void SetUpReturnToStartMenu(void) {}
void SetWhiteoutRespawnWarpAndHealerNpc(struct WarpData *warp) {}
bool8 ShouldEggHatch(void) { return FALSE; }
void ShowMapNamePopup(bool8 a) {}
void ShowStartMenu(void) {}
void ShowWarpArrowSprite(u8 a, u8 b, u8 c) {}
void StartAshFieldEffect(s16 a, s16 b, u16 c, s16 d) {}
void StartEscalator(bool8 a) {}
void StartRevealDisguise(struct ObjectEvent *obj) {}
void StartRoamerBattle(void) {}
void StartSendingKeysToLink(void) {}
void StartWildBattle(void) {}
void StopEscalator(void) {}
void StopMapMusic(void) {}
void StopPokemonLeagueLightingEffectTask(void) {}
void Task_BarnDoorWipe(u8 taskId) {}
void Task_BerryPouch_DestroyDialogueWindowAndRefreshListMenu(u8 taskId) {}
void Task_ReturnToBagFromContextMenu(u8 taskId) {}
void Task_StartMenuHandleInput(u8 taskId) {}
void Task_VsSeeker_0(u8 taskId) {}
void TransferTilesetAnimsBuffer(void) {}
void TrySetMapSaveWarpStatus(void) {}
void UpdateAshFieldEffect(struct Sprite *sprite) {}
void UpdateBubblesFieldEffect(struct Sprite *sprite) {}
void UpdateDisguiseFieldEffect(struct Sprite *sprite) {}
void UpdateFootprintsTireTracksFieldEffect(struct Sprite *sprite) {}
void UpdateHotSpringsWaterFieldEffect(struct Sprite *sprite) {}
void UpdateJumpImpactEffect(struct Sprite *sprite) {}
void UpdateLongGrassFieldEffect(struct Sprite *sprite) {}
void UpdateRevealDisguise(struct Sprite *sprite) {}
void UpdateSandPileFieldEffect(struct Sprite *sprite) {}
void UpdateShadowFieldEffect(struct Sprite *sprite) {}
void UpdateShortGrassFieldEffect(struct Sprite *sprite) {}
void UpdateSparkleFieldEffect(struct Sprite *sprite) {}
void UpdateSplashFieldEffect(struct Sprite *sprite) {}
void UpdateSurfBlobFieldEffect(struct Sprite *sprite) {}
void UpdateTallGrassFieldEffect(struct Sprite *sprite) {}
void UpdateTilesetAnimations(void) {}
void UpdateVsSeekerStepCounter(void) {}
void UseFameChecker(MainCallback savedCallback) { (void)savedCallback; }
void UseRegisteredKeyItemOnField(void) {}
void UsedPokemonCenterWarp(void) {}
bool8 ValidateSavedWonderCard(void) { return FALSE; }
void WaitFanfare(bool8 a) {}
bool8 WaitFieldEffectSpriteAnim(struct Sprite *sprite) { return TRUE; }
void WonderNews_IncrementStepCounter(void) {}
void WriteFlashScanlineEffectBuffer(u8 a) {}

/* Specials and Scrcmd Stubs */
static u16 NativeSpecial_HealPlayerParty(void)
{
    HealPlayerParty();
    return 0;
}

u16 (*const gSpecials[])(void) = {
    NativeSpecial_HealPlayerParty,
};
u16 (*const *gSpecialsEnd)(void) = gSpecials + ARRAY_COUNT(gSpecials);
const u8 *const gStdScripts[] = { NULL };
const u8 *const gStdScriptsEnd[] = { NULL };
const u8 *const gStdStringPtrs[] = { NULL };

void AddCoins(u16 coins) { (void)coins; }
void RemoveCoins(u16 coins) { (void)coins; }
void ShowCoinsWindow(u16 x, u16 y) { (void)x; (void)y; }
void HideCoinsWindow(void) {}
void PrintCoinsString(u16 coins) { (void)coins; }
void PlaySlotMachine(u8 a, void *cb) { (void)a; (void)cb; }
void AnimateFlash(u8 a) { (void)a; }
void BattleSetup_ConfigureTrainerBattle(const u8 *script) { (void)script; }
const u8 *BattleSetup_GetScriptAddrAfterBattle(void) { return NULL; }
const u8 *BattleSetup_GetTrainerPostBattleScript(void) { return NULL; }
void ClearTrainerFlag(u16 id) { (void)id; }
void SetTrainerFlag(u16 id) { (void)id; }
bool8 HasTrainerBeenFought(u16 id) { (void)id; return FALSE; }
void CreateDecorationShop1Menu(void) {}
void CreateDecorationShop2Menu(void) {}
void CreatePokemartMenu(const u16 *items) { (void)items; }
void CreateScriptedWildMon(u16 species, u8 level, u16 item) { (void)species; (void)level; (void)item; }
void StartScriptedWildBattle(void) {}
void StartTrainerBattle(void) {}
void FadeOutBGMTemporarily(u8 a) { (void)a; }
bool8 IsBGMPausedOrStopped(void) { return FALSE; }
u8 GetLeadMonIndex(void) { return 0; }
void MapPreview_SetFlag(u16 a) { (void)a; }
void PlayCry_Script(u16 species, u8 a) { (void)species; (void)a; }
void QL_AvoidDisplay(void *a) { (void)a; }
void QL_DestroyAbortedDisplay(void) {}
void QuestLog_RecordEnteredMap(u16 a) { (void)a; }
u8 ScriptGiveEgg(u16 species) { (void)species; return 0; }
u8 ScriptGiveMon(u16 species, u8 level, u16 item, u32 a, u32 b, u8 c) { return 0; }
bool8 ScriptMenu_HidePokemonPic(void) { return FALSE; }
bool8 ScriptMenu_ShowPokemonPic(u16 species, u8 x, u8 y) { return FALSE; }
bool8 ScriptMenu_Multichoice(u8 left, u8 top, u8 multichoiceId, bool8 ignoreBPress) { return FALSE; }
bool8 ScriptMenu_MultichoiceGrid(u8 left, u8 top, u8 multichoiceId, bool8 ignoreBPress, u8 columnCount) { return FALSE; }
bool8 ScriptMenu_MultichoiceWithDefault(u8 left, u8 top, u8 multichoiceId, bool8 ignoreBPress, u8 defaultChoice) { return FALSE; }
bool8 ScriptMenu_YesNo(u8 left, u8 top) { return FALSE; }
void ScriptSetMonMoveSlot(u8 partyIdx, u16 move, u8 slot) {}
void SetMysteryEventScriptStatus(u8 status) { (void)status; }
void SetSavedWeather(u16 weather) { (void)weather; }
