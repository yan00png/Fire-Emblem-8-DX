// Salvamento: a SRAM do jogo (gGbaSram) vive em <pasta da ROM>/save/fe8.sav (pendrive via drivers USB, ou host:).
// Carrega ao iniciar; grava quando a SRAM mudou e ficou estavel por ~0,5 s. So usa a pasta "save".
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "ps2_common.h"
#include "ps2_log.h"
#include "ps2_save.h"

extern unsigned char gGbaSram[0x10000];
#define SAVE_SIZE 0x10000

static unsigned char sShadow[SAVE_SIZE];
static char sDir[160], sFile[176];
static int sDirty, sQuiet, sOn;
const char *Ps2SaveDir(void) { return sOn ? sDir : 0; }

void Ps2Save_Init(const char *romPath)
{
    char base[128];
    sOn = 0;
    if (!ps2_dir_of(romPath, base, sizeof base) || ps2_is_cdrom(base)) { Ps2Log("save: midia somente leitura, sem salvamento"); return; }
    snprintf(sDir, sizeof sDir, "%ssave", base);
    snprintf(sFile, sizeof sFile, "%s/fe8.sav", sDir);
    sOn = 1;
    FILE *f = fopen(sFile, "rb");
    if (f) {
        static unsigned char tmp[SAVE_SIZE];
        size_t n = fread(tmp, 1, SAVE_SIZE, f);
        fclose(f);
        if (n >= 0x8000) { memcpy(gGbaSram, tmp, n); Ps2Log("save: carregado %s (%u bytes)", sFile, (unsigned)n); }
        else Ps2Log("save: %s pequeno demais (%u bytes), ignorado", sFile, (unsigned)n);
    } else Ps2Log("save: %s nao existe (novo)", sFile);
    memcpy(sShadow, gGbaSram, SAVE_SIZE);
}

static int Flush(void)
{
    mkdir(sDir, 0777);   // ja existir nao e erro
    FILE *f = fopen(sFile, "wb");
    if (!f) { Ps2Log("save: nao consegui abrir %s para gravar", sFile); return 0; }
    size_t n = fwrite(gGbaSram, 1, SAVE_SIZE, f);
    fclose(f);
    Ps2Log("save: gravado %s (%u bytes)", sFile, (unsigned)n);
    return n == SAVE_SIZE;
}

void Ps2Save_Tick(void)
{
    static int div;
#ifdef SAVE_TEST
    { static int t; if (++t == 600) gGbaSram[0x7000] ^= 1; }
#endif
    if (!sOn || (++div & 7)) return;
    if (memcmp(sShadow, gGbaSram, SAVE_SIZE)) { sDirty = 1; sQuiet = 0; memcpy(sShadow, gGbaSram, SAVE_SIZE); return; }
    if (sDirty && ++sQuiet >= 4) {
        sDirty = 0;
        if (!Flush()) sDirty = 1, sQuiet = -40;   // tenta de novo bem depois (~5 s)
    }
}

int Ps2Cfg_Read(const char *name, void *buf, int max)
{
    char p[200];
    if (!sOn) return 0;
    snprintf(p, sizeof p, "%s/%s", sDir, name);
    FILE *f = fopen(p, "rb");
    if (!f) return 0;
    int n = (int)fread(buf, 1, max, f);
    fclose(f);
    return n;
}

void Ps2Cfg_Write(const char *name, const void *buf, int n)
{
    char p[200];
    if (!sOn) return;
    mkdir(sDir, 0777);
    snprintf(p, sizeof p, "%s/%s", sDir, name);
    FILE *f = fopen(p, "wb");
    if (!f) { Ps2Log("cfg: nao gravou %s", p); return; }
    fwrite(buf, 1, n, f);
    fclose(f);
}
