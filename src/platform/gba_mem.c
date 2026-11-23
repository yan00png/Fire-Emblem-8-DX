#include "gba_mem.h"

// Alinhado em 16 bytes: o EE real exige acesso alinhado (o PCSX2 perdoa, o PS2 não).
uint8_t gGbaPalram[GBA_PLTT_SIZE] __attribute__((aligned(16)));
uint8_t gGbaVram[GBA_VRAM_SIZE]   __attribute__((aligned(0x20000)));   // o jogo calcula indices de tile por (endereco << 17) >> 22: exige VRAM alinhada a 32 KB
uint8_t gGbaOam[GBA_OAM_SIZE]     __attribute__((aligned(16)));
uint8_t gGbaIo[GBA_IO_SIZE]       __attribute__((aligned(16)));
uint8_t gGbaSram[0x10000]         __attribute__((aligned(16)));   // SRAM/flash (salvamento) do jogo
const uint8_t *gGbaRom = 0;
unsigned int gGbaRomSize = 0;

void *PlatformGbaAddr(uint32_t a)
{
    switch (a >> 24) {
    case 0x04: return gGbaIo   + (a & (GBA_IO_SIZE - 1));
    case 0x05: return gGbaPalram + (a & (GBA_PLTT_SIZE - 1));
    case 0x06: { uint32_t o = a & 0x1FFFF; if (o >= GBA_VRAM_SIZE) o -= 0x8000; return gGbaVram + o; }
    case 0x07: return gGbaOam  + (a & (GBA_OAM_SIZE - 1));
    default:   return (void *)(uintptr_t)a; // EWRAM/IWRAM/ROM: ponteiro nativo
    }
}
