#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "platform/gba_mem.h"
#include "platform/bios.h"
#include "platform/dma.h"
#include "platform/lcd.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALHOU: %s (linha %d)\n", #c, __LINE__); fails++; } } while (0)

int main(void)
{
    // LZ77: cabeçalho 0x10 | tamanho 8; 'A','B' literais, depois cópia (len 6, disp 2) = "ABABABAB"
    uint8_t lz[] = { 0x10, 0x08, 0x00, 0x00,  0x20,  'A', 'B',  0x30, 0x01 };
    uint8_t out[16] = {0};
    CHECK(LZ77UnCompWram(lz, out) == 8);
    CHECK(memcmp(out, "ABABABAB", 8) == 0);
    uint8_t bad[] = { 0x20, 0x08, 0, 0 };
    CHECK(LZ77UnCompWram(bad, out) == -1);

    // CpuSet: cópia 16 bits e preenchimento 32 bits
    uint16_t a[4] = {1,2,3,4}, b[4] = {0};
    CpuSet(a, b, 4); CHECK(b[3] == 4);
    uint32_t f = 0xDEADBEEF, m[4] = {0};
    CpuSet(&f, m, 4 | (1u<<24) | (1u<<26)); CHECK(m[0] == 0xDEADBEEF && m[3] == 0xDEADBEEF);

    // CpuFastSet arredonda para múltiplos de 8 palavras
    static uint32_t src8[8] = {1,2,3,4,5,6,7,8}, dst8[8];
    CpuFastSet(src8, dst8, 3); CHECK(dst8[7] == 8);

    CHECK(Div(10, 3) == 3 && DivRem(10, 3) == 1 && Sqrt(144) == 12);

    // DMA imediato 16 bits para a VRAM; HBlank só registra
    uint16_t pat[4] = {0x1111, 0x2222, 0x3333, 0x4444};
    PlatformDmaTrigger(3, pat, gGbaVram, 4, 0x8000);
    CHECK(((uint16_t *)gGbaVram)[3] == 0x4444);
    PlatformDmaTrigger(0, pat, gGbaIo, 4, 0x8000 | 0x2000);
    CHECK(PlatformDmaHBlank(0) != NULL && ((uint16_t *)gGbaIo)[3] == 0);

    // Endereços literais do GBA mapeiam para os buffers
    CHECK(PlatformGbaAddr(0x06000010) == gGbaVram + 0x10);
    CHECK(PlatformGbaAddr(0x05000002) == gGbaPalram + 2);

    // LCD: modo 0, BG0+BG1 ligados, BG1CNT com prio 2, charBase 1, 8bpp, scrBase 5
    GbaIoWrite16(IO_DISPCNT, 0x0300);
    GbaIoWrite16(IO_BG0CNT + 2, 2 | (1 << 2) | (1 << 7) | (5 << 8));
    GbaIoWrite16(IO_BG0HOFS + 4, 0x105);
    struct LcdState s; LcdDecode(&s);
    CHECK(s.mode == 0 && s.bgOn[0] && s.bgOn[1] && !s.bgOn[2]);
    CHECK(s.bg[1].prio == 2 && s.bg[1].charBase == 1 && s.bg[1].bpp8 == 1 && s.bg[1].scrBase == 5);
    CHECK(s.bg[1].hofs == 0x105);

    printf(fails ? "%d falha(s)\n" : "todos os testes passaram\n", fails);
    return fails != 0;
}
