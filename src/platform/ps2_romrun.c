// Restaura os dados do jogo que a ferramenta tools/romstrip.py removeu do ELF.
// O ELF publicado nao carrega gráficos/mapas/textos: a tabela gRomMap lista (destino, offset na ROM, tamanho);
// no boot os trechos sao copiados da fe8.gba do usuario (ja validada por SHA-1).
#include <stdio.h>
#include <stdint.h>
#include "ps2_log.h"
#include "ps2_romrun.h"

#define ROMMAP_MAX 4096
struct RomRun { uint32_t dst, rom, len; };
// precisa ficar em .data (valor inicial nao zero) para existir no arquivo ELF; a ferramenta preenche run[] e n
struct { char magic[8]; uint32_t n; uint32_t pad; struct RomRun run[ROMMAP_MAX]; }
    gRomMap __attribute__((used, aligned(16))) = { { 'F', 'E', '8', 'R', 'M', 'A', 'P', '1' }, 0, 0, { { 1, 1, 1 } } };

void Fe8LoadProgress(const char *msg, int pct) __attribute__((weak));

int Ps2Rom_Restore(const char *path)
{
    uint32_t n = gRomMap.n;
    if (n == 0) { Ps2Log("romrun: ELF completo, nada a restaurar"); return 1; }
    FILE *f = fopen(path, "rb");
    if (!f) { Ps2Log("romrun: nao abri %s", path); return 0; }
    uint32_t bytes = 0, cur = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        const struct RomRun *r = &gRomMap.run[i];
        if (r->rom != cur && fseek(f, (long)r->rom, SEEK_SET) != 0) { fclose(f); Ps2Log("romrun: seek falhou"); return 0; }
        if (fread((void *)(uintptr_t)r->dst, 1, r->len, f) != r->len) { fclose(f); Ps2Log("romrun: leitura falhou run %u", i); return 0; }
        cur = r->rom + r->len;
        { uint32_t nb = bytes + r->len; if (Fe8LoadProgress && (nb >> 19) != (bytes >> 19)) Fe8LoadProgress("CARREGANDO DADOS DO JOGO", 45 + (int)((uint64_t)nb * 40 / 11243497u)); bytes = nb; }
    }
    fclose(f);
    Ps2Log("romrun: %u trechos, %u bytes restaurados da ROM", n, bytes);
    return 1;
}
