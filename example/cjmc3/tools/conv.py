#!/usr/bin/env python3
"""CJMC3 asset converter -> src/assets.h (z88dk #asm data).

Inputs (gfx/): sprites.png (the sprite sheet, see below), ZX0-compressed thirds/logo/ending (*.zx0, made from the raw
*.bin with `zx0 -f`), tile sets (gfxchars.bin, udg.bin, chars.bin), the
boniato table, the backdrop/thirds table and the beeper effects; and
orig/sources.bas, whose screen-drawing subroutines are interpreted by
layout.py to get every screen's tile map."""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from layout import load_lines, Interp, SCREEN_LINES

G = os.path.join(ROOT, 'gfx')
rd = lambda n: open(os.path.join(G, n), 'rb').read()

TILE_GFX, TILE_UDG, TILE_FONT = 0, 96, 117

def tile_of(code, font):
    if code >= 144: return TILE_UDG + code - 144
    return font + code - 32

# ------------------------------------------------------------ layout streams
# F0 r c: cursor   F1 a: attribute   F2 n t: n x tile   F3 n t1 t2: n x pair
# FF: end          t (< F0): tile at cursor, cursor advances (wrapping)
def encode(cells, font):
    out = []
    cur = None          # cursor (row, col) after the last emitted cell
    attr = None
    order = sorted(k for k in cells if k[0] < 24 and k[1] < 32)
    items = [(r, c, tile_of(cells[(r, c)][0], font), cells[(r, c)][1]) for r, c in order]
    i = 0
    while i < len(items):
        r, c, t, a = items[i]
        if cur != (r, c):
            out += [0xF0, r, c]
        if a != attr:
            out += [0xF1, a]; attr = a
        # contiguous run with the same attribute
        run = [items[i]]
        j = i + 1
        while j < len(items):
            pr, pc = run[-1][0], run[-1][1]
            nr, nc = (pr, pc + 1) if pc < 31 else (pr + 1, 0)
            if (items[j][0], items[j][1]) != (nr, nc) or items[j][3] != a: break
            run.append(items[j]); j += 1
        tiles = [x[2] for x in run]
        k = 0
        while k < len(tiles):
            n = 1
            while k + n < len(tiles) and tiles[k + n] == tiles[k] and n < 255: n += 1
            if n >= 4:
                out += [0xF2, n, tiles[k]]; k += n; continue
            p = 1
            while k + 2 * p + 1 < len(tiles) and tiles[k + 2 * p] == tiles[k] and tiles[k + 2 * p + 1] == tiles[k + 1] and p < 255: p += 1
            if p >= 3 and tiles[k] != tiles[k + 1]:
                out += [0xF3, p, tiles[k], tiles[k + 1]]; k += 2 * p; continue
            out.append(tiles[k]); k += 1
        lr, lc = run[-1][0], run[-1][1]
        cur = (lr, lc + 1) if lc < 31 else (lr + 1, 0)
        i = j
    out.append(0xFF)
    assert all(0 <= b < 256 for b in out)
    return out

L = load_lines(os.path.join(ROOT, 'orig', 'sources.bas'))
layouts = []
for np_ in range(12):
    it = Interp(L)
    for n in SCREEN_LINES[np_]:
        it.run_line(n)
    layouts.append(encode(it.cells, TILE_GFX))

# title texts (lines 6010-6020): text font, permanent INK 6 PAPER 0
it = Interp(L)
it.perm = {'i': 6, 'p': 0, 'b': 0, 'f': 0}
# the port no longer runs on fourSpriter: credit the engine it does run on
L[6015] = 'PRINT AT 19,4;"\\{i3}SPLIB2 BITMAPBG \\{i5}SHOWCASE"'
for n in (6010, 6015, 6020):
    it.run_line(n)
title = encode(it.cells, TILE_FONT)

# ------------------------------------------------------------ sprites
# gfx/sprites.png: 16x16 frames side by side, in this order:
#   0 player facing right, 1 player facing left, 2-4 the three enemies,
#   5 the boniato carried over the player's head (top 8 lines).
# Colours: black (or transparent) = see-through, white (any light colour)
# = ink, red = solid paper (the mask). A frame with no red at all gets an
# automatic mask: the inside of the figure plus a 1 px halo.
# If sprites.png doesn't exist it is created from the original UDGs.
import png
SPR_PNG = os.path.join(G, 'sprites.png')
SPR_NAMES = ['player right', 'player left', 'enemy 1', 'enemy 2', 'enemy 3', 'carried boniato']
INK, PAPER, MASK = (255, 255, 255), (0, 0, 0), (255, 0, 0)

def chr8(src, i): return src[i * 8:i * 8 + 8]
def bits16(tl, tr, bl, br):
    rows = []
    for top, bot in ((tl, tr), (bl, br)):
        for y in range(8):
            rows.append((top[y] << 8) | bot[y])
    return rows
def auto_mask(rows):
    """1 = see-through. Outside the figure (flood from the edges) minus a
    1 px halo around the ink."""
    ink = [[(rows[y] >> (15 - x)) & 1 for x in range(16)] for y in range(16)]
    out = [[0] * 16 for _ in range(16)]
    st = [(x, y) for x in range(16) for y in (0, 15)] + [(x, y) for y in range(16) for x in (0, 15)]
    while st:
        x, y = st.pop()
        if 0 <= x < 16 and 0 <= y < 16 and not out[y][x] and not ink[y][x]:
            out[y][x] = 1; st += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    m = []
    for y in range(16):
        v = 0
        for x in range(16):
            keep = out[y][x]
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < 16 and 0 <= yy < 16 and ink[yy][xx]: keep = 0
            v = (v << 1) | keep
        m.append(v)
    return m

if not os.path.exists(SPR_PNG):
    udg = rd('udg.bin'); gfx = rd('gfxchars.bin'); blank = bytes(8)
    fr = [bits16(*[chr8(udg, b + k) for k in range(4)]) for b in (0, 4, 8, 12, 16)]
    fr.append(bits16(chr8(gfx, ord('o') - 32), chr8(gfx, ord('p') - 32), blank, blank))
    img = [[PAPER] * (16 * len(fr)) for _ in range(16)]
    for n, g in enumerate(fr):
        m = auto_mask(g)
        for y in range(16):
            for x in range(16):
                if (g[y] >> (15 - x)) & 1: img[y][n * 16 + x] = INK
                elif not (m[y] >> (15 - x)) & 1: img[y][n * 16 + x] = MASK
    png.write(SPR_PNG, 16 * len(fr), 16, img)
    print('created', SPR_PNG)

def classify(px):
    r, g, b, a = px
    if a < 128: return 0                                  # transparent
    if r > 160 and g < 100 and b < 100: return 2          # red: mask
    return 1 if (r + g + b) >= 384 else 0                 # light: ink

w, h, img = png.read(SPR_PNG)
if h != 16 or w != 16 * len(SPR_NAMES):
    sys.exit('%s must be %dx16 (%d frames of 16x16: %s), it is %dx%d'
             % (SPR_PNG, 16 * len(SPR_NAMES), len(SPR_NAMES), ', '.join(SPR_NAMES), w, h))
BL = [0, 255]
spr = BL * 8
for n in range(len(SPR_NAMES)):
    g = []; mk = []; manual = False
    for y in range(16):
        gv = mv = 0
        for x in range(16):
            c = classify(img[y][n * 16 + x])
            gv = (gv << 1) | (c == 1)
            mv = (mv << 1) | (c == 0)
            manual |= c == 2
        g.append(gv); mk.append(mv)
    m = mk if manual else auto_mask(g)
    for sh in (8, 0):
        for y in range(16):
            spr += [(g[y] >> sh) & 255, (m[y] >> sh) & 255]
        spr += BL * 8
assert len(spr) == 16 + 96 * len(SPR_NAMES)

# ------------------------------------------------------------ output
def asm_bytes(data, per=16):
    return '\n'.join('\tdefb ' + ','.join(str(b) for b in data[i:i + per]) for i in range(0, len(data), per))

o = ['/* Generated by tools/conv.py - do not edit */',
     '#define TILE_GFX %d\n#define TILE_UDG %d\n#define TILE_FONT %d' % (TILE_GFX, TILE_UDG, TILE_FONT),
     '#define SPR_FRAME_SIZE 96',
     '#define LAYOUT_MAX %d' % max(len(l) for l in layouts),
     '#define SPR_PLAYER_R 0\n#define SPR_PLAYER_L 1\n#define SPR_ENEMY 2\n#define SPR_BONUS 5', '',
     'extern unsigned char tiles[];', 'extern unsigned char sprite_frames[];',
     'extern unsigned char backdrop_thirds[];', 'extern unsigned char boniatos_init[];',
     'extern unsigned char title_layout[];', 'extern unsigned char sfx_data[];', 'extern unsigned char sfx_step[];',
     'extern unsigned char zx0_logo[];', 'extern unsigned char zx0_ending[];']
for n in ('p1t1', 'p1t2', 'p1t3', 'p2t1', 'p2t2', 'p2t3'):
    o.append('extern unsigned char zx0_%s[];' % n)
for n in range(12):
    o.append('extern unsigned char layout_%d[];' % n)
o.append('\n#asm')
o.append('._tiles\n' + asm_bytes(list(rd('gfxchars.bin')) + list(rd('udg.bin')) + list(rd('chars.bin'))))
o.append('._sprite_frames_pad\n' + asm_bytes(spr[:16]))
o.append('._sprite_frames\n' + asm_bytes(spr[16:]))
o.append('._backdrop_thirds\n' + asm_bytes(list(rd('backdropthirds.bin'))))
o.append('._boniatos_init\n' + asm_bytes(list(rd('boniatos.bin')), 12))
o.append('._sfx_data\n' + asm_bytes(list(rd('sonidos.bin'))))
o.append('._sfx_step\n' + asm_bytes(list(rd('step.bin'))))
for n in ('p1t1', 'p1t2', 'p1t3', 'p2t1', 'p2t2', 'p2t3', 'logo', 'ending'):
    o.append('._zx0_%s\n' % n + asm_bytes(list(rd(n + '.zx0'))))
# screen layouts are ZX0-packed too (5.8K -> 2.1K); the game depacks the
# current one into layout_buf[LAYOUT_MAX]. Packed layouts are cached in
# gfx/layouts/, so zx0 is only needed when a layout changes.
import subprocess
ZX0 = os.environ.get('ZX0', 'zx0')
LD = os.path.join(G, 'layouts')
os.makedirs(LD, exist_ok=True)
for n, l in enumerate(layouts):
    fb, fz = os.path.join(LD, 'layout_%d.bin' % n), os.path.join(LD, 'layout_%d.zx0' % n)
    if not (os.path.exists(fb) and os.path.exists(fz) and open(fb, 'rb').read() == bytes(l)):
        open(fb, 'wb').write(bytes(l))
        try:
            subprocess.run([ZX0, '-f', fb, fz], check=True, capture_output=True)
        except OSError:
            sys.exit('layout %d changed and the zx0 compressor was not found '
                     '(https://github.com/einar-saukas/ZX0; or set ZX0=/path/to/zx0)' % n)
    o.append('._layout_%d\n' % n + asm_bytes(list(open(fz, 'rb').read())))
o.append('._title_layout\n' + asm_bytes(title))
o.append('#endasm\n')
open(os.path.join(ROOT, 'src', 'assets.h'), 'w').write('\n'.join(o))
print('layouts', sum(len(l) for l in layouts), 'bytes; title', len(title), '; sprites', len(spr))
