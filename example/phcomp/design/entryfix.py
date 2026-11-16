#!/usr/bin/env python3
"""One-off (kept as a record): keeps enemies away from where you come into
a room (tools/entry.py, 32 px). Enemies that pass by an entry on the
left or right edge get their range trimmed to start 32 px in; the pairs
of Infinito's blue phase and the rooms entered from above are set by
hand. Edits gfx/games.txt."""
import os, re, sys, subprocess
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
P = os.path.join(ROOT, 'gfx', 'games.txt')
LO, HI = 32, 208

hand = {
	# the big robot of the blue phase: two androids side by side
	('i1_04', 'linear 16 112 200 112 1 android 4'): 'linear 40 112 184 112 1 android 4',
	('i1_04', 'linear 32 112 216 112 1 android 4'): 'linear 56 112 200 112 1 android 4',
	('i1_05', 'linear 16 112 200 112 2 android 4'): 'linear 40 112 184 112 2 android 4',
	('i1_05', 'linear 32 112 216 112 2 android 4'): 'linear 56 112 200 112 2 android 4',
	('i1_06', 'linear 200 112 16 112 2 android 4'): 'linear 184 112 40 112 2 android 4',
	('i1_06', 'linear 216 112 32 112 2 android 4'): 'linear 200 112 56 112 2 android 4',
	('i3_04', 'linear 16 112 200 112 1 android 4'): 'linear 40 112 184 112 1 android 4',
	('i3_04', 'linear 32 112 216 112 1 android 4'): 'linear 56 112 200 112 1 android 4',
	('i3_05', 'linear 16 112 200 112 2 android 4'): 'linear 40 112 184 112 2 android 4',
	('i3_05', 'linear 32 112 216 112 2 android 4'): 'linear 56 112 200 112 2 android 4',
	('i3_06', 'linear 200 112 16 112 2 android 4'): 'linear 184 112 40 112 2 android 4',
	('i3_06', 'linear 216 112 32 112 2 android 4'): 'linear 200 112 56 112 2 android 4',
	# the train of Infinito: four androids together
	('i1_09', 'linear 16 112 136 112 1 android 6'): 'linear 40 112 136 112 1 android 6',
	('i1_09', 'linear 32 112 152 112 1 android 5'): 'linear 56 112 152 112 1 android 5',
	('i1_09', 'linear 48 112 168 112 1 android 4'): 'linear 72 112 168 112 1 android 4',
	('i1_09', 'linear 64 112 184 112 1 android 2'): 'linear 88 112 184 112 1 android 2',
	('i3_09', 'linear 16 112 136 112 1 android 6'): 'linear 40 112 136 112 1 android 6',
	('i3_09', 'linear 32 112 152 112 1 android 5'): 'linear 56 112 152 112 1 android 5',
	('i3_09', 'linear 48 112 168 112 1 android 4'): 'linear 72 112 168 112 1 android 4',
	('i3_09', 'linear 64 112 184 112 1 android 2'): 'linear 88 112 184 112 1 android 2',
	# coming in from above
	('luna_07', 'drop 136 8 40 144 0 drop 5'): 'drop 88 8 40 144 0 drop 5',
	('luna_07', 'bouncer 24 16 220 120 1 bat 6'): 'bouncer 24 64 112 120 1 bat 6',
	('i1_08', 'linear 152 112 232 112 1 android 3'): 'linear 136 112 176 112 1 android 3',
	('i3_08', 'linear 152 112 232 112 1 android 3'): 'linear 136 112 176 112 1 android 3',
	('v2_06', 'random 140 64 220 0 1 cop 3'): 'random 40 144 200 0 1 cop 3',
	('bru_07', 'hopper 104 144 232 24 2 witch 0'): 'hopper 160 144 232 24 2 witch 0',
}

def trim(t, v, sides):
    x1, y1, x2, y2, sp = v
    lo = LO if 'L' in sides else 0
    hi = HI if 'R' in sides else 240
    sp = max(sp, 1)
    if t in ('linear', 'bouncer', 'hopper'):
        a, b = min(x1, x2), max(x1, x2)
        a, b = max(a, lo), min(b, hi)
        b -= (b - a) % sp
        if b < a: b = a
        if t == 'linear' and x1 > x2: x1, x2 = b, a
        else: x1, x2 = a, b
    elif t in ('chaser', 'random', 'ghost', 'faller'):
        x1, x2 = max(x1, lo), min(x2, hi)
    return [x1, y1, x2, y2, v[4]]

# which enemy lines are near which entries (left / right edges)
out = subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'entry.py'), '32'],
                     capture_output=True, text=True).stdout
near = {}
for l in out.split('\n'):
    m = re.match(r'\S+ (\S+) from ([LRUD]) \([^)]*\): (.*)$', l)
    if m: near.setdefault((m.group(1), m.group(3).strip()), set()).add(m.group(2))
lines = open(P, encoding='utf-8').read().split('\n')
room = None
for i, l in enumerate(lines):
    w = l.split()
    if not w: continue
    if w[0] == 'room': room = w[1]; continue
    key = (room, l.strip())
    if key in hand:
        lines[i] = l[:len(l) - len(l.lstrip())] + hand[key]
    elif key in near and near[key] & {'L', 'R'}:
        v = trim(w[0], [int(x) for x in w[1:6]], near[key])
        lines[i] = l[:len(l) - len(l.lstrip())] + ' '.join([w[0]] + [str(x) for x in v] + w[6:])
open(P, 'w', encoding='utf-8').write('\n'.join(lines))
print('entryfix.py: done')

# The big robot (two androids together) can't be jumped over: the blue
# rooms get a platform 4 rows up to wait on while it goes by.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import maplib
m = maplib.MapFile(os.path.join(ROOT, 'design', 'infinito.map'))
def row(name, r, text):
    g = list(m.grid(name)); g[r] = (text + ' ' * 32)[:32]; m.set_grid(name, g)
for r in ('i1_04', 'i1_05', 'i1_06'):
    row(r, 12, '              NNNN')
row('i3_04', 11, '')
row('i3_04', 12, '      NNNN            NNNN')
m.save()
print('entryfix.py: refuges in the blue rooms')
