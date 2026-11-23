// Compara o caminho GS (cache T8/CLUT/atlas + lista de desenho, rasterizada por um GS simulado em C)
// com o renderizador por software de referência, usando dados aleatórios e mudanças incrementais.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform/gba_mem.h"
#include "platform/lcd.h"
#include "render.h"
#include "gfx_cache.h"
#include "render_plan.h"

static struct DrawList sList;

static int FloorDiv(int a, int b) { int q = a / b; if ((a % b) && ((a < 0) != (b < 0))) q--; return q; }
static inline uint32_t Bgr555ToAbgr(uint16_t c)
{
    uint32_t r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
    r = (r << 3) | (r >> 2); g = (g << 3) | (g >> 2); b = (b << 3) | (b >> 2);
    return 0x80000000u | (b << 16) | (g << 8) | r;
}

// "GS" simulado: amostragem nearest no centro do pixel, wrap por máscara, CLUT, alfa 0 = descarta.
static void Raster(const struct DrawList *dl, uint32_t *fb)
{
    uint32_t back = dl->forcedBlank ? 0xFFFFFFFFu : Bgr555ToAbgr(dl->backdrop);
    for (int i = 0; i < 240 * 160; i++) fb[i] = back;
    for (int i = 0; i < dl->n; i++) {
        const struct DrawQuad *q = &dl->q[i];
        const uint8_t *pix; int w, h;
        if (q->tex == TEX_OBJ) { pix = gCache.objAtlas; w = OBJ_ATLAS_W; h = OBJ_ATLAS_H; }
        else { pix = gCache.bg[q->tex].pix; w = gCache.bg[q->tex].w; h = gCache.bg[q->tex].h; }
        int dx = q->x2 - q->x1, dy = q->y2 - q->y1;
        for (int py = (q->y1 < 0 ? 0 : q->y1); py < (q->y2 > 160 ? 160 : q->y2); py++)
        for (int px = (q->x1 < 0 ? 0 : q->x1); px < (q->x2 > 240 ? 240 : q->x2); px++) {
            int u = FloorDiv(q->u1 * 2 * dx + (2 * (px - q->x1) + 1) * (q->u2 - q->u1), 2 * dx);
            int v = FloorDiv(q->v1 * 2 * dy + (2 * (py - q->y1) + 1) * (q->v2 - q->v1), 2 * dy);
            u = ((u % w) + w) % w; v = ((v % h) + h) % h;
            uint32_t c = gCache.clut[q->clut][pix[v * w + u]];
            if (c >> 24) fb[py * 240 + px] = c;
        }
    }
}

static uint32_t sRng = 1;
static uint32_t R(void) { sRng = sRng * 1664525u + 1013904223u; return sRng >> 8; }

static void RandVram(void) { for (int i = 0; i < GBA_VRAM_SIZE; i++) gGbaVram[i] = (R() % 3 == 0) ? 0 : (uint8_t)R(); }
static void RandPal(void)  { for (int i = 0; i < GBA_PLTT_SIZE; i++) gGbaPalram[i] = (uint8_t)R(); }
static void RandOam(int i)
{
    uint16_t *o = (uint16_t *)gGbaOam + i * 4;
    uint16_t a0 = R() & 255;
    if (R() % 8 == 0) a0 |= 0x200;
    if (R() % 12 == 0) a0 |= 0x100;
    if (R() % 12 == 0) a0 |= 2 << 10;
    int shape = (R() % 20 == 0) ? 3 : R() % 3;
    a0 |= shape << 14;
    o[0] = a0;
    o[1] = (R() & 0x1FF) | ((R() & 3) << 12) | ((R() & 3) << 14);
    o[2] = (R() & 0x3FF) | ((R() & 3) << 10) | ((R() & 15) << 12);
}
static void RandCfg(int dispToo)
{
    if (dispToo) {
        uint16_t d = R() % 2;                       // modo 0 ou 1
        d |= (R() & 15) << 8;                       // BGs
        if (R() % 4) d |= 1 << 12;                  // OBJ
        if (R() % 2) d |= 1 << 6;                   // 1D
        if (R() % 30 == 0) d |= 1 << 7;             // forced blank
        GbaIoWrite16(IO_DISPCNT, d);
    }
    for (int b = 0; b < 4; b++)
        GbaIoWrite16(IO_BG0CNT + b * 2, (R() & 3) | ((R() & 3) << 2) | ((R() & 1) << 7) | ((R() & 31) << 8) | ((R() & 3) << 14));
}
static void RandScroll(void) { for (int b = 0; b < 4; b++) { GbaIoWrite16(IO_BG0HOFS + b * 4, R() & 0x1FF); GbaIoWrite16(IO_BG0HOFS + b * 4 + 2, R() & 0x1FF); } }

static int sFail, sFrames;
static void Compare(const char *what, int seed, int frame)
{
    static uint32_t a[256 * 256], b[240 * 160];
    struct LcdState s; LcdDecode(&s);
    RenderFrame(a, 256);
    GfxCacheUpdate(&s);
    PlanFrame(&s, &sList);
    Raster(&sList, b);
    sFrames++;
    for (int y = 0; y < 160; y++) for (int x = 0; x < 240; x++)
        if (a[y * 256 + x] != b[y * 240 + x]) {
            if (sFail < 6) printf("DIFERE [%s] seed=%d quadro=%d em (%d,%d): soft=%08x gs=%08x  modo=%d bgOn=%d%d%d%d obj=%d quads=%d\n",
                what, seed, frame, x, y, a[y * 256 + x], b[y * 240 + x], s.mode, s.bgOn[0], s.bgOn[1], s.bgOn[2], s.bgOn[3], s.objOn, sList.n);
            sFail++; return;
        }
}

int main(void)
{
    for (int seed = 1; seed <= 12; seed++) {
        sRng = seed * 7919u;
        GfxCacheInit();
        RandVram(); RandPal();
        for (int i = 0; i < 128; i++) RandOam(i);
        RandCfg(1); RandScroll();
        Compare("inicial", seed, 0);
        for (int f = 1; f <= 120; f++) {
            switch (R() % 8) {
            case 0: for (int k = R() % 60; k >= 0; k--) gGbaVram[R() % GBA_VRAM_SIZE] = (uint8_t)R(); break;
            case 1: { // mexe no mapa/gráficos de um BG específico
                int b = R() % 4; uint16_t c = GbaIoRead16(IO_BG0CNT + b * 2);
                uint32_t map = ((c >> 8) & 31) * 0x800 + R() % 0x1000, chr = ((c >> 2) & 3) * 0x4000 + R() % 0x4000;
                for (int k = R() % 40; k >= 0; k--) { gGbaVram[(map + R() % 64) % GBA_VRAM_SIZE] = (uint8_t)R(); gGbaVram[(chr + R() % 256) % GBA_VRAM_SIZE] = (uint8_t)R(); }
            } break;
            case 2: for (int k = R() % 30; k >= 0; k--) gGbaPalram[R() % GBA_PLTT_SIZE] = (uint8_t)R(); break;
            case 3: for (int k = R() % 10; k >= 0; k--) RandOam(R() % 128); break;
            case 4: RandScroll(); break;
            case 5: if (R() % 3 == 0) RandCfg(0); break;        // troca charBase/scrBase/tamanho/bpp
            case 6: if (R() % 3 == 0) RandCfg(1); break;        // troca também modo e BGs ligados
            case 7: for (int k = R() % 50; k >= 0; k--) { uint32_t u = 0x10000 + R() % 0x8000; gGbaVram[u] = (uint8_t)R(); } break;   // tiles de OBJ
            }
            Compare("incremental", seed, f);
        }
    }
    printf("%d quadros comparados, %d divergência(s)\n", sFrames, sFail);
    return sFail != 0;
}
