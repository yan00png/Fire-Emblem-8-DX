// Plataforma PS2: reinício do IOP e carga dos módulos embutidos (USB FAT32/exFAT, controle).
#pragma once
// a0 = argv[0] (de onde o ELF foi iniciado). O IOP é reiniciado, exceto quando o ELF vem de CD/DVD ou de host:/PCSX2.
void Ps2Platform_Init(const char *a0);
int  Ps2Platform_IopWasReset(void);
int  Ps2Platform_LoadIrx(const char *name, void *buf, unsigned int size);
