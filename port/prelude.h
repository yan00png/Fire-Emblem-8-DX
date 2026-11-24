// Incluído à força em todo arquivo da decomp (-include). O agbcc trazia libc implícita; aqui vem do newlib do ps2dev.
#pragma once
#include <string.h>
#include <stddef.h>

/* No GBA, ler/escrever por ponteiro NULL cai na BIOS/IWRAM baixa e e inofensivo; no PS2 e TLB miss.
   FE8_U(p) devolve p, ou uma unidade zerada quando p e NULL (para lookups que podem falhar: GetUnitFromCharId etc.). */
static unsigned char gFe8NullUnitBuf[256] __attribute__((aligned(8)));
#define FE8_U(p) ({ __typeof__(p) fe8_p_ = (p); fe8_p_ ? fe8_p_ : (__typeof__(p))(void *)gFe8NullUnitBuf; })
