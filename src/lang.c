// Idioma do jogo: ingles (original) ou portugues do Brasil.
// Os textos em portugues ficam FORA do repositorio e do ELF: <pasta da ROM>/save/fe8_ptbr.txt, lido no boot.
// Formato: uma mensagem por linha, "ID=texto" (ID decimal do gMsgTable); '#' comeca comentario.
// No texto: {XX} = byte hexadecimal (codigos de controle, ex.: {01} = quebra de linha, {80}{12}...),
// letras acentuadas em UTF-8 (á é ã ç ...) viram o byte Latin-1 da fonte do jogo.
// Mensagem sem traducao no arquivo cai no ingles original.
// Dump dos textos originais (para traduzir): crie o arquivo vazio save/dump_msgs.flag; ao iniciar o jogo grava save/msgs_en.txt.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "platform/ps2_save.h"
#include "platform/ps2_log.h"

#define MSG_COUNT 3404
#define MSG_MAXLEN 0x554

int gPortLang;                 /* 0 = ingles, 1 = portugues */
int gPortLangDump;             /* 1 enquanto o dump le os textos originais */
static uint8_t *sBuf;
static uint32_t sOff[MSG_COUNT];   /* deslocamento+1 em sBuf; 0 = sem traducao */
static int sLoaded, sCount;

const uint8_t *PortLangGet(int index)
{
    if (gPortLang != 1 || gPortLangDump || !sLoaded || index < 0 || index >= MSG_COUNT || !sOff[index]) return 0;
    return sBuf + sOff[index] - 1;
}
int PortLangAvailable(void) { return sLoaded; }

static int Hex(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* ---- glifos: a fonte US ja tem a maioria dos acentos latinos (posicoes Latin-1); faltam ã õ Ã Õ ---- */
struct Glyph { void *sjisNext; uint8_t sjisByte1, width; uint32_t bitmap[16]; };
#ifdef FE8_GAME
extern struct Glyph *TextGlyphs_Talk[256], *TextGlyphs_System[256];
static struct Glyph sTilde[2][4];
#endif

static void PatchGlyphs(void)
{
#ifdef FE8_GAME
    static const struct { int ch, from, row; } g[4] = { { 0xE3, 0xE4, 1 }, { 0xF5, 0xF6, 1 }, { 0xC3, 0xC4, -1 }, { 0xD5, 0xD6, -1 } };
    struct Glyph **tabs[2] = { TextGlyphs_Talk, TextGlyphs_System };
    for (int t = 0; t < 2; t++)
        for (int k = 0; k < 4; k++)
        {
            struct Glyph *src = tabs[t][g[k].from];
            if (!src || tabs[t][g[k].ch]) continue;
            /* minusculas: trema na linha 4 -> til nas linhas 3-4; maiusculas: trema na linha 2 -> til nas linhas 1-2 */
            int up = g[k].row > 0;                  /* 1 = minuscula */
            int r0 = up ? 3 : 1;
            struct Glyph *d = &sTilde[t][k];
            *d = *src; d->sjisNext = 0;
            d->bitmap[r0 + 1] = 0; d->bitmap[r0] = 0;
            if (!up) d->bitmap[2] = 0;              /* maiuscula: trema esta na linha 2 */
            else d->bitmap[4] = 0;
            d->bitmap[r0] |= (3u << 2) | (3u << 8);
            d->bitmap[r0 + 1] |= (3u << 4) | (3u << 6);
            tabs[t][g[k].ch] = d;
        }
#endif
}

static void Parse(char *txt, int n)
{
    int line = 0; char *p = txt, *end = txt + n;
    sBuf = (uint8_t *)malloc((size_t)n + 16); if (!sBuf) return;
    uint32_t used = 1;
    while (p < end)
    {
        char *e = p; while (e < end && *e != '\n') e++;
        char *nx = e + 1; if (e > p && e[-1] == '\r') e--;
        line++;
        if (p < e && *p != '#')
        {
            char *q = p; int id = 0, ok = 0;
            while (q < e && *q >= '0' && *q <= '9') { id = id * 10 + (*q - '0'); q++; ok = 1; }
            if (ok && q < e && *q == '=' && id < MSG_COUNT)
            {
                uint8_t *o = sBuf + used; int len = 0; q++;
                while (q < e && len < MSG_MAXLEN)
                {
                    unsigned char c = (unsigned char)*q;
                    if (c == '{' && q + 3 < e + 1 && Hex(q[1]) >= 0 && Hex(q[2]) >= 0 && q[3] == '}') { o[len++] = (uint8_t)(Hex(q[1]) * 16 + Hex(q[2])); q += 4; }
                    else if (c >= 0xC2 && c <= 0xC3 && q + 1 < e) { o[len++] = (uint8_t)(((c & 3) << 6) | ((unsigned char)q[1] & 0x3F)); q += 2; }
                    else if (c >= 0x80) q++;                 /* UTF-8 fora do Latin-1: ignora */
                    else { o[len++] = c; q++; }
                }
                o[len] = 0;
                sOff[id] = used + 1; used += (uint32_t)len + 1; sCount++;
            }
            else Ps2Log("ptbr: linha %d invalida", line);
        }
        p = nx;
    }
}

void PortLangInit(void)
{
    enum { MAXF = 1 << 20 };
    char *txt = (char *)malloc(MAXF);
    PatchGlyphs();
    if (!txt) return;
    int n = Ps2Cfg_Read("fe8_ptbr.txt", txt, MAXF - 1);
    if (n > 0) { Parse(txt, n); sLoaded = sCount > 0; Ps2Log("ptbr: %d mensagens", sCount); }
    else Ps2Log("ptbr: save/fe8_ptbr.txt nao encontrado");
    free(txt);
}

/* ---- dump dos textos originais ---- */
#ifdef FE8_GAME
extern char *GetStringFromIndexInBuffer(int index, char *buffer);
extern const uint8_t *const gMsgTable[];

void PortLangDumpMsgs(void)
{
    static char tmp[0x800], line[0x2000];
    char path[200]; extern const char *Ps2SaveDir(void);
    snprintf(path, sizeof path, "%s/msgs_en.txt", Ps2SaveDir());
    FILE *f = fopen(path, "wb");
    if (!f) { Ps2Log("ptbr: nao gravou %s", path); return; }
    gPortLangDump = 1;
    for (int i = 0; i < MSG_COUNT; i++)
    {
        size_t o = 0;
        if (!gMsgTable[i]) continue;
        GetStringFromIndexInBuffer(i, tmp);
        if (!tmp[0]) continue;
        o += (size_t)snprintf(line + o, 16, "%d=", i);
        for (const unsigned char *s = (const unsigned char *)tmp; *s && o < sizeof line - 16; s++)
        {
            if (*s >= 0x20 && *s < 0x7F && *s != '{') line[o++] = (char)*s;
            else if (*s >= 0xA0) { line[o++] = (char)(0xC0 | (*s >> 6)); line[o++] = (char)(0x80 | (*s & 0x3F)); }
            else o += (size_t)snprintf(line + o, 8, "{%02X}", *s);
        }
        line[o++] = '\n';
        fwrite(line, 1, o, f);
    }
    gPortLangDump = 0;
    fclose(f);
    Ps2Log("ptbr: dump concluido");
}
#else
void PortLangDumpMsgs(void) {}
#endif
