#!/usr/bin/env python3
"""Design helper (not part of the build): turns the ASCII room maps in
design/*.map into gfx/rooms.txt, in the room editor's compact format.
Once gfx/rooms.txt exists, edit it with the editor; this script is how the
first version of the rooms was typed. Running it again overwrites
gfx/rooms.txt.

A .map file:
  legend C TILE ATTR [W H]   character C of the maps: TILE (a number or a
                             name of design/tilenames.txt) in colour ATTR
                             (a number, or P<paper>I<ink> plus B = BRIGHT,
                             solid; F = FLASH, deadly; BF = a platform). W H:
                             a figure of W x H tiles (TILE is its first, the
                             others follow, row by row): the map must have C
                             in all its cells, in whole figures. W
                             "zigzag": a zigzag of TILE and TILE+1.
                             Legends last for the rest of the file.
  room NAME [BG [TEXT]]      then 22 lines of up to 32 characters: the play
                             area. ' ' and '.' are empty. A line "over"
                             after them: 22 more lines, drawn on top.
Usage: design/mkrooms.py design/a.map design/b.map ...  (in this order)
"""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
names = {}
for l in open(os.path.join(HERE, 'tilenames.txt')):
    i, n = l.split()
    names[n] = int(i)

def attr(s):
    if s.isdigit(): return int(s)
    m = re.match(r'P(\d)I(\d)(B?)(F?)$', s)
    if not m: sys.exit('bad attribute %s' % s)
    return (int(m.group(1)) << 3) | int(m.group(2)) | (64 if m.group(3) else 0) | (128 if m.group(4) else 0)

def tile(s):
    if s.isdigit() or s == '255': return int(s)
    if s.startswith('"') and len(s) == 3: return ord(s[1]) - 32
    return names[s]

rooms = []
legend = {}
for fn in sys.argv[1:]:
    lines = open(fn).read().split('\n')
    i = 0
    while i < len(lines):
        l = lines[i]
        w = l.split('#')[0].split()
        if not w:
            i += 1; continue
        if w[0] == 'legend':
            c = l.split()[1]
            rest = l.split()[2:]
            if len(rest) > 2 and rest[2] == 'zigzag':      # W 0: a zigzag
                legend[c] = (tile(rest[0]), attr(rest[1]), 0, 1)
            else:
                legend[c] = (tile(rest[0]), attr(rest[1]),
                             int(rest[2]) if len(rest) > 2 else 1, int(rest[3]) if len(rest) > 3 else 1)
            i += 1
        elif w[0] == 'room':
            name = w[1] if len(w) > 1 else '?'
            # room NAME may have spaces up to the numbers
            m = re.match(r'room\s+(.+?)((\s+\d+){0,2})\s*$', l)
            name = m.group(1)
            nums = [int(x) for x in m.group(2).split()]
            hdr = (nums + [0, 0])[:2]
            grids = []
            if i + 1 < len(lines) and lines[i + 1].strip() == 'rows':
                # sparse form: "NN|cells" lines (row NN), until "endrows";
                # "over" starts a new layer
                i += 2
                grid = [' ' * 32 for _ in range(22)]
                while lines[i].strip() != 'endrows':
                    if lines[i].strip() == 'over':
                        grids.append(grid); grid = [' ' * 32 for _ in range(22)]
                    elif lines[i].strip():
                        mm = re.match(r'\s*(\d+)\|(.*)$', lines[i])
                        if not mm: sys.exit('%s: bad row line %d' % (fn, i + 1))
                        grid[int(mm.group(1))] = (mm.group(2) + ' ' * 32)[:32]
                    i += 1
                i += 1
                grids.append(grid)
                rooms.append((name, hdr, grids, dict(legend), fn))
                continue
            while True:
                grid = []
                for k in range(22):
                    row = lines[i + 1 + k] if i + 1 + k < len(lines) else ''
                    grid.append((row + ' ' * 32)[:32])
                i += 23
                grids.append(grid)
                if i < len(lines) and lines[i].strip() == 'over':
                    continue                # another layer, drawn on top
                break
            rooms.append((name, hdr, grids, dict(legend), fn))
        else:
            sys.exit('%s: bad line %d: %s' % (fn, i + 1, l))

def encode(grid, leg, where):
    done = [[False] * 32 for _ in range(22)]
    objs = []
    for y in range(22):
        for x in range(32):
            c = grid[y][x]
            if done[y][x] or c in ' .': continue
            if c not in leg: sys.exit('%s: no legend for %r at %d,%d' % (where, c, x, y))
            t, a, fw, fh = leg[c]
            # widest run, then as many rows as match
            w = 0
            while x + w < 32 and grid[y][x + w] == c and not done[y][x + w]: w += 1
            h = 1
            while y + h < 22 and all(grid[y + h][x + k] == c and not done[y + h][x + k] for k in range(w)): h += 1
            if fw == 0:                                     # zigzag
                for yy in range(y, y + h):
                    for xx in range(x, x + w): done[yy][xx] = True
                if (x + y) & 1:
                    print('%s: zigzag %r at %d,%d starts on the other phase' % (where, c, x, y))
                objs.append('zigzag %d %d %d %d %d %d' % (x, y, t, a, x + w - 1, y + h - 1))
                continue
            w -= w % fw; h -= h % fh
            if w == 0 or h == 0:
                sys.exit('%s: %r at %d,%d is not a whole %dx%d figure' % (where, c, x, y, fw, fh))
            for yy in range(y, y + h):
                for xx in range(x, x + w): done[yy][xx] = True
            if fw == 1 and fh == 1:
                if w == 1 and h == 1: objs.append('char %d %d %d %d' % (x, y, t, a))
                else: objs.append('rect %d %d %d %d %d %d' % (x, y, t, a, x + w - 1, y + h - 1))
            else:
                objs.append('pattern %d %d %d %d %d %d %d %d' % (x, y, t, a, x + w - 1, y + h - 1, fw, fh))
    return objs

head = open(os.path.join(HERE, 'rooms_header.txt')).read()
out = [head]
for name, hdr, grids, leg, fn in rooms:
    out.append('room %s' % name)
    out.append('header %d %d' % tuple(hdr))
    for grid in grids:
        out += encode(grid, leg, '%s room %s' % (fn, name))
    out.append('end')
    out.append('')
open(os.path.join(ROOT, 'gfx', 'rooms.txt'), 'w').write('\n'.join(out))
print('mkrooms.py: %d rooms' % len(rooms))
