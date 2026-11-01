#!/bin/sh
# Build Eleuterio against the splib2 in this repository, TILE MODE variant.
#
# Needs: the z88dk classic toolchain in the PATH (../../docs/TOOLCHAIN.md),
# Python 3 (standard library only), and ../../splib2t.lib, the library
# built with BITMAPBG = 0. This script builds it if it isn't there
# (../../build.sh tiles). A prebuilt copy is in ../../prebuilt/.
set -e
cd "$(dirname "$0")"
LIB=$(cd "${SPLIB:-../..}" && pwd)
[ -f "$LIB/splib2t.lib" ] || "$LIB/build.sh" tiles || cp "$LIB/prebuilt/splib2t.lib" "$LIB/"
python3 tools/conv.py
cd src
zcc +zx -vn -m -unsigned -zorg=24200 -I$LIB -l$LIB/splib2t -o eleuterio.bin eleuterio.c
printf 'binary ends at %X (must stay below $D800, where splib2 block memory starts)\n' $((24200 + $(stat -c %s eleuterio.bin)))
appmake +zx -b eleuterio.bin -o ../eleuterio.tap --org 24200
