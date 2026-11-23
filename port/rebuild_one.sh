#!/bin/bash
# Uso: port/rebuild_one.sh <dir_decomp> <dir_build> <arquivo.c ...>  -> recompila só esses arquivos e refaz o link
D=$(cd "$1" && pwd); B=$2; shift 2; P=$(cd "$(dirname "$0")" && pwd); R=$(dirname "$P")
export PS2DEV=/opt/ps2dev PS2SDK=/opt/ps2dev/ps2sdk GSKIT=/opt/ps2dev/gsKit PATH=$PATH:/opt/ps2dev/bin:/opt/ps2dev/ee/bin:/opt/ps2dev/iop/bin:/opt/ps2dev/ps2sdk/bin
CF="-c -O2 -std=gnu11 -w -G0 -iquote $P/include -iquote $D/include -iquote $D -include $P/prelude.h -D_EE -DNONMATCHING=1 -DBUGFIX=1 -fno-strict-aliasing -fgnu89-inline"
for f in "$@"; do
  case $f in /*) src=$f; out=$B/c/zz_$(basename $f .c).o;; *) src=$D/$f; out=$B/c/$(basename $f .c).o;; esac
  (cd $D && mips64r5900el-ps2-elf-gcc $CF $src -o $out) || exit 1
done
cd $R && rm -f fe8_ps2.elf src/*.o src/platform/*.o && make FE8_BUILD=$B 2>&1 | grep -E ' error|undefined|multiple' | head
ls -l $R/fe8_ps2.elf | awk '{print $5}'
