#include <stdio.h>
#include "gba/m4a_internal.h"
#include "global.h"
#include "m4a.h"
#include "link.h"
#include "link_rfu.h"
#include "save_failed_screen.h"
#include "help_system.h"
#include "load_save.h"
#include "bg.h"
#include "malloc.h"
#include "decompress.h"
#include "text.h"
void m4aSoundInit(void) {}
void m4aSoundMain(void) {}
void m4aSoundVSync(void) {}
void m4aSoundVSyncOff(void) {}
void m4aSoundVSyncOn(void) {}
void m4aSongNumStart(u16 n) { (void)n; }
void m4aSongNumStartOrChange(u16 n) { (void)n; }
void m4aSongNumStop(u16 n) { (void)n; }
void m4aMPlayAllStop(void) {}
void m4aMPlayContinue(struct MusicPlayerInfo *mplayInfo) { (void)mplayInfo; }
void m4aMPlayFadeOut(struct MusicPlayerInfo *mplayInfo, u16 speed) { (void)mplayInfo; (void)speed; }
void m4aMPlayFadeOutTemporarily(struct MusicPlayerInfo *mplayInfo, u16 speed) { (void)mplayInfo; (void)speed; }
void m4aMPlayFadeIn(struct MusicPlayerInfo *mplayInfo, u16 speed) { (void)mplayInfo; (void)speed; }
void m4aMPlayImmInit(struct MusicPlayerInfo *mplayInfo) { (void)mplayInfo; }
void m4aMPlayStop(struct MusicPlayerInfo *mplayInfo) { (void)mplayInfo; }

// High-level Sound Stubs
void InitMapMusic(void) {}
void PlayBGM(u16 songNum) { (void)songNum; }
void StopBGM(void) {}
void FadeOutBGM(u8 speed) { (void)speed; }
void FadeInBGM(u8 speed) { (void)speed; }
void PlaySE(u16 songNum) { (void)songNum; }
void PlaySE12WithPanning(u16 songNum, s8 pan) { (void)songNum; (void)pan; }
void PlayFanfare(u16 songNum) { (void)songNum; }
void FadeOutFanfare(u8 speed) { (void)speed; }
bool8 IsFanfareTaskInactive(void) { return TRUE; }
bool8 IsSEPlaying(void) { return FALSE; }
bool8 IsBGMStopped(void) { return TRUE; }
void SoundEngine_Init(void) {}

// RFU & Link Hardware Stubs
void InitRFU(void) {}
void InitLink(void) {}
void OpenLink(void) {}
void CloseLink(void) {}
bool8 IsLinkTaskFinished(void) { return TRUE; }
u8 GetLinkPlayerCount(void) { return 1; }
bool8 IsLinkMaster(void) { return TRUE; }
void DestroyWirelessStatusIndicatorSprite(void) {}


// Save Failed Screen Stub
bool32 RunSaveFailedScreen(void) { return FALSE; }
void SetNotInSaveFailedScreen(void) {}

// Decompress / Mon pic stubs
bool8 RunHelpSystemCallback(void) { return FALSE; }

// SLoop Service Stub
u8 SetSLoopSvc(void) { return 0; }

// Additional Link & Engine Stubs
void LinkVSync(void) {}
void RfuVSync(void) {}
bool8 HandleLinkConnection(void) { return FALSE; }
bool32 TryReceiveLinkBattleData(void) { return FALSE; }
void UpdateWirelessStatusIndicatorSprite(void) {}
void rfu_REQ_stopMode(void) {}
u16 rfu_waitREQComplete(void) { return 0; }
u8 gWirelessCommType = 0;
void Timer3Intr(void) {}
void SetFlashTimerIntr(bool8 enable) { (void)enable; }
void MapMusicMain(void) {}
struct PokemonCrySong gPokemonCrySongs[1] = {{0}};
struct SoundInfo gSoundInfo = {0};
static const struct FontInfo gFontInfos[] = 
{
    [FONT_SMALL] = {
        .fontFunction = FontFunc_Small,
        .maxLetterWidth = 8,
        .maxLetterHeight = 13,
        .letterSpacing = 0,
        .lineSpacing = 0,
        .fgColor = 2,
        .bgColor = 1,
        .shadowColor = 3,
    },
    [FONT_NORMAL_COPY_1] = {
        .fontFunction = FontFunc_NormalCopy1,
        .maxLetterWidth = 8,
        .maxLetterHeight = 14,
        .letterSpacing = 0,
        .lineSpacing = 0,
        .fgColor = 2,
        .bgColor = 1,
        .shadowColor = 3,
    },
    [FONT_NORMAL] = {
        .fontFunction = FontFunc_Normal,
        .maxLetterWidth = 10,
        .maxLetterHeight = 14,
        .letterSpacing = 1,
        .lineSpacing = 0,
        .fgColor = 2,
        .bgColor = 1,
        .shadowColor = 3,
    },
    [FONT_NORMAL_COPY_2] = {
        .fontFunction = FontFunc_NormalCopy2,
        .maxLetterWidth = 10,
        .maxLetterHeight = 14,
        .letterSpacing = 1,
        .lineSpacing = 0,
        .fgColor = 2,
        .bgColor = 1,
        .shadowColor = 3,
    },
    [FONT_MALE] = {
        .fontFunction = FontFunc_Male,
        .maxLetterWidth = 10,
        .maxLetterHeight = 14,
        .letterSpacing = 0,
        .lineSpacing = 0,
        .fgColor = 2,
        .bgColor = 1,
        .shadowColor = 3,
    },
    [FONT_FEMALE] = {
        .fontFunction = FontFunc_Female,
        .maxLetterWidth = 10,
        .maxLetterHeight = 14,
        .letterSpacing = 0,
        .lineSpacing = 0,
        .fgColor = 2,
        .bgColor = 1,
        .shadowColor = 3,
    },
    [FONT_BRAILLE] = {
        .fontFunction = NULL,
        .maxLetterWidth = 8,
        .maxLetterHeight = 16,
        .letterSpacing = 0,
        .lineSpacing = 2,
        .fgColor = 2,
        .bgColor = 1,
        .shadowColor = 3,
    },
    [FONT_BOLD] = {
        .fontFunction = NULL,
        .maxLetterWidth = 8,
        .maxLetterHeight = 12,
        .letterSpacing = 0,
        .lineSpacing = 0,
        .fgColor = 2,
        .bgColor = 1,
        .shadowColor = 3,
    }
};

u8 gQuestLogPlaybackState = 0;
bool8 gHelpSystemEnabled = FALSE;

// Battle BG offsets referenced by scanline_effect.c

void QuestLog_CutPlayback(void) {}

__attribute__((weak)) void CB2_InitCopyrightScreenAfterBootup(void)
{
    printf("[Engine] CB2_InitCopyrightScreenAfterBootup called! State = %d\n", gMain.state);
}

// Title Screen & Background Helper Stubs
static void *sTempTileDataBuffers[32] = {NULL};
static u8 sTempTileDataBufferCursor = 0;






// Title Screen Transitions & State Stubs
void CB2_InitBerryFixProgram(void) {}
void CB2_InitCopyrightScreenAfterTitleScreen(void) {}
void CB2_SaveClearScreen_Init(void) {}
void PlayCry_Normal(u16 species, s8 pan) { (void)species; (void)pan; }
void FadeOutMapMusic(u8 speed) { (void)speed; }
bool8 IsNotWaitingForBGMStop(void) { return TRUE; }
u8 LoadGameSave(u8 saveType) { (void)saveType; return 0; }
void Save_ResetSaveCounters(void) {}
u16 gSaveFileStatus = 1; // SAVE_STATUS_OK
void SetPokemonCryStereo(u32 mode) { (void)mode; }
void HelpSystem_Disable(void) {}
void HelpSystem_Enable(void) {}
void SetHelpContext(u8 helpContext) { (void)helpContext; }

// Font & UI Stubs

s32 GetGlyphWidth_Braille(u16 fontId, bool32 isJapanese) { (void)fontId; (void)isJapanese; return 0; }
const u8 *DynamicPlaceholderTextUtil_GetPlaceholderPtr(u8 id) { (void)id; return NULL; }
struct MusicPlayerInfo gMPlayInfo_BGM = {0};
u8 gQuestLogState = 0;
const struct OamData gOamData_AffineOff_ObjNormal_16x16 = {0};
void CB2_InitMysteryGift(void) {}
bool8 IsWirelessAdapterConnected(void) { return FALSE; }
void TryStartQuestLogPlayback(u8 taskId) { (void)taskId; }
