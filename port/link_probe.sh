#!/bin/sh
# Uso: port/link_probe.sh <dir_objs_c> <dir_objs_dados>
# Linka tudo ignorando símbolos indefinidos (só para medir tamanho/relocações) e lista o que ainda falta.
PS2SDK=${PS2SDK:-/opt/ps2dev/ps2sdk}; N=mips64r5900el-ps2-elf-nm
echo 'int main(void){return 0;}' > /tmp/stubmain.c; mips64r5900el-ps2-elf-gcc -c /tmp/stubmain.c -o /tmp/stubmain.o
mips64r5900el-ps2-elf-gcc -mno-gpopt -G0 -T$PS2SDK/ee/startup/linkfile -L$PS2SDK/ee/lib -Wl,--unresolved-symbols=ignore-all -o /tmp/fe8_link.elf /tmp/stubmain.o $1/*.o $2/*.o -lc -lkernel && mips64r5900el-ps2-elf-size /tmp/fe8_link.elf
$N -g --defined-only $1/*.o $2/*.o | awk 'NF==3 {print $3}' | sort -u > /tmp/def.txt
$N -u $1/*.o $2/*.o | awk '{print $2}' | sort -u | comm -13 /tmp/def.txt - > /tmp/indefinidos.txt
echo "indefinidos: $(wc -l < /tmp/indefinidos.txt)  (lista em /tmp/indefinidos.txt)"
