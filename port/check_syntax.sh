#!/bin/sh
# Uso: port/check_syntax.sh <dir_da_decomp> [arquivos...]
# Checa a sintaxe de cada src/*.c com o GCC do EE, usando o shim de tipos em port/include.
D=$1; shift; P=$(cd "$(dirname "$0")" && pwd)
cd "$D" || exit 1
rm -rf /tmp/cc && mkdir -p /tmp/cc
FILES=${*:-$(ls src/*.c)}
CFLAGS="-fsyntax-only -std=gnu11 -w -iquote $P/include -iquote include -iquote . -include $P/prelude.h -D_EE -fno-strict-aliasing"
echo $FILES | tr ' ' '\n' | xargs -P 8 -I{} sh -c 'n=$(basename {} .c); mips64r5900el-ps2-elf-gcc '"$CFLAGS"' {} >/tmp/cc/$n.log 2>&1; echo $? >/tmp/cc/$n.rc'
ok=0; bad=0; for f in /tmp/cc/*.rc; do if [ "$(cat $f)" = 0 ]; then ok=$((ok+1)); else bad=$((bad+1)); fi; done
echo "ok=$ok falhas=$bad"
for f in /tmp/cc/*.rc; do n=$(basename $f .rc); [ "$(cat $f)" != 0 ] && echo "$n: $(grep -m1 -E 'error' /tmp/cc/$n.log | cut -c1-160)"; done
