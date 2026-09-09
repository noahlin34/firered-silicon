#include "global.h"
#include "gflib.h"
#include "link.h"
#include "link_rfu.h"
#include "load_save.h"
#include "m4a.h"
#include "random.h"
#include "gba/flash_internal.h"
#include "help_system.h"
#include "new_menu_helpers.h"
#include "overworld.h"
#include "play_time.h"
#include "intro.h"
#include "battle_controllers.h"
#include "scanline_effect.h"
#include "save_failed_screen.h"
#include "quest_log.h"
#include "sloopsvc.h"
#include "title_screen.h"
#ifdef PORTABLE
#include "platform/platform.h"
int printf(const char *, ...);
void exit(int);
int gEngineMaxFrames = 0;
static int sEngineFrameCount = 0;
#endif
#ifndef PORTABLE
extern u32 intr_main[];
#endif

static void VBlankIntr(void);
static void HBlankIntr(void);
static void VCountIntr(void);
static void SerialIntr(void);
static void IntrDummy(void);

#if REVISION >= 0xA && !MODERN
const char OtherBuildDateTime[] = "2025 12 19 16:01";
#endif

#if REVISION >= 0xA
// OtherBuildDateTime is probably in some other file?
__attribute__((aligned(4)))
#endif
const u8 gGameVersion = GAME_VERSION;

const u8 gGameLanguage = GAME_LANGUAGE;

#if MODERN
const char BuildDateTime[] = __DATE__ " " __TIME__;
#else
#if REVISION == 0
const char BuildDateTime[] = "2004 04 26 11:20";
#elif REVISION == 1
const char BuildDateTime[] = "2004 07 20 09:30";
#elif REVISION == 0xA
const char BuildDateTime[] = "2025 12 19 15:38 22afedd9";
#endif //REVISION
#endif //MODERN

const IntrFunc gIntrTableTemplate[] =
{
    VCountIntr, // V-count interrupt
    SerialIntr, // Serial interrupt
    Timer3Intr, // Timer 3 interrupt
    HBlankIntr, // H-blank interrupt
    VBlankIntr, // V-blank interrupt
    IntrDummy,  // Timer 0 interrupt
    IntrDummy,  // Timer 1 interrupt
    IntrDummy,  // Timer 2 interrupt
    IntrDummy,  // DMA 0 interrupt
    IntrDummy,  // DMA 1 interrupt
    IntrDummy,  // DMA 2 interrupt
    IntrDummy,  // DMA 3 interrupt
    IntrDummy,  // Key interrupt
    IntrDummy,  // Game Pak interrupt
};

#define INTR_COUNT ((int)(sizeof(gIntrTableTemplate)/sizeof(IntrFunc)))

COMMON_DATA u16 gKeyRepeatStartDelay = 0;
COMMON_DATA u8 gLinkTransferringData = 0;
COMMON_DATA struct Main gMain = {0};
COMMON_DATA u16 gKeyRepeatContinueDelay = 0;
COMMON_DATA u8 gSoftResetDisabled = 0;
COMMON_DATA IntrFunc gIntrTable[INTR_COUNT] = {0};
COMMON_DATA u8 sVcountAfterSound = 0;
COMMON_DATA bool8 gLinkVSyncDisabled = 0;
COMMON_DATA u32 IntrMain_Buffer[0x200] = {0};
COMMON_DATA u8 sVcountAtIntr = 0;
COMMON_DATA u8 sVcountBeforeSound = 0;
COMMON_DATA u8 gPcmDmaCounter = 0;

static IntrFunc * const sTimerIntrFunc = gIntrTable + 0x7;

EWRAM_DATA u8 gDecompressionBuffer[0x4000] = {0};
EWRAM_DATA u16 gTrainerId = 0;

static void UpdateLinkAndCallCallbacks(void);
static void InitMainCallbacks(void);
static void CallCallbacks(void);
static void ReadKeys(void);
void InitIntrHandlers(void);
static void WaitForVBlank(void);
void EnableVCountIntrAtLine150(void);

#define B_START_SELECT (B_BUTTON | START_BUTTON | SELECT_BUTTON)

void AgbMain()
{
#if REVISION >= 0xA
    svc_stubbed();
#endif
#if MODERN
    // Modern compilers are liberal with the stack on entry to this function,
    // so RegisterRamReset may crash if it resets IWRAM.
    RegisterRamReset(RESET_ALL & ~RESET_IWRAM);
#ifndef PORTABLE
    asm("mov\tr1, #0xC0\n"
        "\tlsl\tr1, r1, #0x12\n"
        "\tmov\tr2, #0xFC\n"
        "\tlsl\tr2, r2, #0x7\n"
        "\tadd\tr2, r1, r2\n"
        "\tmov\tr0, #0\n"
        "\tmov\tr3, r0\n"
        "\tmov\tr4, r0\n"
        "\tmov\tr5, r0\n"
        ".LCU%=:\n"
        "\tstmia\tr1!, {r0, r3, r4, r5}\n"
        "\tstmia\tr1!, {r0, r3, r4, r5}\n"
        "\tstmia\tr1!, {r0, r3, r4, r5}\n"
        "\tstmia\tr1!, {r0, r3, r4, r5}\n"
        "\tstmia\tr1!, {r0, r3, r4, r5}\n"
        "\tstmia\tr1!, {r0, r3, r4, r5}\n"
        "\tstmia\tr1!, {r0, r3, r4, r5}\n"
        "\tstmia\tr1!, {r0, r3, r4, r5}\n"
        "\tcmp\tr1, r2\n"
        "\tbcc\t.LCU%=\n"
        :
        :
        : "r0", "r1", "r2", "r3", "r4", "r5", "memory"
    );
#endif
#else
    RegisterRamReset(RESET_ALL);
#endif //MODERN

#if REVISION >= 0xA
    *(vu16 *)BG_PLTT = RGB_BLACK;
#else
    *(vu16 *)BG_PLTT = RGB_WHITE;
#endif
    InitGpuRegManager();
    REG_WAITCNT = WAITCNT_PREFETCH_ENABLE | WAITCNT_WS0_S_1 | WAITCNT_WS0_N_3;
    InitKeys();
    InitIntrHandlers();
    m4aSoundInit();
    EnableVCountIntrAtLine150();
    InitRFU();
    CheckForFlashMemory();
    InitMainCallbacks();
    InitMapMusic();
    ClearDma3Requests();
    ResetBgs();
    InitHeap(gHeap, HEAP_SIZE);
    SetDefaultFontsPointer();

    gSoftResetDisabled = FALSE;
    gHelpSystemEnabled = FALSE;

    SetNotInSaveFailedScreen();

    // Revision 10 has no calls into libisagbprn except this one.
#if !defined(NDEBUG) || REVISION >= 0xA
#if (LOG_HANDLER == LOG_HANDLER_MGBA_PRINT)
    (void) MgbaOpen();
#elif (LOG_HANDLER == LOG_HANDLER_AGB_PRINT)
    AGBPrintInit();
#endif
#endif

#if REVISION >= 1
    if (gFlashMemoryPresent != TRUE)
        SetMainCallback2(NULL);
#endif

    gLinkTransferringData = FALSE;

    for (;;)
    {
        ReadKeys();

        if (gSoftResetDisabled == FALSE
         && (gMain.heldKeysRaw & A_BUTTON)
         && (gMain.heldKeysRaw & B_START_SELECT) == B_START_SELECT)
        {
            rfu_REQ_stopMode();
#if REVISION < 0xA
            rfu_waitREQComplete();
#endif
            DoSoftReset();
        }

        if (Overworld_SendKeysToLinkIsRunning() == TRUE)
        {
            gLinkTransferringData = TRUE;
            UpdateLinkAndCallCallbacks();
            gLinkTransferringData = FALSE;
        }
        else
        {
            gLinkTransferringData = FALSE;
            UpdateLinkAndCallCallbacks();

            if (Overworld_RecvKeysFromLinkIsRunning() == 1)
            {
                gMain.newKeys = 0;
                ClearSpriteCopyRequests();
                gLinkTransferringData = TRUE;
                UpdateLinkAndCallCallbacks();
                gLinkTransferringData = FALSE;
            }
        }

        PlayTimeCounter_Update();
        MapMusicMain();
        WaitForVBlank();
    }
}

static void UpdateLinkAndCallCallbacks(void)
{
    if (!HandleLinkConnection())
        CallCallbacks();
}

static void InitMainCallbacks(void)
{
    gMain.vblankCounter1 = 0;
    gMain.vblankCounter2 = 0;
    gMain.callback1 = NULL;
#ifndef PORTABLE
    SetMainCallback2(CB2_InitCopyrightScreenAfterBootup);
#else
    SetMainCallback2(CB2_InitTitleScreen);
#endif
    gSaveBlock2Ptr = &gSaveBlock2;
    gSaveBlock1Ptr = &gSaveBlock1;
    gSaveBlock2.encryptionKey = 0;
    gQuestLogPlaybackState = QL_PLAYBACK_STATE_STOPPED;
#if REVISION >= 0xA
    svc_SetSaveBlock2(&gSaveBlock2);
#endif
}

static void CallCallbacks(void)
{
    if (!RunSaveFailedScreen() && !RunHelpSystemCallback())
    {
        if (gMain.callback1)
            gMain.callback1();

        if (gMain.callback2)
            gMain.callback2();
    }
}

void SetMainCallback2(MainCallback callback)
{
    gMain.callback2 = callback;
    gMain.state = 0;
}

void StartTimer1(void)
{
    REG_TM1CNT_H = 0x80;
}

void SeedRngAndSetTrainerId(void)
{
    u16 val = REG_TM1CNT_L;
    SeedRng(val);
    REG_TM1CNT_H = 0;
    gTrainerId = val;
}

u16 GetGeneratedTrainerIdLower(void)
{
    return gTrainerId;
}

void EnableVCountIntrAtLine150(void)
{
    u16 gpuReg = (GetGpuReg(REG_OFFSET_DISPSTAT) & 0xFF) | (150 << 8);
    SetGpuReg(REG_OFFSET_DISPSTAT, gpuReg | DISPSTAT_VCOUNT_INTR);
    EnableInterrupts(INTR_FLAG_VCOUNT);
}

void InitKeys(void)
{
    gKeyRepeatContinueDelay = 5;
    gKeyRepeatStartDelay = 40;

    gMain.heldKeys = 0;
    gMain.newKeys = 0;
    gMain.newAndRepeatedKeys = 0;
    gMain.heldKeysRaw = 0;
    gMain.newKeysRaw = 0;
}

static void ReadKeys(void)
{
    u16 keyInput = REG_KEYINPUT ^ KEYS_MASK;
    gMain.newKeysRaw = keyInput & ~gMain.heldKeysRaw;
    gMain.newKeys = gMain.newKeysRaw;
    gMain.newAndRepeatedKeys = gMain.newKeysRaw;

    // BUG: Key repeat won't work when pressing L using L=A button mode
    // because it compares the raw key input with the remapped held keys.
    // Note that newAndRepeatedKeys is never remapped either.

    if (keyInput != 0 && gMain.heldKeys == keyInput)
    {
        gMain.keyRepeatCounter--;

        if (gMain.keyRepeatCounter == 0)
        {
            gMain.newAndRepeatedKeys = keyInput;
            gMain.keyRepeatCounter = gKeyRepeatContinueDelay;
        }
    }
    else
    {
        // If there is no input or the input has changed, reset the counter.
        gMain.keyRepeatCounter = gKeyRepeatStartDelay;
    }

    gMain.heldKeysRaw = keyInput;
    gMain.heldKeys = gMain.heldKeysRaw;

    // Remap L to A if the L=A option is enabled.
    if (gSaveBlock2Ptr->optionsButtonMode == OPTIONS_BUTTON_MODE_L_EQUALS_A)
    {
        if (JOY_NEW(L_BUTTON))
            gMain.newKeys |= A_BUTTON;

        if (JOY_HELD(L_BUTTON))
            gMain.heldKeys |= A_BUTTON;
    }

    if (JOY_NEW(gMain.watchedKeysMask))
        gMain.watchedKeysPressed = TRUE;
}

void InitIntrHandlers(void)
{
    int i;

    for (i = 0; i < INTR_COUNT; i++)
        gIntrTable[i] = gIntrTableTemplate[i];

#ifndef PORTABLE
    DmaCopy32(3, intr_main, IntrMain_Buffer, sizeof(IntrMain_Buffer));

    INTR_VECTOR = IntrMain_Buffer;
#endif

    SetVBlankCallback(NULL);
    SetHBlankCallback(NULL);
    SetSerialCallback(NULL);

    REG_IME = 1;

    EnableInterrupts(INTR_FLAG_VBLANK);
}

void SetVBlankCallback(IntrCallback callback)
{
    gMain.vblankCallback = callback;
}

void SetHBlankCallback(IntrCallback callback)
{
    gMain.hblankCallback = callback;
}

void SetVCountCallback(IntrCallback callback)
{
    gMain.vcountCallback = callback;
}

void SetSerialCallback(IntrCallback callback)
{
    gMain.serialCallback = callback;
}

extern void CopyBufferedValuesToGpuRegs(void);
extern void ProcessDma3Requests(void);

static void VBlankIntr(void)
{
    if (gWirelessCommType)
        RfuVSync();
    else if (!gLinkVSyncDisabled)
        LinkVSync();

    if (gMain.vblankCounter1)
        (*gMain.vblankCounter1)++;

    if (gMain.vblankCallback)
        gMain.vblankCallback();

    gMain.vblankCounter2++;

    CopyBufferedValuesToGpuRegs();
    ProcessDma3Requests();

    gPcmDmaCounter = gSoundInfo.pcmDmaCounter;

#if !defined(NDEBUG) || REVISION >= 0xA
    sVcountBeforeSound = REG_VCOUNT;
#endif
    m4aSoundMain();
#if !defined(NDEBUG) || REVISION >= 0xA
    sVcountAfterSound = REG_VCOUNT;
#endif

    TryReceiveLinkBattleData();
    Random();
    UpdateWirelessStatusIndicatorSprite();

    INTR_CHECK |= INTR_FLAG_VBLANK;
    gMain.intrCheck |= INTR_FLAG_VBLANK;
}

void InitFlashTimer(void)
{
    IntrFunc **func = (IntrFunc **)&sTimerIntrFunc;
    SetFlashTimerIntr(2, *func);
}

static void HBlankIntr(void)
{
    if (gMain.hblankCallback)
        gMain.hblankCallback();

    INTR_CHECK |= INTR_FLAG_HBLANK;
    gMain.intrCheck |= INTR_FLAG_HBLANK;
}

static void VCountIntr(void)
{
#if !defined(NDEBUG) || REVISION >= 0xA
    sVcountAtIntr = REG_VCOUNT;
#endif
    m4aSoundVSync();
    INTR_CHECK |= INTR_FLAG_VCOUNT;
    gMain.intrCheck |= INTR_FLAG_VCOUNT;
}

static void SerialIntr(void)
{
    if (gMain.serialCallback)
        gMain.serialCallback();

    INTR_CHECK |= INTR_FLAG_SERIAL;
    gMain.intrCheck |= INTR_FLAG_SERIAL;
}

void RestoreSerialTimer3IntrHandlers(void)
{
    gIntrTable[1] = SerialIntr;
    gIntrTable[2] = Timer3Intr;
}

static void IntrDummy(void)
{}

static void WaitForVBlank(void)
{
#ifndef PORTABLE
    gMain.intrCheck &= ~INTR_FLAG_VBLANK;

    while (!(gMain.intrCheck & INTR_FLAG_VBLANK))
        ;
#else
    Platform_UpdateInput();
    if (gEngineMaxFrames > 0)
    {
        if (sEngineFrameCount >= 5 && sEngineFrameCount <= 10)
            REG_KEYINPUT &= ~(1 << 3); // START to skip intro fade
        if (sEngineFrameCount >= 30 && sEngineFrameCount <= 35)
            REG_KEYINPUT &= ~(1 << 3); // START to enter game from title screen
        if (sEngineFrameCount >= 230 && sEngineFrameCount <= 231)
            REG_KEYINPUT &= ~(1 << 7); // DOWN to move cursor to NEW GAME
        if (sEngineFrameCount >= 240 && sEngineFrameCount <= 245)
            REG_KEYINPUT &= ~(1 << 0); // A to confirm NEW GAME
        if (sEngineFrameCount >= 320 && sEngineFrameCount <= 325)
            REG_KEYINPUT &= ~(1 << 0); // A to advance Controls Guide page 2
        if (sEngineFrameCount >= 360 && sEngineFrameCount <= 365)
            REG_KEYINPUT &= ~(1 << 0); // A to advance Controls Guide page 3
        if (sEngineFrameCount >= 400 && sEngineFrameCount <= 405)
            REG_KEYINPUT &= ~(1 << 0); // A to exit Controls Guide into Pikachu intro
        if (sEngineFrameCount >= 520 && sEngineFrameCount <= 525)
            REG_KEYINPUT &= ~(1 << 0); // A: advance Pikachu intro page
        if (sEngineFrameCount >= 560 && sEngineFrameCount <= 565)
            REG_KEYINPUT &= ~(1 << 0); // A: advance to Oak speech
        if (sEngineFrameCount >= 620 && ((sEngineFrameCount - 620) % 90) < 6)
        {
            if (sEngineFrameCount < 4800 || sEngineFrameCount > 5000)
                REG_KEYINPUT &= ~(1 << 0); // A: advance Oak speech dialog pages periodically
        }
        if (sEngineFrameCount >= 4910 && sEngineFrameCount <= 4912)
            REG_KEYINPUT &= ~(1 << 7); // DOWN to GREEN
        if (sEngineFrameCount >= 4930 && sEngineFrameCount <= 4932)
            REG_KEYINPUT &= ~(1 << 7); // DOWN to GARY
        if (sEngineFrameCount >= 4950 && sEngineFrameCount <= 4955)
            REG_KEYINPUT &= ~(1 << 0); // A to select GARY
        if ((sEngineFrameCount >= 6800 && sEngineFrameCount <= 6820) || (sEngineFrameCount >= 9220 && sEngineFrameCount <= 9240))
            REG_KEYINPUT &= ~(1 << 7); // DOWN to step south in bedroom
    }
    REG_VCOUNT = 160; // Start of VBlank
    VBlankIntr();
    Platform_RenderAndPresent();
    if (gEngineMaxFrames > 0 && ++sEngineFrameCount >= gEngineMaxFrames)
    {
        printf("[Engine] Reached %d frames in boot test! Saving screenshot...\n", sEngineFrameCount);
        Platform_SaveScreenshot("engine_boot_output.bmp");
        printf("[Engine] Engine boot test successful!\n");
        exit(0);
    }
#endif
}

void SetVBlankCounter1Ptr(u32 *ptr)
{
    gMain.vblankCounter1 = ptr;
}

void DisableVBlankCounter1(void)
{
    gMain.vblankCounter1 = NULL;
}

void DoSoftReset(void)
{
    REG_IME = 0;
    m4aSoundVSyncOff();
    ScanlineEffect_Stop();
    DmaStop(1);
    DmaStop(2);
    DmaStop(3);
    SoftReset(RESET_ALL & ~RESET_SIO_REGS);
}

void ClearPokemonCrySongs(void)
{
    CpuFill16(0, gPokemonCrySongs, MAX_POKEMON_CRIES * sizeof(struct PokemonCrySong));
}
