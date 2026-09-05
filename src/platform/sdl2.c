#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL.h>

#include "global.h"
#include "platform/platform.h"
#include "platform/ppu.h"
static SDL_Window *sWindow = NULL;
static SDL_Renderer *sRenderer = NULL;
static SDL_Texture *sTexture = NULL;
static bool sRunning = false;

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
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
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
    SDL_RenderPresent(sRenderer);
}

void Platform_RenderAndPresent(void)
{
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
