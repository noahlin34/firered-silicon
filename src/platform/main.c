#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <execinfo.h>
#include <SDL.h>
#include "global.h"
#include "platform/platform.h"
#include "global.fieldmap.h"
#include "field_effect.h"
#include "constants/event_bg.h"
#include "heal_location.h"
#include "field_screen_effect.h"
#include "field_poison.h"
#include "coins.h"
#include "pokedex.h"
#include "pokedex_screen.h"
#include "sound.h"
#include "field_specials.h"
#include "constants/heal_locations.h"
#include "constants/pokedex.h"
#include "constants/flags.h"
#include "constants/items.h"
#include "constants/maps.h"
#include "constants/map_groups.h"
#include "global.fieldmap.h"

static void CrashHandler(int sig)
{
    void *callstack[128];
    int frames = backtrace(callstack, 128);
    char **strs = backtrace_symbols(callstack, frames);
    fprintf(stderr, "\n[FATAL] Caught signal %d (%s)!\n", sig, strsignal(sig));
    if (strs)
    {
        for (int i = 0; i < frames; ++i)
            fprintf(stderr, "  %s\n", strs[i]);
        free(strs);
    }
    exit(sig);
}

extern void AgbMain(void);



int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    signal(SIGBUS, CrashHandler);
    signal(SIGSEGV, CrashHandler);
    printf("=========================================\n");
    printf(" Pokemon FireRed - Apple Silicon Native  \n");
    printf(" Architecture: ARM64 Mach-O             \n");
    printf(" Subsystem: Software GBA PPU Compositor  \n");
    printf("=========================================\n");

    // The smoke tests that used to run here now live in tests/ and run under
    // `make tests` / ./firered-tests, where each one is isolated in its own
    // process with reporting and a timeout. Keeping them here as well would be a
    // second convention for the same assertions -- and it ran them on every
    // launch of the game, before SDL init, with no way to filter or attribute a
    // failure. The PPU scene that was `--test` had no assertions at all; the
    // real compositor checks are tests/ppu.c.
    if (Platform_Init(argc, argv) != 0)
    {
        fprintf(stderr, "Failed to initialize platform window.\n");
        return 1;
    }

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--boot-test") == 0)
        {
            extern int gEngineMaxFrames;
            gEngineMaxFrames = (i + 1 < argc && argv[i + 1][0] != '-') ? atoi(argv[++i]) : 240;
            printf("[Engine] Running boot test for %d frames...\n", gEngineMaxFrames);
        }
        else if (strcmp(argv[i], "--skip-intro") == 0)
        {
            gPlatformSkipIntro = true;
            printf("[Engine] Dev boot: fresh save, spawning in the player's bedroom (intro skipped).\n");
        }
        else if (strcmp(argv[i], "--post-rival") == 0)
        {
            gPlatformSkipIntro = true;
            gPlatformSkipStory = true;
            printf("[Engine] Dev boot: fresh save, starter received and first rival battle won (Oak's Lab).\n");
        }
        else if (strcmp(argv[i], "--dev-panel") == 0)
        {
            gPlatformDevPanelEnabled = true;
            printf("[Engine] Dev panel: warp window enabled (F3 toggles).\n");
        }
        else if (strcmp(argv[i], "--dev-panel-keys") == 0)
        {
            // Headless panel driving: one keystroke per frame, taken verbatim
            // from the next argument. e.g. --dev-panel-keys "2,13,5"
            // (2 characters of the filter, ENTER, then DOWN and ENTER).
            if (i + 1 < argc)
                gPlatformDevPanelKeySequence = argv[++i];
        }
    }

    // The destination table walks gMapGroups and the map layouts, which the
    // engine rewrites while it runs, so it is built here -- after Platform_Init
    // has an SDL context and every flag has been read, before AgbMain starts.
    Platform_DevPanelInit();

    printf("[Engine] Booting Pokemon FireRed CPU Engine (AgbMain)...\n");
    AgbMain();
    Platform_Cleanup();
    return 0;
}
