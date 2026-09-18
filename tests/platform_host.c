// The harness's half of the platform seam.
//
// `firered-tests` links the same engine objects as the game but NOT
// src/platform/{sdl2,dev_panel,main}.c. Those three define 16 symbols; the
// engine references 7 of them, and this file provides all 7 so the substitution
// cannot fail to link:
//
//   Platform_SubmitAudioFrame         src/main.c   (audio out; see below)
//   Platform_UpdateInput              src/main.c + src/platform/bios.c
//   Platform_RenderAndPresent         src/main.c   <- the frame tick
//   Platform_SaveScreenshot           src/main.c
//   Platform_DevPanelUpdate           src/main.c
//   Platform_DevPanelSaveScreenshot   src/main.c
//   Platform_VerifySaveBackedStubs    src/main.c
//   gPlatformDevPanelEnabled          src/main.c + tests
//
// Nothing in src/ is test-specific and nothing in src/ includes this file. The
// seam works because the engine already routed every frame through
// Platform_RenderAndPresent; excluding sdl2.o takes the display clock with it,
// so the suite runs the engine as fast as the CPU allows instead of pacing to
// 59.7275 Hz (measured 4x wall-time difference on a boot).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <execinfo.h>

#include "internal.h"

#include "gba/io_reg.h"
#include "gba/m4a_internal.h"
#include "platform/platform.h"

// --- SDL-facing symbols, reimplemented without SDL --------------------------

void Platform_UpdateInput(void)
{
    // The harness owns keys directly. Writing them here (rather than in a test
    // body) reproduces the real ordering: ReadKeys() has run for this frame and
    // the next one reads what we leave behind.
    Input_ApplyToKeys();
}

void Platform_RenderAndPresent(void)
{
    // This is the once-per-frame seam. The render itself is skipped -- tests
    // render on demand into their own buffer via Test_RenderFrame().
    Harness_FrameTick();
}

void Platform_SaveScreenshot(const char *filename)
{
    // Tests capture through Test_SaveScreenshot(), which writes the harness
    // framebuffer directly. The engine only calls this from its boot-test
    // frame-limit path, which the harness never reaches.
    (void)filename;
}

// The developer panel is a second SDL window used for manual testing and is
// deliberately not covered; the engine's frame loop calls these two, so they
// exist only to satisfy the link.
bool gPlatformDevPanelEnabled = false;

void Platform_DevPanelUpdate(void) {}
void Platform_DevPanelSaveScreenshot(const char *filename) { (void)filename; }

// The save-backed guard is a test, not engine behaviour, so the engine's call
// site is a no-op here; tests/save_backed.c covers the same ground with the
// reporting and isolation the harness provides.
void Platform_VerifySaveBackedStubs(void) {}

// The test binary has no audio device and no SDL at all, so the mixed buffer is
// not played. It IS recorded, because whether the engine hands its mix to the
// platform is observable behaviour that has been wrong: the driver mixed
// correctly into gSoundInfo.pcmBuffer and nothing ever called this function, so
// the game booted, the song sequenced, and the device stayed silent. A test that
// reads gSoundInfo.pcmBuffer cannot catch that -- it passes either way.
// tests/sound.c asserts through these counters via Harness_AudioSubmitCount.
int gHarnessAudioSubmits = 0;
long gHarnessAudioMagnitude = 0;

void Platform_SubmitAudioFrame(const struct SoundInfo *soundInfo)
{
    int i;
    long sum = 0;

    if (!soundInfo)
        return;

    gHarnessAudioSubmits++;
    for (i = 0; i < soundInfo->pcmSamplesPerVBlank; i++)
    {
        sum += soundInfo->pcmBuffer[i] < 0 ? -soundInfo->pcmBuffer[i]
                                           : soundInfo->pcmBuffer[i];
        sum += soundInfo->pcmBuffer[PCM_DMA_BUF_SIZE + i] < 0
             ? -soundInfo->pcmBuffer[PCM_DMA_BUF_SIZE + i]
             : soundInfo->pcmBuffer[PCM_DMA_BUF_SIZE + i];
    }
    if (sum > gHarnessAudioMagnitude)
        gHarnessAudioMagnitude = sum;
}

// --- crash reporting --------------------------------------------------------
// A crashing test is reported with the faulting address and a symbolized frame,
// then the process exits with a reserved code the runner attributes to the test.
#define CRASH_EXIT_CODE 99

static void CrashHandler(int sig)
{
    void *frames[64];
    int count = backtrace(frames, 64);

    fprintf(stderr, "      signal %d (%s)\n", sig, strsignal(sig));
    if (count > 0)
        backtrace_symbols_fd(frames, count, 2);
    fflush(NULL);
    _exit(CRASH_EXIT_CODE);
}

void Harness_InstallCrashHandler(void)
{
    signal(SIGSEGV, CrashHandler);
    signal(SIGBUS, CrashHandler);
    signal(SIGABRT, CrashHandler);
    signal(SIGILL, CrashHandler);
    signal(SIGFPE, CrashHandler);
}
