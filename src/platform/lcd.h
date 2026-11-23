// Decodifica os registradores de vídeo gravados pelo jogo em gGbaIo (via FlushLCDControl).
#pragma once
#include <stdint.h>

#define IO_DISPCNT 0x000
#define IO_BG0CNT  0x008
#define IO_BG0HOFS 0x010
#define IO_WIN0H   0x040
#define IO_BLDCNT  0x050
#define IO_BLDALPHA 0x052
#define IO_BLDY    0x054

struct LcdState {
    uint8_t  mode;                      // 0-5
    uint8_t  bgOn[4], objOn, win0On, win1On, objWinOn, forcedBlank, obj1D;
    struct { uint8_t prio, charBase, scrBase, bpp8, size; uint16_t hofs, vofs; } bg[4];
    uint16_t bldcnt, bldalpha; uint8_t bldy;
    uint16_t backdrop;                  // palram[0], BGR555
};
void LcdDecode(struct LcdState *out);

// BG ativo para o modo atual (modo 0: BG0-3; modo 1: BG0-1 de tiles; BG afim ainda não suportado).
int LcdBgActive(const struct LcdState *s, int b);
