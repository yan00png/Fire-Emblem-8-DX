#pragma once
#include <stdint.h>
// Canais 0-3. Chamado quando o jogo grava o registrador de controle (DMAxCNT_H).
// Transferências imediatas executam na hora; DMAs de HBlank ficam registrados
// para o renderizador aplicar por scanline.
struct PlatformDmaChannel { uint32_t src, dst; uint16_t count; uint16_t ctrl; };
void PlatformDmaTrigger(int ch, const void *src, void *dst, uint16_t count, uint16_t ctrl);
const struct PlatformDmaChannel *PlatformDmaHBlank(int ch);   // NULL se não houver
void PlatformDmaClearHBlank(void);
