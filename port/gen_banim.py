#!/usr/bin/env python3
"""Gera os dados de animação de batalha (banim_*) para o port PS2.
Uso: gen_banim.py <dir_decomp> <dir_saida>
O build original usa scripts/arm_compressing_linker.py (objcopy + LZ77 + ponteiros resolvidos no endereço da ROM).
Aqui: cada banim_*_motion.s é montado com o gas do EE; os blocos sem ponteiros (oam_l, oam_r, modes) viram .bin com
cabeçalho "cópia crua" (tipo 0, tratado por LZ77UnCompWram em src/platform/bios.c); o script (.data.script), que contém
ponteiros absolutos para os sheets, fica como objeto normal (relocado pelo linker) também com cabeçalho cru.
Saída: <saida>/banim_blobs.o (+ <saida>/motion/*.o)."""
import os, re, struct, subprocess, sys
D, O = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
EE = "mips64r5900el-ps2-elf-"
W = "/tmp/banim_work"
subprocess.run(["rm", "-rf", W]); os.makedirs(W + "/include"); os.makedirs(W + "/data/banim"); os.makedirs(O + "/motion", exist_ok=True)

def run(*a, **k): return subprocess.run(a, check=True, capture_output=True, text=True, **k)
def fix_comments(txt): return re.sub(r"^(\s*)//", r"\1#", txt.replace("@", "#"), flags=re.M)

for f in os.listdir(D + "/include"):
    if f.endswith(".inc"): open(f"{W}/include/{f}", "w").write(fix_comments(open(f"{D}/include/{f}").read()))

def label_of(path):  # mesma regra do arm_compressing_linker.py
    p = os.path.basename(path).split(".")
    return p[0] if p[1] == "4bpp" else p[0] + "_" + p[1]

def assemble(src, obj): run(EE + "as", "-I", W + "/include", "-I", W, src, "-o", obj)
def section_bytes(obj, sec, out):
    run(EE + "objcopy", "-O", "binary", "-j", sec, obj, out)
    return open(out, "rb").read() if os.path.exists(out) else b""

entries = []
for line in open(D + "/linker_script_banim.txt"):
    if line[0] in "#\n": continue
    obj_sec, _, comp = line.partition(">")
    obj = obj_sec.split("|")[0].strip()
    entries.append(obj)

motions = sorted(e for e in entries if e.endswith("_motion.o"))
raw = {}   # caminho do arquivo do linker script -> bytes a incluir
for m in motions:
    base = os.path.basename(m)[:-2]                      # banim_X_motion
    name = base[:-len("_motion")]
    src = fix_comments(open(f"{D}/data/banim/{base}.s").read())
    s1 = f"{W}/data/banim/{base}.s"; open(s1, "w").write(src)
    o1 = f"{W}/{base}.o1"; assemble(s1, o1)
    oam_l = section_bytes(o1, ".data.oam_l", f"{W}/{name}_oam_l.raw")
    oam_r = section_bytes(o1, ".data.oam_r", f"{W}/{name}_oam_r.raw")
    modes = section_bytes(o1, ".data.modes", f"{W}/{name}_modes.raw")
    script_size = len(section_bytes(o1, ".data.script", f"{W}/{name}_script.raw"))
    hdr = lambda n: struct.pack("<I", n << 8)            # tipo 0 = cópia crua
    raw[f"data/banim/{name}_oam_l.bin.lz"] = hdr(len(oam_l)) + oam_l
    raw[f"data/banim/{name}_oam_r.bin.lz"] = hdr(len(oam_r)) + oam_r
    raw[f"data/banim/{name}_modes.bin"] = modes
    # passo 2: o script ganha rótulo + cabeçalho cru na frente; o resto fica igual
    new = re.sub(r"(\t\.section \.data\.script\n)", r"\1\t.global %s_motion_o\n%s_motion_o:\n\t.word %d\n" % (name, name, script_size << 8), src, count=1)
    assert f"{name}_motion_o:" in new, name
    s2 = f"{W}/data/banim/{base}_p2.s"; open(s2, "w").write(new)
    o2 = f"{O}/motion/{base}.o"; assemble(s2, o2)
    # remove as seções já extraídas (não duplicar dados no ELF)
    run(EE + "objcopy", "-R", ".data.oam_l", "-R", ".data.oam_r", "-R", ".data.modes", o2)

# arquivo de blobs: um rótulo por entrada do linker script (sheets/paletas do disco, oam/modes dos dumps acima)
lines = ['\t.section .rodata.banim,"a"']
for e in entries:
    if e.endswith("_motion.o"): continue
    lab = label_of(e)
    if e in raw:
        fn = f"{W}/blob_{lab}.bin"; open(fn, "wb").write(raw[e]); path = fn
    else:
        path = f"{D}/{e}"
    lines += ["\t.balign 4", f"\t.global {lab}", f"{lab}:", f'\t.incbin "{path}"']
open(f"{W}/banim_blobs.s", "w").write("\n".join(lines) + "\n")
run(EE + "as", f"{W}/banim_blobs.s", "-o", f"{O}/banim_blobs.o")
print("motions:", len(motions), "blobs:", len(lines) // 4, "->", O)
