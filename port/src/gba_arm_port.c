// Port para C das rotinas escritas à mão em ARM (asm/arm.s e asm/arm_call.s da decomp do FE8).
// Compilado junto com a decomp (usa os headers dela). Cada função segue o comportamento exato do assembly original,
// inclusive os detalhes que o comentário em C do projeto omite (ex.: TmFillRect faz (width+1)x(height+1)).
#include "global.h"
#include "bmidoten.h"
#include "bmmap.h"

extern u16 gPaletteBuffer[];
extern u8  gFadeComponents[];
extern s8  gFadeComponentStep[];

// ---- ColorFadeTick (arm.s:13) ----
// Para cada paleta (31..0) com passo != 0, soma o passo a cada componente (guardado com viés +0x20) e grava a cor RGB555 clampada.
void CALLARM_ColorFadeTick(void)
{
    int pal, col;
    for (pal = 31; pal >= 0; pal--) {
        s8 step = gFadeComponentStep[pal];
        u8 *c;
        if (step == 0) continue;
        c = gFadeComponents + 0x30 + pal * 0x30;           // fim da paleta (3 bytes por cor, 16 cores)
        for (col = 15; col >= 0; col--) {
            int r, g, b;
            c -= 3;
            c[0] += step; r = c[0] - 0x20; r = r < 0 ? 0 : (r >= 0x20 ? 0x1f : r);
            c[1] += step; g = c[1] - 0x20; g = g < 0 ? 0 : (g >= 0x20 ? 0x1f : g);
            c[2] += step; b = c[2] - 0x20; b = b < 0 ? 0 : (b >= 0x20 ? 0x1f : b);
            gPaletteBuffer[pal * 16 + col] = (u16)(r + (g << 5) + (b << 10));
        }
    }
}

// ---- ClearOam (arm.s:107): põe Y=160 (fora da tela) em cada OBJ; `count` = número de OBJs (o original limpa em grupos de 16) ----
void ClearOAMBuffer(void *buf, int count)
{
    u32 *p = buf;
    int n = (count >> 4) << 4;
    while (n-- > 0) { *p = 160; p += 2; }
}

// ---- Checksum32 (arm.s:~330): soma e xor de meias-palavras ----
u32 ComputeChecksum32(const u32 *buf, int size)
{
    const u16 *p = (const u16 *)buf;
    u32 add = 0, xr = 0;
    int len = size - 2;
    while (len >= 0) { add += *p; xr ^= *p; p++; len -= 2; }
    return (u16)add + (xr << 16);
}

// ---- TmFillRect (arm.s:185): preenche (width+1) x (height+1) entradas, linha de 32 entradas ----
void TileMap_FillRect(u16 *dest, int width, int height, int fill)
{
    int y, x;
    for (y = 0; y <= height; y++) {
        u16 *row = dest + y * 0x20;
        for (x = 0; x <= width; x++) row[x] = (u16)fill;
    }
}

// ---- TmCopyRect (arm.s:215): width x height, linhas de 32 entradas ----
void TileMap_CopyRect(u16 *src, u16 *dst, int width, int height)
{
    int y, x;
    if (width <= 0 || height <= 0) return;
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++) dst[y * 0x20 + x] = src[y * 0x20 + x];
}

// ---- TmApplyTsa (arm.s:275): aplica uma TSA (largura, altura, dados) de baixo para cima, somando `tileref` ----
void CallARM_FillTileRect(u16 *tilemap, const void *tsa_, int tileref)
{
    const u8 *tsa = tsa_;
    int width = tsa[0], height = tsa[1], w, h;
    const u16 *src = (const u16 *)(tsa + 2);
    u16 *dst = tilemap + height * 0x20;
    for (h = height; h >= 0; h--) {
        for (w = width; w >= 0; w--) *dst++ = (u16)(*src++ + tileref);
        dst = dst - width - 1 - 0x20;
    }
}

// ---- PutOamHi / PutOamLo (arm.s:306/364) ----
// gOamHiPutIt/gOamLoPutIt já são declarados em variables.h
int gPutOamCalls, gPutOamObjs, gPutOamLast[4];   // diagnóstico
static void PutOamExt(u16 **pdst, int x, int y, const u16 *list, int oam2)
{
    gPutOamCalls++;
    if (list[0] != 0) { gPutOamObjs += list[0]; gPutOamLast[0] = x; gPutOamLast[1] = y; gPutOamLast[2] = list[0]; gPutOamLast[3] = (int)(intptr_t)(*pdst); }
    u16 *dst = *pdst;
    int count = list[0], i;
    const u16 *src;
    u32 ux = (u32)x & 0xFFFF, uy = (u32)y & 0xFFFF;
    if (count == 0) return;
    src = list + 1;
    *pdst = dst + count * 4;                                  // 8 bytes por OBJ
    for (i = 0; i < count; i++, src += 3, dst += 4) {
        u32 a0 = src[0], a1 = src[1];
        dst[0] = (u16)(((a0 | uy) & 0xFF00) | ((a0 + uy) & 0xFF));
        dst[1] = (u16)(((a1 | ux) & 0xFE00) | ((a1 + ux) & 0x1FF));
        dst[2] = (u16)(src[2] + oam2);
    }
}
// Protecao: lista de OAM invalida (ponteiro pequeno/nulo) -> registra quem chamou (uma vez por origem) e nao desenha.
extern void Ps2Log(const char *, ...);
static int BadOamList(const u16 *l, void *ra, const char *who, int x, int y)
{
    if ((uintptr_t)l >= 0x100000u && (uintptr_t)l < 0x02000000u && !((uintptr_t)l & 1)) return 0;
    static void *seen[16]; static int n;
    for (int i = 0; i < n; i++) if (seen[i] == ra) return 1;
    if (n < 16) seen[n++] = ra;
    Ps2Log("OAMBAD %s lista=%p x=%d y=%d chamador=%p", who, (const void *)l, x, y, ra);
    return 1;
}
void PutOamHi(int x, int y, const u16 *oam_list, int oam2) { if (BadOamList(oam_list, __builtin_return_address(0), "Hi", x, y)) return; PutOamExt((u16 **)&gOamHiPutIt, x, y, oam_list, oam2); }
void PutOamLo(int x, int y, const u16 *oam_list, int oam2) { if (BadOamList(oam_list, __builtin_return_address(0), "Lo", x, y)) return; PutOamExt((u16 **)&gOamLoPutIt, x, y, oam_list, oam2); }

// ---- DrawGlyph (arm.s:384): expande 16 linhas de glifo 2bpp -> 4bpp usando a tabela de paleta `pal` (256 entradas u16) ----
static const u32 sBitTable[8] = { 1u<<0, 1u<<2, 1u<<4, 1u<<6, 1u<<8, 1u<<10, 1u<<12, 1u<<14 };
void DrawGlyph(u16 *pal, u32 *dst, u32 *src, int subx)
{
    int i;
    for (i = 0; i < 16; i++, dst++, src++) {
        u64 ref = (u64)*src * sBitTable[subx];
        u32 lo = (u32)ref, hi = (u32)(ref >> 32);
        dst[0]    |= (u32)pal[lo & 0xFF]         | ((u32)pal[(lo >> 8) & 0xFF] << 16);
        dst[0x10] |= (u32)pal[(lo >> 16) & 0xFF] | ((u32)pal[(lo >> 24) & 0xFF] << 16);
        dst[0x20] |= (u32)pal[hi & 0xFF]         | ((u32)pal[(hi >> 8) & 0xFF] << 16);
    }
}

// ---- DecodeString (arm.s:499): descompressão Huffman das mensagens ----
extern const u32 gMsgHuffmanTable[];
extern const u32 *const gMsgHuffmanTableRoot;
void DecodeString(const u8 *src, u8 *dst)
{
    const u32 *root = gMsgHuffmanTableRoot;
    int bits = 0;
    u32 cur = 0;
    for (;;) {
        const u16 *node = (const u16 *)root;
        u32 entry;
        for (;;) {
            if (--bits < 0) { cur = *src++; bits = 7; }
            node = (const u16 *)(gMsgHuffmanTable + node[cur & 1]);
            cur >>= 1;
            entry = *(const u32 *)node;
            if (entry & 0x80000000u) break;                   // folha
        }
        if (entry & 0xFF00) { dst[0] = (u8)entry; dst[1] = (u8)(entry >> 8); dst += 2; continue; }
        dst[0] = (u8)entry;
        if (entry & 0xFF) { dst++; continue; }
        return;                                               // byte 0 = fim da string
    }
}

// ---- MapFloodCoreStep / MapFloodCore (arm.s:583/721): preenchimento do mapa de movimento ----
void MapFloodCoreStep(int connexion, int xPos, int yPos)
{
    struct MovMapFillState *st = &gMovMapFillState;
    int xsrc = st->src->xPos, ysrc = st->src->yPos;
    int xdst = xPos + xsrc, ydst = yPos + ysrc;
    u32 cost = gWorkingTerrainMoveCosts[gBmMapTerrain[ydst][xdst]] + gWorkingBmMap[ysrc][xsrc];
    if (cost >= gWorkingBmMap[ydst][xdst]) return;
    if (st->hasUnit) {
        u8 uid = gBmMapUnit[ydst][xdst];
        if (uid && ((st->unitId ^ uid) & 0x80)) return;       // não aliado
    }
    if (cost > st->movement) return;
    st->dst->xPos = (u8)xdst;
    st->dst->yPos = (u8)ydst;
    st->dst->connexion = (u8)connexion;
    st->dst->leastMoveCost = (u8)cost;
    st->dst++;
    gWorkingBmMap[ydst][xdst] = (u8)cost;
}

void MapFloodCore(void)
{
    struct MovMapFillState *st = &gMovMapFillState;
    int i = 0;
    for (;;) {
        i ^= 1;
        if (i) { st->src = gMovMapFillStPool1; st->dst = gMovMapFillStPool2; }
        else   { st->src = gMovMapFillStPool2; st->dst = gMovMapFillStPool1; }
        if (st->src->connexion == 4) return;
        for (;;) {
            int c = st->src->connexion;
            if (c == 4) break;                                // fim da lista: troca os "pools"
            switch (c) {                                      // tabela de saltos do original (índice = connexion)
            case 0: MapFloodCoreStep(3, 0, -1); MapFloodCoreStep(2, 0, 1); MapFloodCoreStep(0, -1, 0); break;
            case 1: MapFloodCoreStep(3, 0, -1); MapFloodCoreStep(2, 0, 1); MapFloodCoreStep(1, 1, 0); break;
            case 2: MapFloodCoreStep(2, 0, 1); MapFloodCoreStep(0, -1, 0); MapFloodCoreStep(1, 1, 0); break;
            case 3: MapFloodCoreStep(3, 0, -1); MapFloodCoreStep(0, -1, 0); MapFloodCoreStep(1, 1, 0); break;
            case 5: MapFloodCoreStep(3, 0, -1); MapFloodCoreStep(2, 0, 1); MapFloodCoreStep(0, -1, 0); MapFloodCoreStep(1, 1, 0); break;
            }
            st->dst->connexion = 4;
            st->src++;
        }
    }
}
