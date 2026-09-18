#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <SDL.h>

#include "global.h"
#include "gba/m4a_internal.h"
#include "platform/platform.h"
#include "platform/ppu.h"
static SDL_Window *sWindow = NULL;
static SDL_Renderer *sRenderer = NULL;
static SDL_Texture *sTexture = NULL;
static bool sRunning = false;

static double sFrameTicks;
static double sTicksPerMillisecond;

static double sNextFrameDeadline;

/* ---------------------------------------------------------------------------
 * Audio output.
 *
 * The m4a driver mixes one vblank of audio into SoundInfo.pcmBuffer as two
 * mono halves -- FIFO A (right) at the start, FIFO B (left) at
 * pcmBuffer + PCM_DMA_BUF_SIZE -- which on the GBA the two DMA channels feed to
 * the hardware FIFOs. There is no hardware here, so the platform drains that
 * buffer into an SDL audio stream.
 *
 * Buffering is a queue of frames rather than a callback that pulls from the
 * engine: the engine runs on its own 59.7275 Hz clock and the audio device on
 * the host's rate, so the two drift and something has to absorb it. The engine
 * appends (Platform_SubmitAudioFrame, called once per vblank from the sound
 * tick) and the SDL callback consumes; if the queue ever runs dry the callback
 * emits silence rather than stretching the last frame.
 * --------------------------------------------------------------------------- */
static SDL_AudioDeviceID sAudioDevice = 0;
static bool sAudioEnabled = false;
static int sAudioDeviceRate = 0;

/* ~0.25 s of stereo frames at the device rate: enough to cover scheduling
 * jitter, small enough that a stall is not audible for long. */
#define AUDIO_QUEUE_FRAMES 11025
#define AUDIO_QUEUE_SIZE   (AUDIO_QUEUE_FRAMES * 2)

static int16_t sAudioQueue[AUDIO_QUEUE_SIZE];
static volatile int sAudioHead = 0;  /* next write index (frames)   */
static volatile int sAudioTail = 0;  /* next read index (frames)    */
static volatile int sAudioCount = 0; /* frames queued               */
static SDL_mutex *sAudioLock = NULL;

/* The engine's output is 8-bit signed at the device rate, repeated or dropped
 * to match rates: a simple nearest-neighbour resample. The GBA's own output is
 * a 13.379 kHz-ish stream being upsampled to whatever the host runs at, so the
 * extra fidelity of linear interpolation here would be measuring the driver
 * rather than the hardware. */
static void AudioCallback(void *userdata, Uint8 *stream, int len)
{
    int16_t *out = (int16_t *)stream;
    int frames = len / (int)(2 * sizeof(int16_t));
    int i;

    (void)userdata;

    if (!sAudioLock)
    {
        memset(stream, 0, len);
        return;
    }

    SDL_LockMutex(sAudioLock);
    for (i = 0; i < frames; i++)
    {
        if (sAudioCount > 0)
        {
            int idx = sAudioTail * 2;

            out[i * 2 + 0] = sAudioQueue[idx + 0];
            out[i * 2 + 1] = sAudioQueue[idx + 1];
            sAudioTail = (sAudioTail + 1) % AUDIO_QUEUE_FRAMES;
            sAudioCount--;
        }
        else
        {
            out[i * 2 + 0] = 0;
            out[i * 2 + 1] = 0;
        }
    }
    SDL_UnlockMutex(sAudioLock);
}

/* One vblank of engine audio, submitted from VBlankIntr.
 *
 * `pcmBuffer` holds `pcmSamplesPerVBlank` bytes per side and the engine's PCM
 * rate is `pcmFreq` (about 13.4 kHz); the device plays at sAudioDeviceRate. The
 * engine sample for output frame n covers host frames
 * [n*rate/pcmFreq, (n+1)*rate/pcmFreq), so each engine sample is written the
 * whole number of times that interval spans, with the fractional remainder
 * carried in sAudioPhase. Nearest-neighbour is deliberate: the driver's own
 * output is already a low-rate stream and interpolating here would invent
 * detail the hardware never had.
 */
static int sAudioPhase = 0;

void Platform_SubmitAudioFrame(const struct SoundInfo *soundInfo)
{
    int samples;
    int rate;
    int i;

    if (!sAudioEnabled || !sAudioLock || !soundInfo)
        return;

    samples = soundInfo->pcmSamplesPerVBlank;
    rate = soundInfo->pcmFreq;
    if (samples <= 0 || rate <= 0 || sAudioDeviceRate <= 0)
        return;

    SDL_LockMutex(sAudioLock);
    for (i = 0; i < samples; i++)
    {
        /* The engine writes FIFO A (right) at the start of pcmBuffer and
         * FIFO B (left) at + PCM_DMA_BUF_SIZE; m4aSoundInit programs
         * SOUND_A_RIGHT_OUTPUT and SOUND_B_LEFT_OUTPUT accordingly. */
        int16_t right = (int16_t)((int)soundInfo->pcmBuffer[i] << 8);
        int16_t left = (int16_t)((int)soundInfo->pcmBuffer[PCM_DMA_BUF_SIZE + i] << 8);

        sAudioPhase += sAudioDeviceRate;
        while (sAudioPhase >= rate)
        {
            int idx;

            sAudioPhase -= rate;
            if (sAudioCount >= AUDIO_QUEUE_FRAMES)
                break;
            idx = sAudioHead * 2;
            sAudioQueue[idx + 0] = right;
            sAudioQueue[idx + 1] = left;
            sAudioHead = (sAudioHead + 1) % AUDIO_QUEUE_FRAMES;
            sAudioCount++;
        }
    }
    SDL_UnlockMutex(sAudioLock);
}

/* Opens the audio device. Failure is not fatal: the game is still playable
 * silently, and a headless `SDL_VIDEODRIVER=dummy` run has no audio device at
 * all, which must not stop the boot test. */
static void Platform_InitAudio(void)
{
    SDL_AudioSpec want;
    SDL_AudioSpec have;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    {
        printf("[Audio] SDL audio unavailable (%s); running silently\n", SDL_GetError());
        return;
    }

    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = AudioCallback;
    want.userdata = NULL;

    sAudioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (sAudioDevice == 0)
    {
        printf("[Audio] failed to open audio device (%s); running silently\n", SDL_GetError());
        return;
    }

    sAudioDeviceRate = have.freq;
    sAudioLock = SDL_CreateMutex();
    if (!sAudioLock)
    {
        SDL_CloseAudioDevice(sAudioDevice);
        sAudioDevice = 0;
        return;
    }

    memset(sAudioQueue, 0, sizeof(sAudioQueue));
    sAudioHead = sAudioTail = sAudioCount = 0;
    sAudioEnabled = true;
    SDL_PauseAudioDevice(sAudioDevice, 0);

    printf("[Audio] opened %d Hz stereo audio device\n", have.freq);
}

static void WaitForFrameDeadline(void)
{
    double now = (double)SDL_GetPerformanceCounter();

    if (sNextFrameDeadline == 0.0)
        sNextFrameDeadline = now + sFrameTicks;

    while (now < sNextFrameDeadline)
    {
        double remainingMs = (sNextFrameDeadline - now) / sTicksPerMillisecond;
        // Sleep rather than spin; absolute deadlines absorb scheduler oversleep.
        SDL_Delay(remainingMs >= 1.0 ? (Uint32)remainingMs : 1);
        now = (double)SDL_GetPerformanceCounter();
    }

    sNextFrameDeadline += sFrameTicks;
    // A pause or slow frame must not create a backlog of fast catch-up ticks.
    if (sNextFrameDeadline <= now)
        sNextFrameDeadline = now + sFrameTicks;
}

// 15-bit BGR color framebuffer (240 x 160)
static uint16_t sFramebuffer[GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT];

// GBA Keys are active-low (0 = pressed, 1 = released)
// Bits:
// 0: A, 1: B, 2: SELECT, 3: START, 4: RIGHT, 5: LEFT, 6: UP, 7: DOWN, 8: R, 9: L
static uint16_t sKeyState = 0x03FF;

#define KEY_A_MASK      (1 << 0)
#define KEY_B_MASK      (1 << 1)
#define KEY_SELECT_MASK (1 << 2)
#define KEY_START_MASK  (1 << 3)
#define KEY_RIGHT_MASK  (1 << 4)
#define KEY_LEFT_MASK   (1 << 5)
#define KEY_UP_MASK     (1 << 6)
#define KEY_DOWN_MASK   (1 << 7)
#define KEY_R_MASK      (1 << 8)
#define KEY_L_MASK      (1 << 9)

// F3 toggles the developer panel from either window. Every event is offered to
// the panel first: its own window's keys and window events are consumed there,
// and a key aimed at the game window falls through to the GBA key mapping.
static void HandleKeyEvent(SDL_Keycode key, bool pressed)
{
    uint16_t mask = 0;
    switch (key)
    {
        case SDLK_z:
        case SDLK_j:
            mask = KEY_A_MASK;
            break;
        case SDLK_x:
        case SDLK_k:
            mask = KEY_B_MASK;
            break;
        case SDLK_RETURN:
            mask = KEY_START_MASK;
            break;
        case SDLK_BACKSPACE:
        case SDLK_TAB:
            mask = KEY_SELECT_MASK;
            break;
        case SDLK_UP:
        case SDLK_w:
            mask = KEY_UP_MASK;
            break;
        case SDLK_DOWN:
        case SDLK_s:
            mask = KEY_DOWN_MASK;
            break;
        case SDLK_LEFT:
        case SDLK_a:
            mask = KEY_LEFT_MASK;
            break;
        case SDLK_RIGHT:
        case SDLK_d:
            mask = KEY_RIGHT_MASK;
            break;
        case SDLK_q:
            mask = KEY_L_MASK;
            break;
        case SDLK_e:
            mask = KEY_R_MASK;
            break;
        default:
            return;
    }

    if (pressed)
        sKeyState &= ~mask; // Pressed = 0
    else
        sKeyState |= mask;  // Released = 1
}

#ifdef FPS_OVERLAY
// ---- Top-left FPS overlay ----------------------------------------------------
// Development aid: release builds compile it out with FPS_OVERLAY=0 (see
// Makefile.native). Sampled at presentation time so it reports frames the player
// actually sees, independent of the engine's internal frame pacing.
static double sTicksPerSecond;
static double sFpsWindowStart;
static uint32_t sFpsWindowFrames;
static int sFpsValue; // -1 until the first full sample window elapses

#define FPS_SAMPLE_WINDOW_SECONDS 0.5
#define FPS_GLYPH_WIDTH   5
#define FPS_GLYPH_HEIGHT  7
#define FPS_GLYPH_SPACING 1

// 5x7 glyphs; each row byte uses bit 4 for the leftmost column.
struct FpsGlyph
{
    char ch;
    uint8_t rows[FPS_GLYPH_HEIGHT];
};

static const struct FpsGlyph sFpsGlyphs[] =
{
    { '0', { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E } },
    { '1', { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E } },
    { '2', { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F } },
    { '3', { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E } },
    { '4', { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 } },
    { '5', { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E } },
    { '6', { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E } },
    { '7', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } },
    { '8', { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E } },
    { '9', { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C } },
    { 'F', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 } },
    { 'P', { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 } },
    { 'S', { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } },
    { '-', { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 } },
};

static void UpdateFpsCounter(void)
{
    double now = (double)SDL_GetPerformanceCounter();

    if (sFpsWindowStart == 0.0)
        sFpsWindowStart = now;

    sFpsWindowFrames++;

    double elapsed = (now - sFpsWindowStart) / sTicksPerSecond;
    if (elapsed >= FPS_SAMPLE_WINDOW_SECONDS)
    {
        sFpsValue = (int)(sFpsWindowFrames / elapsed + 0.5);
        sFpsWindowStart = now;
        sFpsWindowFrames = 0;
    }
}

// Drawn in logical (240x160) renderer space, after the GBA framebuffer, so the
// overlay never lands in the engine framebuffer or saved screenshots.
static void DrawFpsOverlay(void)
{
    char text[16];
    SDL_Rect pixel;
    size_t i;
    int textWidth;
    const int originX = 2;
    const int originY = 2;

    if (sFpsValue < 0)
        snprintf(text, sizeof(text), "FPS ---");
    else
        snprintf(text, sizeof(text), "FPS %d", sFpsValue);

    textWidth = ((int)strlen(text) * (FPS_GLYPH_WIDTH + FPS_GLYPH_SPACING)) - FPS_GLYPH_SPACING;

    SDL_Rect background = { originX - 1, originY - 1, textWidth + 2, FPS_GLYPH_HEIGHT + 2 };
    SDL_SetRenderDrawBlendMode(sRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(sRenderer, 0, 0, 0, 160);
    SDL_RenderFillRect(sRenderer, &background);

    SDL_SetRenderDrawColor(sRenderer, 255, 255, 255, 255);
    for (i = 0; text[i] != '\0'; i++)
    {
        size_t glyph;
        int x = originX + (int)i * (FPS_GLYPH_WIDTH + FPS_GLYPH_SPACING);

        for (glyph = 0; glyph < ARRAY_COUNT(sFpsGlyphs); glyph++)
        {
            int row, column;

            if (sFpsGlyphs[glyph].ch != text[i])
                continue;

            for (row = 0; row < FPS_GLYPH_HEIGHT; row++)
            {
                for (column = 0; column < FPS_GLYPH_WIDTH; column++)
                {
                    if (!(sFpsGlyphs[glyph].rows[row] & (1 << (FPS_GLYPH_WIDTH - 1 - column))))
                        continue;

                    pixel.x = x + column;
                    pixel.y = originY + row;
                    pixel.w = 1;
                    pixel.h = 1;
                    SDL_RenderFillRect(sRenderer, &pixel);
                }
            }
            break;
        }
    }

    SDL_SetRenderDrawBlendMode(sRenderer, SDL_BLENDMODE_NONE);
}
#endif // FPS_OVERLAY

int Platform_Init(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        fprintf(stderr, "Failed to initialize SDL2: %s\n", SDL_GetError());
        return -1;
    }

    Platform_InitAudio();

    const int scale = 3;
    const int windowWidth = GBA_SCREEN_WIDTH * scale;
    const int windowHeight = GBA_SCREEN_HEIGHT * scale;

    sWindow = SDL_CreateWindow(
        "Pokemon FireRed (Apple Silicon Native)",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        windowWidth,
        windowHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!sWindow)
    {
        fprintf(stderr, "Failed to create SDL window: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    sRenderer = SDL_CreateRenderer(
        sWindow,
        -1,
        SDL_RENDERER_ACCELERATED
    );

    if (!sRenderer)
    {
        sRenderer = SDL_CreateRenderer(sWindow, -1, 0);
    }

    if (!sRenderer)
    {
        fprintf(stderr, "Failed to create SDL renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(sWindow);
        SDL_Quit();
        return -1;
    }

    SDL_RenderSetLogicalSize(sRenderer, GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT);

    sTexture = SDL_CreateTexture(
        sRenderer,
        SDL_PIXELFORMAT_BGR555,
        SDL_TEXTUREACCESS_STREAMING,
        GBA_SCREEN_WIDTH,
        GBA_SCREEN_HEIGHT
    );

    if (!sTexture)
    {
        fprintf(stderr, "Failed to create framebuffer texture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(sRenderer);
        SDL_DestroyWindow(sWindow);
        SDL_Quit();
        return -1;
    }

    // GBA: 280896 CPU cycles per frame at 16777216 Hz (~59.7275 FPS).
    // Presentation VSync is deliberately off: the display is not the game clock.
    double frequency = (double)SDL_GetPerformanceFrequency();
    sFrameTicks = frequency * (280896.0 / 16777216.0);
    sTicksPerMillisecond = frequency / 1000.0;
#ifdef FPS_OVERLAY
    sTicksPerSecond = frequency;
    sFpsWindowStart = 0.0;
    sFpsWindowFrames = 0;
    sFpsValue = -1;
#endif
    sNextFrameDeadline = 0.0;

    sRunning = true;
    return 0;
}

void Platform_UpdateInput(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        // The developer panel window is drawn by its own renderer and owns its
        // own keyboard focus; events that belong to it are consumed here so they
        // never reach the GBA key mapping (the panel's ENTER must not also be
        // the GBA's START).
        if (Platform_DevPanelHandleEvent(&event))
            continue;

        switch (event.type)
        {
            case SDL_QUIT:
                sRunning = false;
                exit(0);
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    sRunning = false;
                    exit(0);
                }
                if (Platform_DevPanelToggleKey(event.key.keysym.sym))
                    break;
                HandleKeyEvent(event.key.keysym.sym, true);
                break;
            case SDL_KEYUP:
                HandleKeyEvent(event.key.keysym.sym, false);
                break;
        }
    }

    // Write keys to GBA I/O register
    REG_KEYINPUT = sKeyState;
}

void Platform_PresentFrame(const uint16_t *framebuffer)
{
    if (!sRenderer || !sTexture)
        return;

    if (framebuffer == NULL)
        framebuffer = sFramebuffer;

    SDL_UpdateTexture(sTexture, NULL, framebuffer, GBA_SCREEN_WIDTH * sizeof(uint16_t));
    SDL_RenderClear(sRenderer);
    SDL_RenderCopy(sRenderer, sTexture, NULL, NULL);

#ifdef FPS_OVERLAY
    UpdateFpsCounter();
    DrawFpsOverlay();
#endif
    SDL_RenderPresent(sRenderer);
}

void Platform_RenderAndPresent(void)
{
    WaitForFrameDeadline();
    PPU_RenderFrame(sFramebuffer);
    Platform_PresentFrame(sFramebuffer);
}

void Platform_SaveScreenshot(const char *filename)
{
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
        (void *)sFramebuffer,
        GBA_SCREEN_WIDTH,
        GBA_SCREEN_HEIGHT,
        16,
        GBA_SCREEN_WIDTH * sizeof(uint16_t),
        SDL_PIXELFORMAT_BGR555
    );
    if (surface)
    {
        SDL_SaveBMP(surface, filename);
        SDL_FreeSurface(surface);
        printf("[PPU] Saved screenshot to %s\n", filename);
    }
}
void Platform_Cleanup(void)
{
    if (sAudioEnabled)
    {
        SDL_PauseAudioDevice(sAudioDevice, 1);
        SDL_CloseAudioDevice(sAudioDevice);
        sAudioDevice = 0;
        sAudioEnabled = false;
    }
    if (sAudioLock)
    {
        SDL_DestroyMutex(sAudioLock);
        sAudioLock = NULL;
    }

    Platform_DevPanelShutdown();

    if (sTexture)
    {
        SDL_DestroyTexture(sTexture);
        sTexture = NULL;
    }
    if (sRenderer)
    {
        SDL_DestroyRenderer(sRenderer);
        sRenderer = NULL;
    }
    if (sWindow)
    {
        SDL_DestroyWindow(sWindow);
        sWindow = NULL;
    }
    SDL_Quit();
}
