#include "chip8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Fontset: each character is 4px wide, 5px tall
uint8_t chip8_fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

void chip8_initialize(chip8 *c)
{
    c->pc     = 0x200;  // Program counter starts at 0x200
    c->opcode = 0;      // Reset current opcode
    c->I      = 0;      // Reset index register
    c->sp     = 0;      // Reset stack pointer

    // Clear display
    memset(c->gfx,   0, sizeof(c->gfx));

    // Clear stack
    memset(c->stack, 0, sizeof(c->stack));

    // Clear registers V0-VF
    memset(c->V,     0, sizeof(c->V));

    // Clear memory
    memset(c->memory, 0, sizeof(c->memory));

    // Load fontset into memory starting at 0x50
    for (int i = 0; i < 80; i++)
        c->memory[0x50 + i] = chip8_fontset[i];

    // Reset timers
    c->delay_timer = 0;
    c->sound_timer = 0;

    // Clear draw flag
    c->draw_flag = 0;
}

void chip8_load_game(chip8 *c, const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        fprintf(stderr, "Error: could not open file '%s'\n", filename);
        return;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    // Make sure the ROM fits in memory
    if (size > (4096 - 0x200)) {
        fprintf(stderr, "Error: ROM too large to fit in memory (%ld bytes)\n", size);
        fclose(f);
        return;
    }

    // Load ROM into memory starting at 0x200
    fread(c->memory + 0x200, 1, size, f);

    fclose(f);
}

void chip8_emulate_cycle(chip8 *c)
{
    // -------------------------
    // FETCH
    // -------------------------
    c->opcode = c->memory[c->pc] << 8 | c->memory[c->pc + 1];

    // -------------------------
    // DECODE + EXECUTE
    // -------------------------
    switch (c->opcode & 0xF000)
    {
        case 0x0000:
            switch (c->opcode & 0x00FF)
            {
                case 0x00E0: // 0x00E0: Clear the screen
                    memset(c->gfx, 0, sizeof(c->gfx));
                    c->draw_flag = 1;
                    c->pc += 2;
                break;

                case 0x00EE: // 0x00EE: Return from subroutine
                    c->sp--;
                    c->pc = c->stack[c->sp];
                    c->pc += 2;
                break;

                case 0x00FE: // SCHIP: Disable extended screen mode (no-op for now)
                    c->pc += 2;
                break;

                default:
                    fprintf(stderr, "Unknown opcode [0x0000]: 0x%X\n", c->opcode);
            }
        break;

        case 0x1000: // 0x1NNN: Jump to address NNN
            c->pc = c->opcode & 0x0FFF;
        break;

        case 0x2000: // 0x2NNN: Call subroutine at NNN
            c->stack[c->sp] = c->pc;
            c->sp++;
            c->pc = c->opcode & 0x0FFF;
        break;

        case 0x3000: // 0x3XNN: Skip next if VX == NN
            if (c->V[(c->opcode & 0x0F00) >> 8] == (c->opcode & 0x00FF))
                c->pc += 4;
            else
                c->pc += 2;
        break;

        case 0x4000: // 0x4XNN: Skip next if VX != NN
            if (c->V[(c->opcode & 0x0F00) >> 8] != (c->opcode & 0x00FF))
                c->pc += 4;
            else
                c->pc += 2;
        break;

        case 0x5000: // 0x5XY0: Skip next if VX == VY
            if (c->V[(c->opcode & 0x0F00) >> 8] == c->V[(c->opcode & 0x00F0) >> 4])
                c->pc += 4;
            else
                c->pc += 2;
        break;

        case 0x6000: // 0x6XNN: Set VX = NN
            c->V[(c->opcode & 0x0F00) >> 8] = c->opcode & 0x00FF;
            c->pc += 2;
        break;

        case 0x7000: // 0x7XNN: VX += NN (no carry flag)
            c->V[(c->opcode & 0x0F00) >> 8] += c->opcode & 0x00FF;
            c->pc += 2;
        break;

        case 0x8000:
            switch (c->opcode & 0x000F)
            {
                case 0x0000: // 0x8XY0: VX = VY
                    c->V[(c->opcode & 0x0F00) >> 8] = c->V[(c->opcode & 0x00F0) >> 4];
                    c->pc += 2;
                break;

                case 0x0001: // 0x8XY1: VX |= VY
                    c->V[(c->opcode & 0x0F00) >> 8] |= c->V[(c->opcode & 0x00F0) >> 4];
                    c->pc += 2;
                break;

                case 0x0002: // 0x8XY2: VX &= VY
                    c->V[(c->opcode & 0x0F00) >> 8] &= c->V[(c->opcode & 0x00F0) >> 4];
                    c->pc += 2;
                break;

                case 0x0003: // 0x8XY3: VX ^= VY
                    c->V[(c->opcode & 0x0F00) >> 8] ^= c->V[(c->opcode & 0x00F0) >> 4];
                    c->pc += 2;
                break;

                case 0x0004: // 0x8XY4: VX += VY, VF = carry
                    if (c->V[(c->opcode & 0x00F0) >> 4] > (0xFF - c->V[(c->opcode & 0x0F00) >> 8]))
                        c->V[0xF] = 1;
                    else
                        c->V[0xF] = 0;
                    c->V[(c->opcode & 0x0F00) >> 8] += c->V[(c->opcode & 0x00F0) >> 4];
                    c->pc += 2;
                break;

                case 0x0005: // 0x8XY5: VX -= VY, VF = NOT borrow
                    if (c->V[(c->opcode & 0x00F0) >> 4] > c->V[(c->opcode & 0x0F00) >> 8])
                        c->V[0xF] = 0;
                    else
                        c->V[0xF] = 1;
                    c->V[(c->opcode & 0x0F00) >> 8] -= c->V[(c->opcode & 0x00F0) >> 4];
                    c->pc += 2;
                break;

                case 0x0006: // 0x8XY6: VX >>= 1, VF = shifted out bit
                    c->V[0xF] = c->V[(c->opcode & 0x0F00) >> 8] & 0x1;
                    c->V[(c->opcode & 0x0F00) >> 8] >>= 1;
                    c->pc += 2;
                break;

                case 0x0007: // 0x8XY7: VX = VY - VX, VF = NOT borrow
                    if (c->V[(c->opcode & 0x0F00) >> 8] > c->V[(c->opcode & 0x00F0) >> 4])
                        c->V[0xF] = 0;
                    else
                        c->V[0xF] = 1;
                    c->V[(c->opcode & 0x0F00) >> 8] = c->V[(c->opcode & 0x00F0) >> 4] - c->V[(c->opcode & 0x0F00) >> 8];
                    c->pc += 2;
                break;

                case 0x000E: // 0x8XYE: VX <<= 1, VF = shifted out bit
                    c->V[0xF] = c->V[(c->opcode & 0x0F00) >> 8] >> 7;
                    c->V[(c->opcode & 0x0F00) >> 8] <<= 1;
                    c->pc += 2;
                break;

                default:
                    fprintf(stderr, "Unknown opcode [0x8000]: 0x%X\n", c->opcode);
            }
        break;

        case 0x9000: // 0x9XY0: Skip next if VX != VY
            if (c->V[(c->opcode & 0x0F00) >> 8] != c->V[(c->opcode & 0x00F0) >> 4])
                c->pc += 4;
            else
                c->pc += 2;
        break;

        case 0xA000: // 0xANNN: I = NNN
            c->I = c->opcode & 0x0FFF;
            c->pc += 2;
        break;

        case 0xB000: // 0xBNNN: PC = V0 + NNN
            c->pc = (c->opcode & 0x0FFF) + c->V[0];
        break;

        case 0xC000: // 0xCXNN: VX = rand() & NN
            c->V[(c->opcode & 0x0F00) >> 8] = (rand() % 256) & (c->opcode & 0x00FF);
            c->pc += 2;
        break;

        case 0xD000: // 0xDXYN: Draw sprite at (VX, VY), N rows tall
        {
            uint16_t x      = c->V[(c->opcode & 0x0F00) >> 8];
            uint16_t y      = c->V[(c->opcode & 0x00F0) >> 4];
            uint16_t height = c->opcode & 0x000F;
            uint16_t pixel;

            c->V[0xF] = 0;
            for (int yline = 0; yline < height; yline++)
            {
                pixel = c->memory[c->I + yline];
                for (int xline = 0; xline < 8; xline++)
                {
                    if ((pixel & (0x80 >> xline)) != 0)
                    {
                        if (c->gfx[(x + xline + ((y + yline) * 64))] == 1)
                            c->V[0xF] = 1;
                        c->gfx[(x + xline + ((y + yline) * 64))] ^= 1;
                    }
                }
            }

            c->draw_flag = 1;
            c->pc += 2;
        }
        break;

        case 0xE000:
            switch (c->opcode & 0x00FF)
            {
                case 0x009E: // 0xEX9E: Skip next if key VX is pressed
                    if (c->key[c->V[(c->opcode & 0x0F00) >> 8]] != 0)
                        c->pc += 4;
                    else
                        c->pc += 2;
                break;

                case 0x00A1: // 0xEXA1: Skip next if key VX is NOT pressed
                    if (c->key[c->V[(c->opcode & 0x0F00) >> 8]] == 0)
                        c->pc += 4;
                    else
                        c->pc += 2;
                break;

                default:
                    fprintf(stderr, "Unknown opcode [0xE000]: 0x%X\n", c->opcode);
            }
        break;

        case 0xF000:
            switch (c->opcode & 0x00FF)
            {
                case 0x0007: // 0xFX07: VX = delay_timer
                    c->V[(c->opcode & 0x0F00) >> 8] = c->delay_timer;
                    c->pc += 2;
                break;

                case 0x000A: // 0xFX0A: Wait for keypress, store in VX
                {
                    int key_pressed = 0;
                    for (int i = 0; i < 16; i++)
                    {
                        if (c->key[i] != 0)
                        {
                            c->V[(c->opcode & 0x0F00) >> 8] = i;
                            key_pressed = 1;
                        }
                    }
                    // Don't advance pc until a key is pressed
                    if (key_pressed)
                        c->pc += 2;
                }
                break;

                case 0x0015: // 0xFX15: delay_timer = VX
                    c->delay_timer = c->V[(c->opcode & 0x0F00) >> 8];
                    c->pc += 2;
                break;

                case 0x0018: // 0xFX18: sound_timer = VX
                    c->sound_timer = c->V[(c->opcode & 0x0F00) >> 8];
                    c->pc += 2;
                break;

                case 0x001E: // 0xFX1E: I += VX
                    c->I += c->V[(c->opcode & 0x0F00) >> 8];
                    c->pc += 2;
                break;

                case 0x0029: // 0xFX29: I = location of sprite for digit VX
                    c->I = 0x50 + (c->V[(c->opcode & 0x0F00) >> 8] * 5);
                    c->pc += 2;
                break;

                case 0x0033: // 0xFX33: Store BCD of VX at I, I+1, I+2
                    c->memory[c->I]     =  c->V[(c->opcode & 0x0F00) >> 8] / 100;
                    c->memory[c->I + 1] = (c->V[(c->opcode & 0x0F00) >> 8] / 10) % 10;
                    c->memory[c->I + 2] = (c->V[(c->opcode & 0x0F00) >> 8] % 100) % 10;
                    c->pc += 2;
                break;

                case 0x0055: // 0xFX55: Store V0-VX in memory starting at I
                    for (int i = 0; i <= (int)((c->opcode & 0x0F00) >> 8); i++)
                        c->memory[c->I + i] = c->V[i];
                    c->pc += 2;
                break;

                case 0x0065: // 0xFX65: Read V0-VX from memory starting at I
                    for (int i = 0; i <= (int)((c->opcode & 0x0F00) >> 8); i++)
                        c->V[i] = c->memory[c->I + i];
                    c->pc += 2;
                break;

                default:
                    fprintf(stderr, "Unknown opcode [0xF000]: 0x%X\n", c->opcode);
            }
        break;

        default:
            fprintf(stderr, "Unknown opcode: 0x%X\n", c->opcode);
    }

    // -------------------------
    // UPDATE TIMERS
    // -------------------------
    if (c->delay_timer > 0)
        c->delay_timer--;

    if (c->sound_timer > 0)
    {
        if (c->sound_timer == 1)
            printf("BEEP!\n");
        c->sound_timer--;
    }
}