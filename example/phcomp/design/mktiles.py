#!/usr/bin/env python3
"""One-off: assemble the compilation's common tileset (gfx/tiles.png,
256x64, 32 tiles a row, white = ink) from cells of the originals'
screenshots and a few hand-drawn tiles. Writes design/tilenames.txt."""
import os, sys
from PIL import Image
OUT = '/home/claude/work/splib2-bitmapbg/example/phcomp'
SC = '/home/claude/work/phc/scratch/'
rom = open('/home/claude/tools/skoolkit/skoolkit/resources/48.rom', 'rb').read()
bru = open('/home/claude/work/phc/orig/brujas.mem', 'rb').read()
tiles = []          # (name, 8 bytes)
def add(name, bm):
    assert len(bm) == 8, name
    tiles.append((name, list(bm)))
# --- font: 32..95 (ROM for 32-47, Brujas' bold font for 48-95)
for ch in range(32, 96):
    src = rom[0x3D00 + (ch - 32) * 8:][:8] if ch < 48 else bru[0xFC58 + (ch - 32) * 8:][:8]
    add('ch%d' % ch, src)
assert len(tiles) == 64
ims = {}
def grab(name, png, c, r, ink=None, inv=False):
    if png not in ims: ims[png] = Image.open(SC + png).convert('RGB')
    px = ims[png].load()
    cols = {}
    for y in range(8):
        for x in range(8):
            p = px[c*8+x, r*8+y]; cols[p] = cols.get(p, 0) + 1
    if ink is None:
        ink = min(cols, key=lambda k: cols[k]) if len(cols) > 1 else None
    bm = [sum(128 >> x for x in range(8) if px[c*8+x, r*8+y] == ink) for y in range(8)]
    if inv: bm = [b ^ 255 for b in bm]
    add(name, bm)
def art(name, rows):
    rows = rows.split()
    assert len(rows) == 8 and all(len(r) == 8 for r in rows), name
    add(name, [sum(128 >> x for x in range(8) if r[x] == '#') for r in rows])
K = (0, 0, 0)
# --- Brujas
grab('brick',     'bt-04.png', 20, 18, K)
grab('grass_t',   'bt-04.png', 5, 20, K)
grab('grass_b',   'bt-04.png', 5, 21, K)
grab('scallop',   'bt-00.png', 3, 2, (0, 0xd7, 0xd7))
grab('dot',       'bt-00.png', 2, 7, (0, 0xd7, 0))
grab('lamp_tl',   'bt-06.png', 22, 15, (0xd7, 0xd7, 0))
grab('lamp_tr',   'bt-06.png', 23, 15, (0xd7, 0xd7, 0))
grab('lamp_bl',   'bt-06.png', 22, 16, (0xd7, 0xd7, 0))
grab('lamp_br',   'bt-06.png', 23, 16, (0xd7, 0xd7, 0))
grab('pole_l',    'bt-06.png', 10, 12, (0, 0, 0xd7))
grab('pole_r',    'bt-06.png', 11, 12, (0, 0, 0xd7))
grab('base_tl',   'bt-06.png', 10, 18, (0, 0, 0xd7))
grab('base_tr',   'bt-06.png', 11, 18, (0, 0, 0xd7))
grab('base_bl',   'bt-06.png', 10, 19, (0, 0, 0xd7))
grab('base_br',   'bt-06.png', 11, 19, (0, 0, 0xd7))
grab('broom_h',   'bt-07.png', 26, 19, (0xd7, 0xd7, 0))
grab('broom_s',   'bt-07.png', 27, 19, (0xd7, 0, 0))
art('water_t', '........ ..##.... .#..#..# #....##. ######## ######## ######## ########')
art('water_b', '######## ######## ###..### ##.##.## ######## ######## #..##### .##.####')
art('water_t2', '........ .....##. #..#..#. .##....# ######## ######## ######## ########')
# --- Luna
grab('lgrass_t',  'lt-00.png', 5, 20, K)
grab('lgrass_b',  'lt-00.png', 5, 21, K)
grab('lsky',      'lj-0790.png', 0, 3, (0, 0, 0xd7))
grab('block_t',   'lj-0790.png', 7, 14, (0, 0xd7, 0xd7), True)
grab('block_b',   'lj-0790.png', 7, 15, (0, 0xd7, 0xd7), True)
grab('tower_l',   'lj-0790.png', 23, 17)
grab('tower_r',   'lj-0790.png', 24, 17)
grab('moon',      'lj-1340.png', 28, 12, K)
# --- V
grab('rock_tl',   'vj-0250.png', 0, 20, K)
grab('rock_tr',   'vj-0250.png', 1, 20, K)
grab('rock_bl',   'vj-0250.png', 0, 21, K)
grab('rock_br',   'vj-0250.png', 1, 21, K)
for i in range(4): grab('big_t%d' % i, 'vj-1000.png', i, 13, K)
for i in range(4): grab('big_b%d' % i, 'vj-1000.png', i, 14, K)
grab('rbrick',    'vj-0250.png', 15, 11)
grab('lava',      'vj-0250.png', 17, 21)
art('lava2', '........ .#...#.. ###.###. ######## ######## ######## ######## ########')
grab('dither',    'vj-0250.png', 0, 0)
grab('mnt_pk_l',  'vj-1000.png', 3, 0)
grab('mnt_pk_r',  'vj-1000.png', 4, 0)
grab('mnt_l',     'vj-1000.png', 2, 1)
grab('mnt_r',     'vj-1000.png', 5, 1)
grab('vground',   'vj-1000.png', 5, 20)
grab('vsoil',     'vj-1000.png', 1, 21)
# --- Infinito
grab('tech',      'ij-0100.png', 0, 16)
art('flower', '........ ..#..... .#.#.... ..#...#. ..#..#.# .#.#..#. #..#..#. ..#.#..#')
# --- generic, hand drawn
art('spikes',  '........ ...#...# ..##..## ..##..## .###.### .###.### ######## ########')
art('ladder',  '#......# ######## #......# #......# #......# ######## #......# #......#')
art('plat',    '######## #.#.#.#. .#.#.#.# ######## ........ ........ ........ ........')
art('girder',  '######## #..##..# .#.#.#.. ..#..#.. .#.#.#.. #..##..# ######## ........')
art('stone',   '.######. #......# #.#....# #......# #....#.# #......# #......# .######.')
art('wall',    '######## #...#... #...#... ######## ..#...#. ..#...#. ######## #...#...')
art('crenel',  '##....## ##....## ######## ######## #...#... ######## ..#...#. ########')
art('window',  '######## #..##..# #..##..# ######## ######## #..##..# #..##..# ########')
art('star',    '...#.... ...#.... ..###... ####### ..###... ..#.#... .#...#.. ........'.replace('#######', '#######.'))
art('cloud_l', '........ ....###. ..###### .####### ######## ######## .####### ........')
art('cloud_r', '........ ##...... ####.... ######.. ######## ######## #######. ........')
art('tree_tl', '.....### ...##### ..###### .####### .####### ######## ######## .#######')
art('tree_tr', '###..... #####... ######.. #######. #######. ######## ######## #######.')
art('tree_bl', '.####### ..###### ....#### .......# .......# .......# ......## .....###')
art('tree_br', '#######. ######.. ####.... #....... #....... #....... ##...... ###.....')
art('bush',    '..####.. .######. ######## ######## ######## .######. ..#..#.. ........')
art('pillar',  '.######. .#.##.#. .#.##.#. .#.##.#. .#.##.#. .#.##.#. .#.##.#. .######.')
art('chain',   '...##... ..#..#.. ..#..#.. ...##... ...##... ..#..#.. ..#..#.. ...##...')
art('bars',    '#.#.#.#. #.#.#.#. #.#.#.#. #.#.#.#. #.#.#.#. #.#.#.#. #.#.#.#. ########')
art('hatch',   '#...#... .#...#.. ..#...#. ...#...# #...#... .#...#.. ..#...#. ...#...#')
art('crater',  '........ ..####.. .#....#. #......# #......# .#....#. ..####.. ........')
art('rock_s',  '........ ...##... ..####.. .##.###. .######. ####.### ######## ########')
art('arrow_r', '........ ....#... ....##.. #######. #######. ....##.. ....#... ........')
art('heart',   '........ .##.##.. ####### ####### .#####.. ..###... ...#.... ........'.replace('####### ', '#######. '))
art('key',     '........ .###.... #...#### #...#.#. .###.... ........ ........ ........')
art('circuit', '#.#..#.. #.####.# #.#..#.# ####.### ..#..#.. ##.###.# .#.#.#.. ##.#.###')
art('screen',  '######## #......# #.#..#.# #......# #.####.# #......# ######## ...##...')
art('fence',   '.#...#.. ######## .#...#.. .#...#.. .#...#.. ######## .#...#.. .#...#..')
art('sand',    '#.#.#.#. .#.#.#.# #.#.#.#. .#.#.#.# #.#.#.#. .#.#.#.# #.#.#.#. .#.#.#.#')
art('solid',   '######## ######## ######## ######## ######## ######## ######## ########')
art('half_t',  '######## ######## ######## ######## ........ ........ ........ ........')
art('half_b',  '........ ........ ........ ........ ######## ######## ######## ########')
art('slope_l', '.......# ......## .....### ....#### ...##### ..###### .####### ########')
art('slope_r', '#....... ##...... ###..... ####.... #####... ######.. #######. ########')
art('skull',   '..####.. .######. ##.##.## ##.##.## .######. ..#..#.. ..####.. ........')
art('bone',    '........ ##....## .######. ..####.. .######. ##....## ........ ........')
# --- the 16 block graphics (ZX 128-143)
for g in range(16):
    tl, tr, bl, br = g & 2, g & 1, g & 8, g & 4
    top = (0xF0 if tl else 0) | (0x0F if tr else 0)
    bot = (0xF0 if bl else 0) | (0x0F if br else 0)
    add('blk%d' % g, [top] * 4 + [bot] * 4)
# --- added later (keep the order: rooms use these numbers)
art('beam',    '...##... ....##.. ...##... ..##.... ...##... ....##.. ...##... ..##....')
art('rail',    '######## ........ #......# ######## ........ ........ ........ ........')
art('moonrock','..####.. .#.##.#. ###..### #.####.# ###..### .######. ..#..#.. ........')
art('cable',   '...#.... ...#.... ...#.... ...#.... ...#.... ...#.... ...#.... ...#....')
art('panel_l', '######## #....... #.##.##. #....... #.###... #....... #.#.#.#. ########')
art('panel_r', '######## .......# .##.##.# .......# ...###.# .......# .#.#.#.# ########')
art('flame',   '...#.... ..##.... .###.#.. .#####.. ######.. #.####.# ######## .######.')
art('gem',     '........ ..####.. .#..#.#. #..#..## .#..#.#. ..#.##.. ...##... ........')
inf1 = open('/home/claude/work/phc/orig/inf1.mem', 'rb').read()
add('brick_m', inf1[0xFF58 + 12 * 8:][:8])      # Infinito's UDG M: left half
add('brick_n', inf1[0xFF58 + 13 * 8:][:8])      # and N: right half (zigzag)
print(len(tiles), 'tiles')
assert len(tiles) <= 256
im = Image.new('RGB', (256, 64))
px = im.load()
for i, (n, bm) in enumerate(tiles):
    for y in range(8):
        for x in range(8):
            if bm[y] & (128 >> x):
                px[(i % 32) * 8 + x, (i // 32) * 8 + y] = (255, 255, 255)
im.save(OUT + '/gfx/tiles.png')
with open(OUT + '/design/tilenames.txt', 'w') as f:
    for i, (n, bm) in enumerate(tiles):
        f.write('%d %s\n' % (i, n))
big = im.resize((256 * 3, 64 * 3), Image.NEAREST); big.save('/home/claude/work/phc/look/tiles3.png')
