// Dev boot: spawn in the player's bedroom on a fresh save, skipping the title
// screen, main menu, controls guide, Oak's speech, and both naming screens.
//
// The save initialization mirrors SetTitleScreenScene_Cry (src/title_screen.c),
// and the identity written below is what the naming screens would have left
// behind: the naming screen's first choices (RED / GREEN, male). The final
// hand-off is CB2_NewGame, exactly where Oak's farewell dialogue ends
// (src/oak_speech.c, Task_OakSpeech_ShrinkPlayerPic).
//
// --post-rival goes one scene further: the starter is already in the party and
// the lab's first rival battle is already won, so the save lands in the state
// PalletTown_ProfessorOaksLab_EventScript_EndRivalBattle leaves behind (scene
// var 4, FLAG_BEAT_RIVAL_IN_OAKS_LAB) with the player free to walk out of the
// lab. Platform_DevBootApplyStoryProgress writes exactly what those scripts
// write; keep the two in sync if the starter scene ever changes.

#include "global.h"
#include "gba/m4a_internal.h"
#include "battle_setup.h"
#include "event_data.h"
#include "event_scripts.h"
#include "item.h"
#include "load_save.h"
#include "malloc.h"
#include "new_game.h"
#include "overworld.h"
#include "save.h"
#include "save_location.h"
#include "script_pokemon_util.h"
#include "string_util.h"
#include "constants/flags.h"
#include "constants/items.h"
#include "constants/maps.h"
#include "constants/opponents.h"
#include "constants/species.h"
#include "constants/vars.h"
#include "platform/platform.h"

bool gPlatformSkipIntro = false;
bool gPlatformSkipStory = false;
bool gPlatformDexObtained = false;

static void CB2_DevBootPostRival(void);

void Platform_DevBootNewGame(void)
{
    SeedRngAndSetTrainerId();
    SetSaveBlocksPointers();
    ResetMenuAndMonGlobals();
    Save_ResetSaveCounters();
    LoadGameSave(SAVE_NORMAL);
    if (gSaveFileStatus == SAVE_STATUS_EMPTY || gSaveFileStatus == SAVE_STATUS_INVALID)
        Sav2_ClearSetDefault();

    // Set the identity Oak's speech would have collected. NewGameInitData
    // preserves rivalName across its own ClearSav1; playerName/playerGender
    // live in SaveBlock2 and survive untouched.
    gSaveBlock2Ptr->playerGender = MALE;
    StringCopy(gSaveBlock2Ptr->playerName, gNameChoice_Red);
    StringCopy(gSaveBlock1Ptr->rivalName, gNameChoice_Green);

    SetPokemonCryStereo(gSaveBlock2Ptr->optionsSound);
    InitHeap(gHeap, HEAP_SIZE);
    SetMainCallback2(gPlatformSkipStory ? CB2_DevBootPostRival : CB2_NewGame);
}

// Reproduces the save state at the end of the starter scene: the player picked
// Bulbasaur, the rival took the counter-pick Charmander, and the rival battle
// has already been won. Values come from PalletTown_ProfessorOaksLab's
// EventScript_ChoseStarter / EventScript_RivalTakesStarter / EndRivalBattle.
void Platform_DevBootApplyStoryProgress(void)
{
    // givemon PLAYER_STARTER_SPECIES, 5 + special HealPlayerParty
    ScriptGiveMon(SPECIES_BULBASAUR, 5, ITEM_NONE, 0, 0, 0);
    HealPlayerParty();

    FlagSet(FLAG_SYS_POKEMON_GET);
    FlagSet(FLAG_PALLET_LADY_NOT_BLOCKING_SIGN);
    FlagSet(FLAG_VISITED_OAKS_LAB);
    FlagSet(FLAG_HIDE_OAK_IN_PALLET_TOWN);   // Oak walked the player to the lab
    FlagClear(FLAG_HIDE_OAK_IN_HIS_LAB);     // ...and is standing in it
    FlagSet(FLAG_HIDE_RIVAL_IN_LAB);         // rival exited after the battle
    FlagSet(FLAG_HIDE_BULBASAUR_BALL);       // player's ball: removeobject
    FlagSet(FLAG_HIDE_CHARMANDER_BALL);      // rival's counter-pick: removeobject
    FlagSet(FLAG_BEAT_RIVAL_IN_OAKS_LAB);
    SetTrainerFlag(TRAINER_RIVAL_OAKS_LAB_CHARMANDER);

    VarSet(VAR_STARTER_MON, 0);                              // Bulbasaur
    VarSet(VAR_MAP_SCENE_PALLET_TOWN_OAK, 1);                // interception done
    VarSet(VAR_MAP_SCENE_PALLET_TOWN_PROFESSOR_OAKS_LAB, 4); // EndRivalBattle
}

// Reproduces the save state at the end of PalletTown_ProfessorOaksLab's
// EventScript_ReceiveDexScene (delivering Oak's Parcel), which is how the player
// obtains the Pokédex in a real playthrough. Written on top of
// Platform_DevBootApplyStoryProgress, and annotated by the script statement each
// line mirrors; keep it in sync if that scene changes.
//
// The START menu only offers POKéDEX while FLAG_SYS_POKEDEX_GET is set
// (SetUpStartMenu_NormalField), and StartMenuPokedexSanityCheck additionally
// refuses to open it while the dex has counted nothing -- so this state is the
// precondition for exercising the screen at all.
void Platform_DevBootApplyDexProgress(void)
{
    FlagSet(FLAG_SYS_POKEDEX_GET);              // setflag FLAG_SYS_POKEDEX_GET
    SetUnlockedPokedexFlags();                  // special SetUnlockedPokedexFlags
    VarSet(VAR_MAP_SCENE_POKEMON_CENTER_TEALA, 1);

    // giveitem_msg ... ITEM_POKE_BALL, 5
    AddBagItem(ITEM_POKE_BALL, 5);

    // The dex units on Oak's desk: removeobject LOCALID_POKEDEX_1 / _2
    FlagSet(FLAG_HIDE_POKEDEX);

    // Scene vars the receiving scene writes at its end.
    VarSet(VAR_MAP_SCENE_PALLET_TOWN_PROFESSOR_OAKS_LAB, 6);
    VarSet(VAR_MAP_SCENE_VIRIDIAN_CITY_MART, 2);
    VarSet(VAR_MAP_SCENE_VIRIDIAN_CITY_OLD_MAN, 1);
    VarSet(VAR_MAP_SCENE_PALLET_TOWN_RIVALS_HOUSE, 1);
    VarSet(VAR_MAP_SCENE_ROUTE22, 1);
}

// CB2_NewGame performs the whole fresh-save map load (bedroom) in one call, so
// the story state is written between it and the warp into the lab. The bedroom
// frame is never rendered: CB2_LoadMap clears the screen and the lab fades in.
static void CB2_DevBootPostRival(void)
{
    CB2_NewGame();

    Platform_DevBootApplyStoryProgress();
    if (gPlatformDexObtained)
        Platform_DevBootApplyDexProgress();

    // Same hand-off as ScrCmd_warp: the warp is applied, then CB2_LoadMap runs
    // the destination map load and hands off to CB2_Overworld.
    SetWarpDestination(MAP_GROUP(MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB),
                       MAP_NUM(MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB), -1, 6, 8);
    WarpIntoMap();
    CB2_LoadMap();
}
