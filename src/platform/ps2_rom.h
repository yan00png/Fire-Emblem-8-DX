// Localização, verificação e carga da ROM (só a USA exata, 16 MB, é suportada).
#pragma once
#include <stddef.h>
// Procura a ROM. Ordem: (1) mesma pasta de onde o ELF foi iniciado (argv0): funciona em CD/DVD/ISO, USB, host: e mc0:;
// (2) lista de dispositivos conhecidos, esperando até `wait_seconds` (o pendrive pode demorar a montar).
int  Ps2Rom_Find(const char *argv0, char *out, size_t n, int wait_seconds);
// Confere tamanho e SHA-1. Se não for a ROM certa, mostra o erro na tela e não volta.
void Ps2Rom_Validate(const char *path);
// Lê a ROM inteira para a RAM (alinhada em 64 bytes). Devolve 1 se deu certo.
int  Ps2Rom_Load(const char *path, const unsigned char **rom, unsigned int *size);
