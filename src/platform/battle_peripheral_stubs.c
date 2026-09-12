#include "global.h"
#include "main.h"

#include "battle_util2.h"
#include "cereader_tool.h"
#include "dynamic_placeholder_text_util.h"
#include "easy_chat.h"
#include "event_scripts.h"
#include "field_specials.h"
#include "fldeff.h"
#include "help_message.h"
#include "item_menu.h"
#include "link_rfu.h"
#include "m4a.h"
#include "pokedex.h"
#include "player_pc.h"
#include "pokemon_jump.h"
#include "pokemon_special_anim.h"
#include "pokemon_special_anim_internal.h"
#include "pokemon_summary_screen.h"
#include "quest_log.h"
#include "region_map.h"
#include "save.h"
#include "sound.h"
#include "start_menu.h"
#include "strings.h"
#include "teachy_tv.h"
#include "trade.h"

#include "constants/pokemon.h"

/*
 * Deliberately unlinked battle-peripheral stubs: the native port links the
 * real battle engine, but not its Safari Zone, tutorial, link, quest-log,
 * summary-screen, field-move, and audio peripherals. These definitions keep
 * those unreachable paths from preventing the native link; they do not
 * replace any normal wild-battle behavior.
 */

/* ---- inert scripts and text used only by unlinked peripheral scenes ---- */
/* EventScript_DoNoIntroTrainerBattle and EventScript_EndQuestLogBattle are now
 * compiled from data/scripts/trainer_battle.inc (see NATIVE_SCRIPT_ROOTS): the
 * early rival battle runs through them, and a `{0x02}` stub ended the script
 * before the battle could start. */
const u8 EventScript_DoTrainerBattleFromApproach[] = { 0x02 };
const u8 EventScript_TryDoDoubleRematchBattle[] = { 0x02 };
const u8 EventScript_TryDoDoubleTrainerBattle[] = { 0x02 };
const u8 EventScript_TryDoNormalTrainerBattle[] = { 0x02 };
const u8 EventScript_TryDoRematchBattle[] = { 0x02 };

const u8 SafariZone_EventScript_OutOfBalls[] = { 0x02 };
const u8 SafariZone_EventScript_OutOfBallsMidBattle[] = { 0x02 };
const u8 SafariZone_EventScript_RetirePrompt[] = { 0x02 };
const u8 SafariZone_EventScript_TimesUp[] = { 0x02 };

const u8 Pokedude_Text_BattlersTakeTurnsAttacking[] = { 0xFF };
const u8 Pokedude_Text_BestIfTargetStatused[] = { 0xFF };
const u8 Pokedude_Text_ButterfreeDoubleResistsGrass[] = { 0xFF };
const u8 Pokedude_Text_ButterfreeGoodAgainstOddish[] = { 0xFF };
const u8 Pokedude_Text_CantDoubleUpOnStatus[] = { 0xFF };
const u8 Pokedude_Text_GrassEffectiveAgainstWater[] = { 0xFF };
const u8 Pokedude_Text_HealStatusRightAway[] = { 0xFF };
const u8 Pokedude_Text_LetMeThrowBall[] = { 0xFF };
const u8 Pokedude_Text_LetsTryShiftingMons[] = { 0xFF };
const u8 Pokedude_Text_MyRattataFasterThanPidgey[] = { 0xFF };
const u8 Pokedude_Text_MyRattataWonGetsEXP[] = { 0xFF };
const u8 Pokedude_Text_PickBestKindOfBall[] = { 0xFF };
const u8 Pokedude_Text_ShiftingUsesTurn[] = { 0xFF };
const u8 Pokedude_Text_SpeedierBattlerGoesFirst[] = { 0xFF };
const u8 Pokedude_Text_UhOhRattataPoisoned[] = { 0xFF };
const u8 Pokedude_Text_UsingItemTakesTurn[] = { 0xFF };
const u8 Pokedude_Text_WaterNotVeryEffectiveAgainstGrass[] = { 0xFF };
const u8 Pokedude_Text_WeakenMonBeforeCatching[] = { 0xFF };
const u8 Pokedude_Text_YayWeManagedToWin[] = { 0xFF };
const u8 Pokedude_Text_YeahWeWon[] = { 0xFF };

const u8 VSSeeker_Text_BatteryNotChargedNeedXSteps[] = { 0xFF };
const u8 VSSeeker_Text_NoTrainersWithinRange[] = { 0xFF };
const u8 VSSeeker_Text_TrainersNotReady[] = { 0xFF };

static const u8 sEmptyNatureName[] = { 0xFF };
const u8 *const gNatureNamePointers[NUM_NATURES] = {
    [NATURE_HARDY] = sEmptyNatureName,
    [NATURE_LONELY] = sEmptyNatureName,
    [NATURE_BRAVE] = sEmptyNatureName,
    [NATURE_ADAMANT] = sEmptyNatureName,
    [NATURE_NAUGHTY] = sEmptyNatureName,
    [NATURE_BOLD] = sEmptyNatureName,
    [NATURE_DOCILE] = sEmptyNatureName,
    [NATURE_RELAXED] = sEmptyNatureName,
    [NATURE_IMPISH] = sEmptyNatureName,
    [NATURE_LAX] = sEmptyNatureName,
    [NATURE_TIMID] = sEmptyNatureName,
    [NATURE_HASTY] = sEmptyNatureName,
    [NATURE_SERIOUS] = sEmptyNatureName,
    [NATURE_JOLLY] = sEmptyNatureName,
    [NATURE_NAIVE] = sEmptyNatureName,
    [NATURE_MODEST] = sEmptyNatureName,
    [NATURE_MILD] = sEmptyNatureName,
    [NATURE_QUIET] = sEmptyNatureName,
    [NATURE_BASHFUL] = sEmptyNatureName,
    [NATURE_RASH] = sEmptyNatureName,
    [NATURE_CALM] = sEmptyNatureName,
    [NATURE_GENTLE] = sEmptyNatureName,
    [NATURE_SASSY] = sEmptyNatureName,
    [NATURE_CAREFUL] = sEmptyNatureName,
    [NATURE_QUIRKY] = sEmptyNatureName,
};

/* Trainer Tower is a separate e-Reader/link peripheral; keep every lookup safe. */
static const struct TrainerTowerFloor sEmptyTrainerTowerFloor = {0};
const struct EReaderTrainerTowerSetSubstruct gTrainerTowerLocalHeader = {
    .numFloors = MAX_TRAINER_TOWER_FLOORS,
};
const struct TrainerTowerFloor *const gTrainerTowerFloors[NUM_TOWER_CHALLENGE_TYPES][MAX_TRAINER_TOWER_FLOORS] = {
    [CHALLENGE_TYPE_SINGLE] = {
        &sEmptyTrainerTowerFloor, &sEmptyTrainerTowerFloor,
        &sEmptyTrainerTowerFloor, &sEmptyTrainerTowerFloor,
        &sEmptyTrainerTowerFloor, &sEmptyTrainerTowerFloor,
        &sEmptyTrainerTowerFloor, &sEmptyTrainerTowerFloor,
    },
    [CHALLENGE_TYPE_DOUBLE] = {
        &sEmptyTrainerTowerFloor, &sEmptyTrainerTowerFloor,
        &sEmptyTrainerTowerFloor, &sEmptyTrainerTowerFloor,
        &sEmptyTrainerTowerFloor, &sEmptyTrainerTowerFloor,
        &sEmptyTrainerTowerFloor, &sEmptyTrainerTowerFloor,
    },
    [CHALLENGE_TYPE_KNOCKOUT] = {
        &sEmptyTrainerTowerFloor, &sEmptyTrainerTowerFloor,
        &sEmptyTrainerTowerFloor, &sEmptyTrainerTowerFloor,
        &sEmptyTrainerTowerFloor, &sEmptyTrainerTowerFloor,
        &sEmptyTrainerTowerFloor, &sEmptyTrainerTowerFloor,
    },
    [CHALLENGE_TYPE_MIXED] = {
        &sEmptyTrainerTowerFloor, &sEmptyTrainerTowerFloor,
        &sEmptyTrainerTowerFloor, &sEmptyTrainerTowerFloor,
        &sEmptyTrainerTowerFloor, &sEmptyTrainerTowerFloor,
        &sEmptyTrainerTowerFloor, &sEmptyTrainerTowerFloor,
    },
};

/* Battle-only friendship and menu bookkeeping are not linked into the port. */

void AppendToList(u8 *list, u8 *pos, u8 newEntry)
{
    (void)list;
    (void)pos;
    (void)newEntry;
}

/* Safari Zone, Teachy TV, and battle-menu transitions are unlinked. */
void CB2_OpenFlyMap(void) {}
void CB2_ReturnToTeachyTV(void) {}
void CB2_SetUpReshowBattleScreenAfterMenu(void) {}
bool32 CEReaderTool_LoadTrainerTower(struct EReaderTrainerTowerSet *ttdata)
{
    (void)ttdata;
    return FALSE;
}

s32 CanRegisterMonForTradingBoard(struct RfuGameCompatibilityData rfuPlayer,
                                     u16 species2, u16 species, u8 isEventLegal)
{
    (void)rfuPlayer;
    (void)species2;
    (void)species;
    (void)isEventLegal;
    return FALSE;
}

void ChooseMonForSoftboiled(u8 taskId)
{
    (void)taskId;
}

/* Easy Chat and placeholder expansion are peripheral UI paths. */
u8 *ConvertEasyChatWordsToString(u8 *dest, const u16 *src, u16 columns, u16 rows)
{
    (void)dest;
    (void)src;
    (void)columns;
    (void)rows;
    return NULL;
}

void DoEasyChatScreen(u8 type, u16 *words, MainCallback callback)
{
    (void)type;
    (void)words;
    (void)callback;
}

bool8 EC_DoesEasyChatStringFitOnLine(const u16 *easyChatWords, u8 columns, u8 rows, u16 maxLength)
{
    (void)easyChatWords;
    (void)columns;
    (void)rows;
    (void)maxLength;
    return FALSE;
}

u8 *DynamicPlaceholderTextUtil_ExpandPlaceholders(u8 *dest, const u8 *src)
{
    (void)dest;
    (void)src;
    return NULL;
}

void DynamicPlaceholderTextUtil_Reset(void) {}

void DynamicPlaceholderTextUtil_SetPlaceholderPtr(u8 idx, const u8 *ptr)
{
    (void)idx;
    (void)ptr;
}

/* Summary, Pokédex, item-use, and level-up animation scenes are unlinked. */
void CreateItemIconSpriteAtMaxCloseness(u16 itemId)
{
    (void)itemId;
}

void CreateLevelUpVerticalSpritesTask(u16 x, u16 y, u16 tileTag, u16 paletteTag,
                                      u16 priority, u16 subpriority)
{
    (void)x;
    (void)y;
    (void)tileTag;
    (void)paletteTag;
    (void)priority;
    (void)subpriority;
}

u8 DexScreen_RegisterMonToPokedex(u16 species)
{
    (void)species;
    return 0;
}

void DrawHelpMessageWindowTilesById(u8 windowId)
{
    (void)windowId;
}

void DrawLevelUpWindowPg1(u16 windowId, u16 *statsBefore, u16 *statsAfter,
                         u8 bgClr, u8 fgClr, u8 shadowClr)
{
    (void)windowId;
    (void)statsBefore;
    (void)statsAfter;
    (void)bgClr;
    (void)fgClr;
    (void)shadowClr;
}

void DrawLevelUpWindowPg2(u16 windowId, u16 *currStats,
                         u8 bgClr, u8 fgClr, u8 shadowClr)
{
    (void)windowId;
    (void)currStats;
    (void)bgClr;
    (void)fgClr;
    (void)shadowClr;
}

void InitPokemonSpecialAnimScene(struct PokemonSpecialAnimScene *buffer, u16 animType)
{
    (void)buffer;
    (void)animType;
}

bool8 LevelUpVerticalSpritesTaskIsRunning(void) { return FALSE; }
bool8 PokemonSpecialAnimSceneInitIsNotFinished(void) { return FALSE; }
void PSA_AfterPoof_ClearMessageWindow(void) {}
void PSA_CreateMonSpriteAtCloseness(u8 closeness) { (void)closeness; }
void PSA_DarkenMonSprite(void) {}
void PSA_FreeWindowBuffers(void) {}
void PSA_HideMessageWindow(void) {}
bool8 PSA_IsItemUseOnMonAnimActive(void) { return FALSE; }
bool8 PSA_IsMessagePrintTaskActive(void) { return FALSE; }
bool8 PSA_IsZoomTaskActive(void) { return FALSE; }
bool8 PSA_LevelUpVerticalSpritesTaskIsRunning(void) { return FALSE; }
void PSA_PrintMessage(u8 messageId) { (void)messageId; }
bool8 PSA_RunPoofAnim(void) { return FALSE; }
void PSA_SetUpItemUseOnMonAnim(u16 itemId, u8 closeness, bool32 a2)
{
    (void)itemId;
    (void)closeness;
    (void)a2;
}
void PSA_SetUpZoomAnim(u8 closeness) { (void)closeness; }
void PSA_ShowMessageWindow(void) {}
void PSA_UseItem_CleanUpForCancel(void) {}
void PSA_UseTM_CleanUpForCancel(void) {}
bool8 PSA_UseTM_RunMachineSetWobble(void) { return FALSE; }
bool8 PSA_UseTM_RunZoomOutAnim(void) { return FALSE; }
void PSA_UseTM_SetUpMachineSetWobble(void) {}
void PSA_UseTM_SetUpZoomOutAnim(void) {}

/* Union Room, trade, link, and field-move helpers are deliberately inert. */
struct RfuGameData *GetHostRfuGameData(void) { return NULL; }
u8 *GetMapNameGeneric(u8 *dest, u16 mapsec)
{
    (void)dest;
    (void)mapsec;
    return NULL;
}

u8 GetMoveSlotToReplace(void) { return 0; }

s32 GetUnionRoomTradeMessageId(struct RfuGameCompatibilityData rfuPlayer,
                               struct RfuGameCompatibilityData rfuPartner,
                               u16 playerSpecies2, u16 partnerSpecies,
                               u8 requestedType, u16 playerSpecies, u8 isEventLegal)
{
    (void)rfuPlayer;
    (void)rfuPartner;
    (void)playerSpecies2;
    (void)partnerSpecies;
    (void)requestedType;
    (void)playerSpecies;
    (void)isEventLegal;
    return 0;
}

void GoToBagMenu(u8 menuType, u8 pocket, MainCallback callback)
{
    (void)menuType;
    (void)pocket;
    (void)callback;
}

void InitOldManBag(void) {}
void InitPokedudeBag(u8 a0) { (void)a0; }
bool32 IsSpeciesAllowedInPokemonJump(u16 species)
{
    (void)species;
    return FALSE;
}
void Mailbox_ReturnToMailListAfterDeposit(void) {}

void QL_FinishRecordingScene(void) {}
bool8 QL_IsRoomToSaveAction(const void *cursor, size_t size)
{
    (void)cursor;
    (void)size;
    return FALSE;
}
bool8 QL_IsRoomToSaveEvent(const void *cursor, size_t size)
{
    (void)cursor;
    (void)size;
    return FALSE;
}
void QL_StartRecordingAction(u16 eventId) { (void)eventId; }
bool32 ReadTrainerTowerAndValidate(void) { return FALSE; }
void ReducePlayerPartyToThree(void) {}

/* Audio is entirely stubbed in the native platform layer. */
struct MusicPlayerInfo gMPlayInfo_SE1 = {0};
struct MusicPlayerInfo gMPlayInfo_SE2 = {0};
void SE12PanpotControl(s8 pan) { (void)pan; }

void SetTeachyTvControllerModeToResume(void) {}

bool8 SetUpFieldMove_Cut(void) { return FALSE; }
bool8 SetUpFieldMove_Dig(void) { return FALSE; }
bool8 SetUpFieldMove_Flash(void) { return FALSE; }
bool8 SetUpFieldMove_RockSmash(void) { return FALSE; }
bool8 SetUpFieldMove_SoftBoiled(void) { return FALSE; }
bool8 SetUpFieldMove_Strength(void) { return FALSE; }
bool8 SetUpFieldMove_SweetScent(void) { return FALSE; }
bool8 SetUpFieldMove_Teleport(void) { return FALSE; }
bool8 ShouldShowBoxWasFullMessage(void) { return FALSE; }

void ShowPokemonSummaryScreen(struct Pokemon *party, u8 cursorPos, u8 lastIdx,
                              void (*callback)(void), u8 a4)
{
    (void)party;
    (void)cursorPos;
    (void)lastIdx;
    (void)callback;
    (void)a4;
}

void ShowSelectMovePokemonSummaryScreen(struct Pokemon *party, u8 cursorPos,
                                        u8 lastIdx, MainCallback callback, u16 a4)
{
    (void)party;
    (void)cursorPos;
    (void)lastIdx;
    (void)callback;
    (void)a4;
}

void TV_PrintIntToStringVar(u8 varidx, s32 number)
{
    (void)varidx;
    (void)number;
}

void Task_TryUseSoftboiledOnPartyMon(u8 taskId) { (void)taskId; }
u8 TrySavingData(u8 saveType) { (void)saveType; return 0; }

struct PlayerPCItemPageStruct gPlayerPcMenuManager = {0};
u16 *gQuestLogDefeatedWildMonRecord = NULL;
u16 *gQuestLogRecordingPointer = NULL;
u16 gQuestLogCurActionIdx = 0;
struct RfuGameCompatibilityData gRfuPartnerCompatibilityData = {0};
u16 gUnionRoomOfferedSpecies = 0;
u8 gUnionRoomRequestedMonType = 0;
