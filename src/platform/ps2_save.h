#pragma once
void Ps2Save_Init(const char *romPath);   // carrega <pasta da ROM>/save/fe8.sav para a SRAM do jogo
void Ps2Save_Tick(void);                  // chamar 1x por quadro: grava quando a SRAM mudou
int Ps2Cfg_Read(const char *name, void *buf, int max);          // le <pasta da ROM>/save/<name>; retorna bytes (0 se nao houver / sem midia gravavel)
void Ps2Cfg_Write(const char *name, const void *buf, int n);
