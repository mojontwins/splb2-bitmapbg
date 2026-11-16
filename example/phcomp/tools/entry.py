#!/usr/bin/env python3
"""Enemies that wait where you come in: for each room of gfx/games.txt
and each exit you can enter it by, finds where the player stands once in
(after falling, if it comes in in the air: that is also where it comes
back after dying) and lists the enemies whose path passes within MARGIN
pixels of that spot, at a height that can hit it.

  tools/entry.py [MARGIN]      (default 32)

What each enemy type sweeps (src/enemies.h): linear, platform: the box of
its two points; chaser, random: its row X1..X2 (a chaser also starts at
X2); drop: its column from Y1 to Y2; ghost: its row X1..X2 (it appears
anywhere there); bouncer: its box; hopper: X1..X2, from Y1 - hop to Y1;
faller: X1..X2 from Y1 down to Y2. Platforms, goals, items: harmless;
toggles: their cells. Python 3 standard library only."""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.argv, MARGIN = sys.argv[:1] + ['-'], int(sys.argv[1]) if len(sys.argv) > 1 else 32
import reach                                    # (prints nothing with '-')
import edges

def sweep(t, v):
    """the box (x0, y0, x1, y1) of top-left corners an enemy can be at"""
    x1, y1, x2, y2 = v[:4]
    if t in ('linear', 'bouncer'):
        return min(x1, x2), min(y1, y2), max(x1, x2), max(y1, y2)
    if t in ('chaser', 'random', 'ghost'):
        return x1, y1, x2, y1
    if t == 'drop':
        return x1, y1, x1, y2
    if t == 'hopper':
        return x1, y1 - (24 if y2 >= 16 else 12), x2, y1
    if t == 'faller':
        return x1, y1, x2, y2
    if t == 'toggle':
        return x1 * 8 - 8, y1 * 8 - 8, x2 * 8, y2 * 8
    return None

games = []
for line in open(os.path.join(edges.G, 'games.txt'), encoding='utf-8'):
    w = re.sub(r'"[^"]*"', '', line).split('#')[0].split()
    if not w: continue
    if w[0] == 'game': games.append((w[1], []))
    elif w[0] == 'room':
        games[-1][1].append([w[1], (w[2:6] + ['-'] * 4)[:4], 'fly' in w[6:], []])
    elif w[0] in ('linear', 'chaser', 'random', 'drop', 'ghost', 'bouncer', 'hopper', 'faller', 'toggle'):
        games[-1][1][-1][3].append((w[0], [int(x) for x in w[1:6]], line.strip()))

def spots(room, side, fly):
    """where the player ends up standing just after coming in by side"""
    out = set()
    if side == 'L': starts = [(0, y) for y in range(0, 161, 8)]
    elif side == 'R': starts = [(240, y) for y in range(0, 161, 8)]
    elif side == 'U': starts = [(x, 0) for x in range(0, 241, 8)]
    else: starts = [(x, 160) for x in range(0, 241, 8)]
    for x, y in starts:
        if any(room.cell(r, c) == '#' for r in range(y >> 3, ((y + 15) >> 3) + 1)
               for c in range(x >> 3, ((x + 15) >> 3) + 1)):
            continue
        if fly:
            out.add((x, y)); continue
        r = reach.air(room, x, y, reach.JUMP_HI if side == 'D' else None, 0)
        if isinstance(r, tuple): out.add(r)
    return out

n = 0
for game, rooms in games:
    for name, ex, fly, ens in rooms:
        room = reach.Room(name, [e != '-' for e in ex])
        for side, k in (('L', 0), ('R', 1), ('U', 2), ('D', 3)):
            if ex[k] == '-': continue
            sp = spots(room, side, fly)
            for t, v, text in ens:
                b = sweep(t, v)
                if not b: continue
                hit = [(x, y) for x, y in sp
                       if b[0] - MARGIN < x < b[2] + MARGIN and b[1] - 13 < y < b[3] + 13]
                if t == 'chaser' and not hit:
                    pass
                if hit:
                    n += 1
                    xs = sorted(set(x for x, y in hit)); ys = sorted(set(y for x, y in hit))
                    print('%s %s from %s (x %s, y %s): %s' % (game, name, side,
                          xs[0] if len(xs) == 1 else '%d-%d' % (xs[0], xs[-1]),
                          ys[0] if len(ys) == 1 else '%d-%d' % (ys[0], ys[-1]), text))
print('entry.py: %d enemies near where you come in (margin %d px)' % (n, MARGIN))
