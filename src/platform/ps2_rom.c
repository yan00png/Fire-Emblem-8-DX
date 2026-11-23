#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include "ps2_common.h"
#include "ps2_log.h"
#include "ps2_rom.h"

static int try_open(const char *p) { FILE *f = fopen(p, "rb"); if (f) { fclose(f); return 1; } return 0; }

static int try_names(const char *dir, char *out, size_t n) {
  const char *names[] = { PS2_ROM_NAME, PS2_ROM_NAME_ALT };
  for (int k = 0; k < 2; k++) {
    if (ps2_is_cdrom(dir)) snprintf(out, n, "%s%s;1", dir, k ? "BASEROM.GBA" : "FE8.GBA");
    else                   snprintf(out, n, "%s%s", dir, names[k]);
    if (try_open(out)) return 1;
  }
  return 0;
}

int Ps2Rom_Find(const char *argv0, char *out, size_t n, int wait_seconds) {
  char dir[128];
  if (argv0 && strchr(argv0, ':') && ps2_dir_of(argv0, dir, sizeof dir) && try_names(dir, out, n)) return 1;
  const char *prefixes[] = { "cdrom0:\\", "host:", "mass:/", "mass0:/", "mc0:/", "" };
  for (int attempt = 0; attempt < wait_seconds; attempt++) {
    for (int i = 0; i < 6; i++) if (try_names(prefixes[i], out, n)) return 1;
    sleep(1);
  }
  return 0;
}

// ---- SHA-1 (leitura em pedaços) ----
typedef struct { uint32_t h[5]; uint64_t len; uint8_t buf[64]; uint32_t n; } Sha1;
#define ROL(x,n) (((x)<<(n))|((x)>>(32-(n))))
static void sha1_block(Sha1 *c, const uint8_t *p) {
  uint32_t w[80], a, b, cc, d, e, t;
  for (int i = 0; i < 16; i++) w[i] = ((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|p[i*4+3];
  for (int i = 16; i < 80; i++) w[i] = ROL(w[i-3]^w[i-8]^w[i-14]^w[i-16], 1);
  a=c->h[0]; b=c->h[1]; cc=c->h[2]; d=c->h[3]; e=c->h[4];
  for (int i = 0; i < 80; i++) {
    uint32_t f, k;
    if (i<20) { f=(b&cc)|(~b&d); k=0x5A827999; } else if (i<40) { f=b^cc^d; k=0x6ED9EBA1; }
    else if (i<60) { f=(b&cc)|(b&d)|(cc&d); k=0x8F1BBCDC; } else { f=b^cc^d; k=0xCA62C1D6; }
    t=ROL(a,5)+f+e+k+w[i]; e=d; d=cc; cc=ROL(b,30); b=a; a=t;
  }
  c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d; c->h[4]+=e;
}
static void sha1_init(Sha1 *c) {
  static const uint32_t iv[5] = {0x67452301,0xEFCDAB89,0x98BADCFE,0x10325476,0xC3D2E1F0};
  memcpy(c->h, iv, sizeof iv); c->len = 0; c->n = 0;
}
static void sha1_update(Sha1 *c, const uint8_t *p, size_t len) {
  c->len += len;
  while (len) {
    size_t k = 64 - c->n; if (k > len) k = len;
    memcpy(c->buf + c->n, p, k); c->n += k; p += k; len -= k;
    if (c->n == 64) { sha1_block(c, c->buf); c->n = 0; }
  }
}
static void sha1_final(Sha1 *c, char *out) {
  uint64_t bits = c->len * 8;
  uint8_t pad = 0x80; sha1_update(c, &pad, 1);
  uint8_t z = 0; while (c->n != 56) sha1_update(c, &z, 1);
  uint8_t lb[8]; for (int k = 0; k < 8; k++) lb[7-k] = (uint8_t)(bits >> (8*k));
  sha1_update(c, lb, 8);
  for (int k = 0; k < 5; k++) sprintf(out + k*8, "%08x", (unsigned)c->h[k]);
}

/* progresso da tela de carregamento (definido no main_ps2.c; ausente em builds sem o jogo) */
void Fe8LoadProgress(const char *msg, int pct) __attribute__((weak));

void Ps2Rom_Validate(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) Ps2Fatal("Nao consegui abrir a ROM:", path, NULL, NULL);
  static uint8_t chunk[262144] __attribute__((aligned(64)));
  Sha1 c; sha1_init(&c);
  long total = 0; size_t got;
  while ((got = fread(chunk, 1, sizeof chunk, f)) > 0) {
    sha1_update(&c, chunk, got); total += (long)got;
    if (Fe8LoadProgress && ((total >> 18) & 1) == 0) Fe8LoadProgress("VERIFICANDO A ROM", (int)(total * 45 / PS2_ROM_SIZE));
  }
  fclose(f);
  char hex[41]; sha1_final(&c, hex);
  if (total != (long)PS2_ROM_SIZE || strcmp(hex, PS2_EXPECTED_SHA1) != 0) {
    char b[96], e[96];
    snprintf(b, sizeof b, "Tamanho: %ld bytes (esperado %u)", total, PS2_ROM_SIZE);
    snprintf(e, sizeof e, "sha1 atual: %.20s...", hex);
    Ps2Fatal("ROM nao suportada (so a USA exata).", b, e, NULL);
  }
}

int Ps2Rom_Load(const char *path, const unsigned char **rom, unsigned int *size) {
  FILE *f = fopen(path, "rb");
  if (!f) return 0;
  unsigned char *buf = memalign(64, PS2_ROM_SIZE);
  if (!buf) { fclose(f); return 0; }
  size_t got = 0, want = PS2_ROM_SIZE;
  while (got < want) {
    size_t k = fread(buf + got, 1, (want - got) > 65536 ? 65536 : (want - got), f);
    if (!k) break;
    got += k;
  }
  fclose(f);
  if (got != want) { free(buf); return 0; }
  *rom = buf; *size = (unsigned)got;
  return 1;
}
