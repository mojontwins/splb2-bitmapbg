"""dis.py dump.bin start end   (dump.bin = memory from $4000)"""
import sys
sys.path.insert(0, '/home/claude/tools/skoolkit')
from skoolkit.disassembler import Disassembler
from skoolkit.snaskool import DisassemblerConfig
class I:
    def __init__(self, address, operation, data, *a):
        self.address, self.operation, self.bytes = address, operation, data
imaker = I
def disasm(mem, a, b):
    cfg = DisassemblerConfig(True, False, 8, 66, 2, False, imaker, '', False)
    d = Disassembler(mem, cfg)
    return d.disassemble(a, b, 'h')
if __name__ == '__main__':
    raw = open(sys.argv[1], 'rb').read()
    mem = [0] * 65536; mem[16384:16384 + len(raw)] = raw
    for i in disasm(mem, int(sys.argv[2], 0), int(sys.argv[3], 0)):
        print('%04X  %-14s %s' % (i.address, ' '.join('%02X' % x for x in i.bytes), i.operation))
