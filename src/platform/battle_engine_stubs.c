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
// Real battle-animation descriptors also used by Pikachu and Oak's platform.
struct PokedudeBattlerState *gPokedudeBattlerStates[MAX_BATTLERS_COUNT];
bool8 gReceivedRemoteLinkPlayers;
struct PokemonStorageSystemData *gStorage;
u16 gTrainerBattleOpponent_A;

/* ---- functions ---- */
void AnimateBoxScrollArrows(bool8 species) { (void)species; }
void BackupHelpContext(void) {}
void BeginEvolutionScene(struct Pokemon* mon, u16 speciesToEvolve, u8, u8 partyId) {}
u8 BitmaskAllOtherLinkPlayers(void) { return 0; }
void CheckShouldAdvanceLinkState(void) {}
void CommitQuestLogWindow1(void) {}
u8 ContextNpcGetTextColor(void) { return 0; }
u8 CountPartyAliveNonEggMonsExcept(u8 slotToIgnore) { return 0; }
u8 CountPartyMons(void) { return 0; }
void CreateBoxMonIconAtPos(u8 boxPosition) {}
u8 CreateHelpMessageWindow(void) { return 0; }
void CreateMovingMonIcon(void) {}
void CreateWirelessStatusIndicatorSprite(u8, u8) {}
void DestroyHelpMessageWindow(u8 a0) {}
void DestroyMovingMonIcon(void) {}
void DestroyPartyMonIcon(u8 partyId) {}
void DestroyReleaseMonIcon(void) {}
void DestroyTask_RfuIdle(void) {}
void DoReleaseMonAnim(u8 mode, u8 position) {}
void EvolutionScene(struct Pokemon* mon, u16 speciesToEvolve, u8, u8 partyId) {}
u16 FontFunc_Braille(struct TextPrinter *textPrinter) { return 0; }
u8 GetBlockReceivedStatus(void) { return 0; }
s16 GetFirstFreeBoxSpot(u8 boxId) { return 0; }
u8 GetLastViewedMonIndex(void) { return 0; }
u8 GetLinkPlayerCount_2(void) { return 0; }
u8 GetMultiplayerId(void) { return 0; }
u16 GetPCBoxToSendMon(void) { return 0; }
bool32 InUnionRoom(void) { return 0; }
bool8 IsActiveItemMoving(void) { return 0; }
bool8 IsBGMPlaying(void) { return 0; }
/* Audio engine (m4a_1.s) is not ported; cries never start, so "finished"
 * must report TRUE or Task_OakSpeech_IsInhabitedFarAndWide spins forever. */
bool8 IsCryFinished(void) { return TRUE; }
bool8 IsCryPlaying(void) { return FALSE; }
bool8 IsCryPlayingOrClearCrySongs(void) { return FALSE; }
bool8 IsDestinationBoxFull(void) { return 0; }
bool8 IsItemIconAnimActive(void) { return 0; }
bool32 IsLinkRecvQueueAtOverworldMax(void) { return 0; }
bool8 IsLinkRfuTaskFinished(void) { return 0; }
u8 ListMenuAddCursorObjectInternal(const struct CursorStruct *cursor, u32 cursorKind) { return 0; }
void ListMenuRemoveCursorObject(u8 taskId, u32 cursorKind) {}
void ListMenuUpdateCursorObject(u8 taskId, u16 x, u16 y, u32 cursorKind) {}
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
void ResetBlockReceivedFlag(u8) {}
void ResetBlockReceivedFlags(void) {}
void ResetMapMusic(void) {}
void RestoreHelpContext(void) {}
bool8 SendBlock(u8, const void *, u16) { return 0; }
void SetCloseLinkCallback(void) {}
void SetControllerToLinkOpponent(void) {}
void SetControllerToLinkPartner(void) {}
void SetControllerToSafari(void) {}
void SetLinkStandbyCallback(void) {}
void SetMovingMonPriority(u8 priority) {}
void SetMovingMonSprite(u8 cursorArea, u8 cursorPos) {}
void SetPCBoxToSendMon(u8) {}
void SetPlacedMonSprite(u8 cursorArea, u8 cursorPos) {}
void SetShiftMonSpritePtr(u8 boxId, u8 position) {}
void SetWirelessCommType1(void) {}
bool8 ShiftMons(void) { return 0; }
void StopCryAndClearCrySongs(void) {}
void Task_WaitForLinkPlayerConnection(u8 taskId) {}
void TryHideItemIconAtPos(u8 cursorArea, u8 cursorPos) {}
bool8 TryHideReleaseMonSprite(void) { return 0; }
void TryLoadItemIconAtPos(u8 cursorArea, u8 cursorPos) {}
void TrySetQuestLogBattleEvent(void) {}
void TrySetQuestLogLinkBattleEvent(void) {}
void m4aMPlayVolumeControl(struct MusicPlayerInfo *mplayInfo, u16 trackBits, u16 volume) {}


/* ---- round-2 stubs ---- */
const u8 gBattleText_GetPumped[];
const u8 gBattleText_MistShroud[];
const u8 gBattleText_Rose[];
/* Real type, not a `const u8[]`: battle_main.c and party_menu.c assign the
 * post-battle main callback into this variable, so a read-only byte array both
 * mismatches the declaration in include/evolution_scene.h and faults on write
 * the first time a battle finishes. */
void (*gCB2_AfterEvolution)(void) = NULL;
const struct MonCoords gCastformFrontSpriteCoords[];
const u8 *const gStatNamesTable[];
#define ABILITY_ON_OPPOSING_FIELD(battlerId, abilityId)(AbilityBattleEffects(ABILITYEFFECT_CHECK_OTHER_SIDE, battlerId, abilityId, 0, 0)) { return 0; }
#define GET_BATTLER_SIDE(battler)((GetBattlerPosition(battler) & BIT_SIDE)) { return 0; }

/* ---- final ---- */

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

void ClearMysteryGift(void) {}
void ClearPlayerLinkBattleRecords(void) {}
void ResetTrainerFanClub(void) {}
void UnionRoomChat_InitializeRegisteredTexts(void) {}
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
u16 GetStarterSpecies(void) { return SPECIES_BULBASAUR; }
