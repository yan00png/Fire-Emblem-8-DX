// Sem log em arquivo (Ps2Log/Ps2Log_OpenFor sao no-op); erros de execucao aparecem na tela (ps2_crash.c).
#pragma once
void Ps2Log(const char *fmt, ...);                 // vai para a memória e, depois de Ps2Log_OpenFor, para o arquivo
void Ps2Log_OpenFor(const char *rom_path);         // log na mesma pasta da ROM (nunca em CD/DVD); despeja o que já foi registrado
void Ps2Fatal(const char *l1, const char *l2, const char *l3, const char *l4);   // mostra o erro na tela e fica parado
