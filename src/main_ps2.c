#include <stdio.h>
#include "gfx_backend.h"
#include "gfx_cache.h"
#include "platform/lcd.h"
#include "platform/gba_mem.h"
#include "platform/ps2_platform.h"
#include "platform/ps2_rom.h"
#include "platform/ps2_log.h"
#ifdef DBG_STATE
#include <stdarg.h>
#include <stdio.h>
static void DbgLog(const char *f, ...) { static FILE *fp; va_list a; if (!fp) fp = fopen("host:state.txt", "w"); if (!fp) return; va_start(a, f); vfprintf(fp, f, a); va_end(a); fputc(10, fp); fflush(fp); }
#undef Ps2Log
#define Ps2Log DbgLog
#endif
#include "platform/ps2_pad.h"
#include "platform/ps2_save.h"

extern void RenderFrame(void);

#ifdef FE8_GAME
#include "platform/bios.h"
extern uint16_t gBG0TilemapBuffer[], gBG1TilemapBuffer[], gBG2TilemapBuffer[];
extern void *gBGVramTilemapPointers[];
extern void AgbMain(void);
extern void *gIRQHandlers[];
// "Interrupção" de VBlank do GBA: roda o handler registrado pelo jogo (OnVBlank = lógica + cópia dos buffers), depois desenha.
static int sFrameNo;
// Diagnóstico: lista a árvore de processos do jogo (nome, posição no script, callback de idle)
struct ProcDbg { const void *script, *cur; void *endCb, *idleCb; const char *name; struct ProcDbg *parent, *child, *next, *prev; };
extern struct ProcDbg *gProcTreeRootArray[8];
extern uint16_t gOam[];
extern int gPutOamCalls, gPutOamObjs, gPutOamLast[4];
extern void *gOamLoPutIt; extern uint32_t *gOamHiPutIt;
static void DumpProcs(struct ProcDbg *p, int depth, int *budget)
{
    for (; p && (*budget)-- > 0; p = p->prev) {
        int idx = p->cur && p->script ? (int)(((const char *)p->cur - (const char *)p->script) / 8) : -1;
        Ps2Log("   %*sproc '%s' cmd#%d idle=%p", depth * 2, "", p->name ? p->name : "?", idx, p->idleCb);
        if (p->name && p->name[0] == 'e' && p->name[1] == 'k' && p->name[2] == 'r' && p->name[3] == 's') {   // diagnostico: campos do ekrsubAnimeEmulator
            const uint32_t *w = (const uint32_t *)p;
            Ps2Log("   RAW %08x %08x %08x %08x %08x %08x %08x %08x", w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
            Ps2Log("   RAW %08x %08x %08x %08x %08x %08x %08x %08x", w[8], w[9], w[10], w[11], w[12], w[13], w[14], w[15]);
            Ps2Log("   RAW %08x %08x %08x %08x %08x %08x %08x %08x", w[16], w[17], w[18], w[19], w[20], w[21], w[22], w[23]);
            const uint32_t *sc = (const uint32_t *)(uintptr_t)w[17];
            if (sc && ((uintptr_t)sc & 3) == 0 && (uintptr_t)sc < 0x02000000) Ps2Log("   SCR %08x %08x %08x %08x %08x %08x %08x %08x", sc[0], sc[1], sc[2], sc[3], sc[4], sc[5], sc[6], sc[7]);
        }
        if (p->child) DumpProcs(p->child, depth + 1, budget);
    }
}
// Diagnóstico: resume o estado de vídeo do jogo em alguns quadros (vai para fe8_log.txt)
static void PortTrace(const struct LcdState *l)
{
    int nzv = 0, nzp = 0, nzo = 0, i;
    for (i = 0; i < GBA_VRAM_SIZE; i++) nzv += gGbaVram[i] != 0;
    for (i = 0; i < GBA_PLTT_SIZE; i++) nzp += gGbaPalram[i] != 0;
    for (i = 0; i < GBA_OAM_SIZE; i += 8) nzo += (*(uint16_t *)(gGbaOam + i) & 0x300) == 0 && *(uint16_t *)(gGbaOam + i + 2) != 0;
#ifdef DBG_STATE
    { extern char gUnitArrayBlue[], gUnitArrayRed[]; int u, shown = 0, nv = 0, bad = 0;
      for (u = 0; u < 112; u++) { const uint32_t *w = (const uint32_t *)((u < 62 ? gUnitArrayBlue + u * 0x48 : gUnitArrayRed + (u - 62) * 0x48));
        if (w[0]) { nv++; if (!w[1]) { bad++; if (shown++ < 6) Ps2Log("   BAD unit#%d: %08x %08x %08x %08x %08x", u, w[0], w[1], w[2], w[3], w[4]); } else if (shown < 3 && u < 3) { shown++; Ps2Log("   ok unit#%d: %08x %08x %08x %08x %08x", u, w[0], w[1], w[2], w[3], w[4]); } }
      } Ps2Log("   units validas=%d sem-classe=%d", nv, bad); }
#endif
    Ps2Log("f%d disp=%04x mode=%d bg=%d%d%d%d obj=%d win=%d%d%d blank=%d bld=%04x/%04x/%u back=%04x vram_nz=%d pal_nz=%d oam_live=%d",
           sFrameNo, GbaIoRead16(0), l->mode, l->bgOn[0], l->bgOn[1], l->bgOn[2], l->bgOn[3], l->objOn, l->win0On, l->win1On,
           l->objWinOn, l->forcedBlank, l->bldcnt, l->bldalpha, l->bldy, l->backdrop, nzv, nzp, nzo);
    if (sFrameNo == 120) {                           // dump bg1: paleta, mapa e tiles
        const uint16_t *pal = (const uint16_t *)gGbaPalram, *map = (const uint16_t *)(gGbaVram + l->bg[1].scrBase * 0x800);
        Ps2Log("   pal0-15: %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x", pal[0], pal[1], pal[2], pal[3], pal[4], pal[5], pal[6], pal[7], pal[8], pal[9], pal[10], pal[11], pal[12], pal[13], pal[14], pal[15]);
        Ps2Log("   map[0..15]: %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x", map[0], map[1], map[2], map[3], map[4], map[5], map[6], map[7], map[8], map[9], map[10], map[11], map[12], map[13], map[14], map[15]);
        int nzmap = 0, nzt = 0; for (i = 0; i < 1024; i++) nzmap += map[i] != 0;
        for (i = 0; i < 0x4000; i++) nzt += gGbaVram[i] != 0;
        Ps2Log("   map entradas!=0: %d  charblock0 bytes!=0: %d  bg1 vram[0x20..]= %02x%02x%02x%02x %02x%02x%02x%02x", nzmap, nzt, gGbaVram[0x20], gGbaVram[0x21], gGbaVram[0x22], gGbaVram[0x23], gGbaVram[0x24], gGbaVram[0x25], gGbaVram[0x26], gGbaVram[0x27]);
        { int n0 = 0, n1 = 0, n2 = 0; for (i = 0; i < 1024; i++) { n0 += gBG0TilemapBuffer[i] != 0; n1 += gBG1TilemapBuffer[i] != 0; n2 += gBG2TilemapBuffer[i] != 0; }
          Ps2Log("   buffers EWRAM: bg0 nz=%d bg1 nz=%d bg2 nz=%d | vram ptrs: %p %p (gGbaVram=%p) bg1buf=%p", n0, n1, n2, gBGVramTilemapPointers[0], gBGVramTilemapPointers[1], (void *)gGbaVram, (void *)gBG1TilemapBuffer);
          Ps2Log("   bg1buf row0: %04x %04x %04x %04x row10: %04x %04x %04x %04x", gBG1TilemapBuffer[0], gBG1TilemapBuffer[1], gBG1TilemapBuffer[2], gBG1TilemapBuffer[3], gBG1TilemapBuffer[320], gBG1TilemapBuffer[321], gBG1TilemapBuffer[322], gBG1TilemapBuffer[323]); }
        for (int r = 0; r < 20; r += 4) { Ps2Log("   map row%02d: %04x %04x %04x %04x %04x %04x %04x %04x", r, map[r*32+0], map[r*32+1], map[r*32+2], map[r*32+3], map[r*32+4], map[r*32+5], map[r*32+6], map[r*32+7]); }
    }
    if (sFrameNo % 600 == 0) { Ps2Log("   [oam] PutOam chamadas=%d objs=%d ultimo x=%d y=%d n=%d dst=%x", gPutOamCalls, gPutOamObjs, gPutOamLast[0], gPutOamLast[1], gPutOamLast[2], gPutOamLast[3]); gPutOamCalls = gPutOamObjs = 0; }
    if (sFrameNo == 1800 || sFrameNo == 3600 || sFrameNo == 5400 || 0) {
        for (int t = 0; t < 8; t++) if (gProcTreeRootArray[t]) { int budget = 40; Ps2Log("   [procs] raiz %d:", t); DumpProcs(gProcTreeRootArray[t], 1, &budget); }
    }
    if (sFrameNo == 3600) {                          // dump title: mapas de BG0/BG1 e OAM
        for (int b = 0; b < 4; b++) {
            const uint16_t *m = (const uint16_t *)(gGbaVram + l->bg[b].scrBase * 0x800); int nz = 0;
            for (i = 0; i < 1024; i++) nz += m[i] != 0;
            Ps2Log("   [title] bg%d map@%02d nz=%d first=%04x %04x %04x", b, l->bg[b].scrBase, nz, m[0], m[1], m[2]);
        }
        int live = 0; const uint16_t *o = (const uint16_t *)gGbaOam;
        for (i = 0; i < 128; i++) {
            uint16_t a0 = o[i * 4], a1 = o[i * 4 + 1], a2 = o[i * 4 + 2];
            if ((a0 & 0x300) == 0x200 || (a0 & 0xFF) == 0xA0) continue;       // desativado ou escondido (Y=160)
            if (live < 12) Ps2Log("   [title] obj%d a0=%04x a1=%04x a2=%04x", i, a0, a1, a2);
            live++;
        }
        Ps2Log("   [title] sprites ativos (OAM do hardware): %d", live);
        { int gl = 0; for (i = 0; i < 128; i++) { uint16_t a0 = gOam[i*4]; if ((a0 & 0x300) == 0x200 || a0 == 0x00a0) continue; if (gl < 8) Ps2Log("   [gOam] obj%d a0=%04x a1=%04x a2=%04x", i, a0, gOam[i*4+1], gOam[i*4+2]); gl++; }
          Ps2Log("   [gOamBufDump] vivos em gOam: %d | gOam=%p LoPutIt=%p HiPutIt=%p", gl, (void *)gOam, gOamLoPutIt, (void *)gOamHiPutIt); }
    }
    for (i = 0; i < 4; i++)
        if (l->bgOn[i]) Ps2Log("   bg%d prio=%d char=%d scr=%d 8bpp=%d size=%d hofs=%d vofs=%d", i, l->bg[i].prio, l->bg[i].charBase, l->bg[i].scrBase, l->bg[i].bpp8, l->bg[i].size, l->bg[i].hofs, l->bg[i].vofs);
}

// diagnostico: uma linha por quadro com todos os procs vivos (nome#cmd/idle), para achar o proc que antecede um travamento
static int ProcLine(struct ProcDbg *p, char *o, int n, int depth)
{
    int k = 0;
    for (; p && depth < 6 && k < n - 60; p = p->next) {
        int idx = p->cur && p->script ? (int)(((const char *)p->cur - (const char *)p->script) / 8) : -1;
        const uint32_t *c = (const uint32_t *)p->cur;
        k += snprintf(o + k, n - k, " [%s#%d idle=%x end=%x cmd=%08x,%08x]", p->name ? p->name : "?", idx, (unsigned)(uintptr_t)p->idleCb, (unsigned)(uintptr_t)p->endCb, c ? c[0] : 0, c ? c[1] : 0);
        if (p->child) k += ProcLine(p->child, o + k, n - k, depth + 1);
    }
    return k;
}
volatile int gTraceOn;
void __cyg_profile_func_enter(void *fn, void *call) __attribute__((no_instrument_function));
void __cyg_profile_func_exit(void *fn, void *call) __attribute__((no_instrument_function));
#ifdef DBG_INSTR
void __cyg_profile_func_enter(void *fn, void *call) { static int busy; static FILE *tf; if (busy) return; busy = 1; if (!tf) tf = fopen("host:trace.txt", "w"); if (tf) { fprintf(tf, "E %x<-%x\n", (unsigned)(uintptr_t)fn, (unsigned)(uintptr_t)call); fflush(tf); } busy = 0; }
#else
void __cyg_profile_func_enter(void *fn, void *call) { static int busy; if (!gTraceOn || busy) return; busy = 1; Ps2Log("T %x<-%x", (unsigned)(uintptr_t)fn, (unsigned)(uintptr_t)call); busy = 0; }
#endif
void __cyg_profile_func_exit(void *fn, void *call) {}
#include "menu.h"
#include "platform/ps2_romrun.h"
#include "platform/ps2_pad.h"
static void PortFrame(void)
{
    uint16_t ie = GbaIoRead16(0x200), ime = GbaIoRead16(0x208);
    void (*h)(void) = (void (*)(void))gIRQHandlers[0];
    if (h && (ie & 1) && (ime & 1)) h();
    struct LcdState lcd; LcdDecode(&lcd);
    sFrameNo++;
#ifdef FE8_GAME
    if (sFrameNo == 180)      /* dump dos textos originais: crie save/dump_msgs.flag e inicie o jogo */
    {
        extern void PortLangDumpMsgs(void); extern int Ps2Cfg_Read(const char *, void *, int); extern void Ps2Cfg_Write(const char *, const void *, int);
        char t[4];
        if (Ps2Cfg_Read("dump_msgs.flag", t, 1) > 0) { PortLangDumpMsgs(); Ps2Cfg_Write("dump_msgs.flag", t, 0); }   /* o arquivo tem 1 byte; depois do dump fica vazio */
    }
#endif
#ifdef CRASH_TEST_FRAME
    if (sFrameNo == CRASH_TEST_FRAME) { for (;;) { __asm__ volatile(""); } }
#endif
#ifdef AUDIO_SONGTEST
    { static const unsigned short L[] = { 0x03, 0x0B, 0x14, 0x19, 0x23, 0x6A, 0x71, 0x78, 0x7B, 0x1B };
      extern void m4aSongNumStart(unsigned short);
      if (sFrameNo >= 300 && ((sFrameNo - 300) % 360) == 0 && (sFrameNo - 300) / 360 < 10) m4aSongNumStart(L[(sFrameNo - 300) / 360]); }
#endif
#ifdef DBG_VRAMDUMP
    if (sFrameNo % 60 == 0) { FILE *vf = fopen("host:vram.bin", "wb"); if (vf) { fwrite(gGbaVram, 1, 0x18000, vf); fwrite(gGbaPalram, 1, 0x400, vf); fwrite(gGbaIo, 1, 0x400, vf); fwrite(gGbaOam, 1, 0x400, vf); fclose(vf); } }
#endif
#ifdef DBG_WATCH
    { extern unsigned char gUnknown_0200E148[]; static unsigned char last_[8]; int ch = 0, k; for (k = 0; k < 8; k++) if (gUnknown_0200E148[k] != last_[k]) { ch = 1; last_[k] = gUnknown_0200E148[k]; }
      if (ch) { FILE *wf = fopen("host:watch.txt", "a"); if (wf) { fprintf(wf, "f%d E148: %02x%02x%02x%02x %02x%02x%02x%02x\n", sFrameNo, last_[0], last_[1], last_[2], last_[3], last_[4], last_[5], last_[6], last_[7]); fclose(wf); } } }
#endif
#ifdef DBG_SAVETEST
    if (sFrameNo == 400) {
        extern char gUnitArrayBlue[]; extern void *GetCharacterData(int), *GetClassData(int);
        extern void WriteSuspendSave(int), ReadSuspendSave(int), InitUnits(void); extern int IsValidSuspendSave(int);
        uint32_t *u = (uint32_t *)(gUnitArrayBlue + 0x48);
        u[0] = (uint32_t)GetCharacterData(1); u[1] = (uint32_t)GetClassData(1);
        ((uint8_t *)u)[8] = 5; ((uint8_t *)u)[0x10] = 3; ((uint8_t *)u)[0x11] = 4;
        { extern unsigned char gBoolSramWorking; extern char gPlaySt[]; FILE *g = fopen("host:lsu.txt", "a"); if (g) { fprintf(g, "TEST sramok=%d playst chapterStateBits=%08x u1=%08x %08x\n", gBoolSramWorking, *(uint32_t*)(gPlaySt + 0x40), u[0], u[1]); fclose(g); } }
        WriteSuspendSave(3);
        int v = IsValidSuspendSave(3);
        InitUnits();
        FILE *f = fopen("host:lsu.txt", "a"); if (f) { fprintf(f, "TEST valid=%d before-read u1: %08x %08x\n", v, ((uint32_t *)(gUnitArrayBlue + 0x48))[0], ((uint32_t *)(gUnitArrayBlue + 0x48))[1]); fclose(f); }
        ReadSuspendSave(3);
        f = fopen("host:lsu.txt", "a"); if (f) { fprintf(f, "TEST after-read u1: %08x %08x lvl=%d\n", ((uint32_t *)(gUnitArrayBlue + 0x48))[0], ((uint32_t *)(gUnitArrayBlue + 0x48))[1], ((uint8_t *)(gUnitArrayBlue + 0x48))[8]); fclose(f); }
    }
#endif
#ifdef DBG_VISIT
    {
        extern char gPlaySt[]; extern void *Proc_Find(const void *); extern const char gProcScr_PlayerPhase[];
        extern const void *GetChapterEventDataPointer(unsigned);
        extern void *SearchAvailableEvent(void *); extern void StartAvailableTileEvent(signed char, signed char);
        extern void *GetUnit(int); extern void *gActiveUnit;
        static int sReady, sDone;
        if (!sDone && Proc_Find(gProcScr_PlayerPhase)) sReady++; else if (!sDone) sReady = 0;
        if (sDone == 2 && sFrameNo % 30 == 0)
        {
            extern const char gProcScr_Talk[], gProcScr_CgTextMain[], gProcScr_BoxDialogue[], gProcScr_E_FACE[], ProcScr_StdEventEngine[];
            unsigned char *ev = (unsigned char *)Proc_Find(ProcScr_StdEventEngine);
            FILE *f = fopen("host:visit.txt", "a");
            if (f) {
                fprintf(f, "f=%d TALK%d CG%d BOX%d FACE%d EV=%p", sFrameNo, Proc_Find(gProcScr_Talk)!=0, Proc_Find(gProcScr_CgTextMain)!=0, Proc_Find(gProcScr_BoxDialogue)!=0, Proc_Find(gProcScr_E_FACE)!=0, (void*)ev);
                if (ev) { const unsigned short *cur = *(const unsigned short **)(ev + 0x38); fprintf(f, " st=%04x op=%04x %04x %04x", *(unsigned short*)(ev+0x3C), cur[0], cur[1], cur[2]); }
                fprintf(f, "\n");
                { extern char gLCDControlBuffer[]; extern const char ProcScr_FadeToBlack[]; unsigned char *fp = (unsigned char *)Proc_Find(ProcScr_FadeToBlack);
                  { extern int gDbgFadeCalls, gDbgFadeBlend; fprintf(f, "  calls=%d lastblend=%d", gDbgFadeCalls, gDbgFadeBlend); } fprintf(f, "  blendY=%d fade=%p", gLCDControlBuffer[0x46], (void *)fp);
                  if (fp) fprintf(f, " unk64=%d unk66=%d sleep=%d flags=%02x lock=%d", *(short *)(fp + 0x64), *(short *)(fp + 0x66), *(short *)(fp + 0x24), fp[0x27], fp[0x28]);
                  fprintf(f, "\n"); }
                if (ev && sFrameNo % 450 == 0) {
                    const unsigned *w = (const unsigned *)ev; int i;
                    extern int DoesBMXFADEExist(void);
                    fprintf(f, "  EVRAW"); for (i = 0; i < 24; i++) fprintf(f, " %08x", w[i]); fprintf(f, "\n  BMXFADE=%d\n", DoesBMXFADEExist());
                    for (int t = 0; t < 8; t++) { struct ProcDbg *q[16]; int n = 0, k; struct ProcDbg *p = gProcTreeRootArray[t]; q[n++] = p; if (!p) continue;
                        for (k = 0; k < n && n < 16; k++) { for (p = q[k]; p && n < 16; p = p->prev) { fprintf(f, "  root%d proc=%p script=%p cmd#%d lock=%d idle=%p\n", t, (void*)p, p->script, p->cur && p->script ? (int)(((const char *)p->cur - (const char *)p->script) / 8) : -1, ((unsigned char*)p)[0x28], p->idleCb); if (p->child && n < 16) q[n++] = p->child; } } }
                }
                fclose(f);
            }
        }
        if (!sDone && sReady > 200)
        {
            struct { const void *list; unsigned script, flag, cmd, money, item; signed char x, y; unsigned char a, b; } info;
            int x, y, ch; FILE *f = fopen("host:visit.txt", "a");
            sDone = 1;
            for (ch = 1; ch < 60 && sDone == 1; ch++)
            {
                gPlaySt[0xE] = (char)ch;
                for (y = 0; y < 40 && sDone == 1; y++) for (x = 0; x < 40; x++)
                {
                    const unsigned *ev = *(const unsigned **)((const char *)GetChapterEventDataPointer((unsigned)ch) + 8);
                    if ((uintptr_t)ev < 0x100000) break;
                    info.list = ev; info.x = x; info.y = y;
                    if (SearchAvailableEvent(&info) && info.cmd == 0x10)
                    {
                        if (f) fprintf(f, "chapter %d visit at %d,%d script=%08x\n", ch, x, y, info.script);
                        { extern const char gProcScr_Minimap[]; extern void Proc_End(void *); extern void UnlockGame(void); void *mm = Proc_Find(gProcScr_Minimap); if (mm) { Proc_End(mm); UnlockGame(); if (f) fprintf(f, "minimap ended\n"); } }
                        gActiveUnit = GetUnit(1);
                        StartAvailableTileEvent(x, y);
                        sDone = 2; break;
                    }
                }
            }
            if (f) { fprintf(f, "done=%d\n", sDone); fclose(f); }
        }
    }
#endif
#ifdef DBG_STATE
    if (sFrameNo % 3 == 0) { PortTrace(&lcd); { int budget = 30; for (int t = 0; t < 8; t++) if (gProcTreeRootArray[t]) { Ps2Log("  [procs] raiz %d", t); DumpProcs(gProcTreeRootArray[t], 1, &budget); } } }
#else
    if (sFrameNo == 1 || sFrameNo == 5 || sFrameNo % 120 == 0 || 0) PortTrace(&lcd);
#endif
    
    RenderFrame();
    Ps2Save_Tick();
    {
        static int sStartHold;
        uint16_t k;
        unsigned raw = Ps2Pad_Read();
#ifdef MENUTEST
        if (sFrameNo > 1500 && sFrameNo < 1510) raw |= (1u<<PS2B_START)|(1u<<PS2B_SELECT);
#endif
        if ((raw & (1u << PS2B_START)) && (raw & (1u << PS2B_SELECT))) { sStartHold = 0; MenuRun(); k = 0x03FF; }
        else
        {
            k = Ps2Pad_GbaKeyInput();
            /* START sozinho so chega ao jogo depois de 3 quadros (da tempo de apertar SELECT para abrir o menu) */
            if (raw & (1u << PS2B_START)) { if (sStartHold < 3) { sStartHold++; k |= 0x0008; } } else sStartHold = 0;
        }
#ifdef AUTOKEY
        if (sFrameNo > 2400 && (sFrameNo % 120) < 4) k &= ~0x0008;           // START
        if (sFrameNo > 2400 && ((sFrameNo + 60) % 120) < 4) k &= ~0x0001;    // A
#endif
        GbaIoWrite16(0x130, k);   // KEYINPUT
    }
}
#endif

#ifdef FE8_GAME
extern void MenuLoading(const char *msg, int pct);
extern void MenuLoadingEnd(void);
void Fe8LoadProgress(const char *msg, int pct) { MenuLoading(msg, pct); }
#endif

int main(int argc, char **argv)
{
    const char *a0 = (argc > 0 && argv && argv[0]) ? argv[0] : "";

    // 1) IOP e módulos (USB FAT32/exFAT)
    Ps2Platform_Init(a0);

#ifdef FE8_GAME
    GfxCacheInit();                              // video no ar antes de ler a ROM: tela de carregamento
    BeInit();
    MenuLoading("PROCURANDO A ROM", 0);
#endif
    // 2) ROM: procura na pasta do ELF e em mass:/ (depois do reset do IOP o pendrive demora mais para montar)
    char path[128];
    int wait_s = Ps2Platform_IopWasReset() ? 20 : 6;
    Ps2Log("procurando a ROM (ate %d s)...", wait_s);
    if (!Ps2Rom_Find(a0, path, sizeof path, wait_s))
        Ps2Fatal("ROM nao encontrada: fe8.gba", "Procurei na pasta do ELF, host:, mass:/, cdrom0: e mc0:", NULL, NULL);
    Ps2Log_OpenFor(path);                        // a partir daqui o log também vai para fe8_log.txt, ao lado da ROM
    Ps2Log("ROM: %s", path);
    Ps2Rom_Validate(path);
    Ps2Log("ROM verificada (sha1 ok)");
    if (!Ps2Rom_Restore(path))
        Ps2Fatal("Falha ao ler os dados do jogo da ROM.", path, NULL, NULL);
#ifndef FE8_GAME
    if (!Ps2Rom_Load(path, &gGbaRom, &gGbaRomSize))
        Ps2Fatal("Falha ao ler a ROM para a RAM.", path, NULL, NULL);
    Ps2Log("ROM carregada: %u bytes em %p", gGbaRomSize, (const void *)gGbaRom);
#endif

#ifdef FE8_GAME
    // 3) o jogo (decomp): AgbMain nunca retorna; cada VBlankIntrWait roda o handler de VBlank e apresenta o quadro
    Ps2Pad_Init();
    Ps2Save_Init(path);
    { extern void Ps2Crash_Init(void); Ps2Crash_Init(); }
    { extern int Ps2Audio_Init(void), Ps2Audio_LoadSoundData(const char *); if (Ps2Audio_Init()) Ps2Audio_LoadSoundData(path); }
    MenuLoadingEnd();
    MenuVideoLoad();
    { extern void PortLangInit(void); PortLangInit(); }
    gPlatformVBlankWait = PortFrame;
    Ps2Log("AgbMain...");
#ifdef NULLTEST
    { volatile int *p = (volatile int *)0x38; *p = 5; int v = *p; Ps2Log("nulltest v=%d", v); }
#endif
    AgbMain();
    return 0;
#endif
    return 0;
}
