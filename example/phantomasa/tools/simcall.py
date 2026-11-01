"""Run routines of the original game inside a loaded snapshot (asset extraction).
Machine(z80).call(addr) runs from addr until it returns, with interrupts off."""
import sys
sys.path.insert(0, '/home/claude/tools/skoolkit')
from skoolkit.snapshot import Snapshot
from skoolkit.simutils import from_snapshot, SP, PC
from skoolkit.csimulator import CSimulator

RET = 0x5B00            # sentinel return address (printer buffer)

class Machine:
    def __init__(self, z80):
        s = Snapshot.get(z80)
        self.sim = from_snapshot(CSimulator, s, {}, {}, {})
        self.mem = self.sim.memory
    def call(self, addr, sp=None):
        r = self.sim.registers
        if sp is not None: r[SP] = sp
        r[SP] -= 2
        self.mem[r[SP]] = RET & 255; self.mem[r[SP] + 1] = RET >> 8
        self.sim.run(addr, RET)
    def screen(self):
        return bytes(self.mem[16384:16384 + 6912])
