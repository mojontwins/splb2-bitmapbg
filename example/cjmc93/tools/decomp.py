#!/usr/bin/env python3
"""Decoder for the compiled BASIC of CJMC93 (ERE's compiler, 1984).

The compiler turns each BASIC line into Z80 code that calls a runtime
(at $5D00-$66F0) and the ROM's calculator. Expressions become RPN: every
operand is pushed on the calculator stack (STKEND, $5C65) as a 5-byte
Spectrum number, then an operator routine is called. The patterns:

  CD 44 5D + 5 bytes          push a number
  CD 8D 5E + length + text    push a string
  21 nn nn CD 64 5D           push the numeric variable at nn
  21 nn nn 22 4D 5C           LET target (DEST) = the variable at nn
  3E xx CD 9F 5D              the string variable xx$ (A = its letter)
  CD 77 65 CD nn nn           GO SUB: CALL into the compiled code
  CD 67 65 CA nn nn           IF: false -> JP Z to the next line
  C3 nn nn                    GO TO

The line table (line number, address) is at $66F6-$6AAB, the code from
$6AD5 (line 10). decode() turns a range of code into tokens; CALLS names
the runtime routines (worked out by disassembling them). listing.py turns
the tokens back into BASIC.

usage: decomp.py FIRST_LINE LAST_LINE     (tokens, one BASIC line per row)
"""
import sys, struct
import os
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'phantomasa', 'tools'))   # z80dis.py
sys.path.insert(0, HERE)
import z80dis
# the 64K memory after loading the tape (orig/mem.bin)
mem = list(open(os.path.join(HERE, '..', 'orig', 'mem.bin'), 'rb').read())
w = lambda a: mem[a] | mem[a + 1] << 8
lines = []
a = 0x66F6
while a < 0x6AAC:
    lines.append((mem[a] << 8 | mem[a + 1], w(a + 2))); a += 4
lines = [l for l in lines if l[0] < 10000]

def zxfloat(b):
    if b[0] == 0:                       # small integer
        v = b[2] | b[3] << 8
        return -(65536 - v) if b[1] == 0xFF else v
    e = b[0] - 128
    m = ((b[1] | 0x80) << 24 | b[2] << 16 | b[3] << 8 | b[4]) / 2 ** 32
    v = m * 2 ** e
    return -v if b[1] & 0x80 else v

NAMES = {}          # var address -> name
CALLS = {
 0x6567:'IF', 0x6577:'GOSUB', 0x61ED:';', 0x640C:'SDEST', 0x63F3:'LET$', 0x63A6:'LET',
 0x6461:'PRINT(', 0x646F:'PI', 0x6469:')PRINT', 0x2007:'AT', 0x2207:'COLOUR', 0x1F3A:'PAUSE',
 0x03F8:'BEEP', 0x0D6B:'CLS', 0x36AF:'INT', 0x3674:'LEN', 0x1E4F:'RANDOMIZE', 0x2580:'ATTR?',
 0x34B3:'PEEK?', 0x6070:'-', 0x6094:'+', 0x6036:'/', 0x6024:'*', 0x6046:'NEG', 0x5FF4:'OR', 0x6004:'AND',
 0x5F06:'CMP5F06', 0x5F20:'CMP5F20', 0x5EFD:'CMP5EFD', 0x5EF8:'CMP5EF8', 0x5F62:'CMP5F62', 0x5F5B:'CMP5F5B', 0x5EE0:'CMP5EE0',
 0x5E66:'RND', 0x5E2D:'INKEY$', 0x6210:'COLRESET', 0x622F:'PLOT', 0x6235:'DRAW?', 0x5DE6:'#1',
 0x6125:'FOR6125', 0x6116:'FOR6116', 0x6134:'NEXT6134', 0x6161:'NEXT6161', 0x5D9F:'SVAR', 0x64C1:'X64C1'}

def var(a):
    return NAMES.get(a, 'v%04X' % a)

def decode(a, end):
    out = []
    while a < end:
        b = mem[a]
        if b == 0xCD:
            t = w(a + 1)
            if t == 0x5D44:
                v = zxfloat(mem[a + 3:a + 8]); out.append('#%s' % (('%g' % v) if v != int(v) else int(v))); a += 8; continue
            if t == 0x5E8D:
                n = w(a + 3); s = bytes(mem[a + 5:a + 5 + n]); out.append('"%s"' % ''.join(chr(c) if 32 <= c < 127 else '{%02X}' % c for c in s)); a += 5 + n; continue
            if t in CALLS:
                out.append(CALLS[t]); a += 3; continue
            out.append('CALL_%04X' % t); a += 3; continue
        if b == 0x21 and mem[a + 3] == 0xCD and w(a + 4) == 0x5D64:
            out.append(var(w(a + 1))); a += 6; continue
        if b == 0x21 and mem[a + 3] == 0x22 and w(a + 4) == 0x5C4D:
            out.append('DEST=' + var(w(a + 1))); a += 6; continue
        i = z80dis.disasm(mem, a, a + 1)[0]
        out.append('[' + i.operation + ']')
        a += len(i.bytes)
    return out

if __name__ == '__main__':
    lo, hi = int(sys.argv[1]), int(sys.argv[2])
    for k, (ln, ad) in enumerate(lines):
        if lo <= ln <= hi:
            nxt = lines[k + 1][1] if k + 1 < len(lines) else 50669
            print('%5d @%04X: ' % (ln, ad) + ' '.join(decode(ad, nxt)))
