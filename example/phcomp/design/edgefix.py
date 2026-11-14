#!/usr/bin/env python3
"""One-off (kept as a record): the changes that made neighbouring rooms
meet at their edges (tools/edges.py) and every room passable
(tools/reach.py). Edits design/*.map; then run mkrooms.py."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import maplib
D = os.path.dirname(os.path.abspath(__file__))
maps = dict((f, maplib.MapFile(os.path.join(D, f + '.map'))) for f in ('brujas', 'luna', 'vee', 'infinito'))
where = {}
for f, m in maps.items():
    for n in m.rooms(): where[n] = m

def G(n): return [list(r) for r in where[n].grid(n)]
def S(n, g): where[n].set_grid(n, [''.join(r) for r in g])
def put(n, r, c, s):
    g = G(n)
    for k, ch in enumerate(s): g[r][c + k] = ch
    S(n, g)
def fill(n, r0, r1, c0, c1, ch):
    g = G(n)
    for r in range(r0, r1 + 1):
        for c in range(c0, c1 + 1): g[r][c] = ch
    S(n, g)

# --- rooms changed by hand
fill('bru_07', 0, 19, 30, 31, 'B')          # no way right: up through 07b, 07c
put('bru_07', 2, 11, 'BBBB')                # a ledge at the top to jump out
fill('bru_07b', 20, 21, 5, 10, ' ')         # nothing under the way up...
put('bru_07b', 17, 9, 'BBBBBBBB')           # ...land here instead
fill('bru_07c', 14, 17, 27, 30, 'B')        # the building solid
fill('v1_06', 20, 20, 0, 31, 'R')           # floor two rows tall
fill('v2_04', 20, 21, 0, 31, 'K')           # rocks instead of the big ones
fill('v2_08', 13, 14, 0, 31, 'K')
fill('v2_09', 13, 14, 0, 31, 'K')
fill('v2_08', 20, 21, 0, 19, 'K')
fill('v2_08', 20, 21, 24, 31, 'K')
put('v1_02', 17, 5, 'RRR')                  # a step up to the first platform
put('v2_02', 17, 5, 'rrr')
put('luna_02', 13, 24, '    ')              # steps over the tower
put('luna_02', 13, 18, 'TTTT')
put('luna_02', 16, 22, 'TT')
fill('luna_03', 18, 19, 20, 29, ' ')        # a gate under the castle
put('luna_04', 12, 17, '      ')            # the ledge was in the way
put('luna_04', 10, 14, 'TTTTTT')
fill('luna_09', 17, 19, 30, 31, 'Y')        # a step up to the launch pad
put('luna_08', 3, 26, '     ')              # you got stuck under the hole
fill('luna_12', 14, 19, 28, 31, ' ')        # down to the base's floor
put('bru_08', 17, 4, 'BB')                  # a step up to the block on the left
put('bru_08', 18, 4, 'BB')
put('bru_08', 19, 4, 'BB')

# --- edges: (left or upper room, right or lower room, which way to copy)
ops = """bru_02 bru_03 A>B
bru_03 bru_04 A>B
bru_04 bru_05 A>B
bru_05 bru_06 A>B
bru_07b bru_07 U>D
bru_07b bru_07c B>A
bru_07c bru_08 A>B
bru_08 bru_fly1 A>B
bru_fly2 bru_09 B>A
bru_09 bru_10 A>B
bru_10 bru_11 B>A
bru_11 bru_12 B>A
luna_01 luna_02 A>B
luna_06 luna_07 U>D
luna_07 luna_08 U>D
luna_08 luna_09 A>B
luna_09 luna_10 A>B
luna_10 luna_11 B>A
luna_11 luna_12 B>A
luna_12 luna_13 A>B
luna_13 luna_14 B>A
v1_03 v1_04 A>B
v1_05 v1_06 U>D
v1_07 v1_06 B>A
v1_08 v1_07 B>A
v1_12 v1_11 A>B
v1_13 v1_12 B>A
v1_14 v1_13 B>A
v2_02 v2_03 B>A
v2_03 v2_04 B>A
v2_05 v2_06 U>D
v2_08 v2_07 A>B
v2_10 v2_09 B>A
v2_11 v2_10 B>A
v2_13 v2_12 A>B
i1_07 i1_08 U>D
i1_10 i1_09 B>A
i1_11 i1_10 A>B
i3_07 i3_08 U>D
i3_10 i3_09 B>A
i3_11 i3_10 A>B
v2_07 v2_06 B>A
i1_09 i1_08 B>A
i3_09 i3_08 B>A"""
for line in ops.split('\n'):
    a, b, how = line.split()
    ga, gb = G(a), G(b)
    if how in ('A>B', 'B>A'):
        for r in range(22):
            for k in range(2):
                if how == 'A>B': gb[r][k] = ga[r][30 + k]
                else: ga[r][30 + k] = gb[r][k]
    else:
        for c in range(32):
            for k in range(2):
                gb[k][c] = ga[20 + k][c]
    S(a, ga); S(b, gb)
for m in maps.values(): m.save()
print('edgefix.py: done')
