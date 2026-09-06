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

// Flash Memory Stub
void CheckForFlashMemory(void)
{
    gFlashMemoryPresent = TRUE;
}

// Save Failed Screen Stub
bool32 RunSaveFailedScreen(void) { return FALSE; }
void SetNotInSaveFailedScreen(void) {}

// Decompress / Mon pic stubs
void DrawSpindaSpots(u16 species, u32 personality, u8 *dest, bool8 isFrontPic) { (void)species; (void)personality; (void)dest; (void)isFrontPic; }
const struct CompressedSpriteSheet gMonFrontPicTable[1] = {{0}};
const struct CompressedSpriteSheet gMonBackPicTable[1] = {{0}};
bool8 RunHelpSystemCallback(void) { return FALSE; }

// SLoop Service Stub
u8 SetSLoopSvc(void) { return 0; }

// Additional Link & Engine Stubs
void LinkVSync(void) {}
void RfuVSync(void) {}
bool8 HandleLinkConnection(void) { return FALSE; }
bool8 Overworld_RecvKeysFromLinkIsRunning(void) { return FALSE; }
bool8 Overworld_SendKeysToLinkIsRunning(void) { return FALSE; }
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
void PlayTimeCounter_Update(void) {}
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

void SetDefaultFontsPointer(void)
{
    SetFontsPointer(&gFontInfos[0]);
}
u8 gQuestLogPlaybackState = 0;
bool8 gHelpSystemEnabled = FALSE;

// Battle BG offsets referenced by scanline_effect.c
u16 gBattle_BG0_X = 0;
u16 gBattle_BG0_Y = 0;
u16 gBattle_BG1_X = 0;
u16 gBattle_BG1_Y = 0;
u16 gBattle_BG2_X = 0;
u16 gBattle_BG2_Y = 0;
u16 gBattle_BG3_X = 0;
u16 gBattle_BG3_Y = 0;

void QuestLog_CutPlayback(void) {}

__attribute__((weak)) void CB2_InitCopyrightScreenAfterBootup(void)
{
    printf("[Engine] CB2_InitCopyrightScreenAfterBootup called! State = %d\n", gMain.state);
}

// Title Screen & Background Helper Stubs
static void *sTempTileDataBuffers[32] = {NULL};
static u8 sTempTileDataBufferCursor = 0;

void ResetTempTileDataBuffers(void)
{
    for (int i = 0; i < 32; i++)
        sTempTileDataBuffers[i] = NULL;
    sTempTileDataBufferCursor = 0;
}

bool8 FreeTempTileDataBuffersIfPossible(void)
{
    if (sTempTileDataBufferCursor)
    {
        for (int i = 0; i < sTempTileDataBufferCursor; i++)
            FREE_AND_SET_NULL(sTempTileDataBuffers[i]);
        sTempTileDataBufferCursor = 0;
    }
    return FALSE;
}
void *MallocAndDecompress(const void *src, u32 *size)
{
    const u8 *srcBytes = (const u8 *)src;
    u32 uncompressedSize = srcBytes[1] | (srcBytes[2] << 8) | (srcBytes[3] << 16);
    if (size)
        *size = uncompressedSize;
    void *ptr = Alloc(uncompressedSize);
    if (ptr)
        LZ77UnCompWram(src, ptr);
    return ptr;
}


void *DecompressAndCopyTileDataToVram(u8 bgId, const void *src, u32 size, u16 offset, u8 mode)
{
    if (sTempTileDataBufferCursor < 32)
    {
        u32 sizeOut = 0;
        void *ptr = MallocAndDecompress(src, &sizeOut);
        if (!size)
            size = sizeOut;
        if (ptr)
        {
            if (mode == 0)
            {
                u32 charBase = GetBgControlAttribute(bgId, BG_CTRL_ATTR_CHARBASEINDEX);
                u8 *dest = (u8 *)BG_CHAR_ADDR(charBase) + offset;
                memcpy(dest, ptr, size);
            }
            else
            {
                u32 mapBase = GetBgControlAttribute(bgId, BG_CTRL_ATTR_MAPBASEINDEX);
                u8 *dest = (u8 *)BG_SCREEN_ADDR(mapBase) + (offset * 32);
                memcpy(dest, ptr, size);
            }
            sTempTileDataBuffers[sTempTileDataBufferCursor++] = ptr;
        }
        return ptr;
    }
    return NULL;
}

void ResetBgPositions(void)
{
    ChangeBgX(0, 0, 0);
    ChangeBgX(1, 0, 0);
    ChangeBgX(2, 0, 0);
    ChangeBgX(3, 0, 0);
    ChangeBgY(0, 0, 0);
    ChangeBgY(1, 0, 0);
    ChangeBgY(2, 0, 0);
    ChangeBgY(3, 0, 0);
}

// Title Screen Transitions & State Stubs
void CB2_InitBerryFixProgram(void) {}
void CB2_InitCopyrightScreenAfterTitleScreen(void) {}
void CB2_SaveClearScreen_Init(void) {}
void PlayCry_Normal(u16 species, s8 pan) { (void)species; (void)pan; }
void FadeOutMapMusic(u8 speed) { (void)speed; }
bool8 IsNotWaitingForBGMStop(void) { return TRUE; }
u8 LoadGameSave(u8 saveType) { (void)saveType; return 0; }
void Save_ResetSaveCounters(void) {}
void Sav2_ClearSetDefault(void) {}
void SetSaveBlocksPointers(void) {}
void ResetMenuAndMonGlobals(void) {}
u16 gSaveFileStatus = 1; // SAVE_STATUS_OK
void SetPokemonCryStereo(u32 mode) { (void)mode; }
void HelpSystem_Disable(void) {}
void HelpSystem_Enable(void) {}
void SetHelpContext(u8 helpContext) { (void)helpContext; }

// Font & UI Stubs
u8 GetFontAttribute(u8 fontId, u8 attributeId)
{
    int result = 0;
    if (gFonts)
    {
        switch (attributeId)
        {
        case FONTATTR_MAX_LETTER_WIDTH:
            result = gFonts[fontId].maxLetterWidth;
            break;
        case FONTATTR_MAX_LETTER_HEIGHT:
            result = gFonts[fontId].maxLetterHeight;
            break;
        case FONTATTR_LETTER_SPACING:
            result = gFonts[fontId].letterSpacing;
            break;
        case FONTATTR_LINE_SPACING:
            result = gFonts[fontId].lineSpacing;
            break;
        case FONTATTR_UNKNOWN:
            result = gFonts[fontId].unk;
            break;
        case FONTATTR_COLOR_FOREGROUND:
            result = gFonts[fontId].fgColor;
            break;
        case FONTATTR_COLOR_BACKGROUND:
            result = gFonts[fontId].bgColor;
            break;
        case FONTATTR_COLOR_SHADOW:
            result = gFonts[fontId].shadowColor;
            break;
        }
    }
    return result;
}

bool8 FlagGet(u16 flag) { (void)flag; return FALSE; }
u16 GetKantoPokedexCount(u8 caseId) { (void)caseId; return 0; }
u16 GetNationalPokedexCount(u8 caseId) { (void)caseId; return 0; }
bool8 IsNationalPokedexEnabled(void) { return FALSE; }
s32 GetGlyphWidth_Braille(u16 fontId, bool32 isJapanese) { (void)fontId; (void)isJapanese; return 0; }
u8 GetUnownLetterByPersonalityLoByte(u32 personality) { (void)personality; return 0; }
const u8 *DynamicPlaceholderTextUtil_GetPlaceholderPtr(u8 id) { (void)id; return NULL; }
struct MusicPlayerInfo gMPlayInfo_BGM = {0};
u8 gQuestLogState = 0;
bool8 gExitStairsMovementDisabled = FALSE;
const struct OamData gOamData_AffineOff_ObjNormal_16x16 = {0};
void StartNewGameScene(void) { printf("[Engine] StartNewGameScene called! Transitioning to Oak's Speech...\n"); }
void CB2_InitMysteryGift(void) {}
bool8 IsMysteryGiftEnabled(void) { return FALSE; }
bool8 IsWirelessAdapterConnected(void) { return FALSE; }
void TryStartQuestLogPlayback(u8 taskId) { (void)taskId; }
