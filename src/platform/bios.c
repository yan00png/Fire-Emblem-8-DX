#include "bios.h"
#include "gba_mem.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

void CpuSet(const void *src, void *dst, uint32_t ctrl)
{
    uint32_t n = ctrl & 0x1FFFFF;
    if ((uintptr_t)src < 0x10000) {   // leitura da BIOS no GBA (lixo/zeros) -> no PS2 seria TLB miss
        int w32_ = (ctrl >> 26) & 1; uint8_t *d_ = dst; uint32_t b_ = n * (w32_ ? 4 : 2); while (b_--) *d_++ = 0; return;
    }
    int fill = (ctrl >> 24) & 1, w32 = (ctrl >> 26) & 1;
    // a BIOS do GBA ignora os bits baixos do endereco (no R5900 seria excecao de alinhamento)
    if (w32) { src = (const void *)((uintptr_t)src & ~3u); dst = (void *)((uintptr_t)dst & ~3u); }
    else     { src = (const void *)((uintptr_t)src & ~1u); dst = (void *)((uintptr_t)dst & ~1u); }
    if (w32) {
        const uint32_t *s = src; uint32_t *d = dst;
        if (fill) { uint32_t v = *s; while (n--) *d++ = v; }
        else      { while (n--) *d++ = *s++; }
    } else {
        const uint16_t *s = src; uint16_t *d = dst;
        if (fill) { uint16_t v = *s; while (n--) *d++ = v; }
        else      { while (n--) *d++ = *s++; }
    }
}

void CpuFastSet(const void *src, void *dst, uint32_t ctrl)
{
    // Sempre 32 bits; a contagem é arredondada para múltiplos de 8 palavras.
    uint32_t n = (ctrl & 0x1FFFFF);
    n = (n + 7) & ~7u;
    if ((uintptr_t)src < 0x10000) { uint32_t *d_ = dst; while (n--) *d_++ = 0; return; }
    const uint32_t *s = (const uint32_t *)((uintptr_t)src & ~3u); uint32_t *d = (uint32_t *)((uintptr_t)dst & ~3u);
    if ((ctrl >> 24) & 1) { uint32_t v = *s; while (n--) *d++ = v; }
    else                  { while (n--) *d++ = *s++; }
}

int LZ77UnCompWram(const void *src, void *dst)
{
    const uint8_t *s = src; uint8_t *d = dst;
    uint32_t hdr = s[0] | (s[1] << 8) | (s[2] << 16) | ((uint32_t)s[3] << 24);
    uint32_t size = hdr >> 8, out = 0;
    if ((hdr & 0xF0) == 0x00) {          // "cópia crua" (dados de banim gerados por gen_banim.py)
        s += 4;
        while (out < size) { d[out] = s[out]; out++; }
        return (int)out;
    }
    if ((hdr & 0xF0) != 0x10) return -1;
    s += 4;
    while (out < size) {
        uint8_t flags = *s++;
        for (int i = 0; i < 8 && out < size; i++, flags <<= 1) {
            if (flags & 0x80) {
                uint32_t len  = (s[0] >> 4) + 3;
                uint32_t disp = (((s[0] & 0xF) << 8) | s[1]) + 1;
                s += 2;
                if (disp > out) return -1;
                while (len-- && out < size) { d[out] = d[out - disp]; out++; }
            } else {
                d[out++] = *s++;
            }
        }
    }
    return (int)out;
}

int LZ77UnCompVram(const void *src, void *dst) { return LZ77UnCompWram(src, dst); }

int32_t Div(int32_t n, int32_t d)    { return d ? n / d : (n < 0 ? -1 : 1); }
int32_t DivRem(int32_t n, int32_t d) { return d ? n % d : n; }

// BgAffineSet / ObjAffineSet: mesma matemática do mGBA (ponto flutuante; a BIOS real usa tabela de seno)
void BgAffineSet(const struct BgAffineSrcData *src, struct BgAffineDstData *dst, int32_t count)
{
    for (int32_t i = 0; i < count; i++, src++, dst++) {
        float ox = src->texX / 256.f, oy = src->texY / 256.f, cx = src->scrX, cy = src->scrY;
        float sx = src->sx / 256.f, sy = src->sy / 256.f;
        float theta = (src->alpha >> 8) / 128.f * 3.14159265358979f;
        float a = cosf(theta), d = a, b = sinf(theta), c = b;
        a *= sx; b *= -sx; c *= sy; d *= sy;
        float rx = ox - (a * cx + b * cy), ry = oy - (c * cx + d * cy);
        dst->pa = (int16_t)(a * 256); dst->pb = (int16_t)(b * 256); dst->pc = (int16_t)(c * 256); dst->pd = (int16_t)(d * 256);
        dst->dx = (int32_t)(rx * 256); dst->dy = (int32_t)(ry * 256);
    }
}

struct ObjAffineSrcData { int16_t sx, sy; uint16_t alpha; };
void ObjAffineSet(const struct ObjAffineSrcData *src, void *dest, int32_t count, int32_t offset)
{
    uint8_t *d = dest;
    for (int32_t i = 0; i < count; i++, src++) {
        float sx = src->sx / 256.f, sy = src->sy / 256.f;
        float theta = (src->alpha >> 8) / 128.f * 3.14159265358979f;
        float a = cosf(theta), dd = a, b = sinf(theta), c = b;
        a *= sx; b *= -sx; c *= sy; dd *= sy;
        *(int16_t *)d = (int16_t)(a * 256);  d += offset;
        *(int16_t *)d = (int16_t)(b * 256);  d += offset;
        *(int16_t *)d = (int16_t)(c * 256);  d += offset;
        *(int16_t *)d = (int16_t)(dd * 256); d += offset;
    }
}

uint16_t Sqrt(uint32_t x) { return (uint16_t)sqrt((double)x); }
int DivArm(int den, int num) { return Div(num, den); }           // DivArm: r0 = divisor, r1 = dividendo
uint16_t ArcTan2(int16_t x, int16_t y)
{
    double a = atan2((double)y, (double)x);                      // 0..65535 = 0..360 graus
    long v = (long)(a / (2.0 * 3.14159265358979) * 65536.0);
    return (uint16_t)(v & 0xFFFF);
}

// Descompressão por run-length (tipo 0x30)
int RLUnCompWram(const void *src, void *dst)
{
    const uint8_t *s = src; uint8_t *d = dst;
    uint32_t hdr = s[0] | (s[1] << 8) | (s[2] << 16) | ((uint32_t)s[3] << 24);
    if ((hdr & 0xF0) != 0x30) return -1;
    uint32_t size = hdr >> 8, out = 0; s += 4;
    while (out < size) {
        uint8_t f = *s++;
        if (f & 0x80) { uint32_t n = (f & 0x7F) + 3; uint8_t v = *s++; while (n-- && out < size) d[out++] = v; }
        else          { uint32_t n = (f & 0x7F) + 1; while (n-- && out < size) d[out++] = *s++; }
    }
    return (int)out;
}
int RLUnCompVram(const void *src, void *dst) { return RLUnCompWram(src, dst); }

// Descompressão Huffman (tipo 0x20; dados de 4 ou 8 bits)
int HuffUnComp(const void *src, void *dst)
{
    const uint8_t *s = src; uint8_t *d = dst;
    uint32_t hdr = s[0] | (s[1] << 8) | (s[2] << 16) | ((uint32_t)s[3] << 24);
    if ((hdr & 0xF0) != 0x20) return -1;
    int bitsPer = hdr & 0xF; uint32_t size = hdr >> 8;
    const uint8_t *tree = s + 4;                                  // tree[0] = tamanho; nó raiz em tree+1
    const uint8_t *bs = s + 4 + (tree[0] + 1) * 2;               // fluxo de bits (palavras de 32 bits, MSB primeiro)
    const uint8_t *node = tree + 1;
    uint32_t out = 0, acc = 0; int nib = 0;
    while (out < size) {
        uint32_t w = bs[0] | (bs[1] << 8) | (bs[2] << 16) | ((uint32_t)bs[3] << 24); bs += 4;
        for (int bit = 31; bit >= 0 && out < size; bit--) {
            int b = (w >> bit) & 1;
            const uint8_t *next = (const uint8_t *)(((uintptr_t)node & ~1u) + (*node & 0x3F) * 2 + 2) + b;
            if (*node & (b ? 0x40 : 0x80)) {                      // folha: *next é o dado
                uint8_t v = *next;
                if (bitsPer == 8) d[out++] = v;
                else { acc |= (uint32_t)(v & 0xF) << (nib * 4); if (++nib == 2) { d[out++] = (uint8_t)acc; acc = 0; nib = 0; } }
                node = tree + 1;
            } else node = next;
        }
    }
    return (int)out;
}

// Controle do sistema
int  MultiBoot(void *param) { (void)param; return 1; }           // falha: sem cabo link no PS2
void SoundBiasReset(void) {}
void SoundBiasSet(void) {}
void SoftReset(uint32_t flags) { (void)flags; }
extern char __ewram_start[], __iwram_start[];
void RegisterRamReset(uint32_t flags)
{
    if (flags & 0x01) memset(__ewram_start, 0, 0x40000);
    if (flags & 0x02) memset(__iwram_start, 0, 0x8000 - 0x200);
    if (flags & 0x04) memset(gGbaPalram, 0, GBA_PLTT_SIZE);
    if (flags & 0x08) memset(gGbaVram, 0, GBA_VRAM_SIZE);
    if (flags & 0x10) memset(gGbaOam, 0, GBA_OAM_SIZE);
}
// Gancho de fim de quadro: o laço principal do port registra aqui o que fazer em cada VBlank.
void (*gPlatformVBlankWait)(void) = 0;
void VBlankIntrWait(void) { if (gPlatformVBlankWait) gPlatformVBlankWait(); }

// SoundMainBTM: no m4a original é usado (via tabela de saltos) como "Clear64byte(void *)"; o header da decomp declara (void),
// por isso a definição fica aqui, fora dos headers dela.
void SoundMainBTM(void *p) { memset(p, 0, 64); }
