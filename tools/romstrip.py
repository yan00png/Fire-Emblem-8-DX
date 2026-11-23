#!/usr/bin/env python3
"""romstrip.py <fe8_ps2.elf (com simbolos)> <link.map> <fe8.gba> <saida.elf>
Remove do ELF os trechos de dados do jogo (objetos de build/data, c, banim) que sao identicos a trechos da ROM,
zerando-os, e grava em gRomMap a lista (destino, offset na ROM, tamanho). No boot, Ps2Rom_Restore copia-os de volta."""
import sys, struct, re, subprocess
elf_p, map_p, rom_p, out_p = sys.argv[1:5]
elf = bytearray(open(elf_p, 'rb').read()); rom = open(rom_p, 'rb').read()
phoff = struct.unpack_from('<I', elf, 0x1c)[0]; phn = struct.unpack_from('<H', elf, 0x2c)[0]; phes = struct.unpack_from('<H', elf, 0x2a)[0]
segs = []
for i in range(phn):
    t, off, va, pa, fsz, msz, fl, al = struct.unpack_from('<8I', elf, phoff + i * phes)
    if t == 1 and fsz: segs.append((off, va, fsz))
def v2o(v):
    for off, va, fsz in segs:
        if va <= v < va + fsz: return off + v - va
    return None
# faixas permitidas: secoes de dados (.data/.rodata/.sdata...) vindas de objetos do jogo (nao do port nem das libs)
allowed = []
cur = None
for ln in open(map_p, errors='replace'):
    m = re.match(r'^ (\.[\w.\-$]+)\s*$', ln)          # nome de secao de entrada em linha propria
    m2 = re.match(r'^ (\.[\w.\-$]+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+)', ln)
    if m: cur = m.group(1); continue
    if m2: sec, a, s, obj = m2.group(1), int(m2.group(2), 16), int(m2.group(3), 16), m2.group(4)
    else:
        m3 = re.match(r'^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+)', ln)
        if not (m3 and cur): continue
        sec, a, s, obj = cur, int(m3.group(1), 16), int(m3.group(2), 16), m3.group(3)
    cur = None if m2 else cur
    if s and not sec.startswith(('.text', '.bss', '.sbss', '.tbss', '.init', '.fini', '.ctors', '.dtors', '.eh', '.pdr', '.mdebug', '.comment', '.reginfo', '.gnu', '.note')) \
       and re.search(r'/build/(c|data|banim)/|rom_slices', obj):
        allowed.append((a, a + s))
allowed.sort()
print('faixas de dados do jogo:', len(allowed), sum(b - a for a, b in allowed), 'bytes')
idx = {}
for o in range(0, len(rom) - 16, 16): idx.setdefault(rom[o:o + 16], o)
runs = []
for a, b in allowed:
    o0 = v2o(a)
    if o0 is None: continue
    seg = elf[o0:o0 + (b - a)]; n = len(seg); i = 0
    while i < n - 16:
        r = idx.get(bytes(seg[i:i + 16]))
        if r is None: i += 4; continue
        s, ro = i, r
        while s > 0 and ro > 0 and seg[s - 1] == rom[ro - 1]: s -= 1; ro -= 1
        e, re_ = i + 16, r + 16
        while e < n and re_ < len(rom) and seg[e] == rom[re_]: e += 1; re_ += 1
        if e - s >= 64 and len(set(seg[s:e])) > 4: runs.append((a + s, ro, e - s))
        i = e if e > i + 16 else i + 4
# junta e ordena por offset da ROM; remove sobreposicoes de destino
runs.sort(key=lambda r: (r[1], r[0]))
out = []
for d, ro, l in runs:
    if out and out[-1][0] + out[-1][2] == d and out[-1][1] + out[-1][2] == ro: out[-1] = (out[-1][0], out[-1][1], out[-1][2] + l)
    else: out.append((d, ro, l))
# gRomMap
sym = subprocess.check_output(['mips64r5900el-ps2-elf-nm', elf_p], text=True)
addr = int(re.search(r'^([0-9a-f]+) \w gRomMap$', sym, re.M).group(1), 16)
to = v2o(addr); assert elf[to:to + 8] == b'FE8RMAP1', 'tabela gRomMap nao encontrada'
assert len(out) <= 4096, len(out)
total = 0
for d, ro, l in out:
    o = v2o(d)
    elf[o:o + l] = bytes(l); total += l
struct.pack_into('<II', elf, to + 8, len(out), 0)
for k, (d, ro, l) in enumerate(out): struct.pack_into('<III', elf, to + 16 + k * 12, d, ro, l)
open(out_p, 'wb').write(elf)
print('trechos:', len(out), 'bytes removidos:', total)
