#!/usr/bin/env python3
"""One-off: make gfx/ from the original game (orig/CHEM2.Z80).

You don't need this to build or develop the game: gfx/ is the source now.
It's kept to document where every graphic came from. It needs SkoolKit
(https://skoolkit.ca, set SKOOLKIT=/path/to/skoolkit) for grab.py, which
runs the original BASIC in an emulator and saves its screens.

  gfx/font.png             the ROM font, characters 32-127
  gfx/tiles.png            characters 128-255: the Spectrum's block graphics
                           (128-143), the game's UDGs A-U (144-164), letters
                           with accents and some new tiles (165-...)
  gfx/sprites.png          sprite frames made from the UDGs
  gfx/screens/NN-name.png  the static screens, cleaned up (see CLEAN below)
"""
import os, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import png

SK = os.environ.get('SKOOLKIT', '/home/claude/tools/skoolkit')
G = os.path.join(ROOT, 'gfx')
GRAB = os.path.join(ROOT, 'build', 'grab')
os.makedirs(os.path.join(G, 'screens'), exist_ok=True)

PAL = [(0, 0, 0), (0, 0, 0xD7), (0xD7, 0, 0), (0xD7, 0, 0xD7),
       (0, 0xD7, 0), (0, 0xD7, 0xD7), (0xD7, 0xD7, 0), (0xD7, 0xD7, 0xD7)]
PALB = [(0, 0, 0), (0, 0, 0xFF), (0xFF, 0, 0), (0xFF, 0, 0xFF),
        (0, 0xFF, 0), (0, 0xFF, 0xFF), (0xFF, 0xFF, 0), (0xFF, 0xFF, 0xFF)]

# ------------------------------------------------------------------ data
sys.path.insert(0, SK)
from skoolkit.snapshot import Snapshot
rom = open(os.path.join(SK, 'skoolkit', 'resources', '48.rom'), 'rb').read()
snap = Snapshot.get(os.path.join(ROOT, 'orig', 'CHEM2.Z80'))
mem = [0] * 16384 + list(snap.ram())
udg = mem[65368:65368 + 21 * 8]            # UDG A..U (UDG = 65368)

glyph = {}                                 # code -> 8 bytes
for c in range(32, 128):
    glyph[c] = list(rom[0x3D00 + (c - 32) * 8:0x3D00 + (c - 31) * 8])
for n in range(16):                        # block graphics, as the ROM draws them
    t = (0xF0 if n & 2 else 0) | (0x0F if n & 1 else 0)
    b = (0xF0 if n & 8 else 0) | (0x0F if n & 4 else 0)
    glyph[128 + n] = [t] * 4 + [b] * 4
for i in range(21):
    glyph[144 + i] = udg[i * 8:i * 8 + 8]

def accent(base, top):
    g = list(glyph[ord(base)]); g[0] |= top[0]; g[1] |= top[1]; return g
NEW = [  # 165.. : what the original faked with CHR$ 8 + "'" and with [ ] |
    accent('a', (0x08, 0x10)), accent('e', (0x08, 0x10)), accent('i', (0x08, 0x10)),
    accent('o', (0x08, 0x10)), accent('u', (0x08, 0x10)), accent('n', (0x34, 0x58)),
    [0x00, 0x10, 0x00, 0x10, 0x10, 0x10, 0x10, 0x00],      # 171 ¡
    [0x00, 0x10, 0x00, 0x10, 0x20, 0x42, 0x3C, 0x00],      # 172 ¿
    # 173-175: a wave (the original drew them with PLOT/DRAW, which tiles
    # can't do): rising, top, falling
    [0x00, 0x00, 0x00, 0x00, 0x01, 0x06, 0x18, 0x60],
    [0x00, 0x00, 0x00, 0x00, 0x00, 0xC3, 0x3C, 0x00],
    [0x00, 0x00, 0x00, 0x00, 0x80, 0x60, 0x18, 0x06],
]
for i, g in enumerate(NEW):
    glyph[165 + i] = g

def cells_png(path, codes, cols, ink=(255, 255, 255), paper=(0, 0, 0)):
    rows = (len(codes) + cols - 1) // cols
    img = [[paper] * (cols * 8) for _ in range(rows * 8)]
    for i, c in enumerate(codes):
        if c is None: continue
        for y in range(8):
            for x in range(8):
                if glyph[c][y] & (128 >> x):
                    img[(i // cols) * 8 + y][(i % cols) * 8 + x] = ink
    png.write(path, cols * 8, rows * 8, img)

cells_png(os.path.join(G, 'font.png'), list(range(32, 128)), 16)
cells_png(os.path.join(G, 'tiles.png'), [c if c in glyph else None for c in range(128, 256)], 16)

# ------------------------------------------------------------------ sprites
# 16x16 frames, 8 per row: white = ink, red = mask (solid paper), black = clear
INK, MASK, CLEAR = (255, 255, 255), (255, 0, 0), (0, 0, 0)
U = udg[20 * 8:21 * 8]                      # the little man (UDG U)
def mirror(rows): return [int('{:08b}'.format(b)[::-1], 2) for b in rows]
frames = {
    0: U,                                                # player, facing right
    1: U[:5] + [0x30, 0x30, 0x38],                       # right, step
    2: mirror(U), 3: mirror(U[:5] + [0x30, 0x30, 0x38]), # left, left step
    4: [0x30, 0x30, 0xB4, 0x78, 0x30, 0x28, 0x48, 0x48], # climbing
    5: [0x30, 0x30, 0x78, 0xB4, 0x30, 0x50, 0x48, 0x90],
    6: [0x00, 0x48, 0x30, 0xFC, 0x30, 0x48, 0x84, 0x00], # dead
    9: udg[17 * 8:18 * 8],                               # the Ghost "a secas" (R)
    10: udg[17 * 8:17 * 8 + 7] + [0xDB],                 #   wobbling
    11: [0xFF] * 4 + [0] * 4,                           # moving platform (CHR$ 131)
    12: udg[19 * 8:20 * 8],                              # falling rock (T)
}
logo = [(udg[0 * 8 + y] << 8) | udg[1 * 8 + y] for y in range(8)] + \
       [(udg[2 * 8 + y] << 8) | udg[3 * 8 + y] for y in range(8)]
def mask_of(rows16):
    """1 = solid paper: the inside of the figure plus a 1 px halo"""
    ink = [[(rows16[y] >> (15 - x)) & 1 for x in range(16)] for y in range(16)]
    out = [[0] * 16 for _ in range(16)]
    st = [(x, y) for x in range(16) for y in (0, 15)] + [(x, y) for y in range(16) for x in (0, 15)]
    while st:
        x, y = st.pop()
        if 0 <= x < 16 and 0 <= y < 16 and not out[y][x] and not ink[y][x]:
            out[y][x] = 1; st += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    return [[0 if not out[y][x] else (0 if any(0 <= x + dx < 16 and 0 <= y + dy < 16 and ink[y + dy][x + dx]
             for dx in (-1, 0, 1) for dy in (-1, 0, 1)) else 1) for x in range(16)] for y in range(16)]
img = [[CLEAR] * 128 for _ in range(32)]
def put_frame(f, rows16):
    m = mask_of(rows16)
    for y in range(16):
        for x in range(16):
            px = INK if (rows16[y] >> (15 - x)) & 1 else (CLEAR if m[y][x] else MASK)
            img[(f // 8) * 16 + y][(f % 8) * 16 + x] = px
for f, r8 in frames.items():
    put_frame(f, [b << 8 for b in r8] + [0] * 8)
put_frame(8, logo)                                       # Ghost-tipo (car, cursor)
png.write(os.path.join(G, 'sprites.png'), 128, 32, img)

# ------------------------------------------------------------------ screens
if not os.environ.get('NOGRAB'):
    subprocess.run([sys.executable, os.path.join(HERE, 'grab.py'), os.path.join(HERE, 'grab.txt'), GRAB], check=True)

def scr_cell(d, r, c):
    return [d[((r & 0x18) << 8) | ((r & 7) << 5) | c | (y << 8)] for y in range(8)]

SCREENS = ['street', 'shop', 'logo', 'city', 'greg1', 'greg2', 'greg3']
# cells to blank (row, col0, col1) or to set (row, col, code): leftovers of
# the moment the screen was saved, and things the game now prints itself
CLEAN = {
    'street': [(22, 0, 31), (23, 0, 31)],                # bottom texts: printed by the game
    'shop': [(1, 18, 19), (9, 2, '-'), (9, 3, '-')],     # Prueba header; cursor damage
    'city': [(10, 0, 31)],                               # "Se hace de noche": typed by the game
    'greg3': [(1, 0, 31)],                               # leftovers of screen 2
}
for i, name in enumerate(SCREENS):
    d = list(open(os.path.join(GRAB, name + '.scr'), 'rb').read())
    for cl in CLEAN.get(name, []):
        r = cl[0]
        if isinstance(cl[2], str):
            g = glyph[ord(cl[2])]
            for y in range(8): d[((r & 0x18) << 8) | ((r & 7) << 5) | cl[1] | (y << 8)] = g[y]
            d[6144 + r * 32 + cl[1]] = d[6144 + r * 32 + cl[1] + 2]   # colours of the text next to it
            continue
        for c in range(cl[1], cl[2] + 1):
            for y in range(8): d[((r & 0x18) << 8) | ((r & 7) << 5) | c | (y << 8)] = 0
            d[6144 + r * 32 + c] = d[6144 + r * 32 + c] & 0xF8 | ((d[6144 + r * 32 + c] >> 3) & 7)
    img = [[None] * 256 for _ in range(192)]
    for r in range(24):
        for c in range(32):
            a = d[6144 + r * 32 + c]; pal = PALB if a & 64 else PAL
            g = scr_cell(d, r, c)
            for y in range(8):
                for x in range(8):
                    img[r * 8 + y][c * 8 + x] = pal[a & 7] if g[y] & (128 >> x) else pal[(a >> 3) & 7]
    png.write(os.path.join(G, 'screens', '%02d-%s.png' % (i + 1, name)), 256, 192, img)
print('gfx/ written')
