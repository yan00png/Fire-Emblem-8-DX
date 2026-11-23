// Buffers reais na RAM que substituem a memória mapeada do GBA.
// O jogo (decomp) nunca deve usar endereços literais (0x04000000...): use estes símbolos.
#pragma once
#include <stdint.h>
#include <stddef.h>

#define GBA_PLTT_SIZE  0x400
#define GBA_VRAM_SIZE  0x18000
#define GBA_OAM_SIZE   0x400
#define GBA_IO_SIZE    0x400

extern uint8_t gGbaPalram[GBA_PLTT_SIZE];
extern uint8_t gGbaVram[GBA_VRAM_SIZE];
extern uint8_t gGbaOam[GBA_OAM_SIZE];
extern uint8_t gGbaIo[GBA_IO_SIZE];

// ROM (16 MB) carregada da USB/disco para a RAM. NULL se ainda não carregada.
extern const uint8_t *gGbaRom;
extern unsigned int gGbaRomSize;

// Região do mapa de endereços do GBA (usada por DMA/CpuSet com ponteiros "GBA").
// No port, ponteiros nativos já valem; esta tradução só serve para endereços literais.
void *PlatformGbaAddr(uint32_t gba_addr);

// Acesso a registradores de I/O por offset (ex.: 0x00 = DISPCNT).
static inline uint16_t GbaIoRead16(uint32_t off)           { return *(volatile uint16_t *)(gGbaIo + off); }
static inline void     GbaIoWrite16(uint32_t off, uint16_t v) { *(volatile uint16_t *)(gGbaIo + off) = v; }
static inline uint32_t GbaIoRead32(uint32_t off)           { return *(volatile uint32_t *)(gGbaIo + off); }
static inline void     GbaIoWrite32(uint32_t off, uint32_t v) { *(volatile uint32_t *)(gGbaIo + off) = v; }

// Ligação com o decomp: em include/gba/io_reg.h trocar
//   #define REG_BASE 0x4000000
// por
//   #define REG_BASE ((uintptr_t)gGbaIo)
// e, em include/gba/defines.h,
//   #define PLTT ((uintptr_t)gGbaPalram), VRAM ((uintptr_t)gGbaVram), OAM ((uintptr_t)gGbaOam)
