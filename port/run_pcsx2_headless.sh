#!/bin/bash
# uso: runpcsx2.sh <elf> <prefixo-shots> <segundos>
cd /home/claude/pcsx2_pkg; export LIBGL_ALWAYS_SOFTWARE=1 QT_QPA_PLATFORM=xcb
cp "$1" ./test.elf; rm -f /tmp/$2_*.png
xvfb-run -a -s "-screen 0 1280x720x24" bash -c '
squashfs-root/usr/bin/pcsx2-qt -batch -fastboot -elf $PWD/test.elf > /tmp/pcsx2_run.txt 2>&1 &
P=$!; sleep '"$3"'; import -window root /tmp/'"$2"'_1.png; sleep 6; import -window root /tmp/'"$2"'_2.png; kill $P; sleep 1'
