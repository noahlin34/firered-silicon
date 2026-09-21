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


/* Safari Zone, Teachy TV, and battle-menu transitions are unlinked. */
void CB2_ReturnToTeachyTV(void) {}
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


/* Easy Chat screen UI is a peripheral path; the word/string helpers are real
   now that src/easy_chat.c is linked. */
void DoEasyChatScreen(u8 type, u16 *words, MainCallback callback)
{
    (void)type;
    (void)words;
    (void)callback;
}




/* The item-use and level-up animation scene (src/pokemon_special_anim_scene.c)
 * is linked, so the 27 stubs that used to live here — the PSA_* helpers,
 * InitPokemonSpecialAnimScene, CreateItemIconSpriteAtMaxCloseness,
 * CreateLevelUpVerticalSpritesTask, DrawLevelUpWindowPg1/2,
 * LevelUpVerticalSpritesTaskIsRunning and
 * PokemonSpecialAnimSceneInitIsNotFinished — are deleted (fix #8). Leaving them
 * would be a duplicate symbol, and worse: they made the scene advance without
 * drawing. */

/* DexScreen_RegisterMonToPokedex is the real src/pokedex_screen.c body (linked);
 * the always-0 stub that used to live here is deleted (fix #8). */
/* Still unlinked: src/help_message.c. */
void DrawHelpMessageWindowTilesById(u8 windowId)
{
    (void)windowId;
}


/* Union Room, trade, link, and field-move helpers are deliberately inert. */
struct RfuGameData *GetHostRfuGameData(void) { return NULL; }

/* The trade menu is an unlinked link peripheral. src/pokemon_summary_screen.c
 * only ever COMPARES savedCallback against this address (to draw the partner's
 * summary during a trade), and nothing hands it over while trade.c is unlinked,
 * so an inert body of the declared type is honest here. */
void CB2_ReturnToTradeMenuFromSummary(void) {}

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


bool32 IsSpeciesAllowedInPokemonJump(u16 species)
{
    (void)species;
    return FALSE;
}
void Mailbox_ReturnToMailListAfterDeposit(void) {}

bool32 ReadTrainerTowerAndValidate(void) { return FALSE; }
void ReducePlayerPartyToThree(void) {}

/* Audio is entirely stubbed in the native platform layer. */

void SetTeachyTvControllerModeToResume(void) {}

/* SetUpFieldMove_Flash is the one that stays: flash is not a FLDEFF_USE_*
 * effect (there is no such id and no animateflash/setflashlevel command in
 * tools/gen_map_data.py), and the real body sits behind #ifndef PORTABLE in the
 * already-linked src/fldeff_flash.c. The other seven come from the
 * src/fldeff_{cut,dig,rocksmash,strength,teleport,sweetscent,softboiled}.c
 * sources linked with them (fix #8). */
bool8 SetUpFieldMove_Flash(void) { return FALSE; }

/* ShowPokemonSummaryScreen/ShowSelectMovePokemonSummaryScreen used to be inert
 * stubs here: opening POKéMON in the START menu, choosing a mon and picking
 * SUMMARY set the party menu's exitCallback to CB2_ShowPokemonSummaryScreen,
 * which called the stub and installed nothing, so the screen faded to black and
 * stayed there. src/pokemon_summary_screen.c is linked now (PREPROC_SRCS). */


u8 TrySavingData(u8 saveType) { (void)saveType; return 0; }

struct PlayerPCItemPageStruct gPlayerPcMenuManager = {0};
struct RfuGameCompatibilityData gRfuPartnerCompatibilityData = {0};
u16 gUnionRoomOfferedSpecies = 0;
u8 gUnionRoomRequestedMonType = 0;
