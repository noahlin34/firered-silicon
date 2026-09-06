#include "global.h"

#include "battle.h"

#include "battle_scripts.h"

#include "data.h"

#include "util.h"

#include "task.h"

#include "sprite.h"

#include "main.h"

#include "menu.h"

#include "constants/battle_move_effects.h"
#include "link.h"

/* Auto-generated battle-engine stubs: the GBA battle engine (assembly scripts +
 * battle controllers) is not linked in the native port. Only data symbols and
 * entry points referenced by non-battle scenes are provided. */


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
struct PokemonStorage* gPokemonStoragePtr;
bool8 gReceivedRemoteLinkPlayers;
u16 gSpecialVar_0x8004;
u16 gSpecialVar_0x8005;
u16 gSpecialVar_0x8006;
u16 gSpecialVar_MonBoxId;
u16 gSpecialVar_MonBoxPos;
u16 gSpecialVar_Result;
struct PokemonStorageSystemData *gStorage;
const u8 gText_NamingScreenKeyboard_01234[];
const u8 gText_NamingScreenKeyboard_56789[];
const u8 gText_NamingScreenKeyboard_ABCDEF[];
const u8 gText_NamingScreenKeyboard_GHIJKL[];
const u8 gText_NamingScreenKeyboard_MNOPQRS[];
const u8 gText_NamingScreenKeyboard_Symbols1[];
const u8 gText_NamingScreenKeyboard_Symbols2[];
const u8 gText_NamingScreenKeyboard_TUVWXYZ[];
const u8 gText_NamingScreenKeyboard_abcdef[];
const u8 gText_NamingScreenKeyboard_ghijkl[];
const u8 gText_NamingScreenKeyboard_mnopqrs[];
const u8 gText_NamingScreenKeyboard_tuvwxyz[];
u16 gTrainerBattleOpponent_A;

/* ---- functions ---- */
void AllocateBattleSpritesData(void) { }
void AllocateMonSpritesGfx(void) { }
u8 AnimateBallOpenParticles(u8 x, u8 y, u8 priority, u8 subpriority, u8 ballId) { return 0; }
void AnimateBoxScrollArrows(bool8 species) { (void)species; }
void BackupHelpContext(void) {}
void BattleAI_HandleItemUseBeforeAISetup(void) {}
s8 BattleAnimAdjustPanning(s8 pan) { return 0; }
void BattleControllerDummy(void) {}
bool8 BattleInitAllSprites(u8 *state, u8 *battlerId) { return 0; }
void BattleInterfaceSetWindowPals(void) {}
u8 BattleSetup_GetTerrainId(void) { return 0; }
void BattleStopLowHpSound(void) {}
void BeginEvolutionScene(struct Pokemon* mon, u16 speciesToEvolve, u8, u8 partyId) {}
u8 BitmaskAllOtherLinkPlayers(void) { return 0; }
void BufferBattlePartyCurrentOrderBySide(u8 battlerId, u8 flankId) {}
void CB2_NewGame(void) {}
s16 CalculatePanIncrement(s16 sourcePan, s16 targetPan, s16 incrementPan) { return 0; }
void CheckShouldAdvanceLinkState(void) {}
void ClearBattleAnimationVars(void) {}
void ClearRematchStateByTrainerId(void) {}
void ClearStdWindowAndFrameToTransparent(u8 windowId, bool8 copyToVram) {}
void ClearTemporarySpeciesSpriteData(u8 battlerId, bool8 dontClearSubstitute) {}
void ClearTopBarWindow(void) {}
void CommitQuestLogWindow1(void) {}
u8 ContextNpcGetTextColor(void) { return 0; }
void CopyEReaderTrainerName5(u8 *dest) {}
void CopyItemName(u16 itemId, u8 *string) {}
void CopyObjectGraphicsInfoToSpriteTemplate(u16 graphicsId, void (*callback)(struct Sprite *), struct SpriteTemplate *spriteTemplate, const struct SubspriteTable **subspriteTables) { (void)graphicsId; (void)callback; (void)spriteTemplate; (void)subspriteTables; }
u8 CountPartyAliveNonEggMonsExcept(u8 slotToIgnore) { return 0; }
u8 CountPartyMons(void) { return 0; }
void CreateBoxMonIconAtPos(u8 boxPosition) {}
u8 CreateHelpMessageWindow(void) { return 0; }
void CreateMovingMonIcon(void) {}
u8 CreateObjectGraphicsSprite(u16 graphicsId, SpriteCallback callback, s16 x, s16 y, u8 subpriority) { return 0; }
u8 CreateTopBarWindowLoadPalette(u8 bg, u8 width, u8 yPos, u8 palette, u16 baseTile) { return 0; }
void CreateWirelessStatusIndicatorSprite(u8, u8) {}
void CreateYesNoMenu(const struct WindowTemplate *window, u8 fontId, u8 left, u8 top, u16 baseTileNum, u8 paletteNum, u8 initialCursorPos) {}
void DestroyAnimSoundTask(u8 taskId) {}
void DestroyAnimSprite(struct Sprite *sprite) {}
void DestroyAnimVisualTask(u8 taskId) {}
void DestroyHelpMessageWindow(u8 a0) {}
void DestroyMovingMonIcon(void) {}
void DestroyPartyMonIcon(u8 partyId) {}
void DestroyReleaseMonIcon(void) {}
void DestroyTask_RfuIdle(void) {}
void DestroyTopBarWindow(void) {}
void DoReleaseMonAnim(u8 mode, u8 position) {}
void DrawBattleEntryBackground(void) {}
void DrawDialogFrameWithCustomTileAndPalette(u8 windowId, bool8 copyToVram, u16 tileNum, u8 paletteNum) {}
void DrawStdFrameWithCustomTileAndPalette(u8 windowId, bool8 copyToVram, u16 baseTileNum, u8 paletteNum) {}
void EvolutionScene(struct Pokemon* mon, u16 speciesToEvolve, u8, u8 partyId) {}
u8 FlagClear(u16 id) { return 0; }
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
u8 GetCurrentRegionMapSectionId(void) { return 0; }
u8 GetCurrentWeather(void) { return 0; }
u8 GetEreaderTrainerClassId(void) { return 0; }
s16 GetFirstFreeBoxSpot(u8 boxId) { return 0; }
u8 GetLastViewedMonIndex(void) { return 0; }
u8 GetLinkPlayerCount_2(void) { return 0; }
u8 GetMultiplayerId(void) { return 0; }
u16 GetPCBoxToSendMon(void) { return 0; }
u8 GetPartyIdFromBattlePartyId(u8 battlePartyId) { return 0; }
u8 GetRivalAvatarGraphicsIdByStateIdAndGender(u8 state, u8 gender) { return 0; }
u16 GetRivalBattleFlags(void) { return 0; }
s8 GetSetPokedexFlag(u16 nationalNum, u8 caseId) { return 0; }
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
bool8 IsCryFinished(void) { return 0; }
bool8 IsCryPlaying(void) { return 0; }
bool8 IsCryPlayingOrClearCrySongs(void) { return 0; }
bool8 IsDestinationBoxFull(void) { return 0; }
bool32 IsEnigmaBerryValid(void) { return 0; }
bool8 IsItemIconAnimActive(void) { return 0; }
bool32 IsLinkRecvQueueAtOverworldMax(void) { return 0; }
bool8 IsLinkRfuTaskFinished(void) { return 0; }
bool8 IsMsgSignpost(void) { return 0; }
bool32 IsUpdateLinkStateCBActive(void) { return 0; }
u8 ItemIdToBallId(u16 itemId) { return 0; }
u8 ItemIdToBerryType(u16 item) { return 0; }
u8 ItemId_GetHoldEffect(u16 itemId) { return 0; }
u8 ItemId_GetHoldEffectParam(u16 itemId) { return 0; }
const u8 *ItemId_GetName(u16 itemId) { return 0; }
bool8 ItemIsMail(u16 itemId) { return 0; }
s16 KeepPanInRange(s16 a, s32 oldPan) { return 0; }
u8 LaunchBallFadeMonTask(bool8 unFadeLater, u8 battlerId, u32 arg2, u8 ballId) { return 0; }
u8 ListMenuAddCursorObjectInternal(const struct CursorStruct *cursor, u32 cursorKind) { return 0; }
void ListMenuRemoveCursorObject(u8 taskId, u32 cursorKind) {}
void ListMenuUpdateCursorObject(u8 taskId, u16 x, u16 y, u32 cursorKind) {}
void LoadBattleMenuWindowGfx(void) {}
void LoadBattleTextboxAndBackground(void) {}
void LoadWirelessStatusIndicatorSpriteGfx(void) {}
u16 MailSpeciesToSpecies(u16 mailSpecies, u16 *buffer) { return 0; }
void MapNamePopupWindowIdSetDummy(void) {}
u8 Menu_GetCursorPos(void) { return 0; }
u8 Menu_InitCursor(u8 windowId, u8 fontId, u8 left, u8 top, u8 cursorHeight, u8 numChoices, u8 initialCursorPos) { return 0; }
u8 Menu_MoveCursor(s8 cursorDelta) { return 0; }
s8 Menu_ProcessInput(void) { return 0; }
s8 Menu_ProcessInputNoWrapAround(void) { return 0; }
s8 Menu_ProcessInputNoWrapClearOnChoose(void) { return 0; }
void MoveSaveBlocks_ResetHeap(void) {}
bool8 MultiMove_CanPlaceSelection(void) { return 0; }
u8 MultiMove_GetOriginPosition(void) { return 0; }
bool8 MultiMove_TryMoveGroup(u8 dir) { return 0; }
void MultiplyInvertedPaletteRGBComponents(u16 i, u8 r, u8 g, u8 b) {}
bool32 Overworld_LinkRecvQueueLengthMoreThan2(void) { return 0; }
void PlayCry_ByMode(u16 species, s8 pan, u8 mode) {}
void PlayCry_ReleaseDouble(u16 species, s8 pan, u8 mode) {}
void PlayNewMapMusic(u16 songNum) {}
void PlaySE1WithPanning(u16 songNum, s8 pan) {}
void PlaySE2WithPanning(u16 songNum, s8 pan) {}
void PrintTextArray(u8 windowId, u8 fontId, u8 left, u8 top, u8 lineHeight, u8 itemCount, const struct MenuAction *strs) {}
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
void SetRoamerInactive(void) {}
void SetShiftMonSpritePtr(u8 boxId, u8 position) {}
struct WindowTemplate SetWindowTemplateFields(u8 bg, u8 left, u8 top, u8 width, u8 height, u8 paletteNum, u16 baseBlock) { struct WindowTemplate t; t.bg = bg; t.tilemapLeft = left; t.tilemapTop = top; t.width = width; t.height = height; t.paletteNum = paletteNum; t.baseBlock = baseBlock; return t; }
void SetWirelessCommType1(void) {}
bool8 ShiftMons(void) { return 0; }
bool32 ShouldPlayNormalMonCry(struct Pokemon *mon) { return 0; }
void ShowPartyMenuToShowcaseMultiBattleParty(void) {}
void StopCryAndClearCrySongs(void) {}
void SwitchPartyMonSlots(u8 slot, u8 slot2) {}
void Task_WaitForLinkPlayerConnection(u8 taskId) {}
void TopBarWindowPrintString(const u8 *string, u8 unUsed, bool8 copyToVram) {}
void TopBarWindowPrintTwoStrings(const u8 *string, const u8 *string2, bool8 fgColorChooser, u8 notUsed, bool8 copyToVram) {}
void TryHideItemIconAtPos(u8 cursorArea, u8 cursorPos) {}
bool8 TryHideReleaseMonSprite(void) { return 0; }
void TryLoadItemIconAtPos(u8 cursorArea, u8 cursorPos) {}
void TrySetQuestLogBattleEvent(void) {}
void TrySetQuestLogLinkBattleEvent(void) {}
void UpdateRoamerHPStatus(struct Pokemon *mon) {}
bool8 UproarWakeUpCheck(u8 battlerId) { return 0; }
u16 VarGet(u16 id) { return 0; }
bool8 VarSet(u16 id, u16 value) { return 0; }
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
void BattleScriptExecute(const u8 *BS_ptr) {}
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
u32 GetBoxMonDataAt(u8 boxId, u8 monPosition, s32 request) { return 0; }
u8 *GetBoxNamePtr(u8 boxNumber) { return 0; }
struct BoxPokemon *GetBoxedMonPtr(u8 boxId, u8 monPosition) { return 0; }
u8 GetMoveTarget(u16 move, u8 setTarget) { return 0; }
void HandleAction_RunBattleScript(void) {}
bool8 HandleFaintedMonActions(void) { return 0; }
void HandleLinkBattleSetup(void) {}
bool8 HandleWishPerishSongOnTurnEnd(void) { return 0; }
void InitAnimArcTranslation(struct Sprite *sprite) {}
void InitBattleControllers(void) {}
bool8 IsDoubleBattle(void) { return 0; }
u8 ItemBattleEffects(u8 caseID, u8 battlerId, bool8 moveTurn) { return 0; }
void MarkBattlerForControllerExec(u8 battlerId) {}
void PrepareStringBattle(u16 stringId, u8 battler) {}
void ResetSentPokesToOpponentValue(void) {}
void SetUpBattleVars(void) {}
u8 StorageGetCurrentBox(void) { return 0; }
bool8 TranslateAnimHorizontalArc(struct Sprite *sprite) { return 0; }
void TryClearRageStatuses(void) {}
u8 TrySetCantSelectMoveBattleScript(void) { return 0; }
void UpdateSentPokesToOpponentValue(u8 battler) {}

/* ---- final ---- */
u8 AbilityBattleEffects(u8 caseID, u8 battler, u8 ability, u8 special, u16 moveArg) { (void)caseID; (void)battler; (void)ability; (void)special; (void)moveArg; return 0; }
u8 GetBattlerPosition(u8 battlerId) { (void)battlerId; return 0; }
void (* const gBattleScriptingCommandsTable[])(void) = { NULL };
