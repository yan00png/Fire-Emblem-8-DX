/* Menu do port FE8 (Start+Select), baseado no do mzm-ps2. Menu do frontend, desenhado por cima do ultimo quadro do jogo com retangulos (fonte 5x7 propria,
 * sem depender de fonte da BIOS). Coordenadas virtuais 320x224. */
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "menu.h"
#include "gfx_backend.h"
#include "platform/ps2_pad.h"
#include "platform/ps2_save.h"


static const char *const sGlyph[][7] = {
    /* 0-9 */
    {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."},
    {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."},
    {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"},
    {"####.", "....#", "....#", ".###.", "....#", "....#", "####."},
    {"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."},
    {"#####", "#....", "####.", "....#", "....#", "#...#", ".###."},
    {".###.", "#....", "#....", "####.", "#...#", "#...#", ".###."},
    {"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."},
    {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."},
    {".###.", "#...#", "#...#", ".####", "....#", "....#", ".###."},
    /* A-Z */
    {".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"},
    {"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."},
    {".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."},
    {"####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."},
    {"#####", "#....", "#....", "####.", "#....", "#....", "#####"},
    {"#####", "#....", "#....", "####.", "#....", "#....", "#...."},
    {".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###."},
    {"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"},
    {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####"},
    {"..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."},
    {"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"},
    {"#....", "#....", "#....", "#....", "#....", "#....", "#####"},
    {"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"},
    {"#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"},
    {".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."},
    {"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."},
    {".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"},
    {"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"},
    {".####", "#....", "#....", ".###.", "....#", "....#", "####."},
    {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."},
    {"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."},
    {"#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."},
    {"#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"},
    {"#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"},
    {"#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."},
    {"#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"},
    /* . + / : - < > */
    {".....", ".....", ".....", ".....", ".....", ".....", "..#.."},
    {".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."},
    {"....#", "....#", "...#.", "..#..", ".#...", "#....", "#...."},
    {".....", "..#..", ".....", ".....", "..#..", ".....", "....."},
    {".....", ".....", ".....", "#####", ".....", ".....", "....."},
    {"...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#."},
    {".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#..."},
    /* _ , ( ) ! ? */
    {".....", ".....", ".....", ".....", ".....", ".....", "#####"},
    {".....", ".....", ".....", ".....", "..##.", "..#..", ".#..."},
    {"...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#."},
    {".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#..."},
    {"..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."},
    {".###.", "#...#", "....#", "..##.", "..#..", ".....", "..#.."},
    {"##..#", "##..#", "...#.", "..#..", ".#...", "#..##", "#..##"},   /* % */
};

static int GlyphIndex(char c)
{
    c = (char)toupper((unsigned char)c);
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return 10 + c - 'A';
    switch (c)
    {
        case '.': return 36; case '+': return 37; case '/': return 38; case ':': return 39;
        case '-': return 40; case '<': return 41; case '>': return 42;
        case '_': return 43; case ',': return 44; case '(': return 45; case ')': return 46;
        case '!': return 47; case '?': return 48; case '%': return 49;
    }
    return -1;
}

static void Rect(int x, int y, int w, int h, uint32_t rgb, int a)
{
    BeUiRect(x, y, w, h, (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, a);
}

static int TextWidth(const char *s, int sc) { return (int)strlen(s) * 6 * sc; }
static int TextWidthH(const char *s, int sc2) { return (int)strlen(s) * 6 * sc2 / 2; }

/* sc2 = escala em meios: 2 = 1x, 3 = 1.5x, 4 = 2x */
static void TextH(int x, int y, int sc2, uint32_t rgb, const char *s)
{
    for (; *s; s++, x += 6 * sc2 / 2)
    {
        int g = GlyphIndex(*s);
        if (g < 0) continue;
        for (int r = 0; r < 7; r++)
        {
            const char *row = sGlyph[g][r];
            int y0 = y + r * sc2 / 2, h = (r + 1) * sc2 / 2 - r * sc2 / 2;
            for (int c = 0; c < 5;)
            {
                if (row[c] != '#') { c++; continue; }
                int e = c; while (e < 5 && row[e] == '#') e++;
                Rect(x + c * sc2 / 2, y0, e * sc2 / 2 - c * sc2 / 2, h, rgb, 128);
                c = e;
            }
        }
    }
}
static void Text(int x, int y, int sc, uint32_t rgb, const char *s) { TextH(x, y, sc * 2, rgb, s); }


extern void RenderFrame(void);
extern int gBeDbg[5];
#define K(b) (1u << (b))

/* ---- estado ---- */
enum { PAGE_MAIN, PAGE_VIDEO, PAGE_CONTROL, PAGE_DEBUG, PAGE_PERF };
static int sPage, sSel;
static int sPerfText, sFps, sPerf;
static int sSize = 0;       /* 480i/4:3: 0 = 2.0x, 1 = 2.5x, 2 = ajustar */
static const char *const sSizeName[3] = {"2.0X", "2.5X", "FIT"};
static int sVMode = 1;      /* 0 = 160P, 1 = 480I, 2 = 480P, 3 = 1080I */
static int sAspect = 0;     /* 480P/1080I: 0 = 16:9 (laterais pretas), 1 = 4:3, 2 = 3:2, 3 = 16:9 esticado */
static int sAspect480 = 0;  /* 480I: 0 = 16:9 (laterais pretas), 1 = 4:3 (usa SCREEN SIZE) */
static int sColor = 0;      /* 0 = PS2, 1 = correcao de cor do GBA */
static int sFilter = 0;     /* 0 = OFF, 1 = LCD, 2 = SCANLINE */
static const char *const sModeName[4] = {"160P", "480I", "480P", "1080I"};
static const char *const sAspectName[4] = {"16:9 BARS", "4:3", "3:2", "16:9 STR"};
enum { V_MODE, V_SIZE, V_ASPECT, V_FILTER, V_COLOR, V_PERF, V_FPS, V_LANG, V_BACK };
extern int gPortLang;
extern int PortLangAvailable(void);
static int sConfirm = 0, sConfirmLeft = 0, sPrevVMode = 1;   /* confirmacao do 480p/1080i */
static void Draw(void);
static void DrawOverlays(void);

static int VideoRows(int *ids)
{
    int n = 0;
    ids[n++] = V_MODE;
    if (sVMode >= 1) ids[n++] = V_ASPECT;
    if (sVMode == 1 && sAspect480 == 1) ids[n++] = V_SIZE;
    if (sVMode >= 1) ids[n++] = V_FILTER;
    ids[n++] = V_COLOR;
    ids[n++] = V_PERF; ids[n++] = V_FPS; ids[n++] = V_LANG; ids[n++] = V_BACK;
    return n;
}

/* ---- visual no estilo do Fire Emblem 8: janelas azuis em degrade, moldura creme/dourada, mao apontando, caixa de ajuda em pergaminho ---- */
#define C_FRAME   0xF0E8C0   /* creme */
#define C_GOLD    0xC89830
#define C_NAVY    0x081030
#define C_LIME    0xF8D860   /* dourado dos destaques */
#define C_DIM     0xA8B8E8   /* azul claro dos textos secundarios */
#define C_PARCH   0xE8DCB0
#define C_INK     0x483020
#define C_INK_D   0x907050

static uint32_t Lerp(uint32_t a, uint32_t b, int n, int d)
{
    uint32_t r = 0;
    for (int sh = 0; sh < 24; sh += 8)
    {
        int ca = (a >> sh) & 255, cb = (b >> sh) & 255;
        r |= (uint32_t)(ca + (cb - ca) * n / d) << sh;
    }
    return r;
}

static void Gradient(int x, int y, int w, int h, uint32_t top, uint32_t bot)
{
    for (int i = 0; i < h; i += 2) Rect(x, y + i, w, i + 2 > h ? 1 : 2, Lerp(top, bot, i, h), 124);
}

static void Frame(int x, int y, int w, int h)
{
    Rect(x - 3, y - 3, w + 6, h + 6, C_NAVY, 128);           /* contorno escuro */
    Rect(x - 2, y - 2, w + 4, h + 4, C_FRAME, 128);          /* moldura creme */
    Rect(x - 1, y - 1, w + 2, h + 2, C_GOLD, 128);           /* filete dourado */
    Gradient(x, y, w, h, 0x3858B0, 0x101C58);                /* janela azul */
    Rect(x, y, w, 1, 0x88A0E8, 128);                         /* brilho em cima */
    for (int i = 0; i < 4; i++)                              /* cantos ornamentais */
    {
        int cx = (i & 1) ? x + w - 5 : x + 1, cy = (i & 2) ? y + h - 5 : y + 1;
        Rect(cx, cy, 4, 4, C_GOLD, 128); Rect(cx + 1, cy + 1, 2, 2, C_FRAME, 128);
    }
}

static void Tab(int cx, int y, const char *t)
{
    int w = TextWidthH(t, 3) + 28, x = cx - w / 2;
    Rect(x - 2, y - 2, w + 4, 22, C_NAVY, 128);
    Rect(x - 1, y - 1, w + 2, 20, C_FRAME, 128);
    Gradient(x, y, w, 18, 0x2038A0, 0x0C1448);
    Rect(x, y + 17, w, 1, C_GOLD, 128);
    Rect(x + 4, y + 7, 4, 4, C_LIME, 128); Rect(x + w - 8, y + 7, 4, 4, C_LIME, 128);   /* losangos */
    TextH(x + 14, y + 5, 3, 0x000000, t);                    /* sombra */
    TextH(x + 14, y + 4, 3, 0xFFFFFF, t);
}

static void Hand(int x, int y)
{
    static const char *const g[8] = {
        "   ####      ", "  #wwww####  ", "###wwwwwwww#  ", "#wwwwwwwwwww# ",
        "#wwwwwwwwww#  ", "###wwwwwwww#  ", "  #wwww####  ", "   ####      " };
    static int t; int bob = (t++ >> 3) & 3; bob = bob == 3 ? 1 : bob;
    x += bob;
    for (int r = 0; r < 8; r++)
        for (int c = 0; g[r][c];)
        {
            if (g[r][c] == ' ') { c++; continue; }
            int e = c; while (g[r][e] == g[r][c]) e++;
            Rect(x + c, y + r, e - c, 1, g[r][c] == '#' ? 0x302010 : 0xFFFFFF, 128);
            c = e;
        }
}

static void Bar(int x, int y, int w, int h, int sel)
{
    if (sel)
    {
        Rect(x, y, w, h, C_FRAME, 128);
        Gradient(x + 1, y + 1, w - 2, h - 2, 0x78A0F0, 0x2848B8);
        Hand(x + 3, y + (h - 8) / 2);
    }
    else
    {
        Rect(x, y, w, h, 0x0C1448, 70);
        Rect(x, y + h - 1, w, 1, 0x4868C0, 90);              /* filete discreto */
    }
}

static void DrawValue(int xr, int y, const char *v, uint32_t col, uint32_t arrow)
{
    if (v[0] == '<' && v[1] == ' ')
    {
        char in[16]; int n = (int)strlen(v) - 4; if (n < 0) n = 0; if (n > 15) n = 15;
        memcpy(in, v + 2, n); in[n] = 0;
        int x = xr - (n * 9 + 16);
        Text(x, y + 2, 1, arrow, "<");
        TextH(x + 8, y, 3, col, in);
        Text(x + 8 + n * 9 + 2, y + 2, 1, arrow, ">");
    }
    else TextH(xr - TextWidthH(v, 3), y, 3, col, v);
}

/* caixa de ajuda em pergaminho: descricao da opcao (1 linha) + dica de teclas */
static void DescBar(int px, int y, int pw, const char *desc, const char *hint)
{
    Rect(px + 7, y - 1, pw - 14, 21, C_GOLD, 128);
    Rect(px + 8, y, pw - 16, 19, C_PARCH, 128);
    Rect(px + 8, y + 18, pw - 16, 1, 0xC8B880, 128);
    Text(px + 12, y + 3, 1, C_INK, desc);
    Text(px + 12, y + 11, 1, C_INK_D, hint);
}

static void DrawConfirm(void)
{
    const int pw = 260, ph = 90, px = (320 - pw) / 2, py = (224 - ph) / 2;
    Rect(0, 0, 320, 224, 0x000000, 90);
    Frame(px, py, pw, ph);
    char line[40];
    snprintf(line, sizeof line, "KEEP %s?", sModeName[sVMode]);
    Tab(px + pw / 2, py + 8, line);
    const char *l2 = "X: KEEP   ANY OTHER: UNDO";
    Text(px + (pw - TextWidth(l2, 1)) / 2, py + 36, 1, 0xFFFFFF, l2);
    snprintf(line, sizeof line, "REVERTS IN %d", (sConfirmLeft + 59) / 60);
    Text(px + (pw - TextWidth(line, 1)) / 2, py + 52, 1, C_LIME, line);
    const char *l4 = "(480P/1080I NEED A COMPONENT CABLE)";
    Text(px + (pw - TextWidth(l4, 1)) / 2, py + 68, 1, C_DIM, l4);
}

/* descricao curta da opcao selecionada (sem acentos: a fonte so tem A-Z, 0-9 e sinais) */

static const char *Desc(const char *labels[], int sel, char *buf, int bn)
{
    if (sPage == PAGE_MAIN)
    {
        static const char *const d[4] = { "VOLTA PARA O JOGO", "MODO, TAMANHO, ASPECTO, FILTRO E COR", "ATRIBUI OS BOTOES DO CONTROLE", "TESTES DO RENDERIZADOR E TEMPOS" };
        return d[sel];
    }
    if (sPage == PAGE_CONTROL)
    {
        if (sel < PAD_REMAP_COUNT) { snprintf(buf, bn, "ESQ/DIR: BOTAO DO PS2 PARA O %s", gGbaBtnName[sel]); return buf; }
        return sel == PAD_REMAP_COUNT ? "RESTAURA OS BOTOES PADRAO" : "SALVA E VOLTA AO MENU";
    }
    if (sPage == PAGE_DEBUG)
    {
        static const char *const d[8] = { "LIGA/DESLIGA AS CAMADAS DE FUNDO", "LIGA/DESLIGA OS SPRITES", "LIGA/DESLIGA OS SPRITES AFINS",
                                          "LIGA/DESLIGA FADES E EFEITOS DE COR", "OFF: NAO ENVIA TEXTURAS AO GS (TESTE)",
                                          "TEMPOS POR PARTE DO QUADRO", "MOSTRA OS TEMPOS SOBRE O JOGO", "VOLTA AO MENU" };
        return d[sel];
    }
    if (sPage == PAGE_PERF) return "VOLTA AO DEBUG";
    int ids[8]; VideoRows(ids);
    switch (ids[sel])
    {
        case V_MODE: return "ALTERA A RESOLUCAO (480P+: COMPONENTE)";
        case V_SIZE: return "TAMANHO DA IMAGEM NO 480I";
        case V_ASPECT: return "PROPORCAO DA IMAGEM";
        case V_FILTER: return "EFEITO DE TELA: LCD OU SCANLINES";
        case V_COLOR: return "GBA: CORES COMO NA TELA DO GBA";
        case V_PERF: return "BARRAS DE DESEMPENHO NA TELA";
        case V_FPS: return "MOSTRA OS FPS NO CANTO";
        case V_LANG: return PortLangAvailable() ? "IDIOMA DO JOGO: INGLES OU PORTUGUES" : "SEM ARQUIVO DE TRADUCAO NA PASTA SAVE";
        default: return "VOLTA AO MENU";
    }
}

static void Draw(void)
{
    if (sConfirm) { DrawConfirm(); return; }
    Rect(0, 0, 320, 224, 0x000000, 70);
    const int pw = 260, ph = 176, px = (320 - pw) / 2, py = (224 - ph) / 2;
    Frame(px, py, pw, ph);
    const char *title = sPage == PAGE_PERF ? "PERF (MS)" : sPage == PAGE_MAIN ? "MENU" : sPage == PAGE_VIDEO ? "VIDEO" : sPage == PAGE_CONTROL ? "CONTROLS" : "DEBUG GS";
    Tab(px + pw / 2, py + 5, title);
    { int tw = TextWidthH(title, 3) + 28, tx = px + pw / 2 - tw / 2;
      Rect(px + 8, py + 13, tx - px - 14, 2, C_GOLD, 128); Rect(tx + tw + 6, py + 13, px + pw - 8 - (tx + tw + 6), 2, C_GOLD, 128); }

    const char *labels[12]; char vals[12][20];
    int n = 3;
    if (sPage == PAGE_MAIN)
    {
        n = 4;
        labels[0] = "RESUME GAME"; labels[1] = "VIDEO"; labels[2] = "CONTROLS"; labels[3] = "DEBUG GS";
        for (int i = 0; i < 4; i++) vals[i][0] = 0;
    }
    else if (sPage == PAGE_CONTROL)
    {
        n = PAD_REMAP_COUNT + 2;
        for (int i = 0; i < PAD_REMAP_COUNT; i++)
        {
            labels[i] = gGbaBtnName[i];
            snprintf(vals[i], 14, "< %s >", gPs2BtnName[gPadMap[i]]);
        }
        labels[PAD_REMAP_COUNT] = "RESET DEFAULT"; labels[PAD_REMAP_COUNT + 1] = "BACK";
        vals[PAD_REMAP_COUNT][0] = vals[PAD_REMAP_COUNT + 1][0] = 0;
    }
    else if (sPage == PAGE_DEBUG)
    {
        n = 8;
        labels[0] = "BG"; labels[1] = "OBJ"; labels[2] = "AFFINE"; labels[3] = "OVERLAY"; labels[4] = "UPLOADS";
        labels[5] = "PERF (MS)"; labels[6] = "PERF TEXT"; labels[7] = "BACK";
        for (int i = 0; i < 5; i++) strcpy(vals[i], gBeDbg[i] ? "< ON >" : "< OFF >");
        vals[5][0] = 0; strcpy(vals[6], sPerfText ? "< ON >" : "< OFF >"); vals[7][0] = 0;
    }
    else if (sPage == PAGE_PERF)
    {
        n = 1; labels[0] = "BACK"; vals[0][0] = 0;
        char l[48];
        static const char *nm[6] = { "GAME LOGIC", "SYSTEM", "LAYER CACHE", "TEXTURE UPLOAD", "DRAW LIST", "GS + VSYNC" };
        static const int slot[6] = { 4, 5, 0, 1, 2, 3 };
        float busy = 0;
        for (int i = 0; i < 6; i++)
        {
            float ms = BeProfileMs(slot[i]); if (slot[i] != 3) busy += ms;
            int t = (int)(ms * 100 + 0.5f);
            snprintf(l, sizeof l, "%-15s %2d.%02d", nm[i], t / 100, t % 100);
            Text(px + 14, py + 58 + i * 12, 1, i == 5 ? C_DIM : 0xFFFFFF, l);
        }
        float per = BeProfilePeriodMs();
        int tb = (int)(busy * 100 + 0.5f), tp = (int)(per * 100 + 0.5f), fps = per > 0.1f ? (int)(1000.0f / per + 0.5f) : 0;
        snprintf(l, sizeof l, "%-15s %2d.%02d / 16.67", "CPU BUSY", tb / 100, tb % 100);
        Text(px + 14, py + 58 + 6 * 12 + 4, 1, busy > 16.0f ? 0xFF7070 : 0x70FF90, l);
        snprintf(l, sizeof l, "%-15s %2d.%02d  %d FPS", "FRAME", tp / 100, tp % 100, fps);
        Text(px + 14, py + 58 + 7 * 12 + 4, 1, C_LIME, l);
    }
    else
    {
        int ids[12]; n = VideoRows(ids);
        for (int i = 0; i < n; i++)
        {
            vals[i][0] = 0;
            switch (ids[i])
            {
                case V_MODE: labels[i] = "VIDEO MODE"; snprintf(vals[i], 12, "< %s >", sModeName[sVMode]); break;
                case V_SIZE: labels[i] = "SCREEN SIZE"; snprintf(vals[i], 12, "< %s >", sSizeName[sSize]); break;
                case V_ASPECT: labels[i] = "ASPECT"; snprintf(vals[i], 16, "< %s >", sAspectName[sVMode == 1 ? sAspect480 : sAspect]); break;
                case V_COLOR: labels[i] = "COLOR"; strcpy(vals[i], sColor ? "< GBA >" : "< PS2 >"); break;
                case V_FILTER: labels[i] = "FILTER"; { static const char *const fn[3] = { "< OFF >", "< LCD >", "< SCANLINE >" }; strcpy(vals[i], fn[sFilter]); } break;
                case V_FPS: labels[i] = "FPS MONITOR"; strcpy(vals[i], sFps ? "< ON >" : "< OFF >"); break;
                case V_LANG: labels[i] = "LANGUAGE"; strcpy(vals[i], gPortLang ? "< PORTUGUES >" : "< ENGLISH >"); break;
                case V_PERF: labels[i] = "PERF BARS"; strcpy(vals[i], sPerf ? "< ON >" : "< OFF >"); break;
                default: labels[i] = "BACK"; break;
            }
        }
    }
    const int ry = py + 27, pitch = n >= 9 ? 14 : n >= 8 ? 16 : n >= 7 ? 17 : 18;
    for (int i = 0; i < n; i++)
    {
        int y = ry + i * pitch;
        Bar(px + 8, y, pw - 16, pitch >= 18 ? 16 : pitch >= 14 && pitch < 15 ? 13 : 15, i == sSel);
        TextH(px + 31, y + 4, 3, 0x000030, labels[i]); TextH(px + 31, y + 3, 3, 0xFFFFFF, labels[i]);
        if (vals[i][0]) DrawValue(px + pw - 14, y + 3, vals[i], i == sSel ? 0xFFF090 : C_LIME, i == sSel ? 0xFFFFFF : C_DIM);
    }
    char dbuf[48];
    const char *hint = sPage == PAGE_CONTROL ? "ESQ/DIR: ALTERAR   O: SALVAR E VOLTAR"
                     : (sPage == PAGE_VIDEO || sPage == PAGE_DEBUG) ? "ESQ/DIR OU X: ALTERAR   O: VOLTAR"
                     : "CIMA/BAIXO: MOVER   X: OK   O: VOLTAR";
    DescBar(px, py + ph - 21, pw, sPage == PAGE_PERF ? "TEMPOS MEDIOS POR PARTE DO QUADRO" : Desc(labels, sSel, dbuf, sizeof dbuf), hint);
}

/* ---- configuracoes: <pasta da ROM>/save/video.cfg (so com pen-drive/host; em disco ficam so na sessao) ---- */
static void VideoSave(void)
{
    uint8_t b[13] = { 'F', 'V', 'D', '1', (uint8_t)sVMode, (uint8_t)sSize, (uint8_t)sAspect, (uint8_t)sAspect480, (uint8_t)sPerf, (uint8_t)sFps, (uint8_t)sFilter, (uint8_t)sColor, (uint8_t)gPortLang };
    Ps2Cfg_Write("video.cfg", b, sizeof b);
}

static void Apply(void)
{
    BeSetVideoMode(sVMode, sVMode == 1 ? sAspect480 : sAspect, 0); BeSetVideo(sSize, sPerf);
    BeSetScreenFx(sVMode >= 1 ? sFilter : 0);
    BeSetColorFx(sColor);
}

/* monitor de FPS: media dos ultimos 30 quadros, no canto superior esquerdo */
static void DrawFps(void)
{
    static uint32_t ts[31]; static int idx, filled;
    ts[idx] = BeCycles(); idx = (idx + 1) % 31; if (filled < 31) filled++;
    int fps = 0;
    if (filled == 31)
    {
        uint32_t dt = ts[(idx + 30) % 31] - ts[idx];
        if (dt) fps = (int)(30.0f * 294912000.0f / dt + 0.5f);
    }
    char l[8]; snprintf(l, sizeof l, "%d", fps > 99 ? 99 : fps);
    int ux, uy; BeGameToUi(3, 3, &ux, &uy);
    int w = TextWidth(l, 2), h = 14;
    Rect(ux, uy, w + 8, h + 6, 0x000000, 128);
    Text(ux + 4, uy + 3, 2, fps >= 58 ? 0x70FF90 : fps >= 50 ? 0xFFD040 : 0xFF6060, l);
}

static void DrawPerfOverlay(void)
{
    static const char *nm[6] = { "LOGIC", "SYS", "CACHE", "UPLD", "DRAW", "GS" };
    static const int slot[6] = { 4, 5, 0, 1, 2, 3 };
    char l[40];
    float per = BeProfilePeriodMs();
    int fps = per > 0.1f ? (int)(1000.0f / per + 0.5f) : 0, tp = (int)(per * 10 + 0.5f);
    snprintf(l, sizeof l, "%d FPS %d.%dMS", fps, tp / 10, tp % 10);
    Text(6, 8, 1, 0xFFD040, l);
    for (int i = 0; i < 6; i++)
    {
        int t = (int)(BeProfileMs(slot[i]) * 10 + 0.5f);
        snprintf(l, sizeof l, "%-5s %2d.%d", nm[i], t / 10, t % 10);
        Text(6, 18 + i * 9, 1, i == 5 ? 0x8090B8 : 0xFFFFFF, l);
    }
#ifdef FE8_GAME
    {   // lista de procs do jogo (diagnostico de travamento): nome + indice do comando
        struct PD { const void *script, *cur; void *endCb, *idleCb; const char *name; struct PD *parent, *child, *next, *prev; };
        extern struct PD *gProcTreeRootArray[8];
        int line = 0, r;
        {   // estado do jogo: lock, motor de eventos
            extern unsigned char gBmSt[];
            extern void *Proc_Find(const void *script);
            extern const char ProcScr_StdEventEngine[];
            unsigned *ev = (unsigned *)Proc_Find(ProcScr_StdEventEngine);
            snprintf(l, sizeof l, "LOCK %d GFX %d ST %02X", (int)(signed char)gBmSt[1], (int)(signed char)gBmSt[2], gBmSt[4]);
            Text(130, 8 + 15 * 9, 1, 0xFFD040, l);
            if (ev)
            {
                const unsigned char *b = (const unsigned char *)ev;
                const uint16_t *cur = *(const uint16_t **)(b + 0x38);
                snprintf(l, sizeof l, "EV st%04X stall%d x%d t%d", *(uint16_t *)(b + 0x3C), *(uint16_t *)(b + 0x3E), b[0x41], b[0x42]);
                Text(130, 8 + 16 * 9, 1, 0xFFD040, l);
                if ((uintptr_t)cur > 0x100000 && (uintptr_t)cur < 0x2000000)
                { snprintf(l, sizeof l, "OP %04X %04X %04X %04X", cur[0], cur[1], cur[2], cur[3]); Text(130, 8 + 17 * 9, 1, 0xFFD040, l); }
            }
            else Text(130, 8 + 16 * 9, 1, 0xFFD040, "SEM EVENT ENGINE");
            {
                extern const char gProcScr_Talk[], gProcScr_CgTextMain[], gProcScr_BoxDialogue[], gProcScr_E_FACE[];
                snprintf(l, sizeof l, "TALK %d CG %d BOX %d FACE %d", Proc_Find(gProcScr_Talk) != 0, Proc_Find(gProcScr_CgTextMain) != 0, Proc_Find(gProcScr_BoxDialogue) != 0, Proc_Find(gProcScr_E_FACE) != 0);
                Text(130, 8 + 18 * 9, 1, 0xFFD040, l);
            }
        }
        for (r = 0; r < 8 && line < 15; r++)
        {
            struct PD *st[8]; int sp = 0, guard = 0;
            struct PD *p = gProcTreeRootArray[r];
            if (!p) continue;
            st[sp++] = p;
            while (sp > 0 && line < 15 && guard++ < 40)
            {
                p = st[--sp];
                for (; p && line < 15 && guard++ < 40; p = p->prev)
                {
                    char nm2[20];
                    int idx = (p->cur && p->script) ? (int)(((const char *)p->cur - (const char *)p->script) / 8) : -1;
                    snprintf(nm2, sizeof nm2, "S%06X", (unsigned)((uintptr_t)p->script & 0xFFFFFF));
                    snprintf(l, sizeof l, "%d%s %s #%d L%d", r, sp ? "+" : "", nm2, idx, (int)((const unsigned char *)p)[0x28]);
                    Text(130, 8 + line * 9, 1, 0xA0FFA0, l);
                    line++;
                    if (p->child && sp < 8) st[sp++] = p->child;
                }
            }
        }
    }
#endif
}

static void DrawOverlays(void)
{
    if (sFps) DrawFps();
    if (sPerfText) DrawPerfOverlay();
}

/* ---- tela de carregamento (boot): moldura azul/dourada do menu + cinco joias que acendem com o progresso ---- */
static const char *sLoadMsg = "";
static int sLoadPct, sLoadAnim;

static void Diamond(int cx, int cy, int r, uint32_t rgb, int a)
{
    for (int dy = -r; dy <= r; dy++)
    {
        int h = r - (dy < 0 ? -dy : dy);
        Rect(cx - h, cy + dy, 2 * h + 1, 1, rgb, a);
    }
}

static void Gem(int cx, int cy, int r, uint32_t col, int lit)
{
    if (lit) Diamond(cx, cy, r + 5, col, 26);                          /* brilho em volta */
    Diamond(cx, cy, r + 2, C_GOLD, 128);                               /* engaste dourado */
    Diamond(cx, cy, r + 1, 0x201808, 128);
    Diamond(cx, cy, r, lit ? col : 0x141C48, 128);
    if (lit)
    {
        Diamond(cx - r / 4, cy - r / 4, r / 2, Lerp(col, 0xFFFFFF, 1, 2), 128);   /* faceta clara */
        Rect(cx - r / 3 - 1, cy - r / 2, 2, 2, 0xFFFFFF, 128);
    }
    else Rect(cx - r / 2, cy, r, 1, 0x283468, 128);
}

static void DrawLoading(void)
{
    static const uint32_t gc[5] = { 0xE04030, 0x3070E8, 0x40C050, 0xF0D030, 0xA050D8 };
    sLoadAnim++;
    Gradient(0, 0, 320, 224, 0x14226C, 0x04081C);
    /* moldura da tela */
    Rect(6, 6, 308, 212, C_GOLD, 128); Rect(7, 7, 306, 210, 0x0C1448, 128);
    Gradient(8, 8, 304, 208, 0x1A2C8C, 0x060A28);
    Rect(8, 8, 304, 1, 0x88A0E8, 128);
    for (int i = 0; i < 4; i++)
    {
        int cx = (i & 1) ? 311 : 9, cy = (i & 2) ? 215 : 9;
        Diamond(cx, cy, 5, C_GOLD, 128); Diamond(cx, cy, 2, C_FRAME, 128);
    }

    /* titulo */
    { const char *t = "FIRE EMBLEM"; int w = TextWidthH(t, 6), x = (320 - w) / 2;
      TextH(x + 1, 29, 6, 0x000010, t); TextH(x, 27, 6, C_FRAME, t); }
    { const char *t = "THE SACRED STONES"; int w = TextWidthH(t, 4), x = (320 - w) / 2;
      TextH(x + 1, 57, 4, 0x000010, t); TextH(x, 56, 4, C_LIME, t); }
    Rect(40, 80, 112, 1, C_GOLD, 128); Rect(168, 80, 112, 1, C_GOLD, 128);
    Diamond(160, 80, 5, C_GOLD, 128); Diamond(160, 80, 3, C_FRAME, 128);

    /* as cinco joias: acendem de 20 em 20%; a da vez pisca */
    int lit = sLoadPct / 20; if (lit > 5) lit = 5;
    for (int i = 0; i < 5; i++)
    {
        int on = i < lit || (i == lit && lit < 5 && ((sLoadAnim >> 3) & 1));
        Gem(80 + i * 40, 116, i == 2 ? 19 : 12, gc[i], on);
    }
    if (lit >= 3 && ((sLoadAnim >> 2) & 1 || lit == 5)) { Rect(160 - 1, 116 - 26, 2, 7, 0xFFFFFF, 90); Rect(160 - 1, 116 + 20, 2, 7, 0xFFFFFF, 90); }

    /* janela de status com a barra */
    const int px = 30, py = 152, pw = 260, ph = 48;
    Frame(px, py, pw, ph);
    TextH(px + 12, py + 9, 2, 0xFFFFFF, sLoadMsg);
    { char v[8]; snprintf(v, sizeof v, "%d%%", sLoadPct); TextH(px + pw - 12 - TextWidthH(v, 2), py + 9, 2, C_LIME, v); }
    const int bx = px + 12, by = py + 26, bw = pw - 24, bh = 10;
    Rect(bx - 2, by - 2, bw + 4, bh + 4, C_NAVY, 128);
    Rect(bx - 1, by - 1, bw + 2, bh + 2, C_FRAME, 128);
    Rect(bx, by, bw, bh, 0x0C1448, 128);
    int fw = bw * sLoadPct / 100;
    if (fw > 0)
    {
        Gradient(bx, by, fw, bh, 0x78A0F0, 0x2848B8);
        Rect(bx, by, fw, 1, 0xC0D8FF, 128);
        Rect(bx + fw - 1 < bx ? bx : bx + fw - 1, by, 1, bh, C_LIME, 128);
    }
}

/* pct 0..100; msg em maiusculas sem acento (fonte do menu) */
void MenuLoading(const char *msg, int pct)
{
    sLoadMsg = msg; sLoadPct = pct < 0 ? 0 : pct > 100 ? 100 : pct;
    BeSetUiHook(DrawLoading);
    RenderFrame();
}

void MenuLoadingEnd(void)
{
    MenuLoading("PRONTO!", 100);
    for (int i = 0; i < 24; i++) RenderFrame();
    BeSetUiHook(0);
}

void MenuVideoLoad(void)
{
    uint8_t b[16];
    Ps2Pad_MapLoad();
    int n = Ps2Cfg_Read("video.cfg", b, sizeof b);
    if (n < 12 || b[0] != 'F' || b[1] != 'V' || b[2] != 'D' || b[3] != '1') return;
    if (b[4] > 3 || b[5] > 2 || b[6] > 3 || b[7] > 1 || b[10] > 2) return;
    sVMode = b[4]; sSize = b[5]; sAspect = b[6]; sAspect480 = b[7]; sPerf = b[8] & 1; sFps = b[9] & 1; sFilter = b[10]; sColor = b[11] & 1; gPortLang = (n >= 13 && b[12] == 1) ? 1 : 0;
    Apply();
    BeSetUiHook(sFps ? DrawOverlays : 0);
}

void MenuRun(void)
{
    sPage = PAGE_MAIN; sSel = 0;
    BeSetUiHook(Draw);
    BeProfileFreeze(1);
    unsigned prev = Ps2Pad_Read();
    int waitRelease = 1, closing = 0;
    for (;;)
    {
        RenderFrame();                                   /* mesmo quadro do jogo + menu por cima (e vsync) */
        unsigned p = Ps2Pad_Read();
        unsigned e = p & ~prev; prev = p;
        if (sConfirm)
        {
            if (e & K(PS2B_CROSS)) sConfirm = 0;
            else if ((e && !(e & (K(PS2B_UP) | K(PS2B_DOWN)))) || --sConfirmLeft <= 0)
            { sVMode = sPrevVMode; sConfirm = 0; Apply(); }
            continue;
        }
        if (closing) { if (!p) break; continue; }        /* so volta ao jogo com todos os botoes soltos */
        if (waitRelease) { if (!(p & (K(PS2B_START) | K(PS2B_SELECT)))) waitRelease = 0; continue; }

        int vids[12], vn = VideoRows(vids);
        int count = sPage == PAGE_PERF ? 1 : sPage == PAGE_MAIN ? 4 : sPage == PAGE_DEBUG ? 8 : sPage == PAGE_CONTROL ? PAD_REMAP_COUNT + 2 : vn;
        if (e & K(PS2B_UP)) sSel = (sSel + count - 1) % count;
        if (e & K(PS2B_DOWN)) sSel = (sSel + 1) % count;
        int ok = e & K(PS2B_CROSS), back = e & (K(PS2B_CIRCLE) | K(PS2B_START));
        int dir = ((e >> PS2B_RIGHT) & 1) - ((e >> PS2B_LEFT) & 1);

        if (sPage == PAGE_MAIN)
        {
            if (back) closing = 1;
            else if (ok && sSel == 0) closing = 1;
            else if (ok && sSel == 1) { sPage = PAGE_VIDEO; sSel = 0; }
            else if (ok && sSel == 2) { sPage = PAGE_CONTROL; sSel = 0; }
            else if (ok && sSel == 3) { sPage = PAGE_DEBUG; sSel = 0; }
        }
        else if (sPage == PAGE_CONTROL)
        {
            if (back || (ok && sSel == PAD_REMAP_COUNT + 1)) { Ps2Pad_MapSave(); sPage = PAGE_MAIN; sSel = 2; }
            else if (ok && sSel == PAD_REMAP_COUNT) Ps2Pad_MapReset();
            else if (sSel < PAD_REMAP_COUNT && (dir || ok))
                gPadMap[sSel] = (uint8_t)((gPadMap[sSel] + (dir ? dir : 1) + PS2B_REMAPPABLE) % PS2B_REMAPPABLE);
        }
        else if (sPage == PAGE_DEBUG)
        {
            if (back || (ok && sSel == 7)) { sPage = PAGE_MAIN; sSel = 3; }
            else if (ok && sSel == 5) { sPage = PAGE_PERF; sSel = 0; }
            else if (sSel == 6 && (dir || ok)) sPerfText = !sPerfText;
            else if (sSel < 5 && (dir || ok)) gBeDbg[sSel] = !gBeDbg[sSel];
        }
        else if (sPage == PAGE_PERF)
        {
            if (back || ok) { sPage = PAGE_DEBUG; sSel = 5; }
        }
        else
        {
            int id = vids[sSel];
            if (back || (ok && id == V_BACK)) { sPage = PAGE_MAIN; sSel = 1; }
            else if (dir || ok)
            {
                int d = dir ? dir : 1;
                if (id == V_MODE)
                {
                    sPrevVMode = sVMode;
                    sVMode = (sVMode + d + 4) % 4;
                    Apply();
                    if (sVMode >= 2) { sConfirm = 1; sConfirmLeft = 600; }   /* 10 s para confirmar (cabo componente) */
                    sSel = 0;
                }
                else if (id == V_SIZE) { sSize = (sSize + d + 3) % 3; Apply(); }
                else if (id == V_ASPECT) { if (sVMode == 1) sAspect480 ^= 1; else sAspect = (sAspect + d + 4) % 4; Apply(); }
                else if (id == V_COLOR) { sColor ^= 1; Apply(); }
                else if (id == V_FILTER) { sFilter = (sFilter + (dir < 0 ? 2 : 1)) % 3; Apply(); }
                else if (id == V_PERF) { sPerf = !sPerf; Apply(); }
                else if (id == V_FPS) sFps = !sFps;
                else if (id == V_LANG) { extern int sActiveMsg; gPortLang ^= 1; sActiveMsg = -1; }
            }
        }
    }
    BeProfileFreeze(0);
    BeSetUiHook((sPerfText || sFps) ? DrawOverlays : 0);
    VideoSave();
}
