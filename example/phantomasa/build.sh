#!/bin/sh
# Build Phantomasa (128K, one load) against the splib2 in this repository.
#
# Needs: the z88dk classic toolchain in the PATH (../../docs/TOOLCHAIN.md),
# Python 3 (standard library only). The zx0 compressor
# (https://github.com/einar-saukas/ZX0) only when a screen changes.
#
# The game uses splib2 with a different memory layout (shadow screen at
# $C800, IM2 table and rotation tables at $E000) so the original music
# player can stay at $F600: ../../splib2hi.lib, built here if missing.
set -e
cd "$(dirname "$0")"
LIB=$(cd "${SPLIB:-../..}" && pwd)
[ -f "$LIB/splib2hi.lib" ] || "$LIB/build.sh" variant splib2hi 'BGSHADOW=$C8' 'SProtatetbl=$E000' 'IM2TABLE=$E000'
python3 tools/conv.py
cd src
zcc +zx -vn -m -unsigned -zorg=24200 -I$LIB -l$LIB/splib2hi -o phantomasa.bin phantomasa.c
end=$((24200 + $(stat -c %s phantomasa.bin)))
printf 'binary ends at %X (stack below $C000: keep it under $BE00)\n' $end
[ $end -lt $((0xBE00)) ] || { echo "too big"; exit 1; }
cd ..
python3 tools/mktap.py src/phantomasa.bin phantomasa.tap
