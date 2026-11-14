#!/usr/bin/env python3
"""One-off extraction from the original tape (orig/). Run once; its outputs
in gfx/ are the sources from then on (edit those, not this).

  gfx/font.png          the Spectrum ROM font, characters 32-127
  gfx/tiles.png         block graphics 128-143 and the game's UDGs A-U (144-164)
  gfx/sprites.png       the sprites, built from the UDGs (16x24 frames)
  gfx/backdrops/*.png   castle: the original backdrop (tape block 2);
                        cave and bridge: drawn by the original's code
                        (lines 9080-9092 and 9240-9311), rendered here
  orig/rooms-print.txt  what each room PRINTs, taken from the listing
                        (tools/print2rooms.py made gfx/rooms.txt from it)

Needs orig/cjmc93.bas.txt (tools/listing.py), the ROM (for the font) and PIL
is NOT needed: tools/png.py writes the PNGs."""
import os, sys, re
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import png
from screen import Screen, listing

ROM = open(os.environ.get('ZXROM', '/home/claude/tools/skoolkit/skoolkit/resources/48.rom'), 'rb').read()
UDG = open(os.path.join(ROOT, 'orig', 'blk1.bin'), 'rb').read()
BACK = open(os.path.join(ROOT, 'orig', 'blk2.bin'), 'rb').read()
G = os.path.join(ROOT, 'gfx')
os.makedirs(os.path.join(G, 'backdrops'), exist_ok=True)

PAL = [(0, 0, 0), (0, 0, 0xD7), (0xD7, 0, 0), (0xD7, 0, 0xD7),
       (0, 0xD7, 0), (0, 0xD7, 0xD7), (0xD7, 0xD7, 0), (0xD7, 0xD7, 0xD7)]
PALB = [(0, 0, 0), (0, 0, 0xFF), (0xFF, 0, 0), (0xFF, 0, 0xFF),
        (0, 0xFF, 0), (0, 0xFF, 0xFF), (0xFF, 0xFF, 0), (0xFF, 0xFF, 0xFF)]
W, K, R = (255, 255, 255), (0, 0, 0), (255, 0, 0)

def glyph(c):
    if c >= 0x90:
        return list(UDG[(c - 0x90) * 8:(c - 0x90) * 8 + 8])
    if c >= 0x80:
        n = c - 0x80
        return [((0xF0 if n & 2 else 0) | (0x0F if n & 1 else 0)) if r < 4 else
                ((0xF0 if n & 8 else 0) | (0x0F if n & 4 else 0)) for r in range(8)]
    return list(ROM[0x3D00 + (c - 32) * 8:0x3D08 + (c - 32) * 8])

def sheet(codes, per_row=16):
    rows = (len(codes) + per_row - 1) // per_row
    img = [[K] * (per_row * 8) for _ in range(rows * 8)]
    for i, c in enumerate(codes):
        g = glyph(c)
        for y in range(8):
            for x in range(8):
                if g[y] >> (7 - x) & 1:
                    img[(i // per_row) * 8 + y][(i % per_row) * 8 + x] = W
    return per_row * 8, rows * 8, img

png.write(os.path.join(G, 'font.png'), *sheet(range(32, 128)))
png.write(os.path.join(G, 'tiles.png'), *sheet(range(128, 165)))

# ---------------------------------------------------------------- sprites
# 16x24 frames (2x3 characters), top-aligned. White = ink, red = mask
# (solid paper), black = see-through. [inverse] UDGs are inverted.
SPRITES = [
    # i = inverse (as INVERSE 1), p = printed as PAPER n; INK 0: the figure
    # is the paper, the ink its outline. Both are inverted here, so that the
    # figure is the sprite's ink.
    ('player right',  [['pA', 'pB'], ['pC', 'pD']]),
    ('player left',   [['pE', 'pF'], ['pG', 'pH']]),
    ('knight',        [['pI', 'pJ'], ['pK', 'pL'], ['pM', 'pN']]),
    ('shapeshift',    [['pD', 'pD'], ['pD', 'pA'], ['pA', 'pB']]),     # sub 8060
    ('guard',         [['iA', 'iF'], ['D', 'G']]),                     # line 1430
    ('guardian',      [['pA', 'pB'], ['g15', 'U']]),                   # line 3010
    ('ball',          [['U']]),
    ('platform',      [['pS', 'pT']]),
    ('box',           [['pO', 'pP'], ['pQ', 'pR']]),                   # line 2020
]
def auto_mask(ink, h):
    """1 = see-through: outside the figure (flood from the edges), minus a 1 px halo"""
    out = [[0] * 16 for _ in range(h)]
    st = [(x, y) for x in range(16) for y in (0, h - 1)] + [(x, y) for y in range(h) for x in (0, 15)]
    while st:
        x, y = st.pop()
        if 0 <= x < 16 and 0 <= y < h and not out[y][x] and not ink[y][x]:
            out[y][x] = 1; st += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    m = [[0] * 16 for _ in range(h)]
    for y in range(h):
        for x in range(16):
            keep = out[y][x]
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    if 0 <= x + dx < 16 and 0 <= y + dy < h and ink[y + dy][x + dx]: keep = 0
            m[y][x] = keep
    return m

img = [[K] * 128 for _ in range(24 * ((len(SPRITES) + 7) // 8))]
for n, (name, rows) in enumerate(SPRITES):
    ink = [[0] * 16 for _ in range(24)]
    h = 8 * len(rows)
    for r, row in enumerate(rows):
        for c, ch in enumerate(row):
            inv = ch[0] in 'ip'
            code = 0x80 + int(ch[1:]) if ch.startswith('g') else 0x90 + ord(ch[-1]) - 65
            g = glyph(code)
            for y in range(8):
                b = g[y] ^ (255 if inv else 0)
                for x in range(8):
                    ink[r * 8 + y][c * 8 + x] = b >> (7 - x) & 1
    m = auto_mask(ink, h)
    ox, oy = (n % 8) * 16, (n // 8) * 24
    for y in range(h):
        for x in range(16):
            img[oy + y][ox + x] = W if ink[y][x] else (K if m[y][x] else R)
png.write(os.path.join(G, 'sprites.png'), 128, len(img), img)

# -------------------------------------------------------------- backdrops
def write_screen(path, cells=None, raw=None):
    """cells: Screen.cell grid; raw: 6912 bytes of a .scr"""
    img = [[K] * 256 for _ in range(192)]
    for r in range(24):
        for c in range(32):
            if raw:
                at = raw[6144 + r * 32 + c]
                g = [raw[(r & 0x18) << 8 | y << 8 | (r & 7) << 5 | c] for y in range(8)]
            else:
                ch, at, inv = cells[r][c]
                g = [b ^ (255 if inv else 0) for b in glyph(ch)]
            p = PALB if at & 64 else PAL
            for y in range(8):
                for x in range(8):
                    img[r * 8 + y][c * 8 + x] = p[at & 7] if g[y] >> (7 - x) & 1 else p[at >> 3 & 7]
    png.write(path, 256, 192, img)

write_screen(os.path.join(G, 'backdrops', 'castle.png'), raw=BACK)
s = Screen(); s.run([9080, 9090, 9091, 9092])
write_screen(os.path.join(G, 'backdrops', 'cave.png'), s.cell)
s = Screen(); s.run([9240, 9310, 9311])
write_screen(os.path.join(G, 'backdrops', 'bridge.png'), s.cell)

# ------------------------------------------------------------------ rooms
ROOMS = [
    ('1.1', 'castle', [10]),
    ('1.2', 'castle', [110, 120, 130, 135]),
    ('1.3', 'castle', [171, 180, 190]),
    ('1.4', 'castle', [240, 250]),
    ('1.5', 'castle', []),
    ('1.6', 'castle', [550, 560, 561, 562, 563, 610, 620]),
    ('1.S', 'castle', [2010, 2020, 2030]),
    ('1.7', 'castle', [660, 670, 680]),
    ('1.8', 'castle', [750, 760]),
    ('card2', 'black', [830]),
    ('2.1', 'castle', [940, 950]),
    ('2.2', 'castle', [940, 951]),
    ('card3', 'black', [1100, 1110]),
    ('3.1', 'cave', [1210, 1211, 1212]),
    ('3.2', 'cave', [1210, 1211, 1214]),
    ('3.3', 'cave', [1340]),
    ('card4', 'black', [1470, 1480]),
    ('4.1', 'bridge', []),
    ('end', 'black', [5010]),
]
KEEP = re.compile(r'(PRINT|FOR |NEXT |LET [A-Z]\$=)')
out = []
for name, back, nums in ROOMS:
    out.append('room %s %s' % (name, back))
    for n in nums:
        st = [s for s in listing()[n].split(': ') if KEEP.match(s)]
        if n == 5010:
            st = st[:1]
        if st:
            out.append('  ' + ': '.join(st) + '        # %d' % n)
    out.append('')
RP = os.path.join(ROOT, 'orig', 'rooms-print.txt')
if os.path.exists(RP):
    print('orig/rooms-print.txt exists: not overwritten')
else:
    open(RP, 'w').write('\n'.join(out))
print('done')
