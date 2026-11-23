// Definições comuns da camada de plataforma do PS2 (IOP, USB, ROM, log).
#pragma once
#include <stddef.h>
#include <string.h>

#define PS2_ROM_SIZE      0x1000000u                                   // 16 MB
#define PS2_EXPECTED_SHA1 "c25b145e37456171ada4b0d440bf88a19f4d509f"  // Fire Emblem: The Sacred Stones (USA, Australia)
#define PS2_ROM_NAME      "fe8.gba"                                   // nome esperado (também tenta baserom.gba)
#define PS2_ROM_NAME_ALT  "baserom.gba"

// ROM em CD/DVD (caminho começa com "cdrom")? O disco é somente leitura e usa nomes ISO9660 (FE8.GBA;1).
static inline int ps2_is_cdrom(const char *p) { return strncmp(p, "cdrom", 5) == 0; }

// Pasta de um caminho (inclui a barra final): "mass:/fe8.gba" -> "mass:/". Devolve 0 se não há pasta.
static inline int ps2_dir_of(const char *path, char *out, size_t n) {
  size_t L = strlen(path), cut = 0; int found = 0;
  for (size_t i = 0; i < L; i++) if (path[i] == '/' || path[i] == '\\' || path[i] == ':') { cut = i + 1; found = 1; }
  if (!found || cut + 1 > n) return 0;
  memcpy(out, path, cut); out[cut] = 0;
  return 1;
}
