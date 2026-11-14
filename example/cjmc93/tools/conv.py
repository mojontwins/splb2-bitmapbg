#!/usr/bin/env python3
"""CJMC93 asset converter: gfx/ -> src/assets.h  (run by ../build.sh)

Everything you are expected to edit is a plain file in gfx/. Never edit
src/assets.h: this script rewrites it.

  gfx/font.png            128x48: characters 32-127, 16 per row. White
                          (any light colour) = ink.
  gfx/tiles.png           128x24: characters 128-164, 16 per row: the block
                          graphics 128-143, then the UDGs A-U (144-164).
  gfx/sprites.png         128 wide, 16x24 frames, 8 per row (frame number =
                          row * 8 + column, see SPR_* in the output). White =
                          ink, black = see-through, red = solid paper (mask).
                          A frame with no red at all gets an automatic mask
                          (the inside of the figure plus a 1 px halo).
                          Figures shorter than 24 px go at the top.
  gfx/backdrops/*.png     256x192 screens in Spectrum colours (two per 8x8
                          cell, BRIGHT per cell). castle, cave, bridge. The
                          whole picture goes into the background buffer, so
                          paint anything anywhere, the play area included.
  gfx/pictures/*.png      decoration for the game area: 192 px wide (columns
                          4-27), up to 136 tall (rows 3-19), resting on the
                          bottom frame. One colour: light = ink. No colours
                          are stored: the game area is INK 6. Numbered from
                          1 (PICTURE_ORDER); a room's header names one.
  gfx/rooms.txt           the rooms, from the room editor (tools/roomed),
                          compact encoding (format at the top of the file).
                          Writes gfx/tileset.png for the editor.
  gfx/texts.txt           the rooms' texts, in sets a room's header names.

Backdrops are packed with ZX0 (https://github.com/einar-saukas/ZX0).
Packed copies are cached in gfx/packed/, so the zx0 program is needed only
when a backdrop changes (set ZX0=/path/to/zx0 if it isn't in the PATH).
Only the Python 3 standard library is needed.
"""
import os, re, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import png

G = os.path.join(ROOT, 'gfx')

def die(msg):
    sys.exit('conv.py: ' + msg)

PAL = [(0, 0, 0), (0, 0, 0xD7), (0xD7, 0, 0), (0xD7, 0, 0xD7),
       (0, 0xD7, 0), (0, 0xD7, 0xD7), (0xD7, 0xD7, 0), (0xD7, 0xD7, 0xD7)]
PALB = [(0, 0, 0), (0, 0, 0xFF), (0xFF, 0, 0), (0xFF, 0, 0xFF),
        (0, 0xFF, 0), (0, 0xFF, 0xFF), (0xFF, 0xFF, 0), (0xFF, 0xFF, 0xFF)]

def lit(px):
    return px[3] >= 128 and sum(px[:3]) >= 384

# ============================================================ font + tiles
# tile n = character 32 + n: 0-95 the font, 96-111 block graphics,
# 112-132 the UDGs A-U.
def read_cells(path, count):
    w, h, img = png.read(path)
    rows = (count + 15) // 16
    if w != 128 or h != rows * 8:
        die('%s must be 128x%d, it is %dx%d' % (path, rows * 8, w, h))
    out = []
    for i in range(count):
        out += [sum(128 >> x for x in range(8) if lit(img[(i // 16) * 8 + y][(i % 16) * 8 + x]))
                for y in range(8)]
    return out

tiles = read_cells(os.path.join(G, 'font.png'), 96) + read_cells(os.path.join(G, 'tiles.png'), 37)
NTILES = len(tiles) // 8

# ================================================================= sprites
# sprites.png holds the pictures, 16x24 each, top-aligned. The game uses
# them in three heights (splib2 draws every character a sprite covers, so
# a short figure in a tall sprite costs time for nothing): each frame below
# is a picture of the sheet cut to a height. A sprite 8, 16 or 24 px tall
# is 2, 3 or 4 characters tall (one more for the vertical shift).
PNG_NAMES = ['PLAYER_R', 'PLAYER_L', 'KNIGHT', 'SHAPESHIFT', 'GUARD', 'GUARDIAN', 'BALL',
             'PLATFORM', 'BOX']
FRAMES = [  # name, picture, height
    ('PLAYER_R', 'PLAYER_R', 16), ('PLAYER_L', 'PLAYER_L', 16),
    ('GUARD', 'GUARD', 16), ('GUARDIAN', 'GUARDIAN', 16), ('BOX', 'BOX', 16),
    ('KNIGHT', 'KNIGHT', 24), ('SHAPESHIFT', 'SHAPESHIFT', 24),
    ('DOPPEL_R', 'PLAYER_R', 24), ('DOPPEL_L', 'PLAYER_L', 24),   # 1.5, in a tall sprite
    ('BALL', 'BALL', 8), ('PLATFORM', 'PLATFORM', 8),
]
FH = 24

def classify(px):
    r, g, b, a = px
    if a < 128: return 0                                  # transparent
    if r > 160 and g < 100 and b < 100: return 2          # red: mask
    return 1 if (r + g + b) >= 384 else 0                 # light: ink

def auto_mask(ink):
    """1 = see-through. Outside the figure (flood from the edges) minus a
    1 px halo around the ink."""
    out = [[0] * 16 for _ in range(FH)]
    st = [(x, y) for x in range(16) for y in (0, FH - 1)] + [(x, y) for y in range(FH) for x in (0, 15)]
    while st:
        x, y = st.pop()
        if 0 <= x < 16 and 0 <= y < FH and not out[y][x] and not ink[y][x]:
            out[y][x] = 1; st += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    m = [[0] * 16 for _ in range(FH)]
    for y in range(FH):
        for x in range(16):
            keep = out[y][x]
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    if 0 <= x + dx < 16 and 0 <= y + dy < FH and ink[y + dy][x + dx]: keep = 0
            m[y][x] = keep
    return m

w, h, img = png.read(os.path.join(G, 'sprites.png'))
if w != 128 or h % FH or (w // 16) * (h // FH) < len(PNG_NAMES):
    die('sprites.png must be 128 wide, rows of 16x%d pictures, at least %d (%s)'
        % (FH, len(PNG_NAMES), ', '.join(PNG_NAMES)))
pics = {}
for n, name in enumerate(PNG_NAMES):
    ox, oy = (n % 8) * 16, (n // 8) * FH
    cls = [[classify(img[oy + y][ox + x]) for x in range(16)] for y in range(FH)]
    ink = [[c == 1 for c in row] for row in cls]
    if any(c == 2 for row in cls for c in row):
        m = [[c == 0 for c in row] for row in cls]
    else:
        m = auto_mask(ink)
    pics[name] = (ink, m)

# splib2 masked sprite data: per column, the frame's lines then 8 blank
# lines, as (graphic, mask) pairs. Two columns; the third is splib2's null
# column (see spr_create in the game). Before the first frame, 8 blank
# lines (read when it is shifted down).
BL = [0, 255]
spr = []
frame_off = []
for name, pic, fh in FRAMES:
    ink, m = pics[pic]
    for y in range(fh, FH):
        if any(ink[y]):
            die('sprites.png: %s is used %d px tall, but has ink below that' % (pic, fh))
    frame_off.append(len(spr))
    for half in (0, 8):
        for y in range(fh):
            spr += [sum(128 >> x for x in range(8) if ink[y][half + x]),
                    sum(128 >> x for x in range(8) if m[y][half + x])]
        spr += BL * 8
SPR_NAMES = [f[0] for f in FRAMES]

# ================================================================ backdrops
ZX0 = os.environ.get('ZX0', 'zx0')
PK = os.path.join(G, 'packed')
os.makedirs(PK, exist_ok=True)

def zx0_cached(name, data):
    fs, fz = os.path.join(PK, name + '.bin'), os.path.join(PK, name + '.zx0')
    if not (os.path.exists(fs) and os.path.exists(fz) and open(fs, 'rb').read() == data):
        open(fs, 'wb').write(data)
        try:
            subprocess.run([ZX0, '-f', fs, fz], check=True, capture_output=True)
        except OSError:
            die('%s changed and the zx0 compressor was not found '
                '(https://github.com/einar-saukas/ZX0, or set ZX0=/path/to/zx0)' % name)
    return open(fz, 'rb').read()

def nearest(rgb):
    """-> (colour 0..7, bright 0/1, or None for black)"""
    best = None
    for b, pal in ((0, PAL), (1, PALB)):
        for i, c in enumerate(pal):
            d = sum((x - y) ** 2 for x, y in zip(rgb[:3], c))
            if best is None or d < best[0]:
                best = (d, i, b)
    return best[1], (None if best[1] == 0 else best[2])

# the game area's ink, for backdrops whose game area is empty (black):
# the pictures (gfx/pictures) take it, and so do the sprites over them
PLAY_INK = {'castle': 6, 'cave': 6}

def screen_from_png(path, play_ink=None):
    """-> 6144 bytes of bitmap (display file order) + 768 attributes"""
    w, h, img = png.read(path)
    if (w, h) != (256, 192):
        die('%s must be 256x192, it is %dx%d' % (path, w, h))
    bitmap = bytearray(6144); attrs = bytearray(768)
    for r in range(24):
        for c in range(32):
            px = [[nearest(img[r * 8 + y][c * 8 + x]) for x in range(8)] for y in range(8)]
            cols = {}
            bright = 0
            for row in px:
                for col, b in row:
                    cols[col] = cols.get(col, 0) + 1
                    if b: bright = 1
            # ink = the less common colour (paper = the most common); one
            # colour only: it's the paper, ink 7 (or 0 on a light paper)
            order = sorted(cols, key=lambda k: (-cols[k], k))
            if len(cols) > 2:
                # keep the two main colours; the others become the nearer
                # of the two (by RGB distance)
                print('conv.py: warning: %s: cell row %d col %d has %d colours, keeping two'
                      % (os.path.basename(path), r, c, len(cols)))
                keep = order[:2]
                def rgb(k): return PALB[k] if bright else PAL[k]
                def near(k):
                    return min(keep, key=lambda q: sum((x - y) ** 2 for x, y in zip(rgb(k), rgb(q))))
                px = [[(col if col in keep else near(col), b) for col, b in row] for row in px]
                order = keep
            paper = order[0]
            ink = order[1] if len(order) > 1 else (0 if paper >= 4 else 7)
            if (play_ink is not None and len(order) == 1 and paper == 0
                    and 3 <= r <= 19 and 4 <= c <= 27):
                ink = play_ink
            attrs[r * 32 + c] = bright << 6 | paper << 3 | ink
            for y in range(8):
                b = sum(128 >> x for x in range(8) if px[y][x][0] == ink and ink != paper)
                bitmap[(r & 0x18) << 8 | y << 8 | (r & 7) << 5 | c] = b
    return bytes(bitmap), bytes(attrs)

BACKDROPS = ['castle', 'cave', 'bridge']
packed = {}
for b in BACKDROPS:
    bm, at = screen_from_png(os.path.join(G, 'backdrops', b + '.png'), PLAY_INK.get(b))
    packed[b] = (zx0_cached(b + '_bitmap', bm), zx0_cached(b + '_attrs', at))

# ================================================================= pictures
# Each picture: its lines top to bottom, 24 bytes each, ZX0-packed. The
# game unpacks it straight into the background buffer (pic_unpack), line
# by line, from pixel line pic_top (160 - height: the bottom on row 19).
PD = os.path.join(G, 'pictures')
# numbered from 1 in this order (header PIC in rooms.txt); others after them
PICTURE_ORDER = ['temple', 'castle', 'corridor']
_found = sorted(f[:-4] for f in os.listdir(PD) if f.endswith('.png')) if os.path.isdir(PD) else []
PICTURES = [p for p in PICTURE_ORDER if p in _found] + [p for p in _found if p not in PICTURE_ORDER]
pic_packed = []
pic_top = []
for n in PICTURES:
    w, h, img = png.read(os.path.join(PD, n + '.png'))
    if w != 192 or not 1 <= h <= 136:
        die('pictures/%s.png must be 192 wide and at most 136 tall, it is %dx%d' % (n, w, h))
    raw = bytes(sum(128 >> x for x in range(8) if lit(img[y][c * 8 + x]))
                for y in range(h) for c in range(24))
    pic_packed.append(zx0_cached('pic_' + n, raw))
    pic_top.append(160 - h)

# ==================================================================== rooms
# gfx/rooms.txt, written by the room editor (tools/roomed), "compact"
# encoding (the format is explained at the top of that file). Binary, per
# room, what room_draw () in the game reads:
#   header   BG PIC TEXT
#   char     X Y 0 CODE ATTR
#   rect     X Y 1 CODE ATTR X2 Y2
#   figure   X Y 0x80|(W-1)<<3|(H-1) CODE ATTR        a pattern with one copy
#   pattern  X Y 0x40|(W-1)<<3|(H-1) CODE ATTR X2 Y2  repeated, W and H 1-8
#   end      0xFF
# Editor-only settings (screen, area, tileset, background, export...) are
# ignored here; the ones that change the bytes must be what room_draw ()
# reads: encoding compact, solid_bit 6, and bg_params (BG_PARAMS).
BG_NAMES = ['castle', 'cave', 'bridge', 'black', 'none']
BG_PARAMS = dict((b, 2) for b in range(len(BG_NAMES)))     # picture, texts

def encode_obj(kind, v, where):
    """one object -> bytes (compact encoding)"""
    if kind == 'char':
        x, y, code, attr = v
        return [x, y, 0, code, attr]
    if kind == 'rect':
        x, y, code, attr, x2, y2 = v
        return [x, y, 1, code, attr, x2, y2]
    x, y, code, attr, x2, y2, pw, ph = v               # pattern
    if not (1 <= pw <= 8 and 1 <= ph <= 8):
        die(where + 'patterns can be at most 8x8 (compact encoding)')
    t = ((pw - 1) << 3) | (ph - 1)
    if x2 - x + 1 == pw and y2 - y + 1 == ph:          # exactly one figure
        return [x, y, 0x80 | t, code, attr]
    return [x, y, 0x40 | t, code, attr, x2, y2]

NPAR = {'char': 4, 'rect': 6, 'pattern': 8}
rooms = []                                  # (name, bytes)
seen = {}
hdr = objs = None
for ln, line in enumerate(open(os.path.join(G, 'rooms.txt')), 1):
    w = line.split('#')[0].split()
    if not w:
        continue
    where = 'rooms.txt line %d: ' % ln
    try:
        k = w[0]
        if k == 'room':
            hdr, objs = [0], []
            name = ' '.join(w[1:]) or 'room %d' % len(rooms)
        elif k == 'header':
            hdr = [int(v) & 255 for v in w[1:]] or [0]
        elif k in NPAR:
            v = [int(x) for x in w[1:]]
            if len(v) != NPAR[k]:
                die(where + '%s needs %d numbers' % (k, NPAR[k]))
            if any(n < 0 or n > 255 for n in v):
                die(where + 'numbers must be 0-255')
            if v[2] != 255 and v[2] >= NTILES:
                die(where + 'tile %d: there are %d tiles (255: draw nothing)' % (v[2], NTILES))
            objs += encode_obj(k, v, where)
        elif k == 'end':
            bg = hdr[0]
            if bg not in BG_PARAMS:
                die(where + 'background %d: room_draw () knows %s' % (bg, ', '.join(
                    '%d %s' % (i, n) for i, n in enumerate(BG_NAMES))))
            n = BG_PARAMS[bg]
            par = (hdr[1:] + [0] * n)[:n]
            if par[0] > len(PICTURES):
                die(where + 'picture %d: there are %d (%s)' % (par[0], len(PICTURES), ', '.join(
                    '%d %s' % (i + 1, p) for i, p in enumerate(PICTURES))))
            rooms.append((name, [bg] + par + objs + [255]))
            hdr = objs = None
        elif k in ('encoding', 'solid_bit', 'bg_params'):
            seen[k] = seen.get(k, []) + [w[1:]]
        elif k == 'object':
            die(where + 'custom objects: room_draw () knows none')
    except (TypeError, ValueError, IndexError):
        die(where + 'bad line')
if seen.get('encoding') != [['compact']]:
    die("rooms.txt: set 'encoding compact' (room_draw () reads the compact encoding)")
if seen.get('solid_bit', [['6']]) != [['6']]:
    die("rooms.txt: room_draw () takes BRIGHT (bit 6) as solid: set solid_bit 6")
declared = dict((int(a), int(b)) for a, b in seen.get('bg_params', []))
if declared != BG_PARAMS:
    die("rooms.txt: bg_params must be %s, what room_draw () reads (found %s)"
        % (' / '.join('%d %d' % kv for kv in sorted(BG_PARAMS.items())), declared))

# ==================================================================== texts
# gfx/texts.txt: sets of texts, numbered from 1, named by the rooms'
# headers. Bytes, per set: ROW COL ATTR characters 0, ..., then 0xFF.
texts = {}
cur = None
for ln, line in enumerate(open(os.path.join(G, 'texts.txt'), encoding='utf-8'), 1):
    where = 'texts.txt line %d: ' % ln
    m = re.match(r'\s*(\d+)\s+(\d+)\s+(\d+)\s+"(.*)"', line)
    if m:
        if cur is None:
            die(where + 'a text before the first "text N"')
        r, c, a, t = int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4)
        if r > 23 or c + len(t) > 32 or a > 255 or not t:
            die(where + 'the text must fit in rows 0-23, columns 0-31')
        if any(not 32 <= ord(ch) <= 127 for ch in t):
            die(where + 'characters 32-127 only')
        texts[cur] += [r, c, a] + [ord(ch) for ch in t] + [0]
        continue
    w = line.split('#')[0].split()
    if not w:
        continue
    if w[0] == 'text' and len(w) == 2 and w[1].isdigit() and 1 <= int(w[1]) <= 255:
        cur = int(w[1])
        if cur in texts:
            die(where + 'text %d twice' % cur)
        texts[cur] = []
    else:
        die(where + 'bad line (text N, or ROW COL ATTR "text")')
NTEXTS = max(texts) if texts else 0
for n in range(1, NTEXTS + 1):
    if n not in texts:
        die('texts.txt: text %d is missing (numbers from 1, without gaps)' % n)
for name, r in rooms:
    if r[2] > NTEXTS:
        die('rooms.txt: room %s: text %d, but texts.txt has %d' % (name, r[2], NTEXTS))
texts_bin = []
texts_off = []
for n in range(1, NTEXTS + 1):
    texts_off.append(len(texts_bin))
    texts_bin += texts[n] + [0xFF]

# gfx/tileset.png for the editor: 256 tiles, 32 per row, white on black
# (the colour comes from each object's ATTR)
ts = [[(0, 0, 0)] * 256 for _ in range(64)]
for t in range(NTILES):
    for y in range(8):
        for x in range(8):
            if tiles[t * 8 + y] & (128 >> x):
                ts[(t // 32) * 8 + y][(t % 32) * 8 + x] = (215, 215, 215)
png.write(os.path.join(G, 'tileset.png'), 256, 64, ts)

# ================================================================== beeper
# BEEP d, n of the original: n semitones from middle C. For each n in
# BEEP_MIN..63: the delay count for half a period of the beeper loop in
# the game (26 T-states per count, ~70 T overhead) and the frequency in Hz.
BEEP_MIN = -30
beep = []
for n in range(BEEP_MIN, 64):
    f = 261.63 * 2 ** (n / 12)
    half = 3500000 / (2 * f)
    count = max(1, int((half - 70) / 26))
    hz = int(round(f))
    beep += [count & 255, count >> 8, hz & 255, hz >> 8]

# ================================================================== output
def asm_bytes(data, per=16):
    return '\n'.join('\tdefb ' + ','.join(str(b) for b in data[i:i + per]) for i in range(0, len(data), per))

def cname(n):
    return n.replace('.', '_').upper()

o = ['/* Generated by tools/conv.py from gfx/ - do not edit */', '',
     '#define NTILES %d' % NTILES,
     '#define T_FONT 0          /* tile = character - 32 */',
     '#define T_BLOCK 96        /* block graphics 128-143 */',
     '#define T_UDG 112         /* UDG A = T_UDG, B = T_UDG + 1... */',
     '#define BEEP_MIN %d' % BEEP_MIN]
for i, n in enumerate(SPR_NAMES):
    o.append('#define SPR_%s %d' % (n, i))
o.append('/* per frame: offset in sprite_frames, height class (0: 8 px, 1: 16, 2: 24) */')
o.append('unsigned int spr_off [] = { %s };' % ', '.join(str(x) for x in frame_off))
o.append('unsigned char spr_class [] = { %s };' % ', '.join(str(f[2] // 8 - 1) for f in FRAMES))
for i, b in enumerate(BACKDROPS):
    o.append('#define BD_%s %d' % (b.upper(), i))
o.append('#define BD_BLACK %d' % len(BACKDROPS))
o.append('#define BD_NONE %d' % (len(BACKDROPS) + 1))
for i, (n, r) in enumerate(rooms):
    o.append('#define L_%s %d' % (cname(n), i))
o.append('#define NLAYOUTS %d' % len(rooms))
offs, at = [], 0
for n, r in rooms:
    offs.append(at); at += len(r)
o += ['', 'extern unsigned char tiles[];', 'extern unsigned char sprite_frames[];',
      'extern unsigned char beep_tab[];', 'extern unsigned char rooms_data[];',
      'extern unsigned char texts_data[];',
      'extern unsigned char *bd_bitmap[];', 'extern unsigned char *bd_attrs[];', '',
      '/* room n starts at rooms_data + rooms_off [n] (gfx/rooms.txt) */',
      '/* text set n (from 1) at texts_data + texts_off [n - 1] (gfx/texts.txt) */',
      'unsigned int texts_off[] = { %s };' % (', '.join(map(str, texts_off)) or '0'),
      'unsigned int rooms_off[] = { %s };' % ', '.join(map(str, offs)),
      'unsigned char *bd_bitmap[] = { %s };' % ', '.join('zx0_%s_bitmap' % b for b in BACKDROPS),
      'unsigned char *bd_attrs[] = { %s };' % ', '.join('zx0_%s_attrs' % b for b in BACKDROPS),
      '/* pictures, numbered from 1: %s */' % ', '.join('%d %s' % (i + 1, p) for i, p in enumerate(PICTURES)),
      'unsigned char *pic_data[] = { %s };' % ', '.join(['zx0_pic_%s' % p for p in PICTURES] or ['0']),
      'unsigned char pic_top[] = { %s };' % ', '.join([str(t) for t in pic_top] or ['0'])]
# the arrays above need the labels declared first
decl = []
decl += ['extern unsigned char zx0_%s_bitmap[];\nextern unsigned char zx0_%s_attrs[];' % (b, b)
         for b in BACKDROPS]
decl += ['extern unsigned char zx0_pic_%s[];' % p for p in PICTURES]
i = o.index('extern unsigned char tiles[];')
o[i:i] = decl
o.append('\n#asm')
o.append('._tiles\n' + asm_bytes(tiles))
o.append('._sprite_frames_pad                ; 8 blank lines read when frame 0 is shifted down\n' + asm_bytes(BL * 8))
o.append('._sprite_frames\n' + asm_bytes(spr))
o.append('._beep_tab\n' + asm_bytes(beep))
for b in BACKDROPS:
    o.append('._zx0_%s_bitmap\n' % b + asm_bytes(list(packed[b][0])))
    o.append('._zx0_%s_attrs\n' % b + asm_bytes(list(packed[b][1])))
for i, p in enumerate(PICTURES):
    o.append('._zx0_pic_%s\n' % p + asm_bytes(list(pic_packed[i])))
o.append('._texts_data\n' + asm_bytes(texts_bin or [255]))
o.append('._rooms_data')
for n, r in rooms:
    o.append('\t\t\t\t; room %s\n' % n + asm_bytes(r))
o.append('#endasm\n')
open(os.path.join(ROOT, 'src', 'assets.h'), 'w').write('\n'.join(o))
print('conv.py: %d tiles, %d sprite frames, backdrops %s, %d rooms (%d bytes)' % (
    NTILES, len(SPR_NAMES), ', '.join('%s %d+%d' % (b, len(packed[b][0]), len(packed[b][1])) for b in BACKDROPS),
    len(rooms), sum(len(r) for n, r in rooms)))
print('conv.py: %d text sets (%d bytes)' % (NTEXTS, len(texts_bin)))
print('conv.py: pictures %s' % ', '.join('%s %d' % (p, len(pic_packed[i])) for i, p in enumerate(PICTURES)))
