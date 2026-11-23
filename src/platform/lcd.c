#include "lcd.h"
#include "gba_mem.h"

void LcdDecode(struct LcdState *o)
{
    uint16_t d = GbaIoRead16(IO_DISPCNT);
    o->mode = d & 7;
    o->obj1D = (d >> 6) & 1;
    o->forcedBlank = (d >> 7) & 1;
    for (int i = 0; i < 4; i++) o->bgOn[i] = (d >> (8 + i)) & 1;
    o->objOn = (d >> 12) & 1; o->win0On = (d >> 13) & 1; o->win1On = (d >> 14) & 1; o->objWinOn = (d >> 15) & 1;
    for (int i = 0; i < 4; i++) {
        uint16_t c = GbaIoRead16(IO_BG0CNT + i * 2);
        o->bg[i].prio = c & 3; o->bg[i].charBase = (c >> 2) & 3; o->bg[i].bpp8 = (c >> 7) & 1;
        o->bg[i].scrBase = (c >> 8) & 31; o->bg[i].size = (c >> 14) & 3;
        o->bg[i].hofs = GbaIoRead16(IO_BG0HOFS + i * 4) & 0x1FF;
        o->bg[i].vofs = GbaIoRead16(IO_BG0HOFS + i * 4 + 2) & 0x1FF;
    }
    o->bldcnt = GbaIoRead16(IO_BLDCNT); o->bldalpha = GbaIoRead16(IO_BLDALPHA); o->bldy = GbaIoRead16(IO_BLDY) & 0x1F;
    o->backdrop = *(uint16_t *)gGbaPalram;
}

int LcdBgActive(const struct LcdState *s, int b)
{
    if (s->mode == 0) return s->bgOn[b];
    if (s->mode == 1) return b < 2 && s->bgOn[b];
    return 0;
}
