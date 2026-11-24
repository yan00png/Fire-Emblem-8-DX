# Flags de compilacao da decomp (EE). Uso: P=<dir_port>; . "$P/cflags.sh"  (define CF e CF_NOBUGFIX).
# -funsigned-char: o agbcc (GBA) usa char sem sinal, o EE usa com sinal.
# -mno-check-zero-division: no GBA divisao por zero nao trava; o gcc do EE poe um "break" (excecao) por padrao.
# -fno-aggressive-loop-optimizations: loops da decomp com limite maior que o array (ex.: nodes[0x1C] com i<0x1d) viram loop infinito no gcc moderno (UB); no agbcc eram inofensivos.
# -fno-isolate-erroneous-paths-dereference: o gcc troca desreferencia de NULL comprovada (ex.: UnitActionMenu_Attack(NULL,NULL)) por "teq zero,zero" (trap, crash exc.13); no GBA era leitura inofensiva.
CF="-c -O2 -std=gnu11 -w -G0 -iquote $P/include -iquote include -iquote . -include $P/prelude.h -D_EE -DNONMATCHING=1 -DBUGFIX=1 -fno-strict-aliasing -fno-toplevel-reorder -fgnu89-inline -funsigned-char -mno-check-zero-division -fno-aggressive-loop-optimizations -fno-isolate-erroneous-paths-dereference -fno-delete-null-pointer-checks -ffunction-sections -fdata-sections"
CF_NOBUGFIX=$(echo "$CF" | sed 's/-DBUGFIX=1//')   # bmsave e banim-efxmagic nao compilam com BUGFIX
