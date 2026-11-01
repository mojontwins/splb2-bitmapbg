#!/usr/bin/env python3
"""Eleuterio asset converter: gfx/ + levels.txt -> src/assets.h

Everything you are expected to edit while developing the game is a plain
file in gfx/ or levels.txt. Run ../build.sh (or make.bat) after editing:
this script runs first and regenerates src/assets.h. Never edit assets.h.

  gfx/sprites.png   sprite sheet, 128x128 = 8x8 frames of 16x16 (see below)
  gfx/tiles.png     tile set, 128x48 = 16x6 tiles of 8x8, in Spectrum colours
  levels.txt        the levels (format described in that file)

Only the Python 3 standard library is needed (tools/png.py reads and
writes the PNGs). If gfx/sprites.png or gfx/tiles.png is missing, it is
re-created from the original ZX Basic data in orig/.
"""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import png

G = os.path.join(ROOT, 'gfx')
ORIG = os.path.join(ROOT, 'orig')

def die(msg):
    sys.exit('conv.py: ' + msg)

def basic_array(path, name):
    """numbers inside the { } of 'Dim name ... => { ... }' in a .bas file"""
    s = open(path, encoding='latin-1').read()
    i = s.index(name); j = s.index('{', i); k = s.index('}', j)
    return [int(x) for x in re.findall(r'\d+', s[j:k])]

# ZX Spectrum palette: index 0..7, normal and BRIGHT. The converter accepts
# any colour that is close to one of these (nearest match), so editors that
# use slightly different Spectrum palettes are fine.
PAL = [(0, 0, 0), (0, 0, 0xD7), (0xD7, 0, 0), (0xD7, 0, 0xD7),
       (0, 0xD7, 0), (0, 0xD7, 0xD7), (0xD7, 0xD7, 0), (0xD7, 0xD7, 0xD7)]
PALB = [(0, 0, 0), (0, 0, 0xFF), (0xFF, 0, 0), (0xFF, 0, 0xFF),
        (0, 0xFF, 0), (0, 0xFF, 0xFF), (0xFF, 0xFF, 0), (0xFF, 0xFF, 0xFF)]

def zx_colour(rgb):
    """-> (index 0..7, bright 0/1/None). None = black, valid with either."""
    best = None
    for b, pal in ((0, PAL), (1, PALB)):
        for i, c in enumerate(pal):
            d = sum((x - y) ** 2 for x, y in zip(rgb, c))
            if best is None or d < best[0]:
                best = (d, i, b)
    return best[1], (None if best[1] == 0 else best[2])

# ====================================================================== sprites
#
# gfx/sprites.png: 128x128 pixels, 8 rows x 8 columns of 16x16 frames.
# Frame number = row * 8 + column. The game uses:
#
#   row 0  (frames  0- 7)  Eleuterio: left, left step, right, right step,
#                          climb 1, climb 2, hammer up, hammer down
#   row 1  (frames  8-15)  enemy AI type 0, same order; frames 14-15 are
#   row 2  (frames 16-23)  enemy AI type 1     "fixing the hole" instead
#   row 3  (frames 24-31)  enemy AI type 2     of the hammer
#   row 4  (frames 32-39)  enemy AI type 3
#   rows 5-6               free (empty frames take no memory)
#   row 7  (frames 56-58)  explosion 1, explosion 2, tombstone
#
# Colours: white (any light colour) = ink, black or transparent =
# see-through, red = solid paper (the mask: the background is erased
# there). A frame without any red gets an automatic mask (the inside of the
# figure plus a 1 px halo around the ink). The shipped sheet has that
# automatic mask painted in red so you can see and change it.

SPR_PNG = os.path.join(G, 'sprites.png')
INK, PAPER, MASK = (255, 255, 255), (0, 0, 0), (255, 0, 0)

def auto_mask(rows):
    """rows: 16 ints (16 bits). Returns 16 ints, 1 = see-through."""
    ink = [[(rows[y] >> (15 - x)) & 1 for x in range(16)] for y in range(16)]
    out = [[0] * 16 for _ in range(16)]
    st = [(x, y) for x in range(16) for y in (0, 15)] + [(x, y) for y in range(16) for x in (0, 15)]
    while st:
        x, y = st.pop()
        if 0 <= x < 16 and 0 <= y < 16 and not out[y][x] and not ink[y][x]:
            out[y][x] = 1
            st += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    m = []
    for y in range(16):
        v = 0
        for x in range(16):
            keep = out[y][x]
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < 16 and 0 <= yy < 16 and ink[yy][xx]:
                        keep = 0
            v = (v << 1) | keep
        m.append(v)
    return m

if not os.path.exists(SPR_PNG):
    # the original fourspriter blocks: 4 blocks of 8x8 per frame (TL TR BL BR)
    d = basic_array(os.path.join(ORIG, 'spritegfxdata.bas'), 'spriteGfxData')
    img = [[PAPER] * 128 for _ in range(128)]
    for f in range(64):
        rows = []
        for half in (0, 2):
            for y in range(8):
                rows.append((d[(f * 4 + half) * 8 + y] << 8) | d[(f * 4 + half + 1) * 8 + y])
        if not any(rows):
            continue
        m = auto_mask(rows)
        for y in range(16):
            for x in range(16):
                if (rows[y] >> (15 - x)) & 1:
                    img[(f // 8) * 16 + y][(f % 8) * 16 + x] = INK
                elif not (m[y] >> (15 - x)) & 1:
                    img[(f // 8) * 16 + y][(f % 8) * 16 + x] = MASK
    png.write(SPR_PNG, 128, 128, img)
    print('created', SPR_PNG)

def classify(px):
    r, g, b, a = px
    if a < 128: return 0                           # transparent
    if r > 160 and g < 100 and b < 100: return 2   # red: mask
    return 1 if r + g + b >= 384 else 0            # light: ink

w, h, img = png.read(SPR_PNG)
if (w, h) != (128, 128):
    die('%s must be 128x128 (8x8 frames of 16x16), it is %dx%d' % (SPR_PNG, w, h))

# splib2 sprite data, 2 columns per frame. Each column: 16 lines + 8 blank
# lines, 2 bytes per line (graphic, mask). Before the first frame there are
# 8 blank lines (splib2 reads above the sprite when it shifts it down).
# Empty frames are not stored: they all point at one blank frame.
BL = [0, 255]
spr = BL * 8
frame_offset = []
blank_offset = None
for f in range(64):
    g = []; mk = []; manual = False
    for y in range(16):
        gv = mv = 0
        for x in range(16):
            c = classify(img[(f // 8) * 16 + y][(f % 8) * 16 + x])
            gv = (gv << 1) | (c == 1)
            mv = (mv << 1) | (c == 0)
            manual |= c == 2
        g.append(gv); mk.append(mv)
    if not any(g) and not manual:
        if blank_offset is None:
            blank_offset = len(spr) - 16
            spr += BL * 48            # one blank frame shared by all empty ones
        frame_offset.append(blank_offset)
        continue
    m = mk if manual else auto_mask(g)
    frame_offset.append(len(spr) - 16)
    for sh in (8, 0):
        for y in range(16):
            spr += [(g[y] >> sh) & 255, (m[y] >> sh) & 255]
        spr += BL * 8

# ======================================================================== tiles
#
# gfx/tiles.png: 128x48 pixels, 16 columns x 6 rows of 8x8 tiles, tile
# number = row * 16 + column (0..95), in Spectrum colours. Each tile may use
# at most two colours, both normal or both BRIGHT (black goes with either).
# The higher palette index becomes INK, the lower one PAPER (that is how
# every original tile is made). A one-colour tile is PAPER of that colour
# with INK 7. Sprites take the INK of the cells they are on.
#
# gfx/tiles_ref.bin remembers the exact bytes and attribute of every tile
# from the last conversion: a tile that still looks the same keeps them
# (that only matters for one-colour tiles, whose INK can't be seen).

TIL_PNG = os.path.join(G, 'tiles.png')
TIL_REF = os.path.join(G, 'tiles_ref.bin')
NTILES = 96

def render_tile(bits, attr):
    ink = attr & 7; pap = (attr >> 3) & 7; pal = PALB if attr & 64 else PAL
    return [[pal[ink] if (bits[y] >> (7 - x)) & 1 else pal[pap] for x in range(8)] for y in range(8)]

if not os.path.exists(TIL_PNG):
    d = basic_array(os.path.join(ORIG, 'tilegfxdata.bas'), 'inGameTilesGfxData (767)')
    a = basic_array(os.path.join(ORIG, 'tilegfxdata.bas'), 'inGameTilesGfxDataAttributes')
    img = [[PAPER] * 128 for _ in range(48)]
    for t in range(NTILES):
        cell = render_tile(d[t * 8:t * 8 + 8], a[t])
        for y in range(8):
            for x in range(8):
                img[(t // 16) * 8 + y][(t % 16) * 8 + x] = cell[y][x]
    png.write(TIL_PNG, 128, 48, img)
    open(TIL_REF, 'wb').write(bytes(d + a))
    print('created', TIL_PNG)

w, h, img = png.read(TIL_PNG)
if (w, h) != (128, 48):
    die('%s must be 128x48 (16x6 tiles of 8x8), it is %dx%d' % (TIL_PNG, w, h))
ref = open(TIL_REF, 'rb').read() if os.path.exists(TIL_REF) else None
tile_bits, tile_attr = [], []
for t in range(NTILES):
    cell = [[img[(t // 16) * 8 + y][(t % 16) * 8 + x][:3] for x in range(8)] for y in range(8)]
    cols = [[zx_colour(c) for c in row] for row in cell]
    if ref:
        rb, ra = list(ref[t * 8:t * 8 + 8]), ref[768 + t]
        if [[zx_colour(c) for c in row] for row in render_tile(rb, ra)] == cols:
            tile_bits += rb; tile_attr.append(ra); continue
    idx = sorted({c[0] for row in cols for c in row})
    br = {c[1] for row in cols for c in row} - {None}
    if len(idx) > 2:
        die('tile %d (column %d, row %d of tiles.png) has more than two colours' % (t, t % 16, t // 16))
    if len(br) > 1:
        die('tile %d (column %d, row %d of tiles.png) mixes BRIGHT and normal colours' % (t, t % 16, t // 16))
    bright = 64 if br == {1} else 0
    if len(idx) == 1:
        pap, ink = idx[0], 7
    else:
        pap, ink = idx
    tile_attr.append(bright | (pap << 3) | ink)
    for row in cols:
        v = 0
        for c in row:
            v = (v << 1) | (c[0] == ink and len(idx) == 2)
        tile_bits.append(v)
open(TIL_REF, 'wb').write(bytes(tile_bits + tile_attr))

# ======================================================================= levels
#
# levels.txt, see the comments in that file. Each level becomes the 43
# bytes of the original hammeringLevels() array:
#   7*p      stairs on floor p (bit x = stairs at column x, x = 0 leftmost)
#   7*p+1..6 the six breakable platform pieces of floor p (0..12)
#   28+4e    enemy e: AI type, turns to fix a hole, life, speed (1 fast)
#   40,41,42 platform tiles, background tiles, frame tiles

LEVELS = os.path.join(ROOT, 'levels.txt')
levels = []
cur = None
for n, line in enumerate(open(LEVELS), 1):
    line = line.split(';')[0].strip()
    if not line:
        continue
    where = 'levels.txt line %d: ' % n
    w_ = line.split()
    key = w_[0].lower()
    if key == 'level':
        cur = {'floors': [], 'enemies': [], 'tiles': None}
        levels.append(cur)
        continue
    if cur is None:
        die(where + 'expected "level" first')
    if key == 'floor':
        if len(w_) != 8:
            die(where + 'floor needs a stairs pattern and 6 platform values')
        st = w_[1]
        if len(st) != 7 or set(st) - set('#.'):
            die(where + 'stairs pattern must be 7 characters of # and .')
        vals = [int(x) for x in w_[2:]]
        if any(v < 0 or v > 12 for v in vals):
            die(where + 'platform values go from 0 (hole) to 12 (intact)')
        cur['floors'].append((sum(1 << i for i, c in enumerate(st) if c == '#'), vals))
    elif key == 'enemy':
        kv = dict(p.split('=') for p in w_[1:])
        try:
            cur['enemies'].append([int(kv[k]) for k in ('ai', 'fix', 'life', 'speed')])
        except KeyError as e:
            die(where + 'enemy needs ai= fix= life= speed= (missing %s)' % e)
        if not 0 <= cur['enemies'][-1][0] <= 3:
            die(where + 'ai goes from 0 to 3')
    elif key == 'tiles':
        kv = dict(p.split('=') for p in w_[1:])
        cur['tiles'] = [int(kv['platform']), int(kv['background']), int(kv['frame'])]
    else:
        die(where + 'unknown keyword "%s"' % w_[0])
data = []
for i, l in enumerate(levels):
    if len(l['floors']) != 4 or len(l['enemies']) != 3 or not l['tiles']:
        die('level %d needs 4 floor lines, 3 enemy lines and a tiles line' % i)
    for st, vals in l['floors']:
        data += [st] + vals
    for e in l['enemies']:
        data += e
    data += l['tiles']
if not levels:
    die('levels.txt has no levels')

# ======================================================================= beeper
#
# The original used BEEP .005, n (5 ms, n semitones above middle C). This
# table gives, for n = -12..63, the delay count for half a period of the
# beeper loop in src/eleuterio.c (26 T-states per count, ~70 T overhead)
# and the number of whole periods that fit in 5 ms.
beep = []
for n in range(-12, 64):
    f = 261.63 * 2 ** (n / 12)
    half = 3500000 / (2 * f)
    count = max(1, int((half - 70) / 26))
    cycles = max(1, min(255, round(f * 0.005)))
    beep += [count & 255, count >> 8, cycles]

# ======================================================================= output
def asm_bytes(data, per=16):
    return '\n'.join('\tdefb ' + ','.join(str(b) for b in data[i:i + per]) for i in range(0, len(data), per))

o = ['/* Generated by tools/conv.py from gfx/ and levels.txt - do not edit */', '',
     '#define LEVEL_SIZE   43', '#define LEVEL_COUNT  %d' % len(levels),
     '#define NTILES       %d' % NTILES, '#define BEEP_MIN     -12', '',
     'extern unsigned char tiles[];          /* NTILES x 8 bytes            */',
     'extern unsigned char tile_attrs[];     /* NTILES attributes           */',
     'extern unsigned char sprite_frames[];  /* splib2 masked frame data    */',
     'extern unsigned int  frame_offsets[];  /* 64 offsets into it          */',
     'extern unsigned char levels[];         /* LEVEL_COUNT x LEVEL_SIZE    */',
     'extern unsigned char beep_tab[];       /* count lo, count hi, cycles  */',
     '', '#asm']
o.append('._tiles\n' + asm_bytes(tile_bits))
o.append('._tile_attrs\n' + asm_bytes(tile_attr))
o.append('._frame_offsets\n' + '\n'.join('\tdefw ' + ','.join(str(v) for v in frame_offset[i:i + 8]) for i in range(0, 64, 8)))
o.append('._sprite_frames_pad\n' + asm_bytes(spr[:16]))
o.append('._sprite_frames\n' + asm_bytes(spr[16:]))
o.append('._levels\n' + asm_bytes(data, 43))
o.append('._beep_tab\n' + asm_bytes(beep, 15))
o.append('#endasm\n')
open(os.path.join(ROOT, 'src', 'assets.h'), 'w').write('\n'.join(o))
print('assets: %d levels, %d sprite bytes (%d frames stored), %d tile bytes'
      % (len(levels), len(spr), (len(spr) - 16) // 96, len(tile_bits) + len(tile_attr)))
