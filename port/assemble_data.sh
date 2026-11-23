#!/bin/sh
# Uso: port/assemble_data.sh <dir_da_decomp> <dir_saida>
# Monta data/*.s com o assembler do EE. O gas do MIPS usa '#' (não '@' nem '//') para comentários,
# então a montagem é feita numa cópia com os comentários convertidos. data_fe6sio.s (código ARM do cabo link) é ignorado.
D=$(cd "$1" && pwd); O=$2; W=/tmp/fe8s_work; mkdir -p "$O"; rm -rf $W && mkdir -p $W/src
cp -r $D/include $D/data $W/ && cp -r $D/src/data $W/src/data && cp $D/baserom.gba $W/
for d in graphics sound texts; do ln -s $D/$d $W/$d; done
cd $W
grep -rl '@' include data src/data --include=*.s --include=*.inc | xargs sed -i 's/@/#/g'
find include data src/data \( -name '*.s' -o -name '*.inc' \) | xargs sed -i -E 's#^([[:space:]]*)//#\1\##'
ls data/*.s | grep -v data_fe6sio | xargs -P 8 -I{} sh -c 'n=$(basename {} .s); mips64r5900el-ps2-elf-as -I include -I . {} -o '"$O"'/$n.o 2>/tmp/as_$n.log || echo "FALHOU $n: $(grep -m1 Error /tmp/as_$n.log)"'
echo "objetos de dados: $(ls $O/*.o | wc -l)"
