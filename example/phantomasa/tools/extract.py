#!/usr/bin/env python3
"""ONE-OFF: extract Phantomasa's graphics and music from the original tapes.

You don't need this to build the game: its output (gfx/) is in the repo.
It's kept to document where every byte came from. Needs SkoolKit (for its
Z80 simulator) and the original tapes:

    tools/extract.py PHANTOMASA1_1500BAUD.TZX PHANTOMASA2_1500BAUD.TZX

What it does:
  1. loads each tape in a simulated 48K Spectrum (tap2sna) and lets the
     game run a little, so its loader has unpacked everything;
  2. calls each of the game's "show screen" routines (the RANDOMIZE USR
     47385... / 45341... of the BASIC listings) with interrupts off and
     grabs the 6912-byte screen they draw;
  3. writes every screen as a PNG (gfx/screens/) plus the exact .scr
     (gfx/screens/ref/, used by conv.py to keep unedited cells exact);
  4. writes the font (CHARS = 64344) as gfx/font.png, the player's UDGs
     and the box/platform font glyphs as gfx/sprites.png, and the beeper
     music player with its songs ($F600-$FC57, the same in both tapes)
     as gfx/music.bin.
"""
import os, sys, subprocess, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import png
from simcall import Machine
SK = os.environ.get('SKOOLKIT', '/home/claude/tools/skoolkit')
sys.path.insert(0, SK)
from skoolkit.snapshot import Snapshot
from skoolkit.simutils import from_snapshot
from skoolkit.csimulator import CSimulator
from skoolkit.trace import Tracer

PAL = [(0, 0, 0), (0, 0, 0xD7), (0xD7, 0, 0), (0xD7, 0, 0xD7),
       (0, 0xD7, 0), (0, 0xD7, 0xD7), (0xD7, 0xD7, 0), (0xD7, 0xD7, 0xD7)]
PALB = [(0, 0, 0), (0, 0, 0xFF), (0xFF, 0, 0), (0xFF, 0, 0xFF),
        (0, 0xFF, 0), (0, 0xFF, 0xFF), (0xFF, 0xFF, 0), (0xFF, 0xFF, 0xFF)]

def auto_mask(rows):
    """1 = see-through: outside the figure, minus a 1 px halo"""
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

def scr_to_rows(scr):
    rows = []
    for y in range(192):
        a = ((y & 0xC0) << 5) | ((y & 7) << 8) | ((y & 0x38) << 2)
        row = []
        for cx in range(32):
            b = scr[a + cx]; at = scr[6144 + (y >> 3) * 32 + cx]
            pal = PALB if at & 64 else PAL
            ink, pap = pal[at & 7], pal[(at >> 3) & 7]
            row += [ink if (b >> (7 - x)) & 1 else pap for x in range(8)]
        rows.append(row)
    return rows

def loaded(tape):
    """snapshot of the game a few seconds after loading"""
    td = tempfile.mkdtemp()
    z80 = os.path.join(td, 'g.z80')
    subprocess.run([sys.executable, os.path.join(SK, 'tap2sna.py'), '-c', 'finish-tape=1', tape, z80],
                   check=True, capture_output=True)
    s = Snapshot.get(z80)
    sim = from_snapshot(CSimulator, s, {}, {}, {})
    tr = Tracer(sim, 0, 0, 0, [0] * 16, 0, False)
    sim.set_tracer(tr)
    frames = [0]
    def frame(*args):                         # called once per 50 Hz frame
        frames[0] += 1
        return frames[0] < 300                # ~6 s of emulated time
    tr.run(sim.registers[24], -1, 0, 0, 1, frame, None, None, None, '$', '02X', '04X')
    return z80, bytes(sim.memory[16384:65536])

# (tape, address of the first "LD HL,data / JR show" entry, names)
SCREENS = [
    (0, 49434, ['intro1', 'intro2', 'intro3', 'intro4', 'intro5', 'title',
                'round1', 'round2', 'round3', 'round4', 'round5', 'round6',
                'round7', 'round8', 'round9', 'cezlogo']),
    (1, 45341, ['jankendo', 'mazeA', 'mazeB', 'outro1', 'outro2', 'outro3',
                'outro4', 'outro5', None]),       # None: same title again
]

def main():
    tapes = sys.argv[1:3]
    snaps = [loaded(t) for t in tapes]
    n = 0
    os.makedirs(os.path.join(ROOT, 'gfx', 'screens', 'ref'), exist_ok=True)
    for t, base, names in SCREENS:
        z80, ram = snaps[t]
        for k, name in enumerate(names):
            if not name: continue
            m = Machine(z80)
            for i in range(len(ram)): m.mem[16384 + i] = ram[i]
            for i in range(6912): m.mem[16384 + i] = 0
            m.call(base + 5 * k, sp=0x5BF0)
            scr = m.screen()
            fn = '%02d-%s' % (n, name)
            png.write(os.path.join(ROOT, 'gfx', 'screens', fn + '.png'), 256, 192, scr_to_rows(scr))
            open(os.path.join(ROOT, 'gfx', 'screens', 'ref', fn + '.scr'), 'wb').write(scr)
            print(fn)
            n += 1
    ram = snaps[0][1]
    peek = lambda a: ram[a - 16384]
    # music player + songs: identical in both tapes (except the player's
    # own state bytes $F751-$F75B, reset when a song starts)
    open(os.path.join(ROOT, 'gfx', 'music.bin'), 'wb').write(bytes(peek(a) for a in range(0xF600, 0xFC58)))
    # font, characters 32-127 at CHARS+256 = $FC58
    font = [peek(0xFC58 + i) for i in range(768)]
    rows = [[(255, 255, 255) if (font[(cy // 8 * 16 + cx // 8) * 8 + cy % 8] >> (7 - cx % 8)) & 1 else (0, 0, 0)
             for cx in range(128)] for cy in range(48)]
    png.write(os.path.join(ROOT, 'gfx', 'font.png'), 128, 48, rows)
    # sprites: player from the UDGs a..l ($FF58), box = font e f g h,
    # platform = font p q (top half)
    udg = [peek(0xFF58 + i) for i in range(168)]
    U = lambda c: udg[(ord(c) - 97) * 8:(ord(c) - 96) * 8]
    F = lambda c: font[(ord(c) - 32) * 8:(ord(c) - 31) * 8]
    Z = [0] * 8
    frames = [(U('a'), U('b'), U('c'), U('d')), (U('a'), U('b'), U('e'), U('f')),
              (U('g'), U('h'), U('i'), U('j')), (U('g'), U('h'), U('k'), U('l')),
              (F('e'), F('f'), F('g'), F('h')), (F('p'), F('q'), Z, Z)]
    img = [[(0, 0, 0)] * 96 for _ in range(16)]
    for f, (tl, tr, bl, br) in enumerate(frames):
        rows = [(tl[y] << 8) | tr[y] for y in range(8)] + [(bl[y] << 8) | br[y] for y in range(8)]
        if f < 4: m = auto_mask(rows)
        elif f == 4: m = [0] * 16                          # box: all solid
        else: m = [0] * 8 + [0xFFFF] * 8                   # platform: top half
        for y in range(16):
            for x in range(16):
                if (rows[y] >> (15 - x)) & 1: img[y][f * 16 + x] = (255, 255, 255)
                elif not (m[y] >> (15 - x)) & 1: img[y][f * 16 + x] = (255, 0, 0)
    png.write(os.path.join(ROOT, 'gfx', 'sprites.png'), 96, 16, img)
    print('font, music, sprites')

main()
