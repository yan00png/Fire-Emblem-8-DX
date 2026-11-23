#!/bin/bash
# Uso: port/make_release.sh <dir_build> <fe8.gba> [saida.elf]
# Gera o ELF de lancamento: sem simbolos e sem os dados identicos a trechos da ROM (restaurados da fe8.gba no boot).
set -e
B=$(cd "$1" && pwd); ROM=$(cd "$(dirname "$2")" && pwd)/$(basename "$2"); OUT=${3:-fe8_ps2_release.elf}
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
export PS2DEV=${PS2DEV:-/usr/local/ps2dev} PS2SDK=${PS2SDK:-/usr/local/ps2dev/ps2sdk} GSKIT=${GSKIT:-/usr/local/ps2dev/gsKit}
export PATH=$PATH:$PS2DEV/ee/bin
MAP=$(mktemp /tmp/fe8_map.XXXXXX)
rm -f fe8_ps2.elf src/main_ps2.o
make FE8_BUILD=$B EE_LDFLAGS="-L$PS2SDK/ee/lib -Wl,-zmax-page-size=128 -L$GSKIT/lib -Wl,-Map=$MAP" >/dev/null 2>&1
[ -f fe8_ps2.elf ] || { echo "link falhou"; exit 1; }
python3 tools/romstrip.py fe8_ps2.elf $MAP "$ROM" "$OUT.tmp"
mips64r5900el-ps2-elf-strip -s "$OUT.tmp" -o "$OUT"; rm -f "$OUT.tmp" $MAP
ls -l "$OUT" | awk '{print "ELF final:", $5, "bytes"}'
