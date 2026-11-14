#!/usr/bin/env python3
"""Phantomas Compilation asset converter: gfx/ -> src/assets.h
(run by ../build.sh). Never edit src/assets.h: this script rewrites it.

  gfx/tiles.png     256x64: the common tileset, 32 tiles a row (tile =
                    row * 32 + column). White (any light colour) = ink.
                    Tiles 0-63 are the font (characters 32-95). The room
                    editor uses this file as is.
  gfx/sprites.png   256 wide, 16x16 frames, 16 a row. White = ink, black =
                    see-through, red = solid paper (mask). A frame with no
                    red gets an automatic mask (inside + a 1 px halo).
  gfx/sprites.txt   names for the figures of sprites.png (and how many
                    frames each has), used by games.txt.
  gfx/rooms.txt     every room (and backdrop) of every game, from the room
                    editor, compact encoding. Format at the top of the file.
  gfx/texts.txt     texts printed in rooms, in sets a room's header names.
  gfx/games.txt     the six games: their rooms, exits, enemies, start and
                    ending. Format at the top of the file.

Python 3 standard library only.
"""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import png

G = os.path.join(ROOT, 'gfx')

def die(msg):
    sys.exit('conv.py: ' + msg)

def lit(px):
    return px[3] >= 128 and sum(px[:3]) >= 384

# =================================================================== tiles
w, h, img = png.read(os.path.join(G, 'tiles.png'))
if (w, h) != (256, 64):
    die('tiles.png must be 256x64, it is %dx%d' % (w, h))
cells = []
for t in range(256):
    cells.append([sum(128 >> x for x in range(8) if lit(img[(t // 32) * 8 + y][(t % 32) * 8 + x]))
                  for y in range(8)])
NTILES = max(64, max(t for t in range(256) if any(cells[t])) + 1)
tiles = sum(cells[:NTILES], [])

# ================================================================= sprites
def classify(px):
    r, g, b, a = px
    if a < 128: return 0                                  # transparent
    if r > 160 and g < 100 and b < 100: return 2          # red: mask
    return 1 if (r + g + b) >= 384 else 0                 # light: ink

def auto_mask(ink):
    """1 = see-through: outside the figure (flood from the edges) minus a
    1 px halo around the ink."""
    out = [[0] * 16 for _ in range(16)]
    st = [(x, y) for x in range(16) for y in (0, 15)] + [(x, y) for y in range(16) for x in (0, 15)]
    while st:
        x, y = st.pop()
        if 0 <= x < 16 and 0 <= y < 16 and not out[y][x] and not ink[y][x]:
            out[y][x] = 1; st += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    m = [[0] * 16 for _ in range(16)]
    for y in range(16):
        for x in range(16):
            keep = out[y][x]
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    if 0 <= x + dx < 16 and 0 <= y + dy < 16 and ink[y + dy][x + dx]: keep = 0
            m[y][x] = keep
    return m

w, h, img = png.read(os.path.join(G, 'sprites.png'))
if w != 256 or h % 16:
    die('sprites.png must be 256 wide, rows of 16x16 frames')
pics = []
for n in range((h // 16) * 16):
    ox, oy = (n % 16) * 16, (n // 16) * 16
    cls = [[classify(img[oy + y][ox + x]) for x in range(16)] for y in range(16)]
    ink = [[c == 1 for c in row] for row in cls]
    m = [[c == 0 for c in row] for row in cls] if any(c == 2 for row in cls for c in row) else auto_mask(ink)
    pics.append((ink, m))
used = [n for n in range(len(pics)) if any(any(r) for r in pics[n][0])]
NFRAMES = max(used) + 1

# splib2 masked sprite data, as Lhikoj and CJMC93: per column, the 16
# lines then 8 blank lines, (graphic, mask) pairs: 48 bytes a column, 96 a
# frame. Two columns; the third is splib2's null graphic (spr_create). 8
# blank lines before the first frame (read when it is shifted down).
BL = [0, 255]
spr = BL * 8
for n in range(NFRAMES):
    ink, m = pics[n]
    for half in (0, 8):
        for y in range(16):
            spr += [sum(128 >> x for x in range(8) if ink[y][half + x]),
                    sum(128 >> x for x in range(8) if m[y][half + x])]
        spr += BL * 8

FIG = {}                                    # name -> (first frame, frames)
for ln, line in enumerate(open(os.path.join(G, 'sprites.txt')), 1):
    wd = line.split('#')[0].split()
    if not wd: continue
    if len(wd) != 3 or not wd[0].isdigit() or wd[2] not in ('1', '2', '4'):
        die('sprites.txt line %d: FIRST_FRAME NAME FRAMES (1, 2 or 4)' % ln)
    f, n = int(wd[0]), int(wd[2])
    if f + n > NFRAMES:
        die('sprites.txt line %d: frames %d-%d, sprites.png has %d' % (ln, f, f + n - 1, NFRAMES))
    FIG[wd[1]] = (f, n)

# =================================================================== rooms
# gfx/rooms.txt: binary per room, what room_draw () reads:
#   header   BG TEXT          BG: 0 none, else 1 + rooms.txt index of the
#                             backdrop room it names (see below)
#   char     X Y 0 CODE ATTR
#   rect     X Y 1 CODE ATTR X2 Y2
#   figure   X Y 0x80|(W-1)<<3|(H-1) CODE ATTR        a pattern with one copy
#   pattern  X Y 0x40|(W-1)<<3|(H-1) CODE ATTR X2 Y2  repeated, W and H 1-8
#   zigzag   X Y 2 CODE ATTR X2 Y2      CODE and CODE+1 taking turns, each
#                                       row starting one cell on
#   end      0xFF
BG_PARAMS = 1                               # after BG: the texts set

def encode_obj(kind, v, where):
    if kind == 'char':
        x, y, code, attr = v
        return [x, y, 0, code, attr]
    if kind == 'rect':
        x, y, code, attr, x2, y2 = v
        return [x, y, 1, code, attr, x2, y2]
    if kind == 'zigzag':
        x, y, code, attr, x2, y2 = v
        return [x, y, 2, code, attr, x2, y2]
    x, y, code, attr, x2, y2, pw, ph = v
    if not (1 <= pw <= 8 and 1 <= ph <= 8):
        die(where + 'patterns can be at most 8x8 (compact encoding)')
    t = ((pw - 1) << 3) | (ph - 1)
    if x2 - x + 1 == pw and y2 - y + 1 == ph:
        return [x, y, 0x80 | t, code, attr]
    return [x, y, 0x40 | t, code, attr, x2, y2]

NPAR = {'char': 4, 'rect': 6, 'pattern': 8, 'zigzag': 6}
rooms = []                                  # [name, header, objects bytes]
seen = {}
hdr = objs = None
for ln, line in enumerate(open(os.path.join(G, 'rooms.txt')), 1):
    wd = line.split('#')[0].split()
    if not wd: continue
    where = 'rooms.txt line %d: ' % ln
    try:
        k = wd[0]
        if k == 'room':
            hdr, objs = [0, 0], []
            name = ' '.join(wd[1:]) or 'room %d' % len(rooms)
        elif k == 'header':
            hdr = ([int(v) & 255 for v in wd[1:]] + [0, 0])[:2]
        elif k in NPAR:
            v = [int(x) for x in wd[1:]]
            if len(v) != NPAR[k]:
                die(where + '%s needs %d numbers' % (k, NPAR[k]))
            if any(n < 0 or n > 255 for n in v):
                die(where + 'numbers must be 0-255')
            if v[0] > 31 or v[1] > 23 or (k != 'char' and (v[4] > 31 or v[5] > 23)):
                die(where + 'outside the screen')
            if k == 'zigzag' and v[2] + 1 >= NTILES:
                die(where + 'zigzag: tiles %d and %d, there are %d' % (v[2], v[2] + 1, NTILES))
            if v[2] != 255 and v[2] >= NTILES:
                die(where + 'tile %d: there are %d tiles (255: draw nothing)' % (v[2], NTILES))
            objs += encode_obj(k, v, where)
        elif k == 'end':
            rooms.append([name, hdr, objs])
            hdr = objs = None
        elif k in ('encoding', 'solid_bit', 'bg_params'):
            seen[k] = seen.get(k, []) + [wd[1:]]
        elif k == 'object':
            die(where + 'custom objects: room_draw () knows none')
    except (TypeError, ValueError, IndexError):
        die(where + 'bad line')
if seen.get('encoding') != [['compact']]:
    die("rooms.txt: set 'encoding compact' (room_draw () reads the compact encoding)")
if seen.get('solid_bit', [['6']]) != [['6']]:
    die('rooms.txt: room_draw () takes BRIGHT (bit 6) as solid: set solid_bit 6')
ROOMIDX = dict((r[0], i) for i, r in enumerate(rooms))
if len(ROOMIDX) != len(rooms):
    die('rooms.txt: two rooms with the same name')
BACKDROPS = [r[0] for r in rooms if r[0].startswith('bg ')]
for name, hd, objs in rooms:
    if hd[0] > len(BACKDROPS):
        die('rooms.txt: room %s: backdrop %d, there are %d (%s)' % (name, hd[0], len(BACKDROPS),
            ', '.join('%d %s' % (i + 1, b) for i, b in enumerate(BACKDROPS))))
rooms_bin, rooms_off = [], []
for name, hd, objs in rooms:
    rooms_off.append(len(rooms_bin))
    bg = ROOMIDX[BACKDROPS[hd[0] - 1]] + 1 if hd[0] else 0
    rooms_bin += [bg, hd[1]] + objs + [255]

# =================================================================== texts
texts = {}
cur = None
for ln, line in enumerate(open(os.path.join(G, 'texts.txt'), encoding='utf-8'), 1):
    where = 'texts.txt line %d: ' % ln
    m = re.match(r'\s*(\d+)\s+(\d+)\s+(\d+)\s+"(.*)"', line)
    if m:
        if cur is None:
            die(where + 'a text before the first "text N"')
        r, c, a, t = int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4).upper()
        if r > 23 or c + len(t) > 32 or a > 255 or not t:
            die(where + 'the text must fit in rows 0-23, columns 0-31')
        if any(not 32 <= ord(ch) <= 95 for ch in t):
            die(where + 'characters 32-95 only (no lowercase in the font)')
        texts[cur] += [r, c, a] + [ord(ch) for ch in t] + [0]
        continue
    wd = line.split('#')[0].split()
    if not wd: continue
    if wd[0] == 'text' and len(wd) == 2 and wd[1].isdigit() and 1 <= int(wd[1]) <= 255:
        cur = int(wd[1])
        if cur in texts:
            die(where + 'text %d twice' % cur)
        texts[cur] = []
    else:
        die(where + 'bad line (text N, or ROW COL ATTR "text")')
NTEXTS = max(texts) if texts else 0
for n in range(1, NTEXTS + 1):
    if n not in texts:
        die('texts.txt: text %d is missing (numbers from 1, without gaps)' % n)
for name, hd, objs in rooms:
    if hd[1] > NTEXTS:
        die('rooms.txt: room %s: text %d, but texts.txt has %d' % (name, hd[1], NTEXTS))
texts_bin, texts_off = [], []
for n in range(1, NTEXTS + 1):
    texts_off.append(len(texts_bin))
    texts_bin += texts[n] + [0xFF]

# =================================================================== games
# gfx/games.txt. Binary, per game: a header (game_tab, GAME_* offsets in
# phcomp.c) and its rooms: rooms.txt index, exits L R U D (room of this
# game or 255), flags (1 fly), enemy count, then 8 bytes an enemy:
#   TYPE X1 Y1 X2 Y2 SPEED FRAME MODE
# MODE: bits 0-6 the ink (+64 BRIGHT), bit 7... see ENEMY_* below. For
# "toggle", FRAME is a tile and MODE an attribute.
ETYPES = ['none', 'linear', 'chaser', 'random', 'drop', 'ghost', 'bouncer', 'hopper',
          'platform', 'goal', 'faller', 'toggle', 'item']
MAXEN = 4

def sstr(t, where, maxlen):
    t = t.upper()
    if len(t) > maxlen:
        die(where + 'text longer than %d characters' % maxlen)
    if any(not 32 <= ord(ch) <= 95 for ch in t):
        die(where + 'characters 32-95 only')
    return [ord(ch) for ch in t] + [0]

games = []
g = None
for ln, line in enumerate(open(os.path.join(G, 'games.txt'), encoding='utf-8'), 1):
    where = 'games.txt line %d: ' % ln
    q = re.findall(r'"([^"]*)"', line)
    wd = re.sub(r'"[^"]*"', '', line).split('#')[0].split()
    if not wd: continue
    k = wd[0]
    try:
        if k == 'game':
            g = {'name': wd[1], 'rooms': [], 'title': None, 'menu': None, 'ending': None,
                 'ink': 7, 'start': None, 'lives': 5}
            games.append(g)
        elif g is None:
            die(where + 'everything goes inside a "game"')
        elif k in ('title', 'menu', 'ending'):
            g[k] = q[0]
        elif k == 'ink':
            g['ink'] = int(wd[1])
        elif k == 'lives':
            g['lives'] = int(wd[1])
        elif k == 'start':
            g['start'] = (wd[1], int(wd[2]), int(wd[3]))
        elif k == 'room':
            if wd[1] not in ROOMIDX:
                die(where + 'room %s is not in rooms.txt' % wd[1])
            ex = (wd[2:6] + ['-'] * 4)[:4]
            fl = 0
            for f in wd[6:]:
                if f == 'fly': fl |= 1
                else: die(where + 'unknown room flag %s (fly)' % f)
            g['rooms'].append({'name': wd[1], 'exits': ex, 'en': [], 'ln': ln, 'flags': fl})
        elif k in ETYPES:
            if not g['rooms']:
                die(where + 'an enemy before the first room')
            v = wd[1:]
            if k == 'toggle':
                x1, y1, x2, y2, sp, tile, attr = [int(a) for a in v]
                en = [ETYPES.index(k), x1, y1, x2, y2, sp, tile, attr]
            else:
                if len(v) != 7:
                    die(where + '%s X1 Y1 X2 Y2 SPEED FIGURE INK' % k)
                x1, y1, x2, y2, sp = [int(a) for a in v[:5]]
                if v[5] not in FIG:
                    die(where + 'figure %s is not in sprites.txt' % v[5])
                f, nf = FIG[v[5]]
                ink = int(v[6])
                mode = (ink & 0x47) | {4: 0, 2: 0x10, 1: 0x20}[nf]
                en = [ETYPES.index(k), x1, y1, x2, y2, sp, f, mode]
            if any(not 0 <= a <= 255 for a in en):
                die(where + 'numbers must be 0-255')
            g['rooms'][-1]['en'].append(en)
            if len(g['rooms'][-1]['en']) > MAXEN:
                die(where + 'at most %d enemies a room' % MAXEN)
        elif k == 'end':
            g = None
        else:
            die(where + 'unknown line')
    except (ValueError, IndexError):
        die(where + 'bad line')

games_bin = []
game_hdr = []
for g in games:
    names = [r['name'] for r in g['rooms']]
    for r in g['rooms']:
        for e in r['exits']:
            if e != '-' and e not in names:
                die('games.txt line %d: exit to %s, not a room of %s' % (r['ln'], e, g['name']))
    if not g['start'] or g['start'][0] not in names:
        die('games.txt: game %s needs "start ROOM X Y" with one of its rooms' % g['name'])
    if not g['title'] or not g['menu'] or not g['ending']:
        die('games.txt: game %s needs title, menu and ending' % g['name'])
    off = len(games_bin)
    roff = []
    body = []
    for r in g['rooms']:
        roff.append(len(body))
        body += [ROOMIDX[r['name']]] + [255 if e == '-' else names.index(e) for e in r['exits']]
        body += [r['flags'], len(r['en'])] + sum(r['en'], [])
    g['bin_rooms'] = body
    g['bin_roff'] = roff

# ================================================================== output
def asm_bytes(data, per=16):
    return '\n'.join('\t\tdefb ' + ','.join(str(b) for b in data[i:i + per]) for i in range(0, len(data), per))
def asm_words(data, per=12):
    return '\n'.join('\t\tdefw ' + ','.join(str(b) for b in data[i:i + per]) for i in range(0, len(data), per))
def cname(n):
    return re.sub(r'[^A-Za-z0-9]', '_', n).upper()

o = []
o.append('/* Made by tools/conv.py from gfx/. Do not edit: run build.sh. */\n')
o.append('#define NTILES %d' % NTILES)
o.append('#define NFRAMES %d' % NFRAMES)
o.append('#define NROOMS_ALL %d' % len(rooms))
o.append('#define NGAMES %d' % len(games))
o.append('#define MAXEN %d' % MAXEN)
for i, t in enumerate(ETYPES):
    o.append('#define E_%s %d' % (t.upper(), i))
o.append('')
o.append('extern unsigned char tiles [];')
o.append('extern unsigned char sprite_frames [];')
o.append('extern unsigned char rooms_data [];')
o.append('extern unsigned int rooms_off [];')
o.append('extern unsigned char texts_data [];')
o.append('extern unsigned int texts_off [];')
o.append('extern unsigned char game_tab [];')
o.append('')
o.append('#asm')
o.append('\t._tiles')
o.append(asm_bytes(tiles))
o.append('\t._sprite_frames')
o.append(asm_bytes(spr))
o.append('\t._rooms_off')
o.append(asm_words(rooms_off))
o.append('\t._rooms_data')
o.append(asm_bytes(rooms_bin))
o.append('\t._texts_off')
o.append(asm_words(texts_off) if texts_off else '\t\tdefw 0')
o.append('\t._texts_data')
o.append(asm_bytes(texts_bin) if texts_bin else '\t\tdefb 255')
# per game: its strings, room offsets, rooms
for gi, g in enumerate(games):
    n = 'g%d' % gi
    o.append('\t.%s_menu' % n)
    o.append(asm_bytes(sstr(g['menu'], 'game %s menu: ' % g['name'], 22)))
    o.append('\t.%s_title' % n)
    o.append(asm_bytes(sstr(g['title'], 'game %s title: ' % g['name'], 32)))
    o.append('\t.%s_ending' % n)
    o.append(asm_bytes(sstr(g['ending'], 'game %s ending: ' % g['name'], 600)))
    o.append('\t.%s_roff' % n)
    o.append(asm_words(['%s_rooms+%d' % (n, v) for v in g['bin_roff']], 6))
    o.append('\t.%s_rooms' % n)
    o.append(asm_bytes(g['bin_rooms']))
# the game table: 12 bytes a game (GAME_* in phcomp.c)
o.append('\t._game_tab')
for gi, g in enumerate(games):
    n = 'g%d' % gi
    sr, sx, sy = g['start']
    sri = [r['name'] for r in g['rooms']].index(sr)
    o.append('\t\tdefw %s_menu, %s_title, %s_ending, %s_roff' % (n, n, n, n))
    o.append('\t\tdefb %d, %d, %d, %d    ; start room x y, ink | lives << 4' % (sri, sx, sy, (g['ink'] & 7) | (g['lives'] << 4)))
o.append('#endasm')
o.append('')
# sprite figure numbers for the C code
for name, (f, nf) in FIG.items():
    o.append('#define F_%s %d' % (cname(name), f))
open(os.path.join(ROOT, 'src', 'assets.h'), 'w').write('\n'.join(o) + '\n')

# gfx/tileset.png for the room editor: the tileset as it is
total = len(tiles) + len(spr) + len(rooms_bin) + len(texts_bin) + sum(len(g['bin_rooms']) for g in games)
print('conv.py: %d tiles, %d frames, %d rooms (%d bytes), %d games, data %d bytes'
      % (NTILES, NFRAMES, len(rooms), len(rooms_bin), len(games), total))
