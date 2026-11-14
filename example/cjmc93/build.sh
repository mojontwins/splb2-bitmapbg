#!/bin/sh
# Build CJMC93 against the splib2-bitmapbg in this repository.
#
# Needs: the z88dk classic toolchain in the PATH (../../docs/TOOLCHAIN.md),
# Python 3 (standard library only), and ../../splib2f.lib built with
# BITMAPBG = 1 (../../build.sh). The zx0 compressor is needed only when a
# backdrop in gfx/backdrops changes (set ZX0=/path/to/zx0 if it isn't in
# the PATH): packed copies are kept in gfx/packed.
set -e
cd "$(dirname "$0")"
LIB=$(cd "${SPLIB:-../..}" && pwd)
python3 tools/conv.py
cd src
zcc +zx -vn -m -unsigned -zorg=24200 -I$LIB -l$LIB/splib2f -o cjmc93.bin cjmc93.c
END=$((24200 + $(stat -c %s cjmc93.bin)))
printf 'binary ends at %X (the stack is below $D3E6: keep it under $D2E6)\n' $END
[ $END -lt $((0xD2E6)) ] || { echo "too big"; exit 1; }
appmake +zx -b cjmc93.bin -o ../cjmc93.tap --org 24200
