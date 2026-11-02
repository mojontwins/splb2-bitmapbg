#!/usr/bin/env python3
"""One-off: run the original snapshot (orig/CHEM2.Z80) in SkoolKit's Z80
simulator and save exact screens (.scr, 6912 bytes) at chosen moments.
Needs SkoolKit (SKOOLKIT=/path/to/skoolkit). Not needed to build the game.

usage: grab.py script outdir
script commands, one per line:
  go L A B     make the BASIC interpreter jump to line L (as GO TO L), until
               the line being run is A..B: the jump is retried every frame,
               tapping Z to get out of a PAUSE 0 (a RETURN can undo a jump)
  line A B [T] run until the line being run is A..B (checked once per frame,
               so the program has to stay there: a PAUSE, a loop...)
  key K N      hold key K for N frames      wait N    run N frames
  scr NAME     save the screen as outdir/NAME.scr
"""
import os, sys
sys.path.insert(0, os.environ.get('SKOOLKIT', '/home/claude/tools/skoolkit'))
from skoolkit.snapshot import Snapshot
from skoolkit.simutils import from_snapshot
from skoolkit.csimulator import CSimulator
from skoolkit.trace import Tracer
from skoolkit.kbtracer import KEYS

ROWS = {0xFEFE: 0, 0xFDFE: 1, 0xFBFE: 2, 0xF7FE: 3, 0xEFFE: 4, 0xDFFE: 5, 0xBFFE: 6, 0x7FFE: 7}
HERE = os.path.dirname(os.path.abspath(__file__))
cmds = [l.split('#')[0].split() for l in open(sys.argv[1])]
cmds = [c for c in cmds if c]
out = sys.argv[2]
os.makedirs(out, exist_ok=True)
s = Snapshot.get(os.path.join(HERE, '..', 'orig', 'CHEM2.Z80'))
sim = from_snapshot(CSimulator, s, {}, {}, {'fast_djnz': False, 'fast_ldir': False})
tr = Tracer(sim, 0, 0, 0, [0] * 16, 0, False)
sim.set_tracer(tr)
mem = sim.memory
st = {'i': 0, 'n': 0}

def ppc():
    return mem[23621] + 256 * mem[23622]

def frame(scr, fr, border, kb):
    for i in range(8): kb[i] = 0
    while st['i'] < len(cmds):
        c = cmds[st['i']]
        if c[0] == 'go':
            if int(c[2]) <= ppc() <= int(c[3]):
                st['n'] = 0; st['i'] += 1; continue
            l = int(c[1])
            st['n'] += 1
            if not l <= ppc() <= int(c[3]):     # not there yet: (re)try the jump
                mem[23618], mem[23619], mem[23620] = l & 255, l >> 8, 0
            if st['n'] & 2:
                p, m = KEYS['z']; kb[ROWS[p]] |= m ^ 0xFF
            if st['n'] % 200 == 0: print('  go: at line', ppc(), 'err', mem[23610])
            if st['n'] > 3000: raise SystemExit('go timeout %s' % c)
            return True
        if c[0] == 'scr':
            open(os.path.join(out, c[1] + '.scr'), 'wb').write(bytes(mem[16384:16384 + 6912]))
            print('saved', c[1], 'at line', ppc())
            st['i'] += 1; continue
        if c[0] == 'line':
            if st['n'] == 0: st['n'] = int(c[3]) if len(c) > 3 else 5000
            st['n'] -= 1
            if int(c[1]) <= ppc() <= int(c[2]) or st['n'] == 0:
                if st['n'] == 0: print('timeout', c)
                st['n'] = 0; st['i'] += 1; continue
            return True
        if c[0] in ('wait', 'key'):
            if st['n'] == 0: st['n'] = int(c[-1])
            if c[0] == 'key':
                p, m = KEYS[c[1]]; kb[ROWS[p]] |= m ^ 0xFF
            st['n'] -= 1
            if st['n'] == 0: st['i'] += 1
            return True
        raise SystemExit('bad command %s' % c)
    return False

tr.run(sim.registers[24], -1, 0, 0, 1, frame, None, None, None, '$', '02X', '04X')
