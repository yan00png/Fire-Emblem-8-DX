#!/bin/sh
# Uso: port/gen_assets.sh <dir_da_decomp>   (precisa de baserom.gba na raiz da decomp)
# Gera os assets que os data/*.s referenciam por .incbin: gráficos (.4bpp/.lz/.gbapal), amostras de som e .inc de mapa (jsonproc).
D=$1; cd "$D" || exit 1
for t in bin2c gbagfx jsonproc aif2pcm; do make -C tools/$t -j4 >/dev/null 2>&1 || echo "falhou: tools/$t"; done
grep -rhoE '\.incbin +"[^"]+"' data sound src include | sed -E 's/.*"([^"]+)"/\1/; s#^\./##' | grep -v '^baserom.gba$' | sort -u > /tmp/incs.txt
while read p; do [ -e "$p" ] || echo "$p"; done < /tmp/incs.txt > /tmp/missing.txt
make -k -j8 $(tr '\n' ' ' < /tmp/missing.txt) >/tmp/make_assets.log 2>&1
make -k -j8 $(ls src/data/map/change/*.json src/data/map/obj_anim/*.json src/data/map/pal_anim/*.json | sed 's/\.json$/.inc/' | tr '\n' ' ') >>/tmp/make_assets.log 2>&1
while read p; do [ -e "$p" ] || echo "$p"; done < /tmp/incs.txt | wc -l | sed 's/^/assets ainda ausentes: /'
# sheets/paletas de animacao de batalha vindos do disco (linker_script_banim.txt) e cabecalhos gerados que a decomp inclui
awk '!/^#/ && NF {split($0, a, ">"); split(a[1], b, "|"); gsub(/ +$/, "", b[1]); print b[1]}' linker_script_banim.txt | grep '^graphics/' | sort -u > /tmp/banim_gfx.txt
while read p; do [ -e "$p" ] || echo "$p"; done < /tmp/banim_gfx.txt > /tmp/banim_missing.txt
make -k -j8 $(tr '\n' ' ' < /tmp/banim_missing.txt) >>/tmp/make_assets.log 2>&1
while read p; do [ -e "$p" ] || echo "$p"; done < /tmp/banim_gfx.txt | wc -l | sed 's/^/gfx de banim ainda ausentes: /'
tools/gbagfx/gbagfx graphics/debug_font.png graphics/debug_font.4bpp
tools/bin2c/bin2c graphics/debug_font.4bpp debug_font_4bpp | sed 's/^const //' > graphics/debug_font.4bpp.h
tools/jsonproc/jsonproc src/data/chapter_settings.json src/data/chapter_settings.json.txt src/data/chapter_settings.h
