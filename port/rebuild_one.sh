#!/bin/bash
# Uso: port/rebuild_one.sh <dir_decomp> <dir_build> <arquivo.c ...>  -> recompila so esses arquivos (mesmas flags do build_objs.sh) e refaz o link
D=$(cd "$1" && pwd); B=$2; shift 2; P=$(cd "$(dirname "$0")" && pwd); R=$(dirname "$P")
export PS2DEV=${PS2DEV:-/opt/ps2dev} PS2SDK=${PS2SDK:-/opt/ps2dev/ps2sdk} GSKIT=${GSKIT:-/opt/ps2dev/gsKit}
export PATH=$PATH:$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2SDK/bin
. "$P/cflags.sh"
for f in "$@"; do
  case $f in /*) src=$f; out=$B/c/zz_$(basename $f .c).o;; *) src=$D/$f; out=$B/c/$(basename $f .c).o;; esac
  FL=$CF; case $(basename $f .c) in bmsave|banim-efxmagic) FL=$CF_NOBUGFIX;; esac
  (cd $D && mips64r5900el-ps2-elf-gcc $FL $src -o $out) || exit 1
done
cd $R && rm -f fe8_ps2.elf src/*.o src/platform/*.o && make FE8_BUILD=$B 2>&1 | grep -E ' error|undefined|multiple' | head
ls -l $R/fe8_ps2.elf | awk '{print $5}'
