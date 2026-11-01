#!/usr/bin/env python3
"""Makes phantomasa.tap: one tape, one load, for the 128K.

    tools/mktap.py src/phantomasa.bin phantomasa.tap

Blocks:
  1. BASIC loader "PHANTOMASA" (autorun line 10):
       10 BORDER 0: PAPER 0: INK 0: CLS: CLEAR 24199: LOAD ""CODE: RANDOMIZE USR 24200
  2. the program, CODE 24200 (header + data)
  3. headerless data blocks (flag $FF), read by load_blocks() in the
     program, in the order of build/load_order.txt: the music at $F600,
     then the RAM banks with the packed screens.
"""
import os, sys, struct
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORG = 24200

def block(flag, data):
    body = bytes([flag]) + data
    x = 0
    for b in body: x ^= b
    body += bytes([x])
    return struct.pack('<H', len(body)) + body

def header(kind, name, length, p1, p2):
    n = name.encode('ascii')[:10].ljust(10)
    return block(0, bytes([kind]) + n + struct.pack('<HHH', length, p1, p2))

def num(v):
    """a number in a BASIC line: its digits, then 0x0E and the 5-byte form"""
    return str(v).encode() + bytes([0x0E, 0, 0, v & 255, v >> 8, 0])

BORDER, PAPER, INK, CLS, CLEAR, LOAD, CODE, RANDOMIZE, USR = 0xE7, 0xDA, 0xD9, 0xFB, 0xFD, 0xEF, 0xAF, 0xF9, 0xC0
line = (bytes([BORDER]) + num(0) + b':' + bytes([PAPER]) + num(0) + b':' + bytes([INK]) + num(0) +
        b':' + bytes([CLS]) + b':' + bytes([CLEAR]) + num(ORG - 1) + b':' +
        bytes([LOAD]) + b'""' + bytes([CODE]) + b':' + bytes([RANDOMIZE, USR]) + num(ORG) + b'\r')
prog = struct.pack('>H', 10) + struct.pack('<H', len(line)) + line

binary = open(sys.argv[1], 'rb').read()
tap = header(0, 'PHANTOMASA', len(prog), 10, len(prog)) + block(0xFF, prog)
tap += header(3, 'phantomasa', len(binary), ORG, 32768) + block(0xFF, binary)
B = os.path.join(ROOT, 'build')
for name in open(os.path.join(B, 'load_order.txt')).read().split():
    tap += block(0xFF, open(os.path.join(B, name), 'rb').read())
open(sys.argv[2], 'wb').write(tap)
print('%s: %d bytes' % (sys.argv[2], len(tap)))
