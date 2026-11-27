#!/usr/bin/env python3
"""Aplica à árvore da decomp (idempotente) os ajustes que ligam o código do GBA ao hardware simulado do PS2:
 - REG_BASE/PLTT/VRAM/OAM/EWRAM/IWRAM passam a apontar para os buffers gGbaIo/gGbaPalram/gGbaVram/gGbaOam e para o bloco de RAM do linker;
 - DmaSet/DmaStop chamam a camada de plataforma (que executa a transferência);
 - literais 0x06xxxxxx/0x07xxxxxx/0x05xxxxxx/0x04xxxxxx em src/*.c viram expressões VRAM/OAM/PLTT/REG_BASE + deslocamento;
 - ponteiros para a ROM em opinfo.c apontam para uma fatia da ROM embutida (rom_slices.s);
 - SRAM/flash (0x0E000000) vira o buffer gGbaSram.
Uso: apply_port_patches.py <dir_decomp>"""
import os, re, sys, glob
D = sys.argv[1]
def rd(p): return open(os.path.join(D, p)).read()
def wr(p, s): open(os.path.join(D, p), "w").write(s)
def sub1(p, old, new):
    s = rd(p)
    if new in s: return
    if old not in s:   # ja aplicado por outra etapa (ou decomp diferente): avisa e segue
        print("AVISO: trecho nao encontrado em", p, "->", old[:60].replace("\n", " | "), file=sys.stderr); return
    wr(p, s.replace(old, new, 1))

# --- headers ---
sub1("include/gba/io_reg.h", "#define REG_BASE 0x4000000 // I/O register base address", "#define REG_BASE ((uintptr_t)gGbaIo) // PS2: registradores simulados em RAM")
s = rd("include/gba/defines.h")
if "gGbaPalram" not in s:
    for a, b in [("(*(struct SoundInfo **)0x3007FF0)", "(*(struct SoundInfo **)(__iwram_start + 0x7FF0))"),
                 ("(*(u16 *)0x3007FF8)", "(*(u16 *)(__iwram_start + 0x7FF8))"),
                 ("(*(void **)0x3007FFC)", "(*(void **)(__iwram_start + 0x7FFC))"),
                 ("#define EWRAM_START 0x02000000", "#define EWRAM_START ((uintptr_t)__ewram_start)"),
                 ("#define IWRAM_START 0x03000000", "#define IWRAM_START ((uintptr_t)__iwram_start)"),
                 ("#define PLTT      0x5000000", "#define PLTT      ((uintptr_t)gGbaPalram)"),
                 ("#define VRAM      0x6000000", "#define VRAM      ((uintptr_t)gGbaVram)"),
                 ("#define OAM      0x7000000", "#define OAM      ((uintptr_t)gGbaOam)")]:
        assert a in s, a
        s = s.replace(a, b, 1)
    wr("include/gba/defines.h", s)
sub1("include/gba/flash_internal.h", "#define FLASH_BASE ((u8 *)0xE000000)", "#define FLASH_BASE ((u8 *)gGbaSram)")
s = rd("include/gba/macro.h")
if "PlatformDmaSet" not in s:
    s = re.sub(r"#define DmaSet\(dmaNum, src, dest, control\)\s*\\\n\{.*?\n\}\n",
               "#define DmaSet(dmaNum, src, dest, control) PlatformDmaSet(dmaNum, (const void *)(src), (void *)(dest), (u32)(control))\n", s, count=1, flags=re.S)
    s = re.sub(r"#define DmaStop\(dmaNum\)\s*\\\n\{.*?\n\}\n", "#define DmaStop(dmaNum) PlatformDmaStop(dmaNum)\n", s, count=1, flags=re.S)
    assert "PlatformDmaSet" in s and "PlatformDmaStop" in s
    wr("include/gba/macro.h", s)

# --- literais de endereço em src/*.c ---
def conv(m):
    v = int(m.group(0), 16); reg = v >> 24; off = v & 0xFFFFFF
    base = {6: "VRAM", 7: "OAM", 5: "PLTT", 4: "REG_BASE"}[reg]
    return "(%s + 0x%X)" % (base, off)
pat = re.compile(r"\b0x(?:060[01][0-9A-Fa-f]{4}|0700[0-9A-Fa-f]{4}|0500[0-9A-Fa-f]{4}|0400[0-9A-Fa-f]{4})\b")
n = 0
for f in sorted(glob.glob(D + "/src/*.c")):
    if f.endswith("msg_data.c"): continue
    s = open(f).read(); out = []
    for line in s.split("\n"):
        code = line.split("//")[0]
        if pat.search(code) and "FE8U" not in line and not line.lstrip().startswith(("#include", "#define")):
            line = pat.sub(conv, line) if "//" not in line else pat.sub(conv, code) + line[len(code):]
            n += 1
        out.append(line)
    open(f, "w").write("\n".join(out))
print("linhas com literais de VRAM/OAM/PLTT/IO reescritas:", n)

# --- ponteiros de ROM em opinfo.c -> fatia embutida ---
ROM_LO = 0xA37000
s = rd("src/opinfo.c")
def romconv(m):
    a = int(m.group(1), 16) & 0xFFFFFF
    assert ROM_LO <= a < ROM_LO + 0x5000, hex(a)
    return "(u8*)(gRomSliceOpInfo + 0x%X)" % (a - ROM_LO)
s2 = re.sub(r"\(u8 ?\*\) ?(0x08[0-9A-Fa-f]{6})", romconv, s)
wr("src/opinfo.c", s2)
print("ponteiros de ROM em opinfo.c:", len(re.findall("gRomSliceOpInfo", s2)))

# --- SRAM: base em RAM e rotinas rápidas em C (o original copiava código Thumb para a RAM) ---
sub1("include/agb_sram.h", "#define CART_SRAM_ADDR 0x0E000000", "#define CART_SRAM_ADDR ((uintptr_t)gGbaSram)")
s = rd("src/agb_sram.c")
if "__mips__" not in s:
    s = s.replace("void SetSramFastFunc(void)\n{\n", "void SetSramFastFunc(void)\n{\n#ifdef __mips__\n    ReadSramFast = (void *)ReadSramFast_Core;\n    VerifySramFast = (void *)VerifySramFast_Core;\n    return;\n#endif\n", 1)
    wr("src/agb_sram.c", s)

# --- m4a: SampleFreqSet esperava o VCOUNT mudar (sem varredura de vídeo real no port) ---
s = rd("src/m4a.c")
old = "    while (*(vu8 *)REG_ADDR_VCOUNT == 159)\n        ;\n\n    while (*(vu8 *)REG_ADDR_VCOUNT != 159)\n        ;\n"
if old in s and "#ifndef __mips__\n    while (*(vu8 *)REG_ADDR_VCOUNT" not in s:
    s = s.replace(old, "#ifndef __mips__\n" + old + "#endif\n", 1)
    wr("src/m4a.c", s)

# --- VRAM | offset só valia com VRAM = 0x06000000; agora VRAM é o endereço do buffer ---
sub1("src/hardware.c", "(void *)(VRAM | offset)", "(void *)(VRAM + offset)")

# --- bug do original ("BUG?"): passa o numero do slot como ponteiro. No GBA le a BIOS (inofensivo); no PS2 e leitura em endereco 0..2 = TLB miss. A intencao e o playSt lido ---
sub1("src/difficultymenu.c", "IsGameNotFirstChapter((struct PlaySt *)(uintptr_t)slot)", "IsGameNotFirstChapter(&playSt)")

# --- leituras em NULL que no GBA caem na BIOS/IO (inofensivas) e no PS2 sao TLB miss ---
sub1("src/face.c", "void EndFaceById(int slot) {\n    EndFace(gFaces[slot]);", "void EndFaceById(int slot) {\n    if (gFaces[slot] != NULL) EndFace(gFaces[slot]);")
sub1("src/worldmap_rm.c", "        proc = Proc_FindNext(&procIter);\n        if (proc->index == index)", "        proc = Proc_FindNext(&procIter);\n        if (proc != NULL && proc->index == index)")

# --- AnimDelete(NULL): no GBA escreve na BIOS/IWRAM baixa (inofensivo); no PS2 e TLB miss ---
sub1("src/animedrv.c", "void AnimDelete(struct Anim* anim)\n{\n    if (anim->pPrev == NULL)", "void AnimDelete(struct Anim* anim)\n{\n    if (anim == NULL) return;\n    if (anim->pPrev == NULL)")
# --- AnimDelete do ultimo da lista: pNext == NULL escrevia em NULL+0x34 (inofensivo no GBA, TLB miss no PS2) ---
sub1("src/animedrv.c", "        anim->pNext->pPrev = NULL;", "        if (anim->pNext != NULL) anim->pNext->pPrev = NULL;")
sub1("src/animedrv.c", "        anim->pNext->pPrev = anim->pPrev;\n    }\n\n    anim->state = 0;", "        if (anim->pNext != NULL) anim->pNext->pPrev = anim->pPrev;\n    }\n\n    anim->state = 0;")

# --- loops do{ proc = Proc_FindNext(); proc->x ... }while(proc != NULL): no GBA a escrita em NULL e inofensiva; no PS2 o compilador assume proc != NULL e o laco vira infinito ---
import re as _re
for _f in ("src/worldmap_rm.c", "src/worldmap_automu.c"):
    _s = rd(_f)
    _s2 = _re.sub(r"(?P<i>[ \t]+)proc = Proc_FindNext\(&procIter\);\n(?!\s*\n?\s*if \(proc (==|!=) NULL\))", lambda m: m.group(0) + m.group('i') + "if (proc == NULL) break;\n", _s)
    if _s2 != _s: wr(_f, _s2)
sub1("src/face.c", "void SetFaceBlinkControlById(int slot, int blinkControl) {\n    SetFaceBlinkControl(", "void SetFaceBlinkControlById(int slot, int blinkControl) {\n    if (gFaces[slot] == NULL) return;\n    SetFaceBlinkControl(")
sub1("src/face.c", "int GetFaceDisplayBits(struct FaceProc* proc) {\n    return proc->displayBits;", "int GetFaceDisplayBits(struct FaceProc* proc) {\n    if (proc == NULL) return 0;\n    return proc->displayBits;")
sub1("src/face.c", "    blinkProc = proc->pBlinkProc;\n    blinkProc->blinkControl = blinkControl;", "    blinkProc = proc->pBlinkProc;\n    if (blinkProc == NULL) return;\n    blinkProc->blinkControl = blinkControl;")

# --- enderecos de VRAM do GBA fixos no codigo ((void *)0x6013800 etc.): no PS2 a VRAM e o buffer VRAM; o acesso caia em TLB miss/enderecos reais do PS2 ---
import glob as _glob
for _p in _glob.glob(os.path.join(D, "src", "*.c")):
    if _p.endswith("msg_data.c"): continue
    _s = open(_p).read(); _o = _s
    _s = _re.sub(r"\(void ?\*\)\(0x60([0-9A-Fa-f]{5}) \+", lambda m: "(void *)(VRAM + 0x%X +" % int(m.group(1), 16), _s)
    _s = _re.sub(r"\(void ?\*\)0x60([0-9A-Fa-f]{5})\b", lambda m: "(void *)(VRAM + 0x%X)" % int(m.group(1), 16), _s)
    if _s != _o: open(_p, "w").write(_s)
sub1("src/banim-ekrmain.c", "int GetAnimPosition(struct Anim * anim)\n{\n", "int GetAnimPosition(struct Anim * anim)\n{\n    if (anim == NULL) return EKR_POS_L;\n")
sub1("src/player_interface.c", "    s16 frameCount = proc->unitClock;\n\n    if (unit->statusIndex", "    s16 frameCount = proc->unitClock;\n\n    if (unit == NULL) return;\n    if (unit->statusIndex")

# --- RefreshUnitsOnBmMap: unidade vinda do save com posicao fora do mapa -> no GBA escreve lixo inofensivo, no PS2 e TLB miss ---
_s = rd("src/bmmap.c")
_a = _s.index("void RefreshUnitsOnBmMap(void) {"); _b = _s.index("void RefreshTorchlightsOnBmMap", _a) if "void RefreshTorchlightsOnBmMap" in _s[_a:] else len(_s)
_f = _s[_a:_b]
_f2 = _f.replace("        if (unit->state & US_HIDDEN)\n            continue;", "        if (unit->state & US_HIDDEN)\n            continue;\n        if (unit->xPos < 0 || unit->yPos < 0 || unit->xPos >= gBmMapSize.x || unit->yPos >= gBmMapSize.y)\n            continue;")
_f2 = _f2.replace("            if (unit->state & US_HIDDEN)\n                continue;", "            if (unit->state & US_HIDDEN)\n                continue;\n            if (unit->xPos < 0 || unit->yPos < 0 || unit->xPos >= gBmMapSize.x || unit->yPos >= gBmMapSize.y)\n                continue;")
if _f2 != _f: wr("src/bmmap.c", _s[:_a] + _f2 + _s[_b:])

# --- m4a: ponteiros da ROM (0x08xxxxxx) viram ponteiros reais; PSG precisa saber quando uma nota CGB e disparada ---
_s = rd("src/m4a.c")
if "Ps2Audio_Resolve" not in _s:
    _s = _s.replace("track->cmdPtr = songHeader->part[i];", "track->cmdPtr = (u8 *)Ps2Audio_Resolve((u32)songHeader->part[i]);")
    _s = _s.replace("                    *nrx4ptr = channels->n4;\n                    channels->n4 &= 0x7f;", "                    *nrx4ptr = channels->n4;\n                    Ps2Psg_Trigger(ch);\n                    channels->n4 &= 0x7f;")
    _s = _s.replace("                *nrx4ptr = channels->n4 | 0x80;\n                if (ch == 1", "                *nrx4ptr = channels->n4 | 0x80;\n                Ps2Psg_Trigger(ch);\n                if (ch == 1")
    _s = _s.replace('#include "gba/m4a_internal.h"', '#include "gba/m4a_internal.h"\nextern void *Ps2Audio_Resolve(unsigned int v);\nextern void Ps2Psg_Trigger(unsigned ch);', 1)
    wr("src/m4a.c", _s)

# --- sub_80131F0 (linha do minimapa): linha horizontal divide por zero; no GBA o BIOS devolve algo e o laco nem executa ---
sub1("src/bmlib.c", "val1 = ((x2 - x1) << 0x10) / (y2 - y1);", "val1 = (y2 == y1) ? 0 : ((x2 - x1) << 0x10) / (y2 - y1);")

# --- SRAM: offset do bloco gravado como (uintptr_t)endereco (so funciona com SRAM em 0x0E000000, alinhada a 64 KB). No PS2 a SRAM fica em
#     endereco qualquer: leitura somava a base de novo (+0x10) e carregava lixo (unidades sem classe -> tela preta). Offset passa a ser sempre
#     o real; ao ler, o offset gravado (de saves antigos) e substituido pelo real. ---
sub1("src/bmsave-lib.c", "#if BUGFIX\n    chuck->offset = SramAddrToOffset(GetSaveWriteAddr(index));\n#else\n    chunk->offset = (uintptr_t)GetSaveWriteAddr(index);\n#endif", "    chunk->offset = SramAddrToOffset(GetSaveWriteAddr(index));")
sub1("src/bmsave-lib.c", "    return VerifySaveBlockChecksum(chunk);\n}\n\nvoid WriteSaveBlockInfo", "    if (!VerifySaveBlockChecksum(chunk))\n        return false;\n    chunk->offset = SramAddrToOffset(GetSaveWriteAddr(index));\n    return true;\n}\n\nvoid WriteSaveBlockInfo")

# --- AnimDisplayPrivate: pSpriteData invalido (ex.: pool + offset de quadro obsoleto) -> TLB miss no PS2; no GBA le lixo de bus aberto. Ignora o sprite. ---
sub1("src/animedrv.c", "    if (!oamData)\n        return;\n\n    if ((oamData->header &~ 0xFFFF) == 0xFFFF0000)", "    if (!oamData)\n        return;\n    if ((u32)oamData < 0x100000 || (u32)oamData >= 0x2000000)\n        return;\n    { u32 hdr_ = oamData->header; if ((hdr_ & 0xFFFF0000) == 0xFFFF0000 && (hdr_ & 0xFFFF) > 0x80) return; }\n\n    if ((oamData->header &~ 0xFFFF) == 0xFFFF0000)")

# --- unitlistscreen: gUnknown_0200F15C e u32[8], mas dois laros zeram 20 posicoes. No GBA a variavel e a ultima do overlay (overflow inofensivo);
#     no PS2 vem antes de gUnknown_0200E140/E148/E150 e o overflow corrompia os Text (colunas com tiles lixo e BG2 inundado de tilemap). ---
sub1("src/unitlistscreen.c", "    for (i = 0; i < 20; i++)\n    {\n        gUnknown_0200F15C[i] = UINT8_MAX;", "    for (i = 0; i < 8; i++)\n    {\n        gUnknown_0200F15C[i] = UINT8_MAX;")
sub1("src/unitlistscreen.c", "    for (r4 = 0; r4 < 20; r4++)\n    {\n        gUnknown_0200F15C[r4] = UINT8_MAX;", "    for (r4 = 0; r4 < 8; r4++)\n    {\n        gUnknown_0200F15C[r4] = UINT8_MAX;")

# --- DisplayBmTile: no GBA, ler tile/nevoa fora do mapa devolve lixo inofensivo (espelho da EWRAM/bus aberto); no PS2 da TLB miss. Fora de gBmMapSize: tile 0, sem nevoa. ---
sub1("src/bmmap.c", """    u16* tile = sTilesetConfig + gBmMapBaseTiles[yBmMap][xBmMap];

    // TODO: palette id constants
    u16 base = gBmMapFog[yBmMap][xBmMap] ? (6 << 12) : (11 << 12);
""", """#ifdef _EE
    int oob = ((u32)yBmMap >= (u32)gBmMapSize.y || (u32)xBmMap >= (u32)gBmMapSize.x);
    u16* tile = sTilesetConfig + (oob ? 0 : gBmMapBaseTiles[yBmMap][xBmMap]);
    u16 base = (!oob && gBmMapFog[yBmMap][xBmMap]) ? (6 << 12) : (11 << 12);
#else
    u16* tile = sTilesetConfig + gBmMapBaseTiles[yBmMap][xBmMap];
    u16 base = gBmMapFog[yBmMap][xBmMap] ? (6 << 12) : (11 << 12);
#endif
""")

# --- WriteGameSavePackedUnit: le pClassData->number/pCharacterData->number antes de checar NULL (slot vazio): no GBA le a BIOS, no PS2 e TLB miss ---
sub1("src/bmsave.c", "    unitp.pid = UNIT_CHAR_ID(unit);\n    unitp.jid = unit->pClassData->number;\n", "    unitp.pid = unit->pCharacterData ? UNIT_CHAR_ID(unit) : 0;\n    unitp.jid = unit->pClassData ? unit->pClassData->number : 0;\n")

# --- WriteGameSave: slot de unidade vazio (pCharacterData NULL) -> UNIT_CHAR_ID le NULL+4 (BIOS no GBA, TLB miss no PS2) ---
sub1("src/bmsave.c", "        SGM_SetCharacterKnown(UNIT_CHAR_ID(&gUnitArrayBlue[i]), &info);", "        if (gUnitArrayBlue[i].pCharacterData != NULL) SGM_SetCharacterKnown(UNIT_CHAR_ID(&gUnitArrayBlue[i]), &info);")

# --- apoio: GetUnitSupporterUnit pode devolver NULL (apoiador morto/ausente) e CanUnitSupportNow chama GetUnitTotalSupportLevel(NULL) ---
sub1("src/bmreliance.c", "int GetUnitSupporterCount(struct Unit* unit)\n{\n    if (!UNIT_SUPPORT_DATA(unit))", "int GetUnitSupporterCount(struct Unit* unit)\n{\n    if (unit == NULL || !UNIT_SUPPORT_DATA(unit))")
sub1("src/bmreliance.c", "u8 GetUnitSupporterCharacter(struct Unit* unit, int num)\n{\n    if (!UNIT_SUPPORT_DATA(unit))", "u8 GetUnitSupporterCharacter(struct Unit* unit, int num)\n{\n    if (unit == NULL || !UNIT_SUPPORT_DATA(unit))")

# --- mapa-mundi: sub_80BE9D8 com indice de no fora de 0..0x1B le fora das tabelas (no GBA cai em dados da ROM) ---
sub1("src/worldmap_player_interface.c", "void sub_80BE9D8(struct GMapPIProc * param_1, int param_2)\n{\n", "void sub_80BE9D8(struct GMapPIProc * param_1, int param_2)\n{\n    if ((u32)param_2 >= 0x1C)\n    {\n        gUnknown_0201B430[0x15D] = 0;\n        gUnknown_0201B430[0x15E] = 0;\n        return;\n    }\n")

# --- batalha: lado sem unidade valida (pClassData/pCharacterData NULL) e actor NULL na animacao de mapa: no GBA le a BIOS, no PS2 e TLB miss ---
sub1("src/banim-ekrbattleintro.c", "        animdef1 = unit_bu1->pClassData->pBattleAnimDef;", "        animdef1 = unit_bu1->pClassData ? unit_bu1->pClassData->pBattleAnimDef : 0;")
sub1("src/banim-ekrbattleintro.c", "        animdef2 = unit_bu2->pClassData->pBattleAnimDef;", "        animdef2 = unit_bu2->pClassData ? unit_bu2->pClassData->pBattleAnimDef : 0;")
sub1("src/banim-ekrbattleintro.c", "    pid = unit_bu1->pCharacterData->number - 1;\n    jid = unit_bu1->pClassData->number;", "    pid = unit_bu1->pCharacterData ? unit_bu1->pCharacterData->number - 1 : 0;\n    jid = unit_bu1->pClassData ? unit_bu1->pClassData->number : 0;")
sub1("src/banim-ekrbattleintro.c", "    pid = unit_bu2->pCharacterData->number - 1;\n    jid = unit_bu2->pClassData->number;", "    pid = unit_bu2->pCharacterData ? unit_bu2->pCharacterData->number - 1 : 0;\n    jid = unit_bu2->pClassData ? unit_bu2->pClassData->number : 0;")
sub1("src/mapanim.c", "void SetBattleMuPaletteByIndex(int actorNum)\n{\n", "void SetBattleMuPaletteByIndex(int actorNum)\n{\n    if (gManimSt.actor[0].unit == NULL || gManimSt.actor[1].unit == NULL || gManimSt.actor[0].bu == NULL || gManimSt.actor[1].bu == NULL)\n        return;\n")

# --- lookups de unidade que podem devolver NULL (personagem nao recrutado/ausente) e sao desreferenciados direto: FE8_U (port/prelude.h) troca NULL por uma unidade zerada ---
for _p in _glob.glob(os.path.join(D, "src", "*.c")):
    _s = open(_p).read(); _o = _s
    _s = _re.sub(r"(?<!FE8_U\()GetUnitFromCharId\(([^()]*)\)->", lambda m: "FE8_U(GetUnitFromCharId(%s))->" % m.group(1), _s)
    if _s != _o: open(_p, "w").write(_s)
sub1("src/banim-ekrdragon-myrrh.c", "#define UNIT_FACTION_FAKE(bu) UNIT_FACTION(GetUnitFromCharId(UNIT_CHAR_ID(&(bu)->unit)))", "#define UNIT_FACTION_FAKE(bu) UNIT_FACTION(FE8_U(GetUnitFromCharId(UNIT_CHAR_ID(&(bu)->unit))))")

# --- banim-ekrbattleintro.c: lado sem unidade valida (pClassData NULL) desreferenciado nas checagens de Myrrh/dancer/dragao: troca NULL por buffer zerado ---
_p = os.path.join(D, "src", "banim-ekrbattleintro.c")
_s = open(_p).read()
_s2 = _re.sub(r"\b(unit_bu[12])->pClassData->", lambda m: "((%s)->pClassData ? (%s)->pClassData : (const struct ClassData *)(void *)gFe8NullUnitBuf)->" % (m.group(1), m.group(1)), _s)
if _s2 != _s: open(_p, "w").write(_s2)

# --- CanUnitUseWeapon com unidade vazia (pClassData/pCharacterData NULL): UNIT_CATTRIBUTES le NULL+0x28 (BIOS no GBA, TLB miss no PS2) ---
sub1("src/bmitem.c", "s8 CanUnitUseWeapon(struct Unit* unit, int item) {\n    if (item == 0)\n        return FALSE;\n", "s8 CanUnitUseWeapon(struct Unit* unit, int item) {\n    if (item == 0)\n        return FALSE;\n\n    if (unit == NULL || unit->pClassData == NULL || unit->pCharacterData == NULL)\n        return FALSE;\n")

# --- 16:9 nativo: mapa do capitulo numa camada larga (ver gfx_cache.c / render.c: gPortWide*) ---
sub1("src/bmmap.c", "void UpdateBmMapDisplay(void) {\n    // TODO: figure out\n", r'''#ifdef _EE
extern int BeWsPad(void);
u16 gPortWideTm[2][0x800];     /* [0] = BG3 (mapa), [1] = BG2 (alcance de movimento); 64x32 tiles em 2 blocos de 32x32 (formato GBA, tamanho 1) */
extern int gPortWideAge, gPortWideBg2, gPortWideHofs, gPortWideVofs;
#define PW_COLS 20   /* (15 + 284) / 16 = 18.7 -> 19 metatiles; 20 por folga */
#define PW_ROWS 11
static void PortWidePut(u16* bg, int mx, int my, u16 a, u16 b, u16 c, u16 d) {
    int tx = mx * 2, ty = my * 2;
    u16* o = bg + ((tx >> 5) << 10) + ty * 32 + (tx & 31);
    o[0] = a; o[1] = b; o[32] = c; o[33] = d;
}
static void PortWideMapRender(void) {
    int pad = BeWsPad();
    int ix, iy;
    if (pad == 0) { gPortWideAge = 99; return; }

    {   /* camera: o jogo ve 240 px, a tela mostra pad px a mais de cada lado */
        int w = gBmMapSize.x * 16;
        int lo = pad, hi = w - DISPLAY_WIDTH - pad;
        if (hi < lo) lo = hi = (w - DISPLAY_WIDTH) / 2;
        gBmSt.cameraMax.x = hi;
        if (gBmSt.camera.x < lo) gBmSt.camera.x = lo;
        if (gBmSt.camera.x > hi) gBmSt.camera.x = hi;
    }
    {
        int sx = gBmSt.camera.x - pad;               /* x do mundo na borda esquerda da tela larga */
        int ox = sx >> 4, oy = gBmSt.camera.y >> 4;
        int ov = (gBmSt.gameStateBits & BM_FLAG_0) != 0;
        gPortWideHofs = (sx & 15) + pad;             /* DrawBgStrip desenha a partir de x = -pad */
        gPortWideVofs = gBmSt.camera.y & 15;
        gPortWideBg2 = ov;
        for (iy = 0; iy < PW_ROWS; iy++) {
            for (ix = 0; ix < PW_COLS; ix++) {
                int xm = ox + ix, ym = oy + iy;
                int oob = ((u32)ym >= (u32)gBmMapSize.y || (u32)xm >= (u32)gBmMapSize.x);
                if (oob) {
                    PortWidePut(gPortWideTm[0], ix, iy, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF);
                    if (ov) PortWidePut(gPortWideTm[1], ix, iy, 0, 0, 0, 0);
                    continue;
                }
                {
                    u16* tile = sTilesetConfig + gBmMapBaseTiles[ym][xm];
                    u16 base = gBmMapFog[ym][xm] ? (6 << 12) : (11 << 12);
                    PortWidePut(gPortWideTm[0], ix, iy, base + tile[0], base + tile[1], base + tile[2], base + tile[3]);
                }
                if (ov) {
                    if (gMapMovementSigned[ym][xm] >= 0)
                        PortWidePut(gPortWideTm[1], ix, iy, 0x4280, 0x4281, 0x4282, 0x4283);
                    else if (gMapRangeSigned[ym][xm])
                        PortWidePut(gPortWideTm[1], ix, iy, 0x5280, 0x5281, 0x5282, 0x5283);
                    else
                        PortWidePut(gPortWideTm[1], ix, iy, 0, 0, 0, 0);
                }
            }
        }
        gPortWideAge = 0;
    }
}
#endif

void UpdateBmMapDisplay(void) {
    // TODO: figure out
#ifdef _EE
    PortWideMapRender();
#endif
''')

# --- 16:9 nativo: corte de sprites fora da tela de 240 px alargado pelas colunas extras ---
for _f, _pairs in (("src/bmudisp.c", [("x < -16 || x > DISPLAY_WIDTH+16", "x < -16 - BeWsPad() || x > DISPLAY_WIDTH+16 + BeWsPad()"),
                                      ("x < -16 || x > DISPLAY_WIDTH)", "x < -16 - BeWsPad() || x > DISPLAY_WIDTH + BeWsPad())")]),
                   ("src/mu.c", [("x < -0x10 || x > 0x100 ||", "x < -0x10 - BeWsPad() || x > 0x100 + BeWsPad() ||")]),
                   ("src/bmusailment.c", [("(x < 0) || (x > DISPLAY_WIDTH)", "(x < -BeWsPad()) || (x > DISPLAY_WIDTH + BeWsPad())")])):
    _p = os.path.join(D, _f)
    _s = open(_p).read()
    for _a, _b in _pairs:
        _s = _s.replace(_a, _b)
    open(_p, "w").write(_s)

# --- idioma: GetStringFromIndex*/Buffer consultam a traducao pt-BR (src/lang.c) antes de decodificar o texto original ---
_p = os.path.join(D, "src/msg.c")
_s = open(_p).read()
if "PortLangGet" not in _s:
    _s = _s.replace("EWRAM_DATA struct MsgBuffer sMsgString = {0};",
                    "#ifdef _EE\nextern const u8 *PortLangGet(int index);\n#endif\nEWRAM_DATA struct MsgBuffer sMsgString = {0};", 1)
    _s = _s.replace("""        return sMsgString.buffer1;
    CallARM_DecompText(gMsgTable[index], sMsgString.buffer1);""",
"""        return sMsgString.buffer1;
#ifdef _EE
    { const u8 *pt = PortLangGet(index);
      if (pt) { CopyString(sMsgString.buffer1, (const char *)pt); SetMsgTerminator(sMsgString.buffer1); sActiveMsg = index; return sMsgString.buffer1; } }
#endif
    CallARM_DecompText(gMsgTable[index], sMsgString.buffer1);""", 1)
    _s = _s.replace("""char * GetStringFromIndexInBuffer(int index, char *buffer)
{
""", """char * GetStringFromIndexInBuffer(int index, char *buffer)
{
#ifdef _EE
    { const u8 *pt = PortLangGet(index);
      if (pt) { CopyString(buffer, (const char *)pt); SetMsgTerminator(buffer); return buffer; } }
#endif
""", 1)
    open(_p, "w").write(_s)
