#!/usr/bin/env python3
"""Gera port/fe8_ram.ld a partir do ldscript.txt/sym_iwram.txt da decomp: reaproveita o layout exato de EWRAM (overlays + ewram_data)
e IWRAM (símbolos em offsets fixos), mas num bloco de RAM do EE logo após o .bss (INSERT AFTER .bss)."""
import re, sys, os
D, OUT = sys.argv[1], sys.argv[2]
L = open(D + "/ldscript.txt").read().split("\n")
start = next(i for i, l in enumerate(L) if l.startswith("    ewram_overlay_0 "))
iw    = next(i for i, l in enumerate(L) if l.startswith("    IWRAM "))
rom   = next(i for i, l in enumerate(L) if l.startswith("    ROM "))
ew_part = L[start:iw]
def fwd(t):
    return re.sub(r"^(\s*)\. = (0x[0-9A-Fa-f]+);", r"\1. = MAX(., \2);", t)
def pat(t):
    if t.strip().startswith("/*"): return t      # comentários ficam intactos (o "*/" do padrão fecharia o comentário)
    t = re.sub(r"\b(?:src|asm|sound|data)/([A-Za-z0-9_\-\.]+\.o)\(", r"*/\1(", t)
    return re.sub(r"(?<![/\w*.:-])([A-Za-z0-9_\-\.]+\.o)\(", r"*/\1(", t)
# No MIPS algumas overlays ficam maiores que no ARM e passam do ponto onde começam os dados permanentes (0x20188):
# afasta ewram_data em 0x1000 (os símbolos dele são todos relativos ao início da seção, então ficam coerentes entre si).
ew_part = [l.replace("ewram_data __ewram_start (NOLOAD)", "ewram_data __ewram_start + 0x1000 (NOLOAD)") for l in ew_part]
ew_part = [fwd(pat(l)) for l in ew_part if not re.match(r"\s*\. = 0x[0-9A-Fa-f]+; end = \.;", l)]
sym = [fwd(pat(l)) for l in open(D + "/sym_iwram.txt").read().split("\n")]
out = ["/* Gerado por port/gen_ldscript.py - não edite */",
       "gNumMusicPlayers = 9;", "gMaxLines = 0;",
       "SECTIONS", "{", "    . = ALIGN(64);", "    __ewram_start = .;"]
out += ew_part
out += ["    . = MAX(., __ewram_start + 0x41000);", "    . = ALIGN(64);", "    __iwram_start = .;",
        "    IWRAM __iwram_start (NOLOAD) : ALIGN(4)", "    {"] + sym + ["    }",
        "    . = MAX(., __iwram_start + 0x8000);",
        "    ARMCodeToCopy_Start = gUnknown_03003750;", "    ARMCodeToCopy_End = gUnknown_03003750;",   # sem código ARM: tamanho 0 e ponteiros = (buffer + (função - Start)) = a própria função em C
        "}", "INSERT AFTER .bss;"]
# Mescla o bloco de RAM dentro de uma cópia do linkfile do PS2SDK, antes de "_end = ." (para o heap não pisar nele)
LINK = sys.argv[3] if len(sys.argv) > 3 else os.environ.get("PS2SDK", "/opt/ps2dev/ps2sdk") + "/ee/startup/linkfile"
body = out[out.index("{") + 1: out.index("}", len(out) - 6) if False else len(out) - 2]
body = ["    gNumMusicPlayers = 9;", "    gMaxLines = 0;"] + [l for l in body if l.strip() and not l.startswith(("gNumMusicPlayers", "gMaxLines"))]
lk = open(LINK).read().split("\n")
i = next(k for k, l in enumerate(lk) if re.match(r"\s*_end\s*=", l))
merged = lk[:i] + ["", "\t/* ---- RAM do GBA (port/gen_ldscript.py) ---- */"] + body + [""] + lk[i:]
open(OUT, "w").write("\n".join(merged) + "\n")
print("ok", len(merged), "linhas")
