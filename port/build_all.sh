#!/bin/bash
# Uso: port/build_all.sh <dir_decomp> <dir_build>
# Constrói o ELF do jogo para PS2: patches -> assets -> objetos C -> port em C -> dados (.s) -> banim -> fatias da ROM -> link.
# Requer baserom.gba (USA) na raiz da decomp e o ps2dev no PATH/ambiente.
D=$(cd "$1" && pwd); B=$(mkdir -p "$2" && cd "$2" && pwd); P=$(cd "$(dirname "$0")" && pwd); R=$(dirname "$P")
export PS2DEV=${PS2DEV:-/opt/ps2dev} PS2SDK=${PS2SDK:-/opt/ps2dev/ps2sdk} GSKIT=${GSKIT:-/opt/ps2dev/gsKit}
export PATH=$PATH:$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2SDK/bin
echo "== patches"; python3 $P/apply_port_patches.py $D || true
echo "== assets";  $P/gen_assets.sh $D
echo "== objetos C"; rm -rf $B/c; $P/build_objs.sh $D $B/c
CF="-c -O2 -std=gnu11 -w -G0 -iquote $P/include -iquote $D/include -iquote $D -include $P/prelude.h -D_EE -DNONMATCHING=1 -DBUGFIX=1 -fno-strict-aliasing -fgnu89-inline"
for f in $P/src/*.c; do (cd $D && mips64r5900el-ps2-elf-gcc $CF $f -o $B/c/zz_$(basename $f .c).o); done
echo "== dados"; rm -rf $B/data; mkdir -p $B/data; $P/assemble_data.sh $D $B/data
echo "== banim"; rm -rf $B/banim; python3 $P/gen_banim.py $D $B/banim
echo "== fatias da ROM"; $P/gen_rom_slices.sh $D $B/rom_slices.o
echo "== ld script"; python3 $P/gen_ldscript.py $D $P/fe8_ps2.ld
echo "== link"; cd $R && rm -f fe8_ps2.elf src/*.o src/platform/*.o && make FE8_BUILD=$B 2>&1 | grep -vE '^mips64r5900el-ps2-elf-gcc|^rm |bin2c' | head -30
ls -l $R/fe8_ps2.elf
