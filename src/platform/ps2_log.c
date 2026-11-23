#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <kernel.h>
#include <debug.h>
#include "ps2_common.h"
#include "ps2_log.h"

/* Sem log em arquivo: os erros aparecem na tela (ps2_crash.c). Mantido como no-op para nao mexer nos pontos de chamada. */
void Ps2Log(const char *fmt, ...) { (void)fmt; }
void Ps2Log_OpenFor(const char *rom_path) { (void)rom_path; }

void Ps2Fatal(const char *l1, const char *l2, const char *l3, const char *l4) {
  init_scr();
  scr_printf("\n  Fire Emblem 8 PS2 (port em andamento)\n\n  ERRO\n\n");
  if (l1) scr_printf("  %s\n", l1);
  if (l2) scr_printf("  %s\n", l2);
  if (l3) scr_printf("  %s\n", l3);
  if (l4) scr_printf("  %s\n", l4);
  scr_printf("\n  Use a ROM Fire Emblem: The Sacred Stones (USA)\n  sha1 %s\n  (16777216 bytes) com o nome %s\n", PS2_EXPECTED_SHA1, PS2_ROM_NAME);
  SleepThread();
}
