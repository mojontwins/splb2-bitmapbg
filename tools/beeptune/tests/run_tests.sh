#!/bin/sh
# Beeptune tests: file round trips, the sound renderer, a scripted UI session.
# Run from anywhere; builds what it needs.
set -e
cd "$(dirname "$0")/.."
make -s beeptune headless
cd tests
B=../beeptune
fail=0
ok() { echo "ok   $1"; }
bad() { echo "FAIL $1"; fail=1; }

# text -> binary -> text gives the same text
$B -txt ../demo/demo.txt out_a.txt >/dev/null
$B -bts out_a.txt out_a.bts >/dev/null
$B -txt out_a.bts out_b.txt >/dev/null
cmp -s out_a.txt out_b.txt && ok "txt/bts round trip" || bad "txt/bts round trip"

# the binary is what the player expects: header, order list, offsets
python3 - <<'PY' && ok "binary layout" || bad "binary layout"
b = open('out_a.bts', 'rb').read()
speed, nord, loop, npat = b[0], b[1], b[2], b[3]
assert loop < nord
for i in range(nord): assert b[4 + i] < npat
for p in range(npat):
    o = b[4 + nord + 2 * p] | b[5 + nord + 2 * p] << 8
    rows = b[o]
    assert 1 <= rows and o + 1 + 2 * rows <= len(b)
    for r in range(rows * 2): assert b[o + 1 + r] <= 85
PY

# the WAV lasts what the song says: rows * speed frames (+ the player's
# work between frames + a 0.1 s tail)
$B -wav ../demo/demo.txt out_demo.wav >/dev/null
python3 - <<'PY' && ok "render length" || bad "render length"
import wave
w = wave.open('out_demo.wav'); secs = w.getnframes() / w.getframerate()
expect = 96 * 7 * (538 * 130 + 344) / 3500000 + 0.1
assert abs(secs - expect) < 0.01, (secs, expect)
PY

# a session with the editor
rm -f out_ui.bts
BEEPTUNE_SCRIPT=ui_smoke.txt ../beeptune-headless >/dev/null
$B -txt out_ui.bts out_ui.txt >/dev/null
head -4 out_ui.txt | cmp -s - ui_expected.txt && ok "UI: order list" || bad "UI: order list"
grep -c . out_ui.txt >/dev/null
python3 - <<'PY' && ok "UI: notes" || bad "UI: notes"
t = open('out_ui.txt').read().split('\n')
i = t.index('pattern 0 32')
assert t[i + 1:i + 5] == ['C-4 C-5', 'D-4 ---', 'E-4 D-5', '=== ---'], t[i + 1:i + 5]
j = t.index('pattern 1 32')
assert t[j + 1] == '--- F-4', t[j + 1]
PY
rm -f out_*.txt out_*.bts out_*.wav out_*.bmp
exit $fail
