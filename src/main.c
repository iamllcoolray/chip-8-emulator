#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <SDL2/SDL.h>

#include "chip8.h"

// CHIP-8 display is 64x32, we scale it up for visibility
#define SCALE       10
#define SCREEN_W    (64 * SCALE)
#define SCREEN_H    (32 * SCALE)

// How many cycles to run per frame (60fps target)
// Tweak this to control emulation speed
#define CYCLES_PER_FRAME 10

// -------------------------------------------------------
// Keypad mapping:
//
//   CHIP-8       Keyboard
//   +-+-+-+-+    +-+-+-+-+
//   |1|2|3|C|    |1|2|3|4|
//   |4|5|6|D|    |Q|W|E|R|
//   |7|8|9|E| => |A|S|D|F|
//   |A|0|B|F|    |Z|X|C|V|
//   +-+-+-+-+    +-+-+-+-+
// -------------------------------------------------------
static SDL_Scancode keymap[16] = {
    SDL_SCANCODE_X,    // 0
    SDL_SCANCODE_1,    // 1
    SDL_SCANCODE_2,    // 2
    SDL_SCANCODE_3,    // 3
    SDL_SCANCODE_Q,    // 4
    SDL_SCANCODE_W,    // 5
    SDL_SCANCODE_E,    // 6
    SDL_SCANCODE_A,    // 7
    SDL_SCANCODE_S,    // 8
    SDL_SCANCODE_D,    // 9
    SDL_SCANCODE_Z,    // A
    SDL_SCANCODE_C,    // B
    SDL_SCANCODE_4,    // C
    SDL_SCANCODE_R,    // D
    SDL_SCANCODE_F,    // E
    SDL_SCANCODE_V,    // F
};

void chip8_set_keys(chip8 *c, const uint8_t *sdl_keystate)
{
    for (int i = 0; i < 16; i++)
        c->key[i] = sdl_keystate[keymap[i]] ? 1 : 0;
}

void draw_graphics(chip8 *c, SDL_Renderer *renderer)
{
    // Clear screen to black
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Draw white pixels for every set bit in gfx[]
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (int y = 0; y < 32; y++)
    {
        for (int x = 0; x < 64; x++)
        {
            if (c->gfx[(y * 64) + x])
            {
                SDL_Rect rect = {
                    .x = x * SCALE,
                    .y = y * SCALE,
                    .w = SCALE,
                    .h = SCALE
                };
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }

    SDL_RenderPresent(renderer);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <rom_file>\n", argv[0]);
        return 1;
    }

    // -------------------------------------------------------
    // SDL setup
    // -------------------------------------------------------
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
    {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "CHIP-8",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H,
        SDL_WINDOW_SHOWN
    );
    if (!window)
    {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer)
    {
        fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // -------------------------------------------------------
    // CHIP-8 setup
    // -------------------------------------------------------
    chip8 myChip8;
    chip8_initialize(&myChip8);
    chip8_load_game(&myChip8, argv[1]);

    // -------------------------------------------------------
    // Emulation loop
    // -------------------------------------------------------
    int running = 1;
    SDL_Event event;

    while (running)
    {
        // Handle events
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                running = 0;

            if (event.type == SDL_KEYDOWN)
                if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                    running = 0;
        }

        // Update key state
        const uint8_t *keystate = SDL_GetKeyboardState(NULL);
        chip8_set_keys(&myChip8, keystate);

        // Run several cycles per frame
        for (int i = 0; i < CYCLES_PER_FRAME; i++)
            chip8_emulate_cycle(&myChip8);

        // Redraw if needed
        if (myChip8.draw_flag)
        {
            draw_graphics(&myChip8, renderer);
            myChip8.draw_flag = 0;
        }

        // ~60fps — VSYNC handles this but SDL_Delay is a fallback
        SDL_Delay(16);
    }

    // -------------------------------------------------------
    // Cleanup
    // -------------------------------------------------------
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}