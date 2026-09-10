#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <SDL.h>

#include "global.h"
#include "platform/platform.h"
#include "platform/ppu.h"
static SDL_Window *sWindow = NULL;
static SDL_Renderer *sRenderer = NULL;
static SDL_Texture *sTexture = NULL;
static bool sRunning = false;

static double sFrameTicks;
static double sTicksPerMillisecond;

static double sNextFrameDeadline;

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

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        fprintf(stderr, "Failed to initialize SDL2: %s\n", SDL_GetError());
        return -1;
    }

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
