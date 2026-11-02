#!/usr/bin/env python3
"""Ghost Embustes 2 asset converter: gfx/ + texts.txt -> src/assets.h

Everything you are expected to edit while developing the game is a plain
file. Run ../build.sh after editing: this script runs first and rewrites
src/assets.h (never edit that one).

  gfx/font.png             128x48: characters 32-127 (16 per row), white on
                           black. The game prints all its text with it.
  gfx/tiles.png            128x64: characters 128-255 (16 per row). 128-143
                           are the block graphics, 144-164 the UDGs A-U,
                           165-175 accented letters, ¡ ¿ and the waves. The
                           game's code prints these by number (the T_* names
                           in src/ghostembustes2.c). Colours don't matter
                           here: the game gives each tile its colour.
  gfx/sprites.png          128 wide, rows of 16x16 frames, 8 per row.
                           White = ink, black = see-through, red = solid
                           paper (mask). A frame with no red gets an
                           automatic mask. Frame number = row * 8 + column.
  gfx/screens/NN-name.png  256x192 full screens in Spectrum colours, at most
                           two colours per 8x8 cell. NN orders them; the
                           game shows them as SCR_NAME. Each cell becomes a
                           character of the font or the tile set, or, if it
                           matches none, a new tile that this script adds.
  texts.txt                every text (format explained in the file).

Screens are packed with ZX0 (https://github.com/einar-saukas/ZX0). Packed
copies are cached in gfx/packed/, so the zx0 program is only needed when a
screen changes (set ZX0=/path/to/zx0 if it isn't in the PATH).
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

def zx_colour(rgb):
    """-> (index 0..7, bright 0/1/None). Nearest Spectrum colour; black is
    None-bright (valid with either)."""
    best = None
    for b, pal in ((0, PAL), (1, PALB)):
        for i, c in enumerate(pal):
            d = sum((x - y) ** 2 for x, y in zip(rgb[:3], c))
            if best is None or d < best[0]:
                best = (d, i, b)
    return best[1], (None if best[1] == 0 else best[2])

def lit(px):
    return px[3] >= 128 and sum(px[:3]) >= 384

# ============================================================ font and tiles
glyph = {}                                  # code -> tuple of 8 bytes

def read_cells(path, first, count, keep_empty):
    w, h, img = png.read(path)
    if w != 128 or h != count // 16 * 8:
        die('%s must be 128x%d, it is %dx%d' % (path, count // 16 * 8, w, h))
    for i in range(count):
        g = tuple(sum(128 >> x for x in range(8) if lit(img[(i // 16) * 8 + y][(i % 16) * 8 + x]))
                  for y in range(8))
        if any(g) or keep_empty(first + i):
            glyph[first + i] = g

read_cells(os.path.join(G, 'font.png'), 32, 96, lambda c: True)
read_cells(os.path.join(G, 'tiles.png'), 128, 128, lambda c: c == 128)

# ================================================================== screens
# Each screen: 768 tile numbers then 768 attributes, ZX0-packed.
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

by_pattern = {}
for c in sorted(glyph):                      # lowest code wins (font first)
    by_pattern.setdefault(glyph[c], c)
free_codes = [c for c in range(1, 32)] + [c for c in range(176, 256) if c not in glyph]
extra = []                                   # codes added for screen cells

SD = os.path.join(G, 'screens')
names = sorted(f[:-4] for f in os.listdir(SD) if f.endswith('.png'))
screens = []
for n in names:
    m = re.match(r'(\d+)-([A-Za-z0-9_]+)$', n)
    if not m:
        die('screen %s.png: the name must be NN-name.png' % n)
    w, h, img = png.read(os.path.join(SD, n + '.png'))
    if (w, h) != (256, 192):
        die('screen %s.png must be 256x192' % n)
    tiles, attrs = [], []
    for cy in range(24):
        for cx in range(32):
            cols = [[zx_colour(img[cy * 8 + y][cx * 8 + x]) for x in range(8)] for y in range(8)]
            idx = sorted({c[0] for row in cols for c in row})
            br = {c[1] for row in cols for c in row} - {None}
            where = 'screen %s.png, cell column %d row %d' % (n, cx, cy)
            if len(idx) > 2: die(where + ' has more than two colours')
            if len(br) > 1: die(where + ' mixes BRIGHT and normal colours')
            bright = 64 if br == {1} else 0
            if len(idx) == 1:                # plain cell: a space
                tiles.append(32)
                attrs.append(bright | (idx[0] << 3) | (0 if idx[0] >= 4 else 7))
                continue
            a, b = idx
            pa = tuple(sum(128 >> x for x in range(8) if cols[y][x][0] == a) for y in range(8))
            pb = tuple(sum(128 >> x for x in range(8) if cols[y][x][0] == b) for y in range(8))
            if pb in by_pattern:   code, ink, pap = by_pattern[pb], b, a
            elif pa in by_pattern: code, ink, pap = by_pattern[pa], a, b
            else:
                if not free_codes:
                    die(where + ': no free tile numbers left for new tiles')
                code = free_codes.pop(0); ink, pap = b, a
                glyph[code] = pb; by_pattern[pb] = code; extra.append(code)
            tiles.append(code)
            attrs.append(bright | (pap << 3) | ink)
    screens.append((m.group(2).upper(), zx0_cached(n, bytes(tiles + attrs))))

# ================================================================== sprites
SPR_PNG = os.path.join(G, 'sprites.png')

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

def classify(px):
    r, g, b, a = px
    if a < 128: return 0                           # transparent
    if r > 160 and g < 100 and b < 100: return 2   # red: mask
    return 1 if r + g + b >= 384 else 0            # light: ink

w, h, img = png.read(SPR_PNG)
if w != 128 or h % 16:
    die('%s must be 128 pixels wide and a multiple of 16 high' % SPR_PNG)
NFRAMES = h // 16 * 8
# splib2 sprite data, 2 columns per frame. Each column: 16 lines + 8 blank
# lines, 2 bytes per line (graphic, mask), and 8 blank lines before the
# first frame. Empty frames all point at one blank frame.
BL = [0, 255]
spr = BL * 8
frame_offset = []
blank_offset = None
for f in range(NFRAMES):
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
            spr += BL * 48
        frame_offset.append(blank_offset)
        continue
    m = mk if manual else auto_mask(g)
    frame_offset.append(len(spr) - 16)
    for sh in (8, 0):
        for y in range(16):
            spr += [(g[y] >> sh) & 255, (m[y] >> sh) & 255]
        spr += BL * 8

# ==================================================================== texts
UNI = {'©': 127, '↑': 94, 'á': 165, 'é': 166, 'í': 167, 'ó': 168, 'ú': 169,
       'ñ': 170, '¡': 171, '¿': 172, '£': 96}
for n, ch in enumerate('▝▘▀▗▐▚▜▖▞▌▛▄▟▙█'):      # (128, blank, is a space)
    UNI[ch] = 129 + n
NL = 13

def encode(s, where):
    out = []
    i = 0
    while i < len(s):
        ch = s[i]
        if ch == '{':
            j = s.index('}', i)
            tag = s[i + 1:j]
            if tag == 'nl': out.append(NL)
            elif len(tag) == 1 and 'A' <= tag <= 'U': out.append(144 + ord(tag) - 65)
            else: die('%s: unknown {%s}' % (where, tag))
            i = j + 1
            continue
        if ch in UNI: out.append(UNI[ch])
        elif 32 <= ord(ch) < 127: out.append(ord(ch))
        else: die('%s: character %r is not in the font' % (where, ch))
        i += 1
    return out

texts = []                                   # (name, bytes)
cur = None
for ln, line in enumerate(open(os.path.join(ROOT, 'texts.txt'), encoding='utf-8'), 1):
    line = line.rstrip('\r\n')
    if line.startswith('#') or not line.strip():
        continue
    if line.startswith('@'):
        cur = [line[1:].strip(), [], False]
        if not re.match(r'[a-z0-9_]+$', cur[0]): die('texts.txt line %d: bad name' % ln)
        texts.append(cur)
        continue
    if cur is None: die('texts.txt line %d: text before the first @name' % ln)
    cont = line.endswith('\\')
    if cont: line = line[:-1]
    if cur[1] and not cur[2]: cur[1].append(NL)
    cur[1] += encode(line, 'texts.txt line %d' % ln)
    cur[2] = cont

# =================================================================== beeper
# BEEP d, n of the original: n semitones from middle C. For each n in
# BEEP_MIN..63: the delay count for half a period of the beeper loop in
# the game (26 T-states per count, ~70 T overhead) and the frequency in Hz.
BEEP_MIN = -24
beep = []
for n in range(BEEP_MIN, 64):
    f = 261.63 * 2 ** (n / 12)
    half = 3500000 / (2 * f)
    count = max(1, int((half - 70) / 26))
    hz = int(round(f))
    beep += [count & 255, count >> 8, hz & 255, hz >> 8]

# =================================================================== output
def asm_bytes(data, per=16):
    return '\n'.join('\tdefb ' + ','.join(str(b) for b in data[i:i + per]) for i in range(0, len(data), per))

codes = sorted(glyph)
o = ['/* Generated by tools/conv.py from gfx/ and texts.txt - do not edit */', '',
     '#define NTILECODES   %d' % len(codes),
     '#define NFRAMES      %d' % NFRAMES,
     '#define BEEP_MIN     %d' % BEEP_MIN, '']
for i, (name, _) in enumerate(screens):
    o.append('#define SCR_%-10s %d' % (name, i))
o += ['', 'extern unsigned char tile_code[];       /* NTILECODES character numbers */',
      'extern unsigned char tile_gfx[];        /* 8 bytes for each of them   */',
      'extern unsigned int  scr_addr[];        /* packed screens             */',
      'extern unsigned char sprite_frames[];   /* splib2 masked frame data   */',
      'extern unsigned int  frame_offsets[];   /* NFRAMES offsets into it    */',
      'extern unsigned char beep_tab[];        /* count lo/hi, Hz lo/hi      */']
for name, _, _ in texts:
    o.append('extern unsigned char t_%s[];' % name)
o += ['', '#asm']
o.append('._tile_code\n' + asm_bytes(codes))
o.append('._tile_gfx\n' + asm_bytes([b for c in codes for b in glyph[c]]))
o.append('._scr_addr\n\tdefw ' + ','.join('_scr_%d' % i for i in range(len(screens))))
for i, (name, data) in enumerate(screens):
    o.append('._scr_%d\t\t; %s\n' % (i, name) + asm_bytes(data))
o.append('._frame_offsets\n' + '\n'.join('\tdefw ' + ','.join(str(v) for v in frame_offset[i:i + 8]) for i in range(0, NFRAMES, 8)))
o.append('._sprite_frames_pad\n' + asm_bytes(spr[:16]))
o.append('._sprite_frames\n' + asm_bytes(spr[16:]))
o.append('._beep_tab\n' + asm_bytes(beep))
for name, data, _ in texts:
    o.append('._t_%s\n' % name + asm_bytes(data + [0]))
o.append('#endasm\n')
open(os.path.join(ROOT, 'src', 'assets.h'), 'w').write('\n'.join(o))
print('assets: %d screens (%d bytes packed), %d tiles (%d new from screens: %s), '
      '%d sprite bytes, %d texts (%d bytes)'
      % (len(screens), sum(len(d) for _, d in screens), len(codes), len(extra),
         ','.join(map(str, extra)) or '-', len(spr), len(texts), sum(len(d) + 1 for _, d, _ in texts)))
