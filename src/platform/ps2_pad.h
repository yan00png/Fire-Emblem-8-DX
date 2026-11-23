// Controle do PS2 (libpad) -> registrador KEYINPUT do GBA.
#pragma once
#include <stdint.h>
void Ps2Pad_Init(void);
unsigned short Ps2Pad_GbaKeyInput(void);   // valor para REG_KEYINPUT (bits em 0 = pressionado)
unsigned Ps2Pad_Read(void);                // botoes do PS2 crus (bits PS2B_*, 1 = pressionado), sem remapeamento
enum { PS2B_CROSS, PS2B_CIRCLE, PS2B_SELECT, PS2B_START, PS2B_RIGHT, PS2B_LEFT, PS2B_UP, PS2B_DOWN,
       PS2B_R1, PS2B_L1, PS2B_TRIANGLE, PS2B_SQUARE, PS2B_L2, PS2B_R2 };
#define PAD_REMAP_COUNT 4                  // A, B, L, R
#define PS2B_REMAPPABLE 8                  // X, O, SQUARE, TRIANGLE, L1, R1, L2, R2
extern uint8_t gPadMap[PAD_REMAP_COUNT];
extern const char *const gGbaBtnName[PAD_REMAP_COUNT];
extern const char *const gPs2BtnName[PS2B_REMAPPABLE];
void Ps2Pad_MapLoad(void);
void Ps2Pad_MapSave(void);
void Ps2Pad_MapReset(void);
