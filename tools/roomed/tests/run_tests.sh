#!/bin/sh
# roomed self tests (Linux/macOS): load/save round trip, binary export
# against tools/conv.py, and a scripted UI session with the headless build.
set -e
cd "$(dirname "$0")"
make -C .. roomed headless >/dev/null
R=../roomed
ROOMS=../../../gfx/rooms.txt
fail=0
rm -f out_*
# 1. load -> save -> load -> save gives the same file
cp $ROOMS out_rt1.txt
cp ../../../gfx/tileset.png ../../../gfx/templ.png . 2>/dev/null || true
mkdir -p preview && cp ../../../gfx/preview/*.png preview/
$R -export out_rt1.txt out_rt1.h
python3 check_export.py out_rt1.h ../../../src/assets.h || fail=1
# 2. scripted UI session
cp $ROOMS out_ui.txt
ROOMED_SCRIPT=ui_session.txt ../roomed-headless out_ui.txt >/dev/null
$R -info out_session.txt > out_info.txt
grep -q 'room 0 "scene 0, room 1: empty": 8 objects' out_info.txt || { echo "FAIL ui session"; cat out_info.txt; fail=1; }
grep -q '^rect 5 10 78 ' out_session.txt || { echo "FAIL ui: rect"; fail=1; }
grep -q '^pattern 6 15 64 [0-9]* 9 18 2 2' out_session.txt || { echo "FAIL ui: moved pattern"; fail=1; }
grep -q '^char 20 3 33 ' out_session.txt || { echo "FAIL ui: char"; fail=1; }
grep -q '^crate 20 15 33 ' out_session.txt || { echo "FAIL ui: crate"; fail=1; }
echo "ui session: done (screenshots in tests/out_*.bmp)"
[ $fail = 0 ] && echo "ALL TESTS PASSED"
exit $fail
