#!/bin/sh
# Uso: port/gen_rom_slices.sh <dir_decomp> <saida.o>  -> fatia da ROM referenciada por ponteiros fixos em opinfo.c
D=$(cd "$1" && pwd); O=$2
cat > /tmp/rom_slices.s <<EOS
	.section .rodata.romslice,"a"
	.balign 16
	.global gRomSliceOpInfo
gRomSliceOpInfo:
	.incbin "$D/baserom.gba", 0xA37000, 0x5000
EOS
mips64r5900el-ps2-elf-as /tmp/rom_slices.s -o "$O"
