/* Saida de audio do PS2 (libsd + audsrv) e ponte ROM <-> motor de som m4a.
 * O motor (port/src/m4a_engine.c) entrega as FIFOs A e B (8 bits) na taxa do jogo (13379..42048 Hz);
 * aqui: reamostra (cubica) para 48 kHz, soma o PSG (ps2_psg.c, ja a 48 kHz), roteia L/R conforme SOUNDCNT_H e
 * ajusta levemente a taxa para manter o buffer do audsrv estavel. */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <audsrv.h>
#include <sbv_patches.h>
#include "gba_mem.h"
#include "ps2_psg.h"

extern unsigned char libsd_irx[], audsrv_irx[];
extern unsigned int size_libsd_irx, size_audsrv_irx;

/* ---- dados de som: faixa da ROM que contem voicegroups, amostras, tabela de musicas e musicas ---- */
#define SND_ROM_BASE 0x206000u
#define SND_ROM_END  0x590000u
#define SND_TABLE_ROM_OFFSET 0x224470u
#define SND_TABLE_COUNT 1000
static uint8_t *sSnd;
static uint8_t sZero[64] __attribute__((aligned(16)));

/* converte endereco bruto da ROM do GBA (0x08xxxxxx) em ponteiro real; outros valores passam sem alteracao */
void *Ps2Audio_Resolve(uint32_t v)
{
    if (v >= 0x08000000u && v < 0x0E000000u)
    {
        uint32_t off = v & 0x01FFFFFFu;
        if (sSnd && off >= SND_ROM_BASE && off < SND_ROM_END) return sSnd + (off - SND_ROM_BASE);
        return sZero;
    }
    return (void *)(uintptr_t)v;
}

struct SongRaw { uint32_t header; uint16_t ms, me; };
extern void Ps2Audio_SongTableSet(int i, void *header, unsigned ms, unsigned me);

void Fe8LoadProgress(const char *msg, int pct) __attribute__((weak));

int Ps2Audio_LoadSoundData(const char *romPath)
{
#ifdef NO_SND_LOAD
    return 0;
#endif
    FILE *f = fopen(romPath, "rb");
    if (!f) return 0;
    uint32_t len = SND_ROM_END - SND_ROM_BASE;
    sSnd = memalign(64, len);
    if (!sSnd) { fclose(f); return 0; }
    if (fseek(f, (long)SND_ROM_BASE, SEEK_SET) != 0) { fclose(f); free(sSnd); sSnd = 0; return 0; }
    for (uint32_t pos = 0; pos < len; )
    {
        uint32_t n = len - pos > 0x80000 ? 0x80000 : len - pos;
        if (fread(sSnd + pos, 1, n, f) != n) { fclose(f); free(sSnd); sSnd = 0; return 0; }
        pos += n;
        if (Fe8LoadProgress) Fe8LoadProgress("CARREGANDO AUDIO", 85 + (int)((uint64_t)pos * 15 / len));
    }
    fclose(f);
    const uint8_t *t = sSnd + (SND_TABLE_ROM_OFFSET - SND_ROM_BASE);
    for (int i = 0; i < SND_TABLE_COUNT; i++)
    {
        struct SongRaw e; memcpy(&e, t + i * 8, 8);
        Ps2Audio_SongTableSet(i, Ps2Audio_Resolve(e.header), e.ms, e.me);
    }
    return 1;
}

/* ---- saida ---- */
#define OUT_RATE 48000
static int sAudioOk;

int Ps2Audio_Init(void)
{
    int ret = 0;
    SifInitRpc(0);
    sbv_patch_enable_lmb();
    int a = SifExecModuleBuffer(libsd_irx, size_libsd_irx, 0, NULL, &ret);
    int b = SifExecModuleBuffer(audsrv_irx, size_audsrv_irx, 0, NULL, &ret);
    if (a < 0 || b < 0) return 0;
    if (audsrv_init() != 0) return 0;
    audsrv_fmt_t fmt = { .freq = OUT_RATE, .bits = 16, .channels = 2 };
    if (audsrv_set_format(&fmt) != 0) return 0;
    audsrv_set_volume(MAX_VOLUME);
    Ps2Psg_Reset();
    sAudioOk = 1;
    return 1;
}

int Ps2Audio_Enabled(void) { return sAudioOk; }

int gAudioVol[3] = { 100, 100, 100 };
int gAudioMono;
static int AudioGain(int pct)
{
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct * pct * 256 / 10000;
}

static inline int Cubic(int p0, int p1, int p2, int p3, int t)
{
    int c1 = p2 - p0;
    int c2 = 2 * p0 - 5 * p1 + 4 * p2 - p3;
    int c3 = 3 * (p1 - p2) + p3 - p0;
    int v = ((c3 * t) >> 12) + c2;
    v = ((v * t) >> 12) + c1;
    v = ((v * t) >> 12);
    return p1 + (v >> 1);
}

static inline int Clip16(int v)
{
    int m = v < 0 ? -v : v;
    if (m > 22000)
    {
        int e = m - 22000;
        m = 22000 + (10767 * e) / (e + 10767);
    }
    return v < 0 ? -m : m;
}

#define MAXIN 1024
void Ps2Audio_Submit(const int8_t *fifoA, const int8_t *fifoB, unsigned n, unsigned rate)
{
    static int bufA[3 + MAXIN], bufB[3 + MAXIN];
    static int64_t pos = 1 << 16;
    static short out[2048 * 2] __attribute__((aligned(64)));
    static int fill = -1;

    if (!sAudioOk || !n || !rate || n > MAXIN) return;

    Ps2Psg_Frame(OUT_RATE);

    for (unsigned i = 0; i < n; i++)
    {
        bufA[3 + i] = fifoA[i] * 256;
        bufB[3 + i] = fifoB[i] * 256;
    }

    int step = (int)(((int64_t)rate << 16) / OUT_RATE);
    {
        int q = audsrv_queued();
        if (q >= 0)
        {
            int frames = q >> 2;
            fill = fill < 0 ? frames : (fill * 7 + frames) >> 3;
            const int target = 900;
            int e = ((fill - target) * 1310) / target;
            if (e > 1310) e = 1310;
            if (e < -1310) e = -1310;
            step = (int)(((int64_t)step * (65536 + e)) >> 16);
        }
    }

    unsigned ctl = gGbaIo[0x82] | (gGbaIo[0x83] << 8);
    int master = (gGbaIo[0x84] & 0x80) != 0;
    int gA = (ctl & 4) ? 128 : 64, gB = (ctl & 8) ? 128 : 64;
    int aL = (ctl >> 9) & 1, aR = (ctl >> 8) & 1, bL = (ctl >> 13) & 1, bR = (ctl >> 12) & 1;
    int psg = master && Ps2Psg_Active();
    const int vM = AudioGain(gAudioVol[0]), vD = AudioGain(gAudioVol[1]), vP = AudioGain(gAudioVol[2]);

    unsigned m = 0;
    while ((pos >> 16) <= (int64_t)n && m < 2048)
    {
        int k = (int)(pos >> 16);
        int t = (int)((pos >> 4) & 0xFFF);
        int a = Cubic(bufA[k - 1], bufA[k], bufA[k + 1], bufA[k + 2], t);
        int b = Cubic(bufB[k - 1], bufB[k], bufB[k + 1], bufB[k + 2], t);
        int l = 0, r = 0;
        if (master)
        {
            a = (((a * gA) >> 8) * vD) >> 8;
            b = (((b * gB) >> 8) * vD) >> 8;
            if (aL) l += a;
            if (aR) r += a;
            if (bL) l += b;
            if (bR) r += b;
            if (psg)
            {
                int pl, pr;
                Ps2Psg_Render(&pl, &pr);
                l += (pl * vP) >> 8;
                r += (pr * vP) >> 8;
            }
        }
        if (gAudioMono) l = r = (l + r) >> 1;
        l = (l * vM) >> 8; r = (r * vM) >> 8;
        out[m * 2] = (short)Clip16(l);
        out[m * 2 + 1] = (short)Clip16(r);
        m++;
        pos += step;
    }

    pos -= (int64_t)n << 16;
    if (pos < (1 << 16)) pos = 1 << 16;
    for (int i = 0; i < 3; i++)
    {
        bufA[i] = bufA[n + i];
        bufB[i] = bufB[n + i];
    }

#ifdef AUDIO_DUMP
    { static FILE *f; static unsigned total; if (!f) f = fopen("host:audio_dump.raw", "wb"); if (f && total < (12u << 20)) { fwrite(out, 4, m, f); total += m * 4; if (total >= (12u << 20)) fclose(f); } }
#endif
    if (m)
    {
        int room = audsrv_available() & ~3, bytes = (int)(m * 4);
        if (bytes > room) bytes = room;
        if (bytes > 0) audsrv_play_audio((const char *)out, bytes);
    }
}
