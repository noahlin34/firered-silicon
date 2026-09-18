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
#include "gba/flash_internal.h"
#include "malloc.h"
#include "decompress.h"
#include "sound.h"
#include "text.h"
#include "main.h"


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
void UpdateWirelessStatusIndicatorSprite(void) {}
void rfu_REQ_stopMode(void) {}
u16 rfu_waitREQComplete(void) { return 0; }
u8 gWirelessCommType = 0;
void Timer3Intr(void) {}
/* src/agb_flash.c is deliberately NOT linked: the host build simulates flash
 * with the FLASH_BASE_ buffer and `gFlashMemoryPresent = TRUE` (src/load_save.c,
 * PORTABLE), so none of the real flash driver runs. Linking it only for this
 * symbol pulled in 27 pointer-truncation warnings from unreachable code —
 * ReadFlashId/SetReadFlash1 build Thumb-bit function pointers with
 * `(s32)buf + 1`, which is meaningless on the host (the very hazard fix #1
 * describes). This stub carries the declared type from gba/flash_internal.h
 * (u16, taking an intrFunc out-parameter) so the caller's return value is real. */
u16 SetFlashTimerIntr(u8 timerNum, void (**intrFunc)(void))
{
    (void)timerNum;
    (void)intrFunc;
    return 1; // non-zero = "no timer installed", matching the >= 4 guard
}
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

bool8 gHelpSystemEnabled = FALSE;
/* src/sound.c (now linked) reads this; its owner src/help_system_util.c is not
 * linked. Match the declaration in src/sound.c exactly. */
u8 gDisableHelpSystemVolumeReduce = 0;

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
u8 LoadGameSave(u8 saveType) { (void)saveType; return 0; }
void Save_ResetSaveCounters(void) {}
u16 gSaveFileStatus = 0; // SAVE_STATUS_EMPTY
void HelpSystem_Disable(void) {}
void HelpSystem_Enable(void) {}
void SetHelpContext(u8 helpContext) { (void)helpContext; }

// ROM header fields. On the GBA these live in src/rom_header.s, which only
// gbafix populates and which cannot be assembled for the host. src/mystery_gift.c
// copies them into the Mystery Gift link game data. The values are the ones this
// build's config.mk hands to gbafix: GAME_VERSION=FIRERED/GAME_LANGUAGE=ENGLISH
// gives the code "BPRE", and GAME_REVISION=0 gives software version 0.
const char RomHeaderGameCode[GAME_CODE_LENGTH] = { 'B', 'P', 'R', 'E' };
const char RomHeaderSoftwareVersion = 0;

// Font & UI Stubs

s32 GetGlyphWidth_Braille(u16 fontId, bool32 isJapanese) { (void)fontId; (void)isJapanese; return 0; }
void CB2_InitMysteryGift(void) {}
bool8 IsWirelessAdapterConnected(void) { return FALSE; }
