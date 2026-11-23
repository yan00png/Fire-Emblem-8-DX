// BIOS do GBA reimplementada em C. Entradas/saídas seguem o hardware original.
#pragma once
#include <stdint.h>

// Controle de CpuSet/CpuFastSet: bits 0-20 = contagem, bit 24 = fill, bit 26 = 32 bits (CpuSet)
void CpuSet(const void *src, void *dst, uint32_t ctrl);
void CpuFastSet(const void *src, void *dst, uint32_t ctrl);

// Descompressão LZ77 (cabeçalho 0x10). Retorna bytes escritos, ou -1 se inválido.
// A variante Vram escreve em unidades de 16 bits (como o GBA); aqui o resultado é idêntico.
int LZ77UnCompWram(const void *src, void *dst);
int LZ77UnCompVram(const void *src, void *dst);

int32_t  Div(int32_t num, int32_t den);
int32_t  DivRem(int32_t num, int32_t den);
uint16_t Sqrt(uint32_t x);
uint16_t ArcTan2(int16_t x, int16_t y);
int DivArm(int den, int num);
int RLUnCompWram(const void *src, void *dst);
int RLUnCompVram(const void *src, void *dst);
int HuffUnComp(const void *src, void *dst);
int MultiBoot(void *param);
void SoftReset(uint32_t flags);
void RegisterRamReset(uint32_t flags);
void VBlankIntrWait(void);
void SoundBiasReset(void);
void SoundBiasSet(void);
extern void (*gPlatformVBlankWait)(void);

struct BgAffineSrcData { int32_t texX, texY; int16_t scrX, scrY; int16_t sx, sy; uint16_t alpha; };
struct BgAffineDstData { int16_t pa, pb, pc, pd; int32_t dx, dy; };
void BgAffineSet(const struct BgAffineSrcData *src, struct BgAffineDstData *dst, int32_t count);
