#include "dma.h"
#include <string.h>

#define DMA_ENABLE   0x8000
#define DMA_REPEAT   0x0200
#define DMA_32BIT    0x0400
#define DMA_TIMING   0x3000   // 00 imediato, 01 VBlank, 10 HBlank, 11 especial
#define DMA_SRC_FIX  0x0100
#define DMA_DST_MASK 0x0060   // 00 inc, 01 dec, 10 fixo, 11 inc+reload

static struct PlatformDmaChannel sHBlank[4];
static uint8_t sHBlankOn[4];

static void RunDma(const void *src, void *dst, uint16_t count, uint16_t ctrl, int ch)
{
    uint32_t n = count ? count : (ch == 3 ? 0x10000 : 0x4000);
    int w = (ctrl & DMA_32BIT) ? 4 : 2;
    int sfix = (ctrl & DMA_SRC_FIX) != 0;
    int dmode = (ctrl & DMA_DST_MASK) >> 5;
    /* o DMA do GBA ignora os bits baixos dos enderecos (no R5900 seria excecao de alinhamento) */
    const uint8_t *s = (const uint8_t *)((uintptr_t)src & ~(uintptr_t)(w - 1)); uint8_t *d = (uint8_t *)((uintptr_t)dst & ~(uintptr_t)(w - 1));
    for (uint32_t i = 0; i < n; i++) {
        if (w == 4) *(uint32_t *)d = *(const uint32_t *)s; else *(uint16_t *)d = *(const uint16_t *)s;
        if (!sfix) s += w;
        if (dmode == 0 || dmode == 3) d += w; else if (dmode == 1) d -= w;
    }
}

void PlatformDmaTrigger(int ch, const void *src, void *dst, uint16_t count, uint16_t ctrl)
{
    if (ch < 0 || ch > 3) return;
    if (!(ctrl & DMA_ENABLE)) { sHBlankOn[ch] = 0; return; }
    switch ((ctrl & DMA_TIMING) >> 12) {
    case 0:  RunDma(src, dst, count, ctrl, ch); break;           // imediato
    case 2:  sHBlank[ch] = (struct PlatformDmaChannel){ (uint32_t)(uintptr_t)src, (uint32_t)(uintptr_t)dst, count, ctrl };
             sHBlankOn[ch] = 1; break;                           // HBlank: o renderizador aplica
    default: RunDma(src, dst, count, ctrl, ch); break;           // VBlank/especial: imediato por ora
    }
}

const struct PlatformDmaChannel *PlatformDmaHBlank(int ch) { return (ch >= 0 && ch < 4 && sHBlankOn[ch]) ? &sHBlank[ch] : 0; }
void PlatformDmaClearHBlank(void) { memset(sHBlankOn, 0, sizeof sHBlankOn); }

// Interface usada pelas macros DmaSet/DmaStop da decomp. ctrl32 = contagem (bits 0-15) | controle (bits 16-31).
void PlatformDmaSet(int ch, const void *src, void *dst, unsigned int ctrl32)
{
    uint16_t count = (uint16_t)(ctrl32 & 0xFFFF), ctrl = (uint16_t)(ctrl32 >> 16);
    PlatformDmaTrigger(ch, src, dst, count, ctrl);
}
void PlatformDmaStop(int ch)
{
    if (ch >= 0 && ch < 4) sHBlankOn[ch] = 0;
}

// DMA de HBlank (gradientes, janelas): o renderizador consulta o valor da linha y para um registrador (so 16 bits, fonte incremental).
int PlatformHblankValue(uintptr_t addr, int y, uint16_t *out)
{
    for (int c = 0; c < 4; c++) {
        if (!sHBlankOn[c] || (sHBlank[c].ctrl & DMA_32BIT)) continue;
        uintptr_t dst = (uintptr_t)sHBlank[c].dst; int cnt = sHBlank[c].count ? sHBlank[c].count : 0x10000;
        int dmode = (sHBlank[c].ctrl & DMA_DST_MASK) >> 5;
        if (addr < dst || addr >= dst + (uintptr_t)cnt * 2 || (addr & 1)) continue;
        if (dmode == 2) { *out = ((const uint16_t *)(uintptr_t)sHBlank[c].src)[(sHBlank[c].ctrl & DMA_SRC_FIX) ? 0 : y * cnt]; return 1; }   // destino fixo (1 registrador por linha)
        *out = ((const uint16_t *)(uintptr_t)sHBlank[c].src)[y * cnt + (addr - dst) / 2];
        return 1;
    }
    return 0;
}
