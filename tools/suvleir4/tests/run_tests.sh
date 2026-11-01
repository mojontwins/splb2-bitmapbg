#!/bin/sh
# Suvleir 4 self tests (Linux/macOS): format round trips, legacy import,
# rendering, and a scripted UI session with the headless build.
set -e
cd "$(dirname "$0")"
make -C .. suvleir4 headless >/dev/null
S=../suvleir4
LEGACY=../../../example/lhikoj/orig/legacy-screens
rm -f out_*
fail=0
for f in $LEGACY/0*.bin $LEGACY/scrolly.bin; do
  b=$(basename "$f" .bin)
  $S -convert "$f" out_$b.svl >/dev/null
  # SVL -> legacy -> SVL must give the same bytes
  $S -legacy out_$b.svl out_$b.legacy.bin >/dev/null
  $S -convert out_$b.legacy.bin out_$b.rt.svl >/dev/null
  if ! cmp -s out_$b.svl out_$b.rt.svl; then echo "FAIL round trip $b"; fail=1; fi
  $S -render out_$b.svl out_$b.bmp >/dev/null
done
echo "format round trips: done"
SUVLEIR_SCRIPT=ui_smoke.txt ../suvleir4-headless >/dev/null
$S -info out_square.svl | grep -q "ops: 6 (plot 0, line 4, fill 1, move 1)" || { echo "FAIL ui session"; fail=1; }
echo "ui session: done (screenshots in tests/out_*.bmp)"
[ $fail = 0 ] && echo "ALL TESTS PASSED"
exit $fail
