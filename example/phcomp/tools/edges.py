#!/usr/bin/env python3
"""Checks that rooms meet at their exits: for every exit in gfx/games.txt,
the edge of a room and the edge of the room it leads to must have the
same obstacles, so you never come into a room inside a wall.

  left/right exits: columns 30-31 of the room on the left against
                    columns 0-1 of the room on the right, row by row
  up/down exits:    rows 20-21 of the upper room against rows 0-1 of the
                    lower one, column by column

A cell is free, solid (BRIGHT), deadly (FLASH) or a platform (both).
Reports every row or column where the two sides differ. Run by build.sh
(warnings only); `tools/edges.py -q` prints just the count.
Python 3 standard library only."""
import os, re, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
G = os.path.join(ROOT, 'gfx')

def kind(attr):
    return '.#!='[(attr >> 6) & 3] if attr is not None else '.'

# rooms.txt -> name: (header, objects)
rooms = {}
order = []
for line in open(os.path.join(G, 'rooms.txt')):
    w = line.split('#')[0].split()
    if not w: continue
    if w[0] == 'room':
        name = ' '.join(w[1:]); hdr = [0, 0]; objs = []
    elif w[0] == 'header':
        hdr = [int(v) for v in w[1:3]]
    elif w[0] in ('char', 'rect', 'pattern', 'zigzag'):
        objs.append((w[0], [int(v) for v in w[1:]]))
    elif w[0] == 'end':
        rooms[name] = (hdr, objs); order.append(name)
backdrops = [n for n in order if n.startswith('bg ')]

def grid(name):
    g = [[None] * 32 for _ in range(22)]
    hdr, objs = rooms[name]
    layers = ([rooms[backdrops[hdr[0] - 1]][1]] if hdr[0] else []) + [objs]
    for objs in layers:
        for k, v in objs:
            if k == 'char':
                x, y, t, a = v; x2, y2 = x, y
            else:
                x, y, t, a, x2, y2 = v[:6]
            for yy in range(y, min(y2, 21) + 1):
                for xx in range(x, x2 + 1):
                    g[yy][xx] = a
    return [[kind(a) for a in row] for row in g]

def main():
    # games.txt -> (game, room, exits)
    problems = []
    game = None
    links = []
    for line in open(os.path.join(G, 'games.txt'), encoding='utf-8'):
        w = re.sub(r'"[^"]*"', '', line).split('#')[0].split()
        if not w: continue
        if w[0] == 'game': game = w[1]
        elif w[0] == 'room':
            ex = (w[2:6] + ['-'] * 4)[:4]
            if ex[1] != '-': links.append((game, w[1], ex[1], 'R'))
            if ex[3] != '-': links.append((game, w[1], ex[3], 'D'))
            if ex[0] != '-': links.append((game, ex[0], w[1], 'R'))
            if ex[2] != '-': links.append((game, ex[2], w[1], 'D'))
    seen = set()
    for game, a, b, d in links:
        if (a, b, d) in seen: continue
        seen.add((a, b, d))
        ga, gb = grid(a), grid(b)
        if d == 'R':
            for r in range(22):
                ea, eb = ga[r][30] + ga[r][31], gb[r][0] + gb[r][1]
                if ea != eb:
                    problems.append('%s: %s | %s row %2d: %s | %s' % (game, a, b, r, ea, eb))
        else:
            for c in range(32):
                ea, eb = ga[20][c] + ga[21][c], gb[0][c] + gb[1][c]
                if ea != eb:
                    problems.append('%s: %s over %s column %2d: %s over %s' % (game, a, b, c, ea, eb))
    if '-q' not in sys.argv:
        for p in problems: print(p)
    print('edges.py: %d places where rooms don\'t meet' % len(problems))


if __name__ == '__main__':
    main()
