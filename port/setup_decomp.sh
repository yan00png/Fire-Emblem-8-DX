#!/bin/bash
# Uso: port/setup_decomp.sh <dir_destino>
# Baixa a decomp do FE8 no commit usado pelo port (port/DECOMP_COMMIT) e aplica port/fe8-ps2.patch.
# Depois: copie o baserom.gba (USA) para <dir_destino>/ e rode port/build_all.sh.
set -e
P=$(cd "$(dirname "$0")" && pwd); D=$1; C=$(cat "$P/DECOMP_COMMIT")
mkdir -p "$D" && cd "$D"
git init -q
git remote add origin https://github.com/FireEmblemUniverse/fireemblem8u
git fetch -q --depth 1 origin "$C"
git checkout -q FETCH_HEAD
git apply "$P/fe8-ps2.patch"
echo "decomp pronta em $D (commit $C, patch aplicado). Copie o baserom.gba para la e rode: port/build_all.sh $D <dir_build>"
