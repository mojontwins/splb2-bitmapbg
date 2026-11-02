#!/bin/sh
# Builds demo.tap: the Beeptune player's three modes with demo.txt.
# Needs z88dk classic (zcc, appmake) and ../beeptune (make in ..).
set -e
cd "$(dirname "$0")"
../beeptune -c demo.txt demo_song.h demo_song
zcc +zx -vn -m -unsigned -zorg=32768 -o demo.bin demo.c -lndos
appmake +zx -b demo.bin -o demo.tap --org 32768
printf 'demo.bin: %d bytes\n' $(stat -c %s demo.bin)
