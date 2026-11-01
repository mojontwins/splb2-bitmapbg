#!/bin/sh
# Build the Lhikoj example against the splib2 in this repository.
# Needs the z88dk classic toolchain in the PATH (see the repo README) and
# ../../splib2f.lib built with BITMAPBG = 1 (run ../../build.sh first).
set -e
cd "$(dirname "$0")"
python3 tools_conv.py
cd src
zcc +zx -vn -m -unsigned -zorg=24200 -I../../.. -l../../../splib2f -o lhikoj.bin lhikoj.c
printf 'binary ends at %X (stack top is $D5C0: keep at least 256 bytes free, i.e. end below $D4C0)\n' $((24200 + $(stat -c %s lhikoj.bin)))
appmake +zx -b lhikoj.bin -o ../lhikoj.tap --org 24200
