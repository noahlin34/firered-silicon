// Dev boot: spawn in the player's bedroom on a fresh save, skipping the title
// screen, main menu, controls guide, Oak's speech, and both naming screens.
//
// The save initialization mirrors SetTitleScreenScene_Cry (src/title_screen.c),
// and the identity written below is what the naming screens would have left
// behind: the naming screen's first choices (RED / GREEN, male). The final
// hand-off is CB2_NewGame, exactly where Oak's farewell dialogue ends
// (src/oak_speech.c, Task_OakSpeech_ShrinkPlayerPic).

#include "global.h"
#include "gba/m4a_internal.h"
#include "event_scripts.h"
#include "load_save.h"
#include "malloc.h"
#include "new_game.h"
#include "overworld.h"
#include "save.h"
#include "string_util.h"
#include "platform/platform.h"

bool gPlatformSkipIntro = false;

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
    SetMainCallback2(CB2_NewGame);
}
