#!/bin/sh
# Uso: port/build_objs.sh <dir_da_decomp> <dir_saida>
# Compila todos os src/*.c da decomp do FE8 para objetos MIPS (EE) com o ps2dev.
D=$1; O=$2; P=$(cd "$(dirname "$0")" && pwd)
cd "$D" || exit 1; mkdir -p "$O" /tmp/occ; rm -f /tmp/occ/*
CF="-c -O2 -std=gnu11 -w -G0 -iquote $P/include -iquote include -iquote . -include $P/prelude.h -D_EE -DNONMATCHING=1 -DBUGFIX=1 -fno-strict-aliasing -fno-toplevel-reorder -fgnu89-inline -funsigned-char -ffunction-sections -fdata-sections"
ls src/*.c | xargs -P 8 -I{} sh -c 'n=$(basename {} .c); mips64r5900el-ps2-elf-gcc '"$CF"' {} -o '"$O"'/$n.o >/tmp/occ/$n.log 2>&1; echo $? >/tmp/occ/$n.rc'
# arquivos que nao compilam com BUGFIX: recompila sem
for n in bmsave banim-efxmagic; do CF2=$(echo "$CF" | sed 's/-DBUGFIX=1//'); mips64r5900el-ps2-elf-gcc $CF2 src/$n.c -o "$O"/$n.o >/tmp/occ/$n.log 2>&1; echo $? >/tmp/occ/$n.rc; done
ok=0; bad=0; for f in /tmp/occ/*.rc; do [ "$(cat $f)" = 0 ] && ok=$((ok+1)) || bad=$((bad+1)); done
echo "objetos ok=$ok falhas=$bad"
for f in /tmp/occ/*.rc; do n=$(basename $f .rc); [ "$(cat $f)" != 0 ] && echo "$n: $(grep -m1 -E 'Error|error' /tmp/occ/$n.log | cut -c1-140)"; done
