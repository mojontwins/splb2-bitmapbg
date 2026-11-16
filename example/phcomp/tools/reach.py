#!/usr/bin/env python3
"""Can the player get through every room? For each room of each game in
gfx/games.txt this simulates the player's physics (the same tables and
steps as src/player.h) from every exit it can come in by, trying walks,
high jumps and long jumps (left and right), and lists which exits it can
reach. Also checks that you can stand next to each goal and item. Enemies and
moving platforms are ignored (so a room that needs a
raft shows as stuck). Flying rooms are skipped.

  tools/reach.py           every room
  tools/reach.py ROOM...   only those, with more detail
Python 3 standard library only."""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import edges

def table(name):
    """a table of src/player.h, as signed numbers"""
    src = open(os.path.join(os.path.dirname(HERE), 'src', 'player.h')).read()
    body = re.search(r'unsigned char %s \[\] = \{(.*?)\};' % name, src, re.S).group(1)
    return [int(v) for v in body.replace('\n', ' ').split(',') if v.strip()]
def pairs(t):
    t = t[:t.index(128)]
    return list(zip(t[0::2], t[1::2]))
JUMP_HI = pairs(table('jump_hi'))
JUMP_LO = pairs(table('jump_lo'))
FALL = table('fall_tab')
FALL_AFTER_JUMP = int(re.search(r'#define FALL_AFTER_JUMP\s+(\d+)',
                      open(os.path.join(os.path.dirname(HERE), 'src', 'player.h')).read()).group(1))

class Room:
    def __init__(self, name, exits):
        self.g = edges.grid(name)
        self.ex = exits                 # L R U D: True if there is one
    def cell(self, r, c):
        if r < 0 or r > 21 or c < 0 or c > 31: return '.'
        return self.g[r][c]
    def col_solid(self, c, y):
        return any(self.cell(r, c) == '#' for r in range(y >> 3, ((y + 15) >> 3) + 1))
    def row_or(self, r, x):
        cs = [self.cell(r, c) for c in range(x >> 3, ((x + 15) >> 3) + 1)]
        return any(k in '#=' for k in cs), any(k == '#' for k in cs)
    def deadly(self, x, y):
        return any(self.cell(r, c) == '!' for r in range((y + 2) >> 3, ((y + 15) >> 3) + 1)
                   for c in range((x + 4) >> 3, ((x + 11) >> 3) + 1))
    def on_floor(self, x, y):
        return y & 7 == 0 and self.row_or((y >> 3) + 2, x)[0]

def step_x(room, x, y, d):
    """one pixel across: (new x, exit or None, moved)"""
    if d < 0:
        if x == 0: return x, ('L' if room.ex[0] else None), False
        if x & 7 == 0 and room.col_solid((x >> 3) - 1, y): return x, None, False
        return x - 1, None, True
    if x >= 240: return x, ('R' if room.ex[1] else None), False
    if x & 7 == 0 and room.col_solid((x >> 3) + 2, y): return x, None, False
    return x + 1, None, True

def air(room, x, y, tab, vx):
    """a jump (tab: the arc, vx: its direction 1, -1, 0) or a fall (tab
    None) from (x, y): the ground position, an exit, or 'dead'"""
    def across(x, n):
        for _ in range(n if vx else 0):
            x, e, ok = step_x(room, x, y, vx)
            if e: return x, e
            if not ok: break
        return x, None
    fi = 0
    if tab:
        fi = FALL_AFTER_JUMP
        for dy, dx in tab:
            if dy < 0:
                bump = False
                for _ in range(-dy):
                    if y == 0:
                        if room.ex[2]: return 'U'
                        bump = True; break
                    if y & 7 == 0 and room.row_or((y >> 3) - 1, x)[1]: bump = True; break
                    y -= 1
                if bump:
                    vx, fi = 0, 0
                    break
            else:
                for _ in range(dy):
                    if y >= 160: return 'D' if room.ex[3] else 'dead'
                    if room.on_floor(x, y):
                        return 'dead' if room.deadly(x, y) else (x, y)
                    y += 1
            x, e = across(x, dx)
            if e: return e
            if room.deadly(x, y): return 'dead'
    else:
        vx = 0
    for frame in range(200):
        dy = FALL[min(fi, len(FALL) - 1)]; fi += 1
        for _ in range(dy):
            if y >= 160: return 'D' if room.ex[3] else 'dead'
            if room.on_floor(x, y):
                return 'dead' if room.deadly(x, y) else (x, y)
            y += 1
        x, e = across(x, 1)
        if e: return e
        if room.deadly(x, y): return 'dead'
    return 'dead'

def explore(room, starts):
    seen, found, todo = set(), set(), []
    for x, y in starts:
        r = air(room, x, y, None, 0)
        if isinstance(r, tuple): todo.append(r)
        elif r != 'dead': found.add(r)
    while todo:
        p = todo.pop()
        if p in seen: continue
        seen.add(p)
        x, y = p
        if room.deadly(x, y): continue
        moves = []
        for d in (-1, 1):
            nx, e = x, None
            for _ in range(2):
                nx, e, ok = step_x(room, nx, y, d)
                if e or not ok: break
            if e: found.add(e)
            elif nx != x:
                moves.append(air(room, nx, y, None, 0) if not room.on_floor(nx, y) else (nx, y))
        for tab, vx in ((JUMP_HI, -1), (JUMP_HI, 1), (JUMP_LO, -1), (JUMP_LO, 1)):
            moves.append(air(room, x, y, tab, vx))
        for r in moves:
            if isinstance(r, tuple): todo.append(r)
            elif r != 'dead': found.add(r)
    return found, seen

# the games
games = []
for line in open(os.path.join(edges.G, 'games.txt'), encoding='utf-8'):
    w = re.sub(r'"[^"]*"', '', line).split('#')[0].split()
    if not w: continue
    if w[0] == 'game': games.append((w[1], []))
    elif w[0] == 'room':
        games[-1][1].append((w[1], (w[2:6] + ['-'] * 4)[:4], 'fly' in w[6:], []))
    elif w[0] in ('goal', 'item'):
        games[-1][1][-1][3].append((w[0], int(w[1]), int(w[2])))
only = sys.argv[1:]
bad = 0
for game, rooms in games:
    for name, ex, fly, things in rooms:
        if fly or (only and name not in only): continue
        room = Room(name, [e != '-' for e in ex])
        out = []
        allseen = set()
        for side, k in (('L', 0), ('R', 1), ('U', 2), ('D', 3)):
            if ex[k] == '-': continue
            # where you come in from that side: every y (x, or every x)
            if side == 'L': starts = [(0, y) for y in range(0, 161, 8)]
            elif side == 'R': starts = [(240, y) for y in range(0, 161, 8)]
            elif side == 'U': starts = [(x, 0) for x in range(0, 241, 8)]
            else: starts = [(x, 160) for x in range(0, 241, 8)]
            starts = [(x, y) for x, y in starts if not any(room.cell(r, c) == '#'
                      for r in range(y >> 3, ((y + 15) >> 3) + 1) for c in range(x >> 3, ((x + 15) >> 3) + 1))]
            if side == 'D':                 # coming up: still rising
                found, seen = set(), set()
                for x, y in starts:
                    for vx in (-1, 1):
                        r = air(room, x, 160, JUMP_HI, vx)
                        if isinstance(r, tuple):
                            f2, s2 = explore(room, [r]); found |= f2; allseen |= s2
                        elif r != 'dead': found.add(r)
            else:
                found, seen = explore(room, starts)
                allseen |= seen
            need = set(s for s, kk in (('L', 0), ('R', 1), ('U', 2), ('D', 3)) if ex[kk] != '-' and s != side)
            miss = need - found
            if miss:
                bad += 1
                out.append('from %s cannot reach %s' % (side, ' '.join(sorted(miss))))
        for kind, tx, ty in things:     # can you stand where you touch it?
            if not any(abs(x - tx) < 12 and abs(y - ty) < 13 for x, y in allseen):
                bad += 1
                out.append('cannot reach the %s at %d,%d' % (kind, tx, ty))
        if out or only:
            print('%s %s: %s' % (game, name, '; '.join(out) or 'ok'))
print('reach.py: %d problems' % bad)
