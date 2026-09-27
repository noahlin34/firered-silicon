#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <SDL.h>

#include "global.h"
#include "gba/m4a_internal.h"
#include "platform/platform.h"
#include "platform/ppu.h"
#include "platform/metrics.h"
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

/* The engine's output is 8-bit signed at ~13.4 kHz and the device runs at its
 * own rate, so the seam between them is this layer's job: it interpolates
 * between engine samples.
 *
 * LINEAR, not nearest-neighbour. A zero-order hold repeats each engine sample
 * 3.6 times at 48 kHz, and that step train leaves strong spectral images: the
 * source band mirrors around the sample rate and folds back down into the
 * audible range. Measured on a real run's buffer, a hold puts 2.27% of the
 * signal's energy above the source Nyquist, with its largest image at 11.9 kHz
 * from a ~1.5 kHz tone -- an audible whistle that the game never contained and
 * that no emulator produces. Linear interpolation drops that to 0.12%, a 19x
 * reduction, for one multiply and one add per output frame.
 *
 * This is not adding detail the GBA lacked. The hardware's own reconstruction
 * is a continuous DAC output with far better anti-imaging than either choice
 * here, so interpolating is the closer approximation and a hold is the option
 * that invents artefacts. A higher-order kernel would be better still, but it
 * needs sample history that crosses vblank boundaries, and the driver hands this
 * layer one vblank at a time.
 */
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
            /* The queue ran dry: the device gets silence rather than a
             * stretched last frame. Counted because a queue that ever starves
             * is the difference between "audio is choppy" and "the mixer is
             * producing silence", and the two have different causes. */
            Platform_MetricsAddCounter(METRICS_COUNTER_AUDIO_UNDERRUNS, 1);
            out[i * 2 + 0] = 0;
            out[i * 2 + 1] = 0;
        }
    }
    SDL_UnlockMutex(sAudioLock);
}

/* One vblank of engine audio, submitted from VBlankIntr.
 *
 * The resampling itself lives in src/platform/audio_resample.c so it can be
 * tested without an audio device (the test binary has no SDL, and sdl2.o is not
 * part of it). This function is the seam: it hands the engine's two PCM halves
 * to the resampler and appends the resulting frames to the queue the SDL
 * callback drains.
 *
 * The queue, not the callback, absorbs the drift between the engine's 59.7275 Hz
 * frame clock and the device's rate: the engine pushes a whole vblank at once,
 * while the callback pulls fixed-size blocks.
 */
static int sAudioPos = 0;

/* One vblank of output is 224 engine samples, about 804 frames at 48 kHz. */
#define AUDIO_RESAMPLE_MAX_FRAMES 2048
static int16_t sAudioResampled[AUDIO_RESAMPLE_MAX_FRAMES * 2];

void Platform_SubmitAudioFrame(const struct SoundInfo *soundInfo)
{
    int samples;
    int rate;
    int written;
    int i;

    if (!sAudioEnabled || !sAudioLock || !soundInfo)
        return;

    samples = soundInfo->pcmSamplesPerVBlank;
    rate = soundInfo->pcmFreq;
    if (samples <= 0 || rate <= 0 || sAudioDeviceRate <= 0)
        return;

    /* The engine writes FIFO A (right) at the start of pcmBuffer and FIFO B
     * (left) at + PCM_DMA_BUF_SIZE; m4aSoundInit programs SOUND_A_RIGHT_OUTPUT
     * and SOUND_B_LEFT_OUTPUT accordingly. */
    written = Platform_ResampleVblank(soundInfo->pcmBuffer,
                                      soundInfo->pcmBuffer + PCM_DMA_BUF_SIZE,
                                      samples, rate, sAudioDeviceRate, &sAudioPos,
                                      sAudioResampled,
                                      AUDIO_RESAMPLE_MAX_FRAMES);

    Platform_MetricsAddCounter(METRICS_COUNTER_AUDIO_IN_FRAMES, (uint32_t)samples * 2);
    Platform_MetricsAddCounter(METRICS_COUNTER_AUDIO_OUT_FRAMES, (uint32_t)written);

    SDL_LockMutex(sAudioLock);
    for (i = 0; i < written; i++)
    {
        int idx;

        if (sAudioCount >= AUDIO_QUEUE_FRAMES)
            break;

        idx = sAudioHead * 2;
        sAudioQueue[idx + 0] = sAudioResampled[i * 2 + 0];
        sAudioQueue[idx + 1] = sAudioResampled[i * 2 + 1];
        sAudioHead = (sAudioHead + 1) % AUDIO_QUEUE_FRAMES;
        sAudioCount++;
    }
    /* The queue depth is a low-water mark: what matters is whether it ever
     * reaches zero, not its average. */
    Platform_MetricsSetGauge(METRICS_GAUGE_AUDIO_QUEUE_MIN, (uint32_t)sAudioCount);
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
    sAudioPos = 0;
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
    // Reaching this branch means the frame overran its deadline, so it is
    // counted: overruns classify the frame, resyncs say how many frames were
    // actually abandoned rather than merely late.
    if (sNextFrameDeadline <= now)
    {
        sNextFrameDeadline = now + sFrameTicks;
        Platform_MetricsAddCounter(METRICS_COUNTER_PACER_RESYNCS, 1);
    }
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
// ---- Top-left development overlay --------------------------------------------
// Development aid: release builds compile it out with FPS_OVERLAY=0 (see the
// Makefile). Sampled at presentation time so it reports frames the player
// actually sees, independent of the engine's internal frame pacing.
//
// Two things are drawn, and both live outside sFramebuffer so a screenshot stays
// byte-identical and overlay-free:
//   line 0  FPS, the presented-frame rate (always)
//   lines 1+  the metrics summary, one line per phase and the busiest gauges,
//             only when --metrics enabled the module
#include "platform/font5x7.h"

static double sTicksPerSecond;
static double sFpsWindowStart;
static uint32_t sFpsWindowFrames;
static int sFpsValue; // -1 until the first full sample window elapses

// How often the overlay's metrics text is rebuilt. The counters it reads are
// updated once per window by the module, so redrawing faster would only burn
// glyph loops; this matches the module's own 0.5 s default cadence closely.
#define FPS_SAMPLE_WINDOW_SECONDS 0.5
#define OVERLAY_FONT_W   FONT5X7_WIDTH
#define OVERLAY_FONT_H   FONT5X7_HEIGHT
#define OVERLAY_ADVANCE  FONT5X7_ADVANCE
#define OVERLAY_LINES    6

static void OverlayGlyph(int x, int y, char ch, int scale)
{
    const uint8_t *rows;
    int row, column;

    if (ch < FONT5X7_FIRST || ch > FONT5X7_LAST)
        ch = '?';
    rows = gFont5x7[(int)ch - FONT5X7_FIRST];

    for (row = 0; row < FONT5X7_HEIGHT; row++)
    {
        for (column = 0; column < FONT5X7_WIDTH; column++)
        {
            SDL_Rect pixel;

            if (!(rows[row] & (1 << (FONT5X7_WIDTH - 1 - column))))
                continue;

            pixel.x = x + column * scale;
            pixel.y = y + row * scale;
            pixel.w = scale;
            pixel.h = scale;
            SDL_RenderFillRect(sRenderer, &pixel);
        }
    }
}

static void OverlayText(int x, int y, const char *text, int scale)
{
    int i;

    for (i = 0; text[i] != '\0'; i++)
        OverlayGlyph(x + i * FONT5X7_ADVANCE * scale, y, text[i], scale);
}

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

// The metrics lines. Built from the module's last completed window: it is the
// same aggregate the stdout window line reports, so the HUD and the log never
// disagree about what the frame cost. Empty when --metrics is off, which keeps
// a default build's overlay to one line.
static int BuildMetricsLines(char lines[OVERLAY_LINES][64])
{
    const struct MetricsSummary *m = Platform_MetricsSummary();

    if (m == NULL || m->frames == 0)
        return 0;

    snprintf(lines[0], 64, "%.2fms  idle %d%%", m->frameMs, (int)(m->idlePercent + 0.5));
    snprintf(lines[1], 64, "ppu %.2f  pres %.2f  vbl %.2f",
             m->phaseMs[METRICS_PHASE_PPU], m->phaseMs[METRICS_PHASE_PRESENT],
             m->phaseMs[METRICS_PHASE_VBLANK]);
    snprintf(lines[2], 64, "sprites %u  drawn %u  affine %u",
             m->gauges[METRICS_GAUGE_SPRITES], m->gauges[METRICS_GAUGE_SPRITE_SCANLINES],
             m->gauges[METRICS_GAUGE_AFFINE_SPRITE_SCANLINES]);
    snprintf(lines[3], 64, "heap %uK free %uK",
             m->gauges[METRICS_GAUGE_HEAP_USED] / 1024,
             m->gauges[METRICS_GAUGE_HEAP_LARGEST_FREE] / 1024);
    snprintf(lines[4], 64, "audio q %u under %u",
             m->gauges[METRICS_GAUGE_AUDIO_QUEUE_MIN], (uint32_t)m->audioUnderrunsPerFrame);
    snprintf(lines[5], 64, "over %u  p95 %.2f", m->overruns, m->p95Ms);

    return OVERLAY_LINES;
}

// Drawn in logical (240x160) renderer space, after the GBA framebuffer, so the
// overlay never lands in the engine framebuffer or saved screenshots.
static void DrawFpsOverlay(void)
{
    char fps[16];
    char metrics[OVERLAY_LINES][64];
    int metricLines;
    int lineHeight = (FONT5X7_HEIGHT + 1);
    int maxChars = 0;
    int width;
    int i;

    if (sFpsValue < 0)
        snprintf(fps, sizeof(fps), "FPS ---");
    else
        snprintf(fps, sizeof(fps), "FPS %d", sFpsValue);

    metricLines = BuildMetricsLines(metrics);

    // One backing bar sized to the widest line, so the text stays legible over
    // any scene without a per-line rectangle.
    maxChars = (int)strlen(fps);
    for (i = 0; i < metricLines; i++)
    {
        int n = (int)strlen(metrics[i]);
        if (n > maxChars)
            maxChars = n;
    }
    width = maxChars * FONT5X7_ADVANCE;

    SDL_SetRenderDrawBlendMode(sRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(sRenderer, 0, 0, 0, 160);
    {
        SDL_Rect background = { 1, 1, width + 2,
                                (metricLines + 1) * lineHeight };
        SDL_RenderFillRect(sRenderer, &background);
    }

    SDL_SetRenderDrawColor(sRenderer, 255, 255, 255, 255);
    OverlayText(2, 2, fps, 1);
    for (i = 0; i < metricLines; i++)
        OverlayText(2, 2 + (i + 1) * lineHeight, metrics[i], 1);

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

    // Metrics read this same counter, so the reported frame time is measured
    // with the clock the pacer uses -- not a second, unrelated time source.
    Platform_MetricsSetClock((uint64_t (*)(void))SDL_GetPerformanceCounter,
                             (uint64_t)frequency);

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
    // Idle, PPU and present are three separate phases of the frame budget and
    // this function is where all three happen: the pacer owns the sleep and the
    // renderer owns the upload. The engine's frame loop closes its frame around
    // this call rather than around its own body.
    Platform_MetricsBegin(METRICS_PHASE_IDLE);
    WaitForFrameDeadline();
    Platform_MetricsEnd(METRICS_PHASE_IDLE);

    Platform_MetricsBegin(METRICS_PHASE_PPU);
    PPU_RenderFrame(sFramebuffer);
    Platform_MetricsEnd(METRICS_PHASE_PPU);

    Platform_MetricsBegin(METRICS_PHASE_PRESENT);
    Platform_PresentFrame(sFramebuffer);
    Platform_MetricsEnd(METRICS_PHASE_PRESENT);
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
