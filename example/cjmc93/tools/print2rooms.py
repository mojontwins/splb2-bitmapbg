#!/usr/bin/env python3
"""One-off: orig/rooms-print.txt (the rooms as the original's PRINT
statements) -> gfx/rooms.txt in the room editor's format (see conv.py).

Runs each room's PRINTs (tools/screen.py), then covers what they drew with
as few objects as it can: 2x2 figures (the boxes, UDGs O P / Q R),
rectangles of one tile, 2-tile patterns (brick runs S T S T...), single
cells. Colours become the new rule: in the play area (rows 0-19, columns
4-27) what the original treated as solid (any attribute but 7) gets BRIGHT,
the water (attribute 8) BRIGHT + FLASH (= deadly). SOLID cells become
cells of tile 255 (nothing drawn) with BRIGHT. Text (the font's tiles,
0-95) goes to gfx/texts.txt instead, one set per different text, and the
room's header points to it.
Kept for the record; the rooms are edited in gfx/rooms.txt now."""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from screen import Screen

BD = {'castle': 0, 'cave': 1, 'bridge': 2, 'black': 3, 'none': 4}
PIC = {None: 0, 'temple': 1, 'castle': 2, 'corridor': 3}
PLAY = (0, 19, 4, 27)
BRIGHT, FLASH = 64, 128

rooms = []
for ln in open(os.path.join(ROOT, 'orig', 'rooms-print.txt')):
    ln = ln.rstrip('\n')
    if ln.lstrip().startswith('#'): continue
    ln = re.sub(r'\s{2,}#.*$', '', ln).rstrip()
    if not ln.strip(): continue
    m = re.match(r'room (\S+) (\S+)(?: (\S+))?', ln.strip())
    if m: rooms.append([m.group(1), m.group(2), m.group(3), []])
    else: rooms[-1][3].append(ln.strip())

def cells_of(stmts):
    s = Screen(); s.clear(untouched=True)
    for st in stmts: s.run_text(st)
    g = {}
    for r in range(24):
        for c in range(32):
            v = s.cell[r][c]
            if v is None: continue
            ch, at, inv = v
            inplay = PLAY[0] <= r <= PLAY[1] and PLAY[2] <= c <= PLAY[3]
            if (ch, at) == (32, 7) and inplay: continue      # the backdrop shows
            at &= 0x7F
            if inplay and at == 8: at |= BRIGHT | FLASH
            elif inplay and at != 7: at |= BRIGHT
            g[(r, c)] = (ch - 32, at)
    for r, c in s.solid:
        g[(r, c)] = (255, BRIGHT)
    return g

def objects(g):
    out = []; done = set()
    free = lambda r, c: (r, c) in g and (r, c) not in done
    # 2x2 figures: tiles t t+1 / t+2 t+3, one colour
    for (r, c) in sorted(g):
        if not free(r, c): continue
        t, a = g[(r, c)]
        if t >= 255: continue
        quad = [(r, c), (r, c + 1), (r + 1, c), (r + 1, c + 1)]
        if all(free(*q) for q in quad) and [g[q] for q in quad] == [(t + k, a) for k in range(4)]:
            out.append('pattern %d %d %d %d %d %d 2 2' % (c, r, t, a, c + 1, r + 1)); done.update(quad)
    # 2-tile patterns over several rows (t t+1 t t+1..., the same in each row)
    for (r, c) in sorted(g):
        if not free(r, c): continue
        t, a = g[(r, c)]
        if t >= 255: continue
        run = lambda rr: [(rr, c + i) for i in range(n)]
        n = 0
        while free(r, c + n) and g[(r, c + n)] == (t + (n & 1), a): n += 1
        if n < 2: continue
        h = 1
        while all(free(*q) and g[q] == (t + (i & 1), a) for i, q in enumerate(run(r + h))): h += 1
        if h >= 2:
            out.append('pattern %d %d %d %d %d %d 2 1' % (c, r, t, a, c + n - 1, r + h - 1))
            done.update(q for j in range(h) for q in run(r + j))
    # rectangles of one tile (area 3 or more), widest run first
    for (r, c) in sorted(g):
        if not free(r, c): continue
        v = g[(r, c)]; w = 1
        while free(r, c + w) and g[(r, c + w)] == v: w += 1
        h = 1
        while all(free(r + h, c + k) and g[(r + h, c + k)] == v for k in range(w)): h += 1
        if w * h >= 3 and not (h == 1 and w < 3):
            out.append('rect %d %d %d %d %d %d' % (c, r, v[0], v[1], c + w - 1, r + h - 1))
            done.update((r + j, c + i) for j in range(h) for i in range(w))
    # rows: 2-tile patterns (t, t+1, t, t+1...), pairs, single cells
    for (r, c) in sorted(g):
        if not free(r, c): continue
        t, a = g[(r, c)]
        n = 0
        while free(r, c + n) and g[(r, c + n)] == (t + (n & 1), a) and t < 255: n += 1
        if n >= 2:
            out.append('pattern %d %d %d %d %d %d 2 1' % (c, r, t, a, c + n - 1, r))
            done.update((r, c + i) for i in range(n)); continue
        w = 1
        while free(r, c + w) and g[(r, c + w)] == (t, a): w += 1
        if w >= 2:
            out.append('rect %d %d %d %d %d %d' % (c, r, t, a, c + w - 1, r))
            done.update((r, c + i) for i in range(w)); continue
        out.append('char %d %d %d %d' % (c, r, t, a)); done.add((r, c))
    return out

def texts_of(g):
    """takes the font cells (tiles 0-95) out of g -> [(row, col, attr, str)],
    runs of cells next to each other in one row and colour"""
    out = []
    for (r, c) in sorted(g):
        t, a = g[(r, c)]
        if t >= 96: continue
        gap = c - out[-1][1] - len(out[-1][3]) if out else 99
        if out and out[-1][0] == r and 0 <= gap <= 2 and out[-1][2] == a:
            # one text, the gap as spaces (black-paper spaces aren't drawn)
            out[-1] = (r, out[-1][1], a, out[-1][3] + ' ' * gap + chr(t + 32))
        else:
            out.append((r, c, a, chr(t + 32)))
    for r, c, a, txt in out:
        for i in range(len(txt)): g.pop((r, c + i), None)
    return out

txt = open(os.path.join(HERE, 'rooms_header.txt')).read()
sets = []                                  # [texts, [room names]]
for name, back, pic, stmts in rooms:
    g = cells_of(stmts)
    tx = texts_of(g)
    tid = 0
    if tx:
        for i, (t, who) in enumerate(sets):
            if t == tx: tid = i + 1; who.append(name)
        if not tid:
            sets.append([tx, [name]]); tid = len(sets)
    txt += 'room %s\nheader %d %d %d\n' % (name, BD[back], PIC[pic], tid)
    txt += ''.join(o + '\n' for o in objects(g))
    txt += 'end\n\n'
open(os.path.join(ROOT, 'gfx', 'rooms.txt'), 'w').write(txt)
tt = open(os.path.join(HERE, 'texts_header.txt')).read()
for i, (t, who) in enumerate(sets):
    tt += 'text %d        # %s\n' % (i + 1, ', '.join(who))
    tt += ''.join('%2d %2d %3d "%s"\n' % e for e in t) + '\n'
open(os.path.join(ROOT, 'gfx', 'texts.txt'), 'w').write(tt)
print('gfx/rooms.txt: %d rooms; gfx/texts.txt: %d sets' % (len(rooms), len(sets)))
