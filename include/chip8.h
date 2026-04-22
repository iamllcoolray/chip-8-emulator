#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>

// Fontset (declared here, defined in chip8.c)
extern uint8_t chip8_fontset[80];

typedef struct {
    uint16_t opcode;        // Current opcode (2 bytes)

    uint8_t  memory[4096];  // 4K memory

    uint8_t  V[16];         // 15 general purpose registers V0-VE
                            // V[0xF] is the carry/collision flag

    uint16_t I;             // Index register
    uint16_t pc;            // Program counter (0x000 - 0xFFF)

    // 0x000-0x1FF: Chip-8 interpreter
    // 0x050-0x0A0: Built-in fontset
    // 0x200-0xFFF: Program ROM and work RAM

    uint8_t  gfx[64 * 32];  // 2048 pixels, black or white (1 or 0)

    uint8_t  delay_timer;   // Counts down to 0 at 60Hz
    uint8_t  sound_timer;   // Buzzes when it reaches 0

    uint16_t stack[16];     // 16 levels of stack
    uint16_t sp;            // Stack pointer

    uint8_t  key[16];       // HEX keypad state (0x0 - 0xF)

    uint8_t  draw_flag;     // Set when the screen needs to be redrawn
} chip8;

// Function declarations
void chip8_initialize(chip8 *c);
void chip8_load_game(chip8 *c, const char *filename);
void chip8_emulate_cycle(chip8 *c);
void chip8_set_keys(chip8 *c, const uint8_t *sdl_keystate);

#endif