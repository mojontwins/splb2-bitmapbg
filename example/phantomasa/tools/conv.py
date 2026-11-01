#!/usr/bin/env python3
"""Phantomasa asset converter.

Everything you edit while developing the game is a plain file:

  gfx/screens/NN-name.png  every screen (256x192, Spectrum colours)
  gfx/font.png             the font (128x48: characters 32-127, 16 per row)
  gfx/sprites.png          the sprites (96x16: six 16x16 frames)
  gfx/music.bin            the beeper music player and its songs (binary)
  rounds.txt               the rounds

This script turns them into:

  src/assets.h             data that goes in the main program
  build/bank1.bin, ...     the screens, ZX0-packed, for the 128K RAM banks
  build/high.bin           loaded at $F000 in bank 0: font ($F000),
                           sprite frames ($F300), music player ($F600)

Only the Python 3 standard library is needed, plus the zx0 compressor
(https://github.com/einar-saukas/ZX0) for screens that changed: packed
screens are cached in gfx/packed/, so an unchanged screen is never
recompressed and the game builds without zx0.
"""
import os, re, sys, subprocess, hashlib
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import png

G = os.path.join(ROOT, 'gfx')
B = os.path.join(ROOT, 'build')
os.makedirs(B, exist_ok=True)

def die(msg):
    sys.exit('conv.py: ' + msg)

# ============================================================== colours
PAL = [(0, 0, 0), (0, 0, 0xD7), (0xD7, 0, 0), (0xD7, 0, 0xD7),
       (0, 0xD7, 0), (0, 0xD7, 0xD7), (0xD7, 0xD7, 0), (0xD7, 0xD7, 0xD7)]
PALB = [(0, 0, 0), (0, 0, 0xFF), (0xFF, 0, 0), (0xFF, 0, 0xFF),
        (0, 0xFF, 0), (0, 0xFF, 0xFF), (0xFF, 0xFF, 0), (0xFF, 0xFF, 0xFF)]

_cache = {}
def zx_colour(rgb):
    """nearest Spectrum colour -> (index 0..7, bright 0/1, or None for black)"""
    rgb = tuple(rgb[:3])
    if rgb not in _cache:
        best = None
        for b, pal in ((0, PAL), (1, PALB)):
            for i, c in enumerate(pal):
                d = sum((x - y) ** 2 for x, y in zip(rgb, c))
                if best is None or d < best[0]:
                    best = (d, i, b)
        _cache[rgb] = (best[1], None if best[1] == 0 else best[2])
    return _cache[rgb]

# ============================================================== screens
#
# A PNG becomes a .scr (6144 bytes of bitmap + 768 attributes). For each
# 8x8 cell:
#   - if it looks exactly like that cell in gfx/screens/ref/NN-name.scr,
#     the original bytes and attribute are kept (so INK/PAPER choices that
#     can't be seen in a PNG, which matter for collisions, survive);
#   - otherwise: at most two colours, both normal or both BRIGHT (black
#     goes with anything); the higher palette index is INK, the lower is
#     PAPER; a one-colour cell is PAPER of that colour with INK 7 (so a
#     plain black cell is attribute 7: free space).
# The game reads collisions from the attributes: 7 (white on black) is
# free, 1 (blue on black) kills you when you stand on it, anything else
# is solid.

def scr_offset(y, cx):
    return ((y & 0xC0) << 5) | ((y & 7) << 8) | ((y & 0x38) << 2) | cx

def cell_colours(img, cy, cx):
    return [[zx_colour(img[cy * 8 + y][cx * 8 + x]) for x in range(8)] for y in range(8)]

def ref_cell_colours(scr, cy, cx):
    at = scr[6144 + cy * 32 + cx]
    ink, pap, br = at & 7, (at >> 3) & 7, (at >> 6) & 1
    out = []
    for y in range(8):
        b = scr[scr_offset(cy * 8 + y, cx)]
        row = []
        for x in range(8):
            c = ink if (b >> (7 - x)) & 1 else pap
            row.append((c, None if c == 0 else br))
        out.append(row)
    return out

def png_to_scr(path, ref):
    w, h, img = png.read(path)
    if (w, h) != (256, 192):
        die('%s must be 256x192, it is %dx%d' % (path, w, h))
    scr = bytearray(6912)
    for cy in range(24):
        for cx in range(32):
            cols = cell_colours(img, cy, cx)
            if ref and ref_cell_colours(ref, cy, cx) == cols:
                for y in range(8):
                    a = scr_offset(cy * 8 + y, cx)
                    scr[a] = ref[a]
                scr[6144 + cy * 32 + cx] = ref[6144 + cy * 32 + cx]
                continue
            idx = sorted({c[0] for row in cols for c in row})
            br = {c[1] for row in cols for c in row} - {None}
            where = '%s: cell row %d, column %d' % (os.path.basename(path), cy, cx)
            if len(idx) > 2: die(where + ' has more than two colours')
            if len(br) > 1: die(where + ' mixes BRIGHT and normal colours')
            if len(idx) == 1: pap, ink = idx[0], 7
            else: pap, ink = idx
            scr[6144 + cy * 32 + cx] = (64 if br == {1} else 0) | (pap << 3) | ink
            for y in range(8):
                v = 0
                for c in cols[y]:
                    v = (v << 1) | (len(idx) == 2 and c[0] == ink)
                scr[scr_offset(cy * 8 + y, cx)] = v
    return bytes(scr)

ZX0 = os.environ.get('ZX0', 'zx0')
PK = os.path.join(G, 'packed')
os.makedirs(PK, exist_ok=True)

def zx0_cached(name, data):
    """ZX0-compress data, reusing gfx/packed/name.zx0 when the input is the same"""
    fs, fz = os.path.join(PK, name + '.scr'), os.path.join(PK, name + '.zx0')
    if not (os.path.exists(fs) and os.path.exists(fz) and open(fs, 'rb').read() == data):
        open(fs, 'wb').write(data)
        try:
            subprocess.run([ZX0, '-f', fs, fz], check=True, capture_output=True)
        except OSError:
            die('%s changed and the zx0 compressor was not found '
                '(https://github.com/einar-saukas/ZX0, or set ZX0=/path/to/zx0)' % name)
    return open(fz, 'rb').read()

SD = os.path.join(G, 'screens')
names = sorted(f[:-4] for f in os.listdir(SD) if f.endswith('.png'))
screens = []          # (CONSTANT, packed)
for n in names:
    m = re.match(r'(\d+)-([A-Za-z0-9_]+)$', n)
    if not m: die('screen file names are NN-name.png: ' + n)
    reff = os.path.join(SD, 'ref', n + '.scr')
    ref = open(reff, 'rb').read() if os.path.exists(reff) else None
    scr = png_to_scr(os.path.join(SD, n + '.png'), ref)
    screens.append(('SCR_' + m.group(2).upper(), m.group(2), zx0_cached(n, scr)))

# RAM banks at $C000 on a 128K. Bank 0 is the normal one (shadow screen,
# splib2 tables, music); 2 and 5 are the fixed memory at $8000 and $4000.
# Screens go into 1, 3, 4, 6, 7 in order; each must fit whole in a bank.
BANKS = [1, 3, 4, 6, 7]
banks = {b: bytearray() for b in BANKS}
scr_bank, scr_addr = [], []
for const, name, z in screens:
    for b in BANKS:
        if len(banks[b]) + len(z) <= 16384:
            scr_bank.append(b); scr_addr.append(0xC000 + len(banks[b]))
            banks[b] += z
            break
    else:
        die('the screens don\'t fit in the 128K banks any more')
used_banks = [b for b in BANKS if banks[b]]
for b in used_banks:
    open(os.path.join(B, 'bank%d.bin' % b), 'wb').write(banks[b])

# ============================================================== font
w, h, img = png.read(os.path.join(G, 'font.png'))
if (w, h) != (128, 48): die('font.png must be 128x48 (16x6 characters of 8x8)')
font = []
for c in range(96):
    for y in range(8):
        v = 0
        for x in range(8):
            r, g, b_, a = img[(c // 16) * 8 + y][(c % 16) * 8 + x]
            v = (v << 1) | (a >= 128 and r + g + b_ >= 384)
        font.append(v)

# ============================================================== sprites
#
# gfx/sprites.png: 96x16, six 16x16 frames: player facing left, left
# step, right, right step, box, moving platform (top 8 lines). White =
# ink, black/transparent = see-through, red = solid paper (mask). A frame
# without red gets an automatic mask: the inside of the figure + a 1 px halo.
SPR_NAMES = ['player left', 'player left step', 'player right', 'player right step', 'box', 'platform']

def auto_mask(rows):
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
                    if 0 <= x + dx < 16 and 0 <= y + dy < 16 and ink[y + dy][x + dx]: keep = 0
            v = (v << 1) | keep
        m.append(v)
    return m

def classify(px):
    r, g, b_, a = px
    if a < 128: return 0
    if r > 160 and g < 100 and b_ < 100: return 2
    return 1 if r + g + b_ >= 384 else 0

w, h, img = png.read(os.path.join(G, 'sprites.png'))
if (w, h) != (16 * len(SPR_NAMES), 16):
    die('sprites.png must be %dx16: %s' % (16 * len(SPR_NAMES), ', '.join(SPR_NAMES)))
BL = [0, 255]
spr = BL * 8
for f in range(len(SPR_NAMES)):
    g_, mk, manual = [], [], False
    for y in range(16):
        gv = mv = 0
        for x in range(16):
            c = classify(img[y][f * 16 + x])
            gv = (gv << 1) | (c == 1); mv = (mv << 1) | (c == 0); manual |= c == 2
        g_.append(gv); mk.append(mv)
    m = mk if manual else auto_mask(g_)
    for sh in (8, 0):
        for y in range(16):
            spr += [(g_[y] >> sh) & 255, (m[y] >> sh) & 255]
        spr += BL * 8

# ============================================================== rounds
#
# Each round becomes a record (see ROUND_* in assets.h and rounds.txt):
#   0 stage  1 number  2 screen  3 start W  4 start A  5 exit W (255 any)
#   6 exit A  7 snot first row (signed)  8 snot speed (255 = no snot)
#   9 platform Y (255 none)  10 platform X  11 min  12 max  13 dir (1/255)
#   14 portal Y (255 none)  15 portal X  16 gate  17 door Y (255 none)
#   18 number of boxes, then Y X per box
#   then number of tile strings, then Y X attr length chars... per string
const_of = {name: i for i, (c, name, z) in enumerate(screens)}
rounds = []
cur = None
for ln, line in enumerate(open(os.path.join(ROOT, 'rounds.txt')), 1):
    line = line.split(';')[0].strip()
    if not line: continue
    where = 'rounds.txt line %d: ' % ln
    w_ = line.split()
    k = w_[0].lower()
    def yx(s):
        a, b_ = s.split(','); return int(a), int(b_)
    try:
        if k == 'round':
            kv = dict(p.split('=') for p in w_[3:])
            if kv['screen'] not in const_of: die(where + 'no screen called ' + kv['screen'])
            sw, sa = yx(kv['start'])
            ew, ea = kv['exit'].split(',')
            sn = (255, 255)
            if 'snot' in kv:
                a, b_ = yx(kv['snot']); sn = (a & 255, b_)
            cur = dict(head=[int(w_[1]), int(w_[2]), const_of[kv['screen']], sw, sa,
                             255 if ew == '*' else int(ew), int(ea), sn[0], sn[1]],
                       platform=[255, 0, 0, 0, 0], portal=[255, 0], gate=0, door=255,
                       boxes=[], tiles=[])
            rounds.append(cur)
        elif cur is None:
            die(where + 'expected "round" first')
        elif k == 'box':
            cur['boxes'] += [yx(p) for p in w_[1:]]
        elif k == 'portal':
            cur['portal'] = list(yx(w_[1]))
        elif k == 'platform':
            y, x = yx(w_[1]); lo, hi = yx(w_[2])
            cur['platform'] = [y, x, lo, hi, int(w_[3]) & 255]
        elif k == 'gate':
            cur['gate'] = 1
        elif k == 'door':
            cur['door'] = int(w_[1])
        elif k == 'tiles':
            y, x = yx(w_[1])
            at = int(w_[2][1:], 16) if w_[2].startswith('$') else int(w_[2])
            cur['tiles'].append((y, x, at, w_[3]))
        else:
            die(where + 'unknown keyword ' + w_[0])
    except (KeyError, ValueError, IndexError) as e:
        die(where + 'bad line (%s)' % e)
rdata, roffs = [], []
for r in rounds:
    if len({y & 1 for y, x in r['boxes']}) > 1:
        die('round %d of stage %d: all its boxes must be on rows of the same parity'
            % (r['head'][1], r['head'][0]))
    roffs.append(len(rdata))
    rdata += r['head'] + r['platform'] + r['portal'] + [r['gate'], r['door'], len(r['boxes'])]
    for y, x in r['boxes']: rdata += [y, x]
    rdata.append(len(r['tiles']))
    for y, x, at, t in r['tiles']:
        rdata += [y, x, at, len(t)] + [ord(c) for c in t]

# ============================================================== beeper
# BEEP d, n of the original: n = -12..63 semitones from middle C. For each
# n: delay count for half a period of the beep loop (26 T-states per count,
# ~80 T of overhead at 3.5 MHz), and the frequency / 10 (Hz, 16 bits) so the game
# can work out how many periods last d milliseconds.
beep = []
for n in range(-12, 64):
    f = 261.63 * 2 ** (n / 12)
    count = max(1, int((3500000 / (2 * f) - 80) / 26))
    f10 = max(1, round(f / 10))
    beep += [count & 255, count >> 8, f10 & 255, f10 >> 8]

# ============================================================== output
def asm_bytes(data, per=16):
    return '\n'.join('\tdefb ' + ','.join(str(b_) for b_ in data[i:i + per]) for i in range(0, len(data), per))

o = ['/* Generated by tools/conv.py - do not edit */', '']
for i, (c, n, z) in enumerate(screens):
    o.append('#define %-16s %d    /* %5d bytes packed, bank %d */' % (c, i, len(z), scr_bank[i]))
o += ['#define NSCREENS         %d' % len(screens),
      '#define NROUNDS          %d' % len(rounds),
      '#define SPR_FRAME_SIZE   96',
      '#define BEEP_MIN         (-12)', '',
      '#define FONT_ADDR        0xF000  /* chars 32-127, 8 bytes each (high.bin) */',
      '#define SPRITES_ADDR     0x%04X  /* six masked 16x16 frames (high.bin)    */' % (0xF300 + 16),
      'extern unsigned char  scr_bank[];      /* RAM bank of each screen         */',
      'extern unsigned int   scr_addr[];      /* its address in that bank        */',
      'extern unsigned char  rounds[];        /* round records (see conv.py)     */',
      'extern unsigned int   round_offs[];    /* offset of each round in rounds  */',
      'extern unsigned char  beep_tab[];      /* count (16), Hz/10 (16)          */',
      'extern unsigned char  load_table[];    /* bank, address, length; $FF end  */',
      '', '#asm']
o.append('._scr_bank\n' + asm_bytes(scr_bank))
o.append('._scr_addr\n' + '\n'.join('\tdefw ' + ','.join(str(v) for v in scr_addr[i:i + 8]) for i in range(0, len(scr_addr), 8)))
o.append('._round_offs\n' + '\n'.join('\tdefw ' + ','.join(str(v) for v in roffs[i:i + 8]) for i in range(0, len(roffs), 8)))
o.append('._rounds\n' + asm_bytes(rdata))
o.append('._beep_tab\n' + asm_bytes(beep, 16))
lt = [0, 0x00, 0xF0, (0xFC58 - 0xF000) & 255, (0xFC58 - 0xF000) >> 8]
for b_ in used_banks:
    lt += [b_, 0x00, 0xC0, len(banks[b_]) & 255, len(banks[b_]) >> 8]
o.append('._load_table\n' + asm_bytes(lt + [255], 5))
o.append('#endasm\n')
open(os.path.join(ROOT, 'src', 'assets.h'), 'w').write('\n'.join(o))

music = open(os.path.join(G, 'music.bin'), 'rb').read()
if len(music) != 0xFC58 - 0xF600: die('music.bin must be %d bytes ($F600-$FC57)' % (0xFC58 - 0xF600))
# bank 0, $F000-$FC57: font, sprites (with their 8 blank lines before),
# then the music player at its fixed address $F600
high = bytearray(0xFC58 - 0xF000)
high[0:768] = bytes(font)
if len(spr) > 0x300: die('too many sprite frames for $F300-$F5FF')
high[0x300:0x300 + len(spr)] = bytes(spr)
high[0x600:] = music
open(os.path.join(B, 'high.bin'), 'wb').write(high)
open(os.path.join(B, 'load_order.txt'), 'w').write(' '.join(['high.bin'] + ['bank%d.bin' % b_ for b_ in used_banks]) + '\n')
print('screens: %d (%d bytes packed) in banks %s; rounds: %d; sprites: %d bytes'
      % (len(screens), sum(len(z) for c, n, z in screens),
         ', '.join('%d (%d)' % (b_, len(banks[b_])) for b_ in used_banks), len(rounds), len(spr)))
