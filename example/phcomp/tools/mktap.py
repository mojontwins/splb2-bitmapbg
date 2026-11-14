#!/usr/bin/env python3
"""phcomp.tap: a BASIC loader, the loading screen (gfx/loading.scr, if
there is one: 6912 bytes) and the code (src/phcomp.bin at 24200).
Python 3 standard library only."""
import os, struct
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORG = 24200

def block(flag, data):
    d = bytes([flag]) + data
    x = 0
    for b in d: x ^= b
    d += bytes([x])
    return struct.pack('<H', len(d)) + d

def header(kind, name, length, p1, p2):
    return block(0, bytes([kind]) + name.ljust(10)[:10].encode() + struct.pack('<HHH', length, p1, p2))

def num(n):
    """a number in a BASIC line: the digits, then 0x0E and the 5-byte form"""
    return str(n).encode() + b'\x0e\x00\x00' + struct.pack('<H', n) + b'\x00'

# 10 BORDER 0: PAPER 0: INK 0: CLEAR 24199: LOAD ""SCREEN$: LOAD ""CODE: RANDOMIZE USR 24200
scr = os.path.join(ROOT, 'gfx', 'loading.scr')
has_scr = os.path.exists(scr)
line = (b'\xe7' + num(0) + b':\xda' + num(0) + b':\xd9' + num(0) + b':\xfd' + num(ORG - 1) + b':')
if has_scr:
    line += b'\xef""\xaa:'                  # LOAD ""SCREEN$
line += b'\xef""\xaf:\xf9\xc0' + num(ORG) + b'\x0d'
prog = struct.pack('>H', 10) + struct.pack('<H', len(line)) + line
code = open(os.path.join(ROOT, 'src', 'phcomp.bin'), 'rb').read()
out = header(0, 'PHANTOMAS', len(prog), 10, len(prog)) + block(255, prog)
if has_scr:
    s = open(scr, 'rb').read()
    out += header(3, 'pantalla', len(s), 16384, 32768) + block(255, s)
out += header(3, 'phcomp', len(code), ORG, 32768) + block(255, code)
open(os.path.join(ROOT, 'phcomp.tap'), 'wb').write(out)
print('mktap.py: phcomp.tap, %d bytes of code%s' % (len(code), ', with the loading screen' if has_scr else ''))
