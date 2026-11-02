#!/bin/sh
# Build Ghost Embustes 2 against the splib2 in this repository, TILE MODE
# variant (no shadow screen).
#
# Needs: the z88dk classic toolchain in the PATH (../../docs/TOOLCHAIN.md),
# Python 3 (standard library only), and ../../splib2t.lib, the library
# built with BITMAPBG = 0. This script builds it if it isn't there
# (../../build.sh tiles). The zx0 compressor is needed only when a screen
# in gfx/screens changes (set ZX0=/path/to/zx0 if it isn't in the PATH).
set -e
cd "$(dirname "$0")"
LIB=$(cd "${SPLIB:-../..}" && pwd)
[ -f "$LIB/splib2t.lib" ] || "$LIB/build.sh" tiles || cp "$LIB/prebuilt/splib2t.lib" "$LIB/"
python3 tools/conv.py
cd src
zcc +zx -vn -m -unsigned -zorg=24200 -I$LIB -l$LIB/splib2t -o ghostembustes2.bin ghostembustes2.c
END=$((24200 + $(stat -c %s ghostembustes2.bin)))
printf 'binary ends at %X (must stay below $D800, where splib2 block memory starts)\n' $END
[ $END -lt $((0xD800)) ] || { echo "too big"; exit 1; }
appmake +zx -b ghostembustes2.bin -o ../ghostembustes2.tap --org 24200
