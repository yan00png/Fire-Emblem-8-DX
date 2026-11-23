#include <stdio.h>
#include <string.h>
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libpad.h>
#include "ps2_pad.h"
#include "ps2_log.h"
#include "ps2_save.h"

static char sPadBuf[256] __attribute__((aligned(64)));
static int sPadOk = 0;

void Ps2Pad_Init(void)
{
    int ret, tries;
    ret = SifLoadModule("rom0:SIO2MAN", 0, NULL); Ps2Log("rom0:SIO2MAN -> %d", ret);
    ret = SifLoadModule("rom0:PADMAN", 0, NULL);  Ps2Log("rom0:PADMAN -> %d", ret);
    padInit(0);
    if (!padPortOpen(0, 0, sPadBuf)) { Ps2Log("padPortOpen falhou"); return; }
    for (tries = 0; tries < 200; tries++) {                    // espera até ~2 s o controle ficar estável
        int st = padGetState(0, 0);
        if (st == PAD_STATE_STABLE || st == PAD_STATE_FINDCTP1) break;
        { volatile int w; for (w = 0; w < 1000000; w++) { } }
    }
    sPadOk = 1;
    Ps2Log("pad pronto (estado %d)", padGetState(0, 0));
}

uint8_t gPadMap[PAD_REMAP_COUNT] = { 0, 1, 4, 5 };
const char *const gGbaBtnName[PAD_REMAP_COUNT] = { "A", "B", "L", "R" };
const char *const gPs2BtnName[PS2B_REMAPPABLE] = { "X", "O", "SQUARE", "TRIANGLE", "L1", "R1", "L2", "R2" };
static const uint8_t sMapBit[PS2B_REMAPPABLE] = { PS2B_CROSS, PS2B_CIRCLE, PS2B_SQUARE, PS2B_TRIANGLE, PS2B_L1, PS2B_R1, PS2B_L2, PS2B_R2 };

void Ps2Pad_MapReset(void) { gPadMap[0] = 0; gPadMap[1] = 1; gPadMap[2] = 4; gPadMap[3] = 5; }
void Ps2Pad_MapLoad(void)
{
    uint8_t b[8];
    Ps2Pad_MapReset();
    if (Ps2Cfg_Read("controls.cfg", b, sizeof b) < 8 || b[0] != 'F' || b[1] != 'C' || b[2] != 'T' || b[3] != '1') return;
    for (int i = 0; i < PAD_REMAP_COUNT; i++) if (b[4 + i] >= PS2B_REMAPPABLE) return;
    for (int i = 0; i < PAD_REMAP_COUNT; i++) gPadMap[i] = b[4 + i];
}
void Ps2Pad_MapSave(void)
{
    uint8_t b[8] = { 'F', 'C', 'T', '1', gPadMap[0], gPadMap[1], gPadMap[2], gPadMap[3] };
    Ps2Cfg_Write("controls.cfg", b, sizeof b);
}

unsigned Ps2Pad_Read(void)
{
    struct padButtonStatus b;
    unsigned int pressed, k = 0;
    if (!sPadOk || padGetState(0, 0) != PAD_STATE_STABLE || !padRead(0, 0, &b)) return 0;
    pressed = 0xFFFF ^ b.btns;
    if (pressed & PAD_CROSS) k |= 1u << PS2B_CROSS;
    if (pressed & PAD_CIRCLE) k |= 1u << PS2B_CIRCLE;
    if (pressed & PAD_SQUARE) k |= 1u << PS2B_SQUARE;
    if (pressed & PAD_TRIANGLE) k |= 1u << PS2B_TRIANGLE;
    if (pressed & PAD_SELECT) k |= 1u << PS2B_SELECT;
    if (pressed & PAD_START) k |= 1u << PS2B_START;
    if (pressed & PAD_RIGHT) k |= 1u << PS2B_RIGHT;
    if (pressed & PAD_LEFT) k |= 1u << PS2B_LEFT;
    if (pressed & PAD_UP) k |= 1u << PS2B_UP;
    if (pressed & PAD_DOWN) k |= 1u << PS2B_DOWN;
    if (pressed & PAD_R1) k |= 1u << PS2B_R1;
    if (pressed & PAD_L1) k |= 1u << PS2B_L1;
    if (pressed & PAD_L2) k |= 1u << PS2B_L2;
    if (pressed & PAD_R2) k |= 1u << PS2B_R2;
    if (b.mode >> 4 == 0x7) {
        if (b.ljoy_h > 0xC0) k |= 1u << PS2B_RIGHT; else if (b.ljoy_h < 0x40) k |= 1u << PS2B_LEFT;
        if (b.ljoy_v > 0xC0) k |= 1u << PS2B_DOWN; else if (b.ljoy_v < 0x40) k |= 1u << PS2B_UP;
    }
    return k;
}

// GBA: A/B/L/R pelo mapa (padrao X=A, O=B, L1=L, R1=R); Start/Select/direcional fixos
unsigned short Ps2Pad_GbaKeyInput(void)
{
    unsigned p = Ps2Pad_Read(), keys = 0;
    if (p & (1u << sMapBit[gPadMap[0]])) keys |= 1u << 0;
    if (p & (1u << sMapBit[gPadMap[1]])) keys |= 1u << 1;
    if (p & (1u << PS2B_SELECT)) keys |= 1u << 2;
    if (p & (1u << PS2B_START))  keys |= 1u << 3;
    if (p & (1u << PS2B_RIGHT))  keys |= 1u << 4;
    if (p & (1u << PS2B_LEFT))   keys |= 1u << 5;
    if (p & (1u << PS2B_UP))     keys |= 1u << 6;
    if (p & (1u << PS2B_DOWN))   keys |= 1u << 7;
    if (p & (1u << sMapBit[gPadMap[3]])) keys |= 1u << 8;
    if (p & (1u << sMapBit[gPadMap[2]])) keys |= 1u << 9;
    return (unsigned short)(~keys & 0x03FF);
}
