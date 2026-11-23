// Motor de som m4a (MP2K) do FE8 em C. No original estas rotinas sao ARM escritas a mao (asm/m4a_1.s):
// SoundMain (mixer de amostras), MPlayMain (sequenciador), ply_* (comandos de trilha) e auxiliares.
// A logica segue o comportamento exato do assembly (inclusive o acumulo em 8 bits com wrap e a interpolacao
// linear de 23 bits da versao com taxa variavel). Compilado junto com a decomp (usa os headers dela).
//
// Enderecos: os dados de som ficam na ROM (0x08xxxxxx). As estruturas de estado (trilha/canal) guardam
// ponteiros REAIS; a traducao e feita por Ps2Audio_Resolve ao copiar da ROM (ply_goto, ply_voice, ply_note...).
#include "global.h"
#define MPlayMain MPlayMain_decl
#define ply_note ply_note_decl
#include "gba/m4a_internal.h"
#undef MPlayMain
#undef ply_note

extern void *Ps2Audio_Resolve(uint32_t v);
extern void Ps2Audio_Submit(const s8 *fifoA, const s8 *fifoB, unsigned n, unsigned rate);
extern const u8 gClockTable[];
extern u32 MidiKeyToFreq(struct WaveData *wav, u8 key, u8 fineAdjust);
extern void SoundMainBTM_clear64(void *p);

extern struct MusicPlayerInfo gMPlayInfo_BGM1, gMPlayInfo_BGM2, gMPlayInfo_SE1_SYS1, gMPlayInfo_SE2_SYS2,
                              gMPlayInfo_SE3_BMP1, gMPlayInfo_SE4_BMP2, gMPlayInfo_SE5_BMP3, gMPlayInfo_SE6_BMP4,
                              gMPlayInfo_SE7_EVT;

#define R(v) ((u8 *)Ps2Audio_Resolve((u32)(v)))

u32 umul3232H32(u32 a, u32 b) { return (u32)(((u64)a * b) >> 32); }

char SoundMainRAM[16];
void IrqMain(void) {}

// ------------------------------------------------------------ tabelas
static struct SongHeader sEmptySong;
struct Song gSongTableRW[1024] __asm__("gSongTable");   // preenchida por Ps2Audio_SongTableSet (a partir da ROM)
void Ps2Audio_SongTableSet(int i, void *header, unsigned ms, unsigned me)
{
    gSongTableRW[i].header = header; gSongTableRW[i].ms = ms; gSongTableRW[i].me = me;
}
static void SongTableDefault(void)
{
    int i;
    for (i = 0; i < 1024; i++) if (!gSongTableRW[i].header) gSongTableRW[i].header = &sEmptySong;
}

static struct MusicPlayerTrack sTracks[9][8];
const struct MusicPlayer gMPlayTable[9] = {
    { &gMPlayInfo_BGM1,     sTracks[0], 8, 0 }, { &gMPlayInfo_BGM2,     sTracks[1], 8, 0 },
    { &gMPlayInfo_SE1_SYS1, sTracks[2], 2, 0 }, { &gMPlayInfo_SE2_SYS2, sTracks[3], 2, 1 },
    { &gMPlayInfo_SE3_BMP1, sTracks[4], 4, 0 }, { &gMPlayInfo_SE4_BMP2, sTracks[5], 4, 1 },
    { &gMPlayInfo_SE5_BMP3, sTracks[6], 5, 0 }, { &gMPlayInfo_SE6_BMP4, sTracks[7], 4, 0 },
    { &gMPlayInfo_SE7_EVT,  sTracks[8], 2, 0 },
};

extern void * const gMPlayJumpTableTemplate[];
void MPlayJumpTableCopy(void **table)
{
    int i;
    SongTableDefault();
    for (i = 0; i < 36; i++) table[i] = gMPlayJumpTableTemplate[i];
}

// ------------------------------------------------------------ cadeia de canais
void RealClearChain(void *x)
{
    struct SoundChannel *ch = x;
    struct MusicPlayerTrack *tr = ch->track;
    struct SoundChannel *np, *pp;
    if (!tr) return;
    np = (struct SoundChannel *)ch->np; pp = (struct SoundChannel *)ch->pp;
    if (pp) pp->np = (u32)np; else tr->chan = np;
    if (np) np->pp = (u32)pp;
    ch->track = 0;
}

void TrackStop(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{
    struct SoundChannel *ch;
    (void)info;
    if (!(track->flags & MPT_FLG_EXIST)) return;
    for (ch = track->chan; ch; ch = (struct SoundChannel *)ch->np)
    {
        if (ch->status)
        {
            u8 t = ch->type & 7;
            if (t) { struct SoundInfo *si = SOUND_INFO_PTR; si->CgbOscOff(t); }
            ch->status = 0;
        }
        ch->track = 0;
    }
    track->chan = 0;
}

// ------------------------------------------------------------ comandos de trilha
static inline u32 ReadWord(const u8 *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24); }

void ply_fine(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{
    struct SoundChannel *ch;
    (void)info;
    for (ch = track->chan; ch; ch = (struct SoundChannel *)ch->np)
    {
        if (ch->status & 0xC7) ch->status |= 0x40;
        RealClearChain(ch);
    }
    track->flags = 0;
}

void ply_goto(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{
    (void)info;
    track->cmdPtr = R(ReadWord(track->cmdPtr));
}

void ply_patt(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{
    if (track->patternLevel >= 3) { ply_fine(info, track); return; }
    track->patternStack[track->patternLevel] = track->cmdPtr + 4;
    track->patternLevel++;
    ply_goto(info, track);
}

void ply_pend(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{
    (void)info;
    if (track->patternLevel)
    {
        track->patternLevel--;
        track->cmdPtr = track->patternStack[track->patternLevel];
    }
}

void ply_rept(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{
    u8 *p = track->cmdPtr;
    if (*p == 0) { track->cmdPtr = p + 1; ply_goto(info, track); return; }
    track->repN++;
    if (track->repN >= *p) { track->repN = 0; track->cmdPtr = p + 5; return; }
    track->cmdPtr = p + 1;
    ply_goto(info, track);
}

void ply_prio(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{ (void)info; track->priority = *track->cmdPtr++; }

void ply_tempo(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{
    u32 v = *track->cmdPtr++ * 2;
    info->tempoD = v;
    info->tempoI = (v * info->tempoU) >> 8;
}

void ply_keysh(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{ (void)info; track->keyShift = *track->cmdPtr++; track->flags |= MPT_FLG_PITCHG; }

void ply_voice(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{
    u32 idx = *track->cmdPtr++;
    const u8 *e = R(info->tone) + idx * 12;
    u32 w0 = ReadWord(e), w1 = ReadWord(e + 4), w2 = ReadWord(e + 8);
    u8 type = (u8)w0;
    u32 *dst = (u32 *)&track->tone;
    dst[0] = w0;
    dst[1] = (u32)R(w1);                       // wav: onda / outro voicegroup (RHY, SPL) / valor CGB
    dst[2] = (type & TONEDATA_TYPE_SPL) ? (u32)R(w2) : w2;   // SPL: tabela de keysplit no lugar de ADSR
}

void ply_vol(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{ (void)info; track->vol = *track->cmdPtr++; track->flags |= MPT_FLG_VOLCHG; }

void ply_pan(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{ (void)info; track->pan = *track->cmdPtr++ - 0x40; track->flags |= MPT_FLG_VOLCHG; }

void ply_bend(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{ (void)info; track->bend = *track->cmdPtr++ - 0x40; track->flags |= MPT_FLG_PITCHG; }

void ply_bendr(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{ (void)info; track->bendRange = *track->cmdPtr++; track->flags |= MPT_FLG_PITCHG; }

static void ClearModM_(struct MusicPlayerTrack *t)
{
    t->modM = 0; t->lfoSpeedC = 0;
    t->flags |= t->modT == 0 ? MPT_FLG_PITCHG : MPT_FLG_VOLCHG;
}

void ply_lfos(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{
    (void)info;
    track->lfoSpeed = *track->cmdPtr++;
    if (!track->lfoSpeed) ClearModM_(track);
}

void ply_lfodl(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{ (void)info; track->lfoDelay = *track->cmdPtr++; }

void ply_mod(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{
    (void)info;
    track->mod = *track->cmdPtr++;
    if (!track->mod) ClearModM_(track);
}

void ply_modt(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{
    u8 v = *track->cmdPtr++;
    (void)info;
    if (track->modT != v) { track->modT = v; track->flags |= 0x0F; }
}

void ply_tune(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{ (void)info; track->tune = *track->cmdPtr++ - 0x40; track->flags |= MPT_FLG_PITCHG; }

void ply_port(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{
    extern u8 gGbaIo[];
    u8 reg = track->cmdPtr[0], val = track->cmdPtr[1];
    (void)info;
    track->cmdPtr += 2;
    gGbaIo[0x60 + reg] = val;
}

void ply_endtie(struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{
    struct SoundChannel *ch;
    u8 key;
    (void)info;
    if (*track->cmdPtr < 0x80) { key = *track->cmdPtr++; track->key = key; }
    else key = track->key;
    for (ch = track->chan; ch; ch = (struct SoundChannel *)ch->np)
    {
        if ((ch->status & 0x83) && !(ch->status & 0x40) && ch->mk == key) { ch->status |= 0x40; break; }
    }
}

// recalcula volume do canal (ChnVolSetAsm)
static void ChnVolSet(struct SoundChannel *ch, struct MusicPlayerTrack *tr)
{
    s32 rp = (s8)ch->rp;
    u32 r = (u32)(tr->volMR * (((0x80 + rp) * ch->ve))) >> 14;
    u32 l = (u32)(tr->volML * (((0x7F - rp) * ch->ve))) >> 14;
    s32 rs = (tr->volMR * ((0x80 + rp) * ch->ve)) >> 14, ls = (tr->volML * ((0x7F - rp) * ch->ve)) >> 14;
    (void)r; (void)l;
    ch->rightVolume = (u32)rs > 0xFF ? 0xFF : rs;
    ch->leftVolume = (u32)ls > 0xFF ? 0xFF : ls;
}

void ply_note(u32 noteCmd, struct MusicPlayerInfo *info, struct MusicPlayerTrack *track)
{
    struct SoundInfo *si = SOUND_INFO_PTR;
    const u8 *elem;
    u32 key, prio, ctype;
    s32 forcedPan = 0;
    struct SoundChannel *ch;
    u8 ttype;

    track->gateTime = gClockTable[noteCmd];
    if (*track->cmdPtr < 0x80)
    {
        track->key = *track->cmdPtr++;
        if (*track->cmdPtr < 0x80)
        {
            track->velocity = *track->cmdPtr++;
            if (*track->cmdPtr < 0x80) track->gateTime += *track->cmdPtr++;
        }
    }
    ttype = track->tone.type;
    key = track->key;
    if (ttype & (TONEDATA_TYPE_RHY | TONEDATA_TYPE_SPL))
    {
        u32 idx = key;
        if (ttype & TONEDATA_TYPE_SPL) idx = ((const u8 *)*(u32 *)&track->tone.attack)[key];
        elem = (const u8 *)track->tone.wav + idx * 12;
        if (elem[0] & (TONEDATA_TYPE_RHY | TONEDATA_TYPE_SPL)) return;
        if (ttype & TONEDATA_TYPE_RHY)
        {
            if (elem[3] & 0x80) forcedPan = (s32)((elem[3] - 0xC0) * 2);
            key = elem[1];
        }
    }
    else elem = (const u8 *)&track->tone;

    prio = info->priority + track->priority;
    if (prio > 0xFF) prio = 0xFF;
    ctype = elem[0] & 7;

    if (ctype)
    {
        struct CgbChannel *cgb = si->cgbChans, *c;
        if (!cgb) return;
        c = &cgb[ctype - 1];
        if ((c->sf & 0xC7) && !(c->sf & 0x40))
        {
            if (c->pr > prio) return;
            if (c->pr == prio && (u32)c->tp < (u32)track) return;
        }
        ch = (struct SoundChannel *)c;
    }
    else
    {
        u32 bestPrio = prio, bestTrk = (u32)track;
        struct SoundChannel *cand = 0;
        int haveRel = 0, n;
        ch = 0;
        for (n = 0; n < si->maxChans; n++)
        {
            struct SoundChannel *c = &si->chans[n];
            u8 s = c->status;
            int consider = 1;
            if (!(s & 0xC7)) { ch = c; break; }
            if (s & 0x40)
            {
                if (!haveRel) { haveRel = 1; bestPrio = c->pr; bestTrk = (u32)c->track; cand = c; consider = 0; }
            }
            else if (haveRel) consider = 0;
            if (consider)
            {
                if (c->pr < bestPrio) { bestPrio = c->pr; bestTrk = (u32)c->track; cand = c; }
                else if (c->pr == bestPrio)
                {
                    if ((u32)c->track >= bestTrk) { bestTrk = (u32)c->track; cand = c; }
                }
            }
        }
        if (!ch) { ch = cand; if (!ch) return; }
    }

    // monta o canal
    RealClearChain(ch);
    ch->pp = 0;
    ch->np = (u32)track->chan;
    if (track->chan) track->chan->pp = (u32)ch;
    track->chan = ch;
    ch->track = track;
    track->lfoDelayC = track->lfoDelay;
    if (track->lfoDelay) ClearModM_(track);
    TrkVolPitSet(info, track);
    ch->gt = track->gateTime; ch->mk = track->key; ch->ve = track->velocity; ch->pr = track->runningStatus;
    ch->pr = prio;
    ch->ky = key;
    ch->rp = (u8)forcedPan;
    ch->type = elem[0];
    {
        u32 wav = ReadWord(elem + 4);
        ch->wav = (struct WaveData *)(u32)R(wav);
        *(u32 *)&ch->attack = ReadWord(elem + 8);
    }
    ch->echoVolume = track->echoVolume; ch->echoLength = track->echoLength;
    ChnVolSet(ch, track);
    {
        s32 k = (s32)ch->ky + track->keyM;
        u32 fr;
        if (k < 0) k = 0;
        if (ctype)
        {
            struct CgbChannel *c = (struct CgbChannel *)ch;
            u8 sw = elem[3];
            c->le = elem[2];
            if ((sw & 0x80) || !(sw & 0x70)) sw = 8;
            c->sw = sw;
            c->wp = (u32 *)(uintptr_t)R((u32)ReadWord(elem + 4));
            fr = si->MidiKeyToCgbFreq(ctype, k, track->pitM);
        }
        else fr = MidiKeyToFreq(ch->wav, k, track->pitM);
        ch->freq = fr;
    }
    ch->status = 0x80;
    track->flags &= 0xF0;
}

// ------------------------------------------------------------ sequenciador
void MPlayMain(struct MusicPlayerInfo *mp)
{
    struct SoundInfo *si = SOUND_INFO_PTR;
    int i;
    if (mp->ident != ID_NUMBER) return;
    mp->ident++;
    if (mp->func) ((void (*)(u32))mp->func)(mp->intp);
    if ((s32)mp->status >= 0)
    {
        FadeOutBody(mp);
        if ((s32)mp->status >= 0)
        {
            u32 tempoC = (u16)(mp->tempoC + mp->tempoI);
            mp->tempoC = tempoC;
            while (tempoC >= 150)
            {
                u32 mask = 0;
                for (i = 0; i < mp->trackCount; i++)
                {
                    struct MusicPlayerTrack *tr = &mp->tracks[i];
                    struct SoundChannel *ch;
                    if (!(tr->flags & MPT_FLG_EXIST)) continue;
                    mask |= 1u << i;
                    for (ch = tr->chan; ch; ch = (struct SoundChannel *)ch->np)
                    {
                        if (ch->status & 0xC7)
                        {
                            if (ch->gt) { ch->gt--; if (ch->gt == 0) ch->status |= 0x40; }
                        }
                        else RealClearChain(ch);
                    }
                    if (tr->flags & MPT_FLG_START)
                    {
                        memset(tr, 0, 0x40);
                        tr->flags = MPT_FLG_EXIST; tr->bendRange = 2; tr->volX = 64; tr->lfoSpeed = 22; tr->tone.type = 1;
                    }
                    while (tr->wait == 0)
                    {
                        u8 b = *tr->cmdPtr;
                        if (b < 0x80) b = tr->runningStatus;
                        else { tr->cmdPtr++; if (b >= 0xBD) tr->runningStatus = b; }
                        if (b >= 0xCF) ply_note(b - 0xCF, mp, tr);
                        else if (b > 0xB0)
                        {
                            void (*fn)(struct MusicPlayerInfo *, struct MusicPlayerTrack *);
                            mp->cmd = b - 0xB1;
                            fn = (void *)((void **)si->MPlayJumpTable)[mp->cmd];
                            fn(mp, tr);
                            if (tr->flags == 0) goto next_track;
                        }
                        else tr->wait = gClockTable[b - 0x80];
                    }
                    tr->wait--;
                    if (tr->lfoSpeed != 0 && tr->mod != 0)
                    {
                        if (tr->lfoDelayC != 0) tr->lfoDelayC--;
                        else
                        {
                            u32 c = (u8)(tr->lfoSpeedC + tr->lfoSpeed);
                            s32 x;
                            tr->lfoSpeedC = c;
                            x = ((u8)(c - 0x40) & 0x80) ? (s32)(s8)c : (s32)(0x80 - c);
                            x = (tr->mod * x) >> 6;
                            if ((u8)(tr->modM ^ x) != 0)
                            {
                                tr->modM = x;
                                tr->flags |= (tr->modT == 0) ? MPT_FLG_PITCHG : MPT_FLG_VOLCHG;
                            }
                        }
                    }
                next_track:;
                }
                mp->clock++;
                if (mask == 0) { mp->status = MUSICPLAYER_STATUS_PAUSE; goto done; }
                mp->status = mask;
                tempoC = (u16)(mp->tempoC - 150);
                mp->tempoC = tempoC;
            }
            // aplica volume/pitch pendentes
            for (i = 0; i < mp->trackCount; i++)
            {
                struct MusicPlayerTrack *tr = &mp->tracks[i];
                struct SoundChannel *ch;
                if (!(tr->flags & MPT_FLG_EXIST)) continue;
                if (!(tr->flags & 0x0F)) continue;
                TrkVolPitSet(mp, tr);
                for (ch = tr->chan; ch; ch = (struct SoundChannel *)ch->np)
                {
                    u8 ty;
                    if (!(ch->status & 0xC7)) { RealClearChain(ch); continue; }
                    ty = ch->type & 7;
                    if (tr->flags & MPT_FLG_VOLCHG)
                    {
                        ChnVolSet(ch, tr);
                        if (ty) ((struct CgbChannel *)ch)->mo |= 1;
                    }
                    if (tr->flags & MPT_FLG_PITCHG)
                    {
                        s32 k = (s32)ch->ky + tr->keyM;
                        if (k < 0) k = 0;
                        if (ty) { ch->freq = si->MidiKeyToCgbFreq(ty, k, tr->pitM); ((struct CgbChannel *)ch)->mo |= 2; }
                        else ch->freq = MidiKeyToFreq(ch->wav, k, tr->pitM);
                    }
                }
                tr->flags &= 0xF0;
            }
        }
    }
done:
    mp->ident = ID_NUMBER;
}

// ------------------------------------------------------------ mixer
void m4aSoundVSync(void)
{
    struct SoundInfo *si = SOUND_INFO_PTR;
    s32 c;
    if (!si || si->ident - ID_NUMBER > 1) return;
    c = (s32)si->pcmDmaCounter - 1;
    if (c > 0) si->pcmDmaCounter = c;
    else si->pcmDmaCounter = si->pcmDmaPeriod;      // (no GBA: reinicia o DMA do buffer)
}

static inline void Acc(s8 *a, s8 *b, u32 er, u32 el, s32 s, s32 i)
{
    a[i] = (s8)(a[i] + (((s32)er * s) >> 8));
    b[i] = (s8)(b[i] + (((s32)el * s) >> 8));
}

void SoundMain(void)
{
    struct SoundInfo *si = SOUND_INFO_PTR;
    s32 N, i, n;
    s8 *buf, *w, *A, *B;
    u32 c;
    if (si->ident != ID_NUMBER) return;
    si->ident++;
    if (si->func) ((void (*)(u32))si->func)(si->intp);
    if (si->CgbSound) si->CgbSound();

    N = si->pcmSamplesPerVBlank;
    buf = si->pcmBuffer;
    c = si->pcmDmaCounter;
    w = buf;
    if ((s32)c - 1 > 0) w = buf + (si->pcmDmaPeriod - (c - 1)) * N;
    A = w; B = w + PCM_DMA_BUF_SIZE;

    if (si->reverb)
    {
        s8 *nx = (c == 2) ? buf : w + N;
        s8 *nA = nx, *nB = nx + PCM_DMA_BUF_SIZE;
        for (i = 0; i < N; i++)
        {
            s32 r = (A[i] + B[i] + nA[i] + nB[i]) * si->reverb;
            r >>= 9;
            if (r & 0x80) r++;
            A[i] = (s8)r; B[i] = (s8)r;
        }
    }
    else { memset(A, 0, N); memset(B, 0, N); }

    for (n = 0; n < si->maxChans; n++)
    {
        struct SoundChannel *ch = &si->chans[n];
        u8 status = ch->status;
        u32 ev;
        u32 er, el;
        s8 *loopPtr = 0;
        s32 loopLen = 0;
        struct WaveData *wav;
        s8 *cp;
        s32 ct;

        if (!(status & 0xC7)) continue;
        wav = ch->wav;
        if (status & 0x80)
        {
            if (status & 0x40) { ch->status = 0; continue; }
            status = 3; ch->status = 3;
            ch->cp = (u32)wav->data; ch->ct = wav->size;
            ch->ev = 0; ch->fw = 0;
            if (wav->status & 0xC000) { status |= 0x10; ch->status = status; }
            ev = 0;
            ev += ch->attack;
            if (ev >= 0xFF) { ev = 0xFF; status--; ch->status = status; }
        }
        else
        {
            ev = ch->ev;
            if (status & 4)
            {
                u8 old = ch->echoLength;
                ch->echoLength = old - 1;
                if (old < 2) { ch->status = 0; continue; }
            }
            else if (status & 0x40)
            {
                ev = (ev * ch->release) >> 8;
                if (ev <= ch->echoVolume)
                {
                    ev = ch->echoVolume;
                    if (ev == 0) { ch->status = 0; continue; }
                    status |= 4; ch->status = status;
                }
            }
            else
            {
                u32 ph = status & 3;
                if (ph == 2)
                {
                    ev = (ev * ch->decay) >> 8;
                    if (ev <= ch->sustain)
                    {
                        ev = ch->sustain;
                        if (ev == 0)
                        {
                            ev = ch->echoVolume;
                            if (ev == 0) { ch->status = 0; continue; }
                            status |= 4; ch->status = status;
                        }
                        else { status--; ch->status = status; }
                    }
                }
                else if (ph == 3)
                {
                    ev += ch->attack;
                    if (ev >= 0xFF) { ev = 0xFF; status--; ch->status = status; }
                }
            }
        }
        ch->ev = ev;
        {
            u32 v = ((u32)si->masterVolume + 1) * ev >> 4;
            er = (u8)((ch->rightVolume * v) >> 8);
            el = (u8)((ch->leftVolume * v) >> 8);
            ch->er = er; ch->el = el;
        }
        if (status & 0x10)
        {
            loopPtr = (s8 *)wav->data + wav->loopStart;
            loopLen = wav->size - wav->loopStart;
        }
        cp = (s8 *)ch->cp; ct = ch->ct;

        if (ch->type & TONEDATA_TYPE_FIX)
        {
            for (i = 0; i < N; i++)
            {
                Acc(A, B, er, el, *cp++, i);
                if (--ct == 0)
                {
                    if (loopLen) { cp = loopPtr; ct = loopLen; }
                    else { ch->status = 0; break; }
                }
            }
            ch->cp = (u32)cp; ch->ct = ct;
        }
        else
        {
            u32 fw = ch->fw;
            u32 step = (u32)si->divFreq * ch->freq;
            s32 s0 = cp[0], d;
            int stopped = 0;
            cp++;
            d = cp[0] - s0;
            for (i = 0; i < N; i++)
            {
                u32 adv;
                s32 smp = s0 + (((s32)fw * d) >> 23);
                Acc(A, B, er, el, smp, i);
                fw += step;
                adv = fw >> 23;
                if (adv == 0) continue;
                fw &= ~0x3F800000u;
                ct -= adv;
                if (ct <= 0)
                {
                    s32 ov;
                    if (loopLen == 0) { stopped = 1; break; }
                    ov = -ct;
                    for (;;) { ct += loopLen; if (ct > 0) break; ov -= loopLen; }
                    cp = loopPtr + ov;
                    s0 = cp[0]; cp++; d = cp[0] - s0;
                    continue;
                }
                if (adv == 1) s0 += d;
                else { cp += adv - 1; s0 = cp[0]; }
                cp++;
                d = cp[0] - s0;
            }
            if (stopped) ch->status = 0;
            else { ch->fw = fw; ch->ct = ct; ch->cp = (u32)(cp - 1); }
        }
    }

    Ps2Audio_Submit(A, B, N, si->pcmFreq);
    si->ident = ID_NUMBER;
}
