#!/bin/sh
# tests/go.sh SCRIPT PREFIX: build, load the tap, run a key script
# (/home/claude/work/harness/seq.py) and save screenshots as PREFIXname.png
set -e
cd "$(dirname "$0")/.."
. /home/claude/tools/env.sh
./build.sh 2>&1 | grep -v -i warning | tail -2
cd /home/claude/tools/skoolkit && python3 tap2sna.py -c finish-tape=1 --start 24200 $OLDPWD/phcomp.tap $OLDPWD/tests/test.z80 >/dev/null 2>&1
cd $OLDPWD
MAP=src/phcomp.map BIN=src/phcomp.bin python3 /home/claude/work/harness/seq.py tests/test.z80 $1 $2
