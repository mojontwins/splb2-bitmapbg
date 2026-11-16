#!/bin/sh
# Build the Phantomas Compilation against the splib2 in this repository.
#
# Needs: the z88dk classic toolchain in the PATH (../../docs/TOOLCHAIN.md),
# Python 3 (standard library only), and ../../splib2tc.lib: splib2 in tile
# mode with coloured sprites, made with
#   ../../build.sh variant splib2tc BITMAPBG=0 SPRCOLOUR=1
set -e
cd "$(dirname "$0")"
LIB=$(cd "${SPLIB:-../..}" && pwd)
[ -f "$LIB/splib2tc.lib" ] || { echo "no $LIB/splib2tc.lib: run ../../build.sh variant splib2tc BITMAPBG=0 SPRCOLOUR=1"; exit 1; }
python3 tools/conv.py
python3 tools/edges.py -q      # warnings: rooms that don't meet at an exit
python3 tools/reach.py | tail -1   # warnings: exits you can't reach (tools/reach.py lists them)
python3 tools/entry.py | tail -1   # warnings: enemies where you come in (tools/entry.py lists them)
cd src
zcc +zx -vn -m -unsigned -zorg=24200 -I$LIB -l$LIB/splib2tc -o phcomp.bin phcomp.c
END=$((24200 + $(stat -c %s phcomp.bin)))
printf 'binary ends at %X (the stack is below $EC88: keep it under $EB80)\n' $END
[ $END -lt $((0xEB80)) ] || { echo "too big"; exit 1; }
cd ..
python3 tools/mktap.py
