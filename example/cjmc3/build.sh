#!/bin/sh
# Build CJMC3 against the splib2-bitmapbg in this repository.
#
# Needs: the z88dk classic toolchain in the PATH (see ../../docs/TOOLCHAIN.md),
# Python 3 (standard library only), and ../../splib2f.lib built with
# BITMAPBG = 1 (run ../../build.sh first).
#
# The zx0 compressor (https://github.com/einar-saukas/ZX0) is only needed
# if you change a background third, the logo/ending pictures or a screen
# layout: the packed files are kept in gfx/. Editing gfx/sprites.png
# needs nothing extra.
set -e
cd "$(dirname "$0")"
ZX0=${ZX0:-zx0}
if command -v "$ZX0" >/dev/null 2>&1; then
  for f in p1t1 p1t2 p1t3 p2t1 p2t2 p2t3 logo ending; do
    [ gfx/$f.zx0 -nt gfx/$f.bin ] || "$ZX0" -f gfx/$f.bin gfx/$f.zx0 >/dev/null
  done
fi
ZX0="$ZX0" python3 tools/conv.py
cd src
zcc +zx -vn -m -unsigned -zorg=24200 -I${SPLIB:-../../..} -l${SPLIB:-../../..}/splib2f -o cjmc3.bin cjmc3.c
printf 'binary ends at %X (stack top is $D5C0: keep at least 256 bytes free, i.e. end below $D4C0)\n' $((24200 + $(stat -c %s cjmc3.bin)))
appmake +zx -b cjmc3.bin -o ../cjmc3.tap --org 24200
