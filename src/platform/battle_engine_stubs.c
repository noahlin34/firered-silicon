#include <stdio.h>
#include <stdlib.h>


#include "battle.h"

#include "battle_scripts.h"

#include "data.h"

#include "util.h"
#include "palette.h"


#include "task.h"

#include "sprite.h"

#include "main.h"

#include "menu.h"

#include "constants/battle_move_effects.h"
#include "link.h"

#include "constants/flags.h"
#include "constants/vars.h"
#include "constants/items.h"
#include "constants/species.h"
#include "pokemon_storage_system.h"
#include "string_util.h"
#include "item.h"
#include "event_data.h"
#include "pokemon_storage_system_internal.h"
#include "strings.h"
#include "script.h"
extern u16 gSpecialVar_0x8014;
/* Auto-generated battle-engine stubs: the GBA battle engine (assembly scripts +
 * battle controllers) is not linked in the native port. Only data symbols and
 * entry points referenced by non-battle scenes are provided. */

/* Tripwire: the battle engine is unreachable until the overworld links
 * (CB2_NewGame -> wild encounter). If any of these get called before the
 * real engine is linked, fail loudly instead of silently no-op'ing. */
static void BattleStubHit(const char *name)
{
    printf("[FATAL] Battle engine stub called: %s\n", name);
    fflush(stdout);
    abort();
}


/* ---- data ---- */
const u8 BattleScript_AbilityCuredStatus[];
const u8 BattleScript_ActionSelectionItemsCantBeUsed[];
const u8 BattleScript_ActionSwitch[];
const u8 BattleScript_ApplySecondaryEffect[];
const u8 BattleScript_BattleTowerTrainerBattleWon[];
const u8 BattleScript_BerryConfuseHealEnd2[];
const u8 BattleScript_BerryCureBrnEnd2[];
const u8 BattleScript_BerryCureBrnRet[];
const u8 BattleScript_BerryCureChosenStatusEnd2[];
const u8 BattleScript_BerryCureChosenStatusRet[];
const u8 BattleScript_BerryCureConfusionEnd2[];
const u8 BattleScript_BerryCureConfusionRet[];
const u8 BattleScript_BerryCureFrzEnd2[];
const u8 BattleScript_BerryCureFrzRet[];
const u8 BattleScript_BerryCureParRet[];
const u8 BattleScript_BerryCurePrlzEnd2[];
const u8 BattleScript_BerryCurePsnEnd2[];
const u8 BattleScript_BerryCurePsnRet[];
const u8 BattleScript_BerryCureSlpEnd2[];
const u8 BattleScript_BerryCureSlpRet[];
const u8 BattleScript_BerryFocusEnergyEnd2[];
const u8 BattleScript_BerryPPHealEnd2[];
const u8 BattleScript_BerryStatRaiseEnd2[];
const u8 BattleScript_BideAttack[];
const u8 BattleScript_BideNoEnergyToAttack[];
const u8 BattleScript_BideStoringEnergy[];
const u8 BattleScript_BurnTurnDmg[];
const u8 BattleScript_CastformChange[];
const u8 BattleScript_ColorChangeActivates[];
const u8 BattleScript_CurseTurnDmg[];
const u8 BattleScript_CuteCharmActivates[];
const u8 BattleScript_DamagingWeatherContinues[];
const u8 BattleScript_DisabledNoMore[];
const u8 BattleScript_DrizzleActivates[];
const u8 BattleScript_DroughtActivates[];
const u8 BattleScript_EncoredNoMore[];
const u8 BattleScript_FlashFireBoost[];
const u8 BattleScript_FlashFireBoost_PPLoss[];
const u8 BattleScript_FocusPunchSetUp[];
const u8 BattleScript_GhostGetOutGetOut[];
const u8 BattleScript_GiveExp[];
const u8 BattleScript_GotAwaySafely[];
const u8 BattleScript_HandleFaintedMon[];
const u8 BattleScript_IgnoresAndFallsAsleep[];
const u8 BattleScript_IgnoresAndHitsItself[];
const u8 BattleScript_IgnoresAndUsesRandomMove[];
const u8 BattleScript_IgnoresWhileAsleep[];
const u8 BattleScript_IngrainTurnHeal[];
const u8 BattleScript_IntimidateActivates[];
const u8 BattleScript_IntimidateActivatesEnd3[];
const u8 BattleScript_ItemHealHP_End2[];
const u8 BattleScript_ItemHealHP_RemoveItem[];
const u8 BattleScript_ItemHealHP_Ret[];
const u8 BattleScript_LeechSeedTurnDrain[];
const u8 BattleScript_LinkBattleWonOrLost[];
const u8 BattleScript_LocalBattleLost[];
const u8 BattleScript_LocalTrainerBattleWon[];
const u8 BattleScript_MonMadeMoveUseless[];
const u8 BattleScript_MonMadeMoveUseless_PPLoss[];
const u8 BattleScript_MonTookFutureAttack[];
const u8 BattleScript_MonWokeUpInUproar[];
const u8 BattleScript_MoveHPDrain[];
const u8 BattleScript_MoveHPDrain_PPLoss[];
const u8 BattleScript_MoveUsedFlinched[];
const u8 BattleScript_MoveUsedIsAsleep[];
const u8 BattleScript_MoveUsedIsConfused[];
const u8 BattleScript_MoveUsedIsConfusedNoMore[];
const u8 BattleScript_MoveUsedIsDisabled[];
const u8 BattleScript_MoveUsedIsFrozen[];
const u8 BattleScript_MoveUsedIsImprisoned[];
const u8 BattleScript_MoveUsedIsInLove[];
const u8 BattleScript_MoveUsedIsInLoveCantAttack[];
const u8 BattleScript_MoveUsedIsParalyzed[];
const u8 BattleScript_MoveUsedIsTaunted[];
const u8 BattleScript_MoveUsedLoafingAround[];
const u8 BattleScript_MoveUsedMustRecharge[];
const u8 BattleScript_MoveUsedUnfroze[];
const u8 BattleScript_MoveUsedWokeUp[];
const u8 BattleScript_NightmareTurnDmg[];
const u8 BattleScript_NoMovesLeft[];
const u8 BattleScript_OverworldWeatherStarts[];
const u8 BattleScript_PayDayMoneyAndPickUpItems[];
const u8 BattleScript_PerishSongCountGoesDown[];
const u8 BattleScript_PerishSongTakesLife[];
const u8 BattleScript_PoisonTurnDmg[];
const u8 BattleScript_PrintCantEscapeFromBattle[];
const u8 BattleScript_PrintCantRunFromTrainer[];
const u8 BattleScript_PrintFailedToRunString[];
const u8 BattleScript_PrintFullBox[];
const u8 BattleScript_PrintUproarOverTurns[];
const u8 BattleScript_RainContinuesOrEnds[];
const u8 BattleScript_RainDishActivates[];
const u8 BattleScript_RanAwayUsingMonAbility[];
const u8 BattleScript_RoughSkinActivates[];
const u8 BattleScript_SafeguardEnds[];
const u8 BattleScript_SandStormHailEnds[];
const u8 BattleScript_SandstreamActivates[];
const u8 BattleScript_SelectingDisabledMove[];
const u8 BattleScript_SelectingImprisonedMove[];
const u8 BattleScript_SelectingMoveWithNoPP[];
const u8 BattleScript_SelectingNotAllowedMoveChoiceItem[];
const u8 BattleScript_SelectingNotAllowedMoveTaunt[];
const u8 BattleScript_SelectingTormentedMove[];
const u8 BattleScript_ShedSkinActivates[];
const u8 BattleScript_SideStatusWoreOff[];
const u8 BattleScript_SilphScopeUnveiled[];
const u8 BattleScript_SmokeBallEscape[];
const u8 BattleScript_SoundproofProtected[];
const u8 BattleScript_SpeedBoostActivates[];
const u8 BattleScript_SunlightContinues[];
const u8 BattleScript_SunlightFaded[];
const u8 BattleScript_SynchronizeActivates[];
const u8 BattleScript_ThrashConfuses[];
const u8 BattleScript_TooScaredToMove[];
const u8 BattleScript_TraceActivates[];
const u8 BattleScript_WhiteHerbEnd2[];
const u8 BattleScript_WhiteHerbRet[];
const u8 BattleScript_WildMonFled[];
const u8 BattleScript_WishComesTrue[];
const u8 BattleScript_WrapEnds[];
const u8 BattleScript_WrapTurnDmg[];
const u8 BattleScript_YawnMakesAsleep[];
const u8 Text_MonSentToBoxBillsBoxFull[];
const u8 Text_MonSentToBoxInBillsPC[];
const u8 Text_MonSentToBoxInSomeonesPC[];
const u8 Text_MonSentToBoxSomeonesBoxFull[];
u16 gAnimBattlerSpecies[MAX_BATTLERS_COUNT];
u8 gAnimCustomPanning;
u8 gAnimFriendship;
s16 gBattleAnimArgs[8];
u8 gBattleAnimAttacker;
u8 gBattleAnimTarget;
const struct BgTemplate gBattleBgTemplates[];
u8 gBattlePartyCurrentOrder[PARTY_SIZE / 2];
const u8 *const gBattleScriptsForMoveEffects[];
const u8 *const gBattlescriptsForBallThrow[];
const u8 *const gBattlescriptsForRunningByItem[];
const u8 *const gBattlescriptsForSafariActions[];
const u8 *const gBattlescriptsForUsingItem[];
u16 gBlockRecvBuffer[MAX_RFU_PLAYERS][BLOCK_BUFFER_SIZE / 2];
struct LinkPlayer gLinkPlayers[MAX_RFU_PLAYERS];
u8 gNumSafariBalls;
const struct OamData gOamData_AffineNormal_ObjNormal_64x64;
const struct OamData gOamData_AffineOff_ObjBlend_32x32;
const struct OamData gOamData_AffineOff_ObjNormal_16x8;
const struct OamData gOamData_AffineOff_ObjNormal_32x16;
const struct OamData gOamData_AffineOff_ObjNormal_32x32;
struct PokedudeBattlerState *gPokedudeBattlerStates[MAX_BATTLERS_COUNT];
bool8 gReceivedRemoteLinkPlayers;
struct PokemonStorageSystemData *gStorage;
u16 gTrainerBattleOpponent_A;

/* ---- functions ---- */
void AllocateBattleSpritesData(void) { }
void AllocateMonSpritesGfx(void) { }
u8 AnimateBallOpenParticles(u8 x, u8 y, u8 priority, u8 subpriority, u8 ballId) { return 0; }
void AnimateBoxScrollArrows(bool8 species) { (void)species; }
void BackupHelpContext(void) {}
void BattleAI_HandleItemUseBeforeAISetup(void) {}
s8 BattleAnimAdjustPanning(s8 pan) { BattleStubHit("BattleAnimAdjustPanning"); return 0; }
void BattleControllerDummy(void) { BattleStubHit("BattleControllerDummy"); }
bool8 BattleInitAllSprites(u8 *state, u8 *battlerId) { BattleStubHit("BattleInitAllSprites"); return 0; }
void BattleInterfaceSetWindowPals(void) {}
u8 BattleSetup_GetTerrainId(void) { BattleStubHit("BattleSetup_GetTerrainId"); return 0; }
void BattleStopLowHpSound(void) {}
void BeginEvolutionScene(struct Pokemon* mon, u16 speciesToEvolve, u8, u8 partyId) {}
u8 BitmaskAllOtherLinkPlayers(void) { return 0; }
void BufferBattlePartyCurrentOrderBySide(u8 battlerId, u8 flankId) {}
s16 CalculatePanIncrement(s16 sourcePan, s16 targetPan, s16 incrementPan) { return 0; }
void CheckShouldAdvanceLinkState(void) {}
void ClearBattleAnimationVars(void) {}
void ClearRematchStateByTrainerId(void) {}
void ClearTemporarySpeciesSpriteData(u8 battlerId, bool8 dontClearSubstitute) {}
void CommitQuestLogWindow1(void) {}
u8 ContextNpcGetTextColor(void) { return 0; }
void CopyEReaderTrainerName5(u8 *dest) {}
u8 CountPartyAliveNonEggMonsExcept(u8 slotToIgnore) { return 0; }
u8 CountPartyMons(void) { return 0; }
void CreateBoxMonIconAtPos(u8 boxPosition) {}
u8 CreateHelpMessageWindow(void) { return 0; }
void CreateMovingMonIcon(void) {}
void CreateWirelessStatusIndicatorSprite(u8, u8) {}
void DestroyAnimSoundTask(u8 taskId) {}
void DestroyAnimSprite(struct Sprite *sprite) {}
void DestroyAnimVisualTask(u8 taskId) {}
void DestroyHelpMessageWindow(u8 a0) {}
void DestroyMovingMonIcon(void) {}
void DestroyPartyMonIcon(u8 partyId) {}
void DestroyReleaseMonIcon(void) {}
void DestroyTask_RfuIdle(void) {}
void DoReleaseMonAnim(u8 mode, u8 position) {}
void DrawBattleEntryBackground(void) {}
void EvolutionScene(struct Pokemon* mon, u16 speciesToEvolve, u8, u8 partyId) {}
u16 FontFunc_Braille(struct TextPrinter *textPrinter) { return 0; }
void FreeBattleSpritesData(void) {}
void FreeMonSpritesGfx(void) {}
void FreeTrainerTowerBattleStruct(void) {}
s32 GetAnimBgAttribute(u8 bgId, u8 attributeId) { return 0; }
u8 GetBattleTowerTrainerClassNameId(void) { return 0; }
void GetBattleTowerTrainerName(u8 *text) {}
u8 GetBattlerTurnOrderNum(u8 battlerId) { return 0; }
const struct Berry *GetBerryInfo(u8 berry) { return 0; }
u8 GetBlockReceivedStatus(void) { return 0; }
u8 GetEreaderTrainerClassId(void) { return 0; }
s16 GetFirstFreeBoxSpot(u8 boxId) { return 0; }
u8 GetLastViewedMonIndex(void) { return 0; }
u8 GetLinkPlayerCount_2(void) { return 0; }
u8 GetMultiplayerId(void) { return 0; }
u16 GetPCBoxToSendMon(void) { return 0; }
u8 GetPartyIdFromBattlePartyId(u8 battlePartyId) { return 0; }
u16 GetRivalBattleFlags(void) { return 0; }
const u8 *GetTrainerALoseText(void) { return 0; }
u8 GetTrainerBattleMode(void) { return 0; }
u8 GetTrainerTowerOpponentClass(void) { return 0; }
void GetTrainerTowerOpponentLoseText(u8 *dest, u8 opponentIdx) {}
void GetTrainerTowerOpponentName(u8 *text) {}
void GetTrainerTowerOpponentWinText(u8 *dest, u8 opponentIdx) {}
const u8 *GetTrainerWonSpeech(void) { return 0; }
bool32 InUnionRoom(void) { return 0; }
void InitBattleBgsVideo(void) {}
void InitLinkBattleVsScreen(u8 taskId) {}
void InitTrainerTowerBattleStruct(void) {}
bool8 IsActiveItemMoving(void) { return 0; }
bool8 IsBGMPlaying(void) { return 0; }
bool8 IsBattlerSpriteVisible(u8 battlerId) { return 0; }
/* Audio engine (m4a_1.s) is not ported; cries never start, so "finished"
 * must report TRUE or Task_OakSpeech_IsInhabitedFarAndWide spins forever. */
bool8 IsCryFinished(void) { return TRUE; }
bool8 IsCryPlaying(void) { return FALSE; }
bool8 IsCryPlayingOrClearCrySongs(void) { return FALSE; }
bool8 IsDestinationBoxFull(void) { return 0; }
bool32 IsEnigmaBerryValid(void) { return 0; }
bool8 IsItemIconAnimActive(void) { return 0; }
bool32 IsLinkRecvQueueAtOverworldMax(void) { return 0; }
bool8 IsLinkRfuTaskFinished(void) { return 0; }
u8 ItemIdToBallId(u16 itemId) { return 0; }
u8 ItemIdToBerryType(u16 item) { return 0; }
s16 KeepPanInRange(s16 a, s32 oldPan) { return 0; }
u8 LaunchBallFadeMonTask(bool8 unFadeLater, u8 battlerId, u32 arg2, u8 ballId) { return 0; }
u8 ListMenuAddCursorObjectInternal(const struct CursorStruct *cursor, u32 cursorKind) { return 0; }
void ListMenuRemoveCursorObject(u8 taskId, u32 cursorKind) {}
void ListMenuUpdateCursorObject(u8 taskId, u16 x, u16 y, u32 cursorKind) {}
void LoadBattleMenuWindowGfx(void) {}
void LoadBattleTextboxAndBackground(void) {}
void LoadWirelessStatusIndicatorSpriteGfx(void) {}
void MapNamePopupWindowIdSetDummy(void) {}
bool8 MultiMove_CanPlaceSelection(void) { return 0; }
u8 MultiMove_GetOriginPosition(void) { return 0; }
bool8 MultiMove_TryMoveGroup(u8 dir) { return 0; }
/* Real implementation from field_effect.c (not linked): fades one palette
 * entry toward white; used by the naming-screen cursor flash. */
void PlayCry_ByMode(u16 species, s8 pan, u8 mode) {}
void PlayCry_ReleaseDouble(u16 species, s8 pan, u8 mode) {}
void PlayNewMapMusic(u16 songNum) {}
void PlaySE1WithPanning(u16 songNum, s8 pan) {}
void PlaySE2WithPanning(u16 songNum, s8 pan) {}
void PrintTextOnHelpMessageWindow(const u8 * text, u8 mode) {}
void RecordAbilityBattle(u8 bank, u8 abilityId) {}
void RecordItemEffectBattle(u8 bank, u8 itemEffect) {}
void ResetBlockReceivedFlag(u8) {}
void ResetBlockReceivedFlags(void) {}
void ResetMapMusic(void) {}
void RestoreHelpContext(void) {}
bool8 SendBlock(u8, const void *, u16) { return 0; }
void SetAnimBgAttribute(u8 bgId, u8 attributeId, u8 value) {}
void SetCloseLinkCallback(void) {}
void SetControllerToLinkOpponent(void) {}
void SetControllerToLinkPartner(void) {}
void SetControllerToOakOrOldMan(void) {}
void SetControllerToOpponent(void) {}
void SetControllerToPlayer(void) {}
void SetControllerToPokedude(void) {}
void SetControllerToSafari(void) {}
void SetHealthboxSpriteVisible(u8 healthboxSpriteId) {}
void SetLinkStandbyCallback(void) {}
void SetMoveEffect(bool8 primary, u8 certain) {}
void SetMovingMonPriority(u8 priority) {}
void SetMovingMonSprite(u8 cursorArea, u8 cursorPos) {}
void SetPCBoxToSendMon(u8) {}
void SetPlacedMonSprite(u8 cursorArea, u8 cursorPos) {}
void SetShiftMonSpritePtr(u8 boxId, u8 position) {}
void SetWirelessCommType1(void) {}
bool8 ShiftMons(void) { return 0; }
bool32 ShouldPlayNormalMonCry(struct Pokemon *mon) { return 0; }
void ShowPartyMenuToShowcaseMultiBattleParty(void) {}
void StopCryAndClearCrySongs(void) {}
void SwitchPartyMonSlots(u8 slot, u8 slot2) {}
void Task_WaitForLinkPlayerConnection(u8 taskId) {}
void TryHideItemIconAtPos(u8 cursorArea, u8 cursorPos) {}
bool8 TryHideReleaseMonSprite(void) { return 0; }
void TryLoadItemIconAtPos(u8 cursorArea, u8 cursorPos) {}
void TrySetQuestLogBattleEvent(void) {}
void TrySetQuestLogLinkBattleEvent(void) {}
bool8 UproarWakeUpCheck(u8 battlerId) { return 0; }
void m4aMPlayVolumeControl(struct MusicPlayerInfo *mplayInfo, u16 trackBits, u16 volume) {}


/* ---- round-2 stubs ---- */
const u8 gBattleText_GetPumped[];
const u8 gBattleText_MistShroud[];
const u8 gBattleText_Rose[];
const u8 gCB2_AfterEvolution[] = {0xFF};
const struct MonCoords gCastformFrontSpriteCoords[];
const u8 *const gStatNamesTable[];
const u8 gText_BadEgg[] = {0xFF};
const u8 gText_Burn[] = {0xFF};
const u8 gText_Confusion[] = {0xFF};
const u8 gText_DefendersStatRose[] = {0xFF};
const u8 gText_Ice[] = {0xFF};
const u8 gText_Love[] = {0xFF};
const u8 gText_Paralysis[] = {0xFF};
const u8 gText_PkmnsXPreventsSwitching[] = {0xFF};
const u8 gText_Poison[] = {0xFF};
const u8 gText_Sleep[] = {0xFF};
#define ABILITY_ON_OPPOSING_FIELD(battlerId, abilityId)(AbilityBattleEffects(ABILITYEFFECT_CHECK_OTHER_SIDE, battlerId, abilityId, 0, 0)) { return 0; }
void AllocateBattleResources(void) {}
bool8 AnimTranslateLinear(struct Sprite *sprite) { return 0; }
bool8 AreAllMovesUnusable(void) { return 0; }
void BattleScriptExecute(const u8 *BS_ptr) { BattleStubHit("BattleScriptExecute"); }
u32 BattleStringExpandPlaceholders(const u8 *src, u8 *dst) { return 0; }
u32 BattleStringExpandPlaceholdersToDisplayedString(const u8 *src) { return 0; }
void BtlController_EmitChooseAction(u8 bufferId, u8 action, u16 itemId) {}
void BtlController_EmitChooseItem(u8 bufferId, u8 *arg1) {}
void BtlController_EmitChooseMove(u8 bufferId, bool8 isDoubleBattle, bool8 NoPpNumber, struct ChooseMoveStruct *movePpData) {}
void BtlController_EmitChoosePokemon(u8 bufferId, u8 caseId, u8 arg2, u8 abilityId, u8 *arg4) {}
void BtlController_EmitDrawPartyStatusSummary(u8 bufferId, struct HpAndStatus *hpAndStatus, u8 param) {}
void BtlController_EmitDrawTrainerPic(u8 bufferId) {}
void BtlController_EmitEndBounceEffect(u8 bufferId) {}
void BtlController_EmitGetMonData(u8 bufferId, u8 requestId, u8 monToCheck) {}
void BtlController_EmitIntroSlide(u8 bufferId, u8 terrainId) {}
void BtlController_EmitIntroTrainerBallThrow(u8 bufferId) {}
void BtlController_EmitLinkStandbyMsg(u8 bufferId, u8 mode) {}
void BtlController_EmitLoadMonSprite(u8 bufferId) {}
void CancelMultiTurnMoves(u8 battler) {}
void ClearFuryCutterDestinyBondGrudge(u8 battlerId) {}
u8 DoBattlerEndTurnEffects(void) { return 0; }
u8 DoFieldEndTurnEffects(void) { return 0; }
void FreeBattleResources(void) {}
u8 GetBattlerAtPosition(u8 position) { return 0; }
#define GET_BATTLER_SIDE(battler)((GetBattlerPosition(battler) & BIT_SIDE)) { return 0; }
u8 GetBattlerSide(u8 battlerId) { return 0; }
u8 GetBattlerSpriteCoord(u8 battlerId, u8 coordType) { return 0; }
u8 GetMoveTarget(u16 move, u8 setTarget) { return 0; }
void HandleAction_RunBattleScript(void) {}
bool8 HandleFaintedMonActions(void) { return 0; }
void HandleLinkBattleSetup(void) {}
bool8 HandleWishPerishSongOnTurnEnd(void) { return 0; }
void InitAnimArcTranslation(struct Sprite *sprite) {}
void InitBattleControllers(void) { BattleStubHit("InitBattleControllers"); }
bool8 IsDoubleBattle(void) { return 0; }
u8 ItemBattleEffects(u8 caseID, u8 battlerId, bool8 moveTurn) { return 0; }
void MarkBattlerForControllerExec(u8 battlerId) {}
void PrepareStringBattle(u16 stringId, u8 battler) {}
void ResetSentPokesToOpponentValue(void) {}
void SetUpBattleVars(void) {}
bool8 TranslateAnimHorizontalArc(struct Sprite *sprite) { return 0; }
void TryClearRageStatuses(void) {}
u8 TrySetCantSelectMoveBattleScript(void) { return 0; }
void UpdateSentPokesToOpponentValue(u8 battler) {}

/* ---- final ---- */
u8 AbilityBattleEffects(u8 caseID, u8 battler, u8 ability, u8 special, u16 moveArg) { (void)caseID; (void)battler; (void)ability; (void)special; (void)moveArg; return 0; }
u8 GetBattlerPosition(u8 battlerId) { (void)battlerId; return 0; }
void (* const gBattleScriptingCommandsTable[])(void) = { NULL };

/* ---- new_game dependencies and stubs ---- */
#define SCRIPT_SETFLAG(f) 0x29, (u8)((f) & 0xFF), (u8)(((f) >> 8) & 0xFF)
#define SCRIPT_SETVAR(v, val) 0x1A, (u8)((v) & 0xFF), (u8)(((v) >> 8) & 0xFF), (u8)((val) & 0xFF), (u8)(((val) >> 8) & 0xFF)
#define SCRIPT_END 0x02

const u8 EventScript_ResetAllMapFlags[] = {
    SCRIPT_SETFLAG(FLAG_HIDE_OAK_IN_HIS_LAB),
    SCRIPT_SETFLAG(FLAG_HIDE_OAK_IN_PALLET_TOWN),
    SCRIPT_SETFLAG(FLAG_HIDE_BILL_HUMAN_SEA_COTTAGE),
    SCRIPT_SETFLAG(FLAG_HIDE_PEWTER_CITY_RUNNING_SHOES_GUY),
    SCRIPT_SETFLAG(FLAG_HIDE_POKEHOUSE_FUJI),
    SCRIPT_SETFLAG(FLAG_HIDE_LIFT_KEY),
    SCRIPT_SETFLAG(FLAG_HIDE_SILPH_SCOPE),
    SCRIPT_SETFLAG(FLAG_HIDE_CERULEAN_RIVAL),
    SCRIPT_SETFLAG(FLAG_HIDE_SS_ANNE_RIVAL),
    SCRIPT_SETFLAG(FLAG_HIDE_VERMILION_CITY_OAKS_AIDE),
    SCRIPT_SETFLAG(FLAG_HIDE_SAFFRON_CIVILIANS),
    SCRIPT_SETFLAG(FLAG_HIDE_ROUTE_22_RIVAL),
    SCRIPT_SETFLAG(FLAG_HIDE_OAK_IN_CHAMP_ROOM),
    SCRIPT_SETFLAG(FLAG_HIDE_CREDITS_RIVAL),
    SCRIPT_SETFLAG(FLAG_HIDE_CREDITS_OAK),
    SCRIPT_SETFLAG(FLAG_HIDE_CINNABAR_BILL),
    SCRIPT_SETFLAG(FLAG_HIDE_CINNABAR_SEAGALLOP),
    SCRIPT_SETFLAG(FLAG_HIDE_CINNABAR_POKECENTER_BILL),
    SCRIPT_SETFLAG(FLAG_HIDE_LORELEI_IN_HER_HOUSE),
    SCRIPT_SETFLAG(FLAG_HIDE_SAFFRON_FAN_CLUB_BLACK_BELT),
    SCRIPT_SETFLAG(FLAG_HIDE_SAFFRON_FAN_CLUB_ROCKER),
    SCRIPT_SETFLAG(FLAG_HIDE_SAFFRON_FAN_CLUB_WOMAN),
    SCRIPT_SETFLAG(FLAG_HIDE_SAFFRON_FAN_CLUB_BEAUTY),
    SCRIPT_SETFLAG(FLAG_HIDE_TWO_ISLAND_GAME_CORNER_LOSTELLE),
    SCRIPT_SETFLAG(FLAG_HIDE_TWO_ISLAND_GAME_CORNER_BIKER),
    SCRIPT_SETFLAG(FLAG_HIDE_TWO_ISLAND_WOMAN),
    SCRIPT_SETFLAG(FLAG_HIDE_TWO_ISLAND_BEAUTY),
    SCRIPT_SETFLAG(FLAG_HIDE_TWO_ISLAND_POKE_MANIAC),
    SCRIPT_SETFLAG(FLAG_HIDE_LOSTELLE_IN_HER_HOME),
    SCRIPT_SETFLAG(FLAG_HIDE_THREE_ISLAND_LONE_BIKER),
    SCRIPT_SETFLAG(FLAG_HIDE_FOUR_ISLAND_RIVAL),
    SCRIPT_SETFLAG(FLAG_HIDE_DOTTED_HOLE_SCIENTIST),
    SCRIPT_SETFLAG(FLAG_HIDE_RESORT_GORGEOUS_SELPHY),
    SCRIPT_SETFLAG(FLAG_HIDE_RESORT_GORGEOUS_INSIDE_SELPHY),
    SCRIPT_SETFLAG(FLAG_HIDE_SELPHYS_BUTLER),
    SCRIPT_SETFLAG(FLAG_HIDE_DEOXYS),
    SCRIPT_SETFLAG(FLAG_HIDE_LORELEI_HOUSE_MEOWTH_DOLL),
    SCRIPT_SETFLAG(FLAG_HIDE_LORELEI_HOUSE_CHANSEY_DOLL),
    SCRIPT_SETFLAG(FLAG_HIDE_LORELEIS_HOUSE_NIDORAN_F_DOLL),
    SCRIPT_SETFLAG(FLAG_HIDE_LORELEI_HOUSE_JIGGLYPUFF_DOLL),
    SCRIPT_SETFLAG(FLAG_HIDE_LORELEIS_HOUSE_NIDORAN_M_DOLL),
    SCRIPT_SETFLAG(FLAG_HIDE_LORELEIS_HOUSE_FEAROW_DOLL),
    SCRIPT_SETFLAG(FLAG_HIDE_LORELEIS_HOUSE_PIDGEOT_DOLL),
    SCRIPT_SETFLAG(FLAG_HIDE_LORELEIS_HOUSE_LAPRAS_DOLL),
    SCRIPT_SETFLAG(FLAG_HIDE_POSTGAME_GOSSIPERS),
    SCRIPT_SETFLAG(FLAG_HIDE_FAME_CHECKER_ERIKA_JOURNALS),
    SCRIPT_SETFLAG(FLAG_HIDE_FAME_CHECKER_KOGA_JOURNAL),
    SCRIPT_SETFLAG(FLAG_HIDE_FAME_CHECKER_LT_SURGE_JOURNAL),
    SCRIPT_SETFLAG(FLAG_HIDE_SAFFRON_CITY_POKECENTER_SABRINA_JOURNALS),
    SCRIPT_SETVAR(VAR_MASSAGE_COOLDOWN_STEP_COUNTER, 500),
    SCRIPT_END
};


void ResetPokemonStorageSystem(void)
{
    u16 boxId, boxPosition;

    SetCurrentBox(0);
    for (boxId = 0; boxId < TOTAL_BOXES_COUNT; boxId++)
    {
        for (boxPosition = 0; boxPosition < IN_BOX_COUNT; boxPosition++)
            ZeroBoxMonAt(boxId, boxPosition);
    }
    for (boxId = 0; boxId < TOTAL_BOXES_COUNT; boxId++)
    {
        u8 *dest = StringCopy(GetBoxNamePtr(boxId), gText_Box);
        ConvertIntToDecimalStringN(dest, boxId + 1, STR_CONV_MODE_LEFT_ALIGN, 2);
    }

    for (boxId = 0; boxId < TOTAL_BOXES_COUNT; boxId++)
        SetBoxWallpaper(boxId, boxId % (MAX_DEFAULT_WALLPAPER + 1));
}

void ResetFameChecker(void)
{
    u8 i;
    for (i = 0; i < 16; i++)
    {
        gSaveBlock1Ptr->fameChecker[i].pickState = 0;
        gSaveBlock1Ptr->fameChecker[i].flavorTextFlags = 0;
        gSaveBlock1Ptr->fameChecker[i].unk_0_E = 0;
    }
    gSaveBlock1Ptr->fameChecker[0].pickState = 1;
}

void ClearEnigmaBerries(void) {}
void ClearMysteryGift(void) {}
void ClearPlayerLinkBattleRecords(void) {}
void ResetTrainerFanClub(void) {}
void UnionRoomChat_InitializeRegisteredTexts(void) {}
void ResetTrainerTowerResults(void) {}
void ResetPokemonJumpRecords(void) {}
void ResetBagCursorPositions(void) {}
void ResetTMCaseCursorPos(void) {}
void BerryPouch_CursorResetToTop(void) {}
void ResetQuestLog(void) {}
void InitEasyChatPhrases(void) {}
void NewGameInitPCItems(void) { AddPCItem(ITEM_POTION, 1); }
void ApplyNewEncryptionKeyToBerryPowder(u32 key) { (void)key; }
void QL_AddASLROffset(void *oldSaveBlockPtr) { (void)oldSaveBlockPtr; }
void QuestLogSetFlagOrVar(bool8 isFlag, u16 idx, u16 value) { (void)isFlag; (void)idx; (void)value; }
void SetQuestLogEvent(u16 eventId, const u16 *data) { (void)eventId; (void)data; }
u16 GetStarterSpecies(void) { return SPECIES_BULBASAUR; }
