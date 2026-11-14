#!/usr/bin/env python3
"""Turns the ERE-compiled code of CJMC93 back into readable BASIC.

decomp.py decodes the machine code into tokens (an RPN of calculator
pushes and runtime calls). This script runs them through a small stack
machine to rebuild the expressions in infix, names the variables, maps
jump addresses to line numbers, and writes orig/cjmc93.bas.txt.

Not a perfect decompiler: just enough for this program."""
import sys, re
import os
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import decomp
from decomp import mem, lines, decode, w, DELTA, TABLE_END, VARS



addr2line = {a: l for l, a in lines}
addr2line[TABLE_END] = 'RUN'
nextaddr = {}
for k, (l, a) in enumerate(lines):
    nextaddr[a] = lines[k + 1][1] if k + 1 < len(lines) else VARS
COLOURS = {0x10: 'INK', 0x11: 'PAPER', 0x12: 'FLASH', 0x13: 'BRIGHT', 0x14: 'INVERSE', 0x15: 'OVER'}
BIN = {'+': 6, '-': 6, '*': 7, '/': 7, '=': 4, '<>': 4, '<': 4, '>': 4, '<=': 4, '>=': 4, 'AND': 2, 'OR': 1}
TOKBIN = {'+': '+', '-': '-', '*': '*', '/': '/', 'CMP5F06': '=', 'CMP5F20': '<>', 'CMP5EF8': '<',
          'CMP5EFD': '>', 'CMP5EE0': '>=', 'CMP5F62': '=', 'CMP5F5B': '<>', 'OR': 'OR', 'AND': 'AND',
          'CALL_6014': 'AND'}

def L(a):
    return str(addr2line.get(a, '?%04X' % a))

def sstr(t):
    """a string token -> BASIC with control codes made readable"""
    body = t[1:-1]
    out, cur = [], ''
    i = 0
    while i < len(body):
        m = re.match(r'\{([0-9A-F]{2})\}', body[i:])
        if m:
            c = int(m.group(1), 16)
            if c in COLOURS and i + 4 < len(body):
                m2 = re.match(r'\{([0-9A-F]{2})\}', body[i + 4:])
                if cur: out.append('"%s"' % cur); cur = ''
                out.append('%s %d' % (COLOURS[c], int(m2.group(1), 16)))
                i += 8; continue
            if c >= 0x90:
                cur += '[%s]' % chr(ord('A') + c - 0x90)     # UDG letter
            elif c >= 0x80:
                cur += '[g%X]' % (c - 0x80)                   # block graphic
            else:
                cur += '{%02X}' % c
            i += 4; continue
        cur += body[i]; i += 1
    if cur or not out: out.append('"%s"' % cur)
    return out

def stmt(toks, here):
    st, out, pr, pend = [], [], None, []
    dest = '?'
    gosub_var = False
    def prx():
        nonlocal pr
        if pr is None: pr = []
        return pr
    def pop():
        return st.pop() if st else ('?', 9)
    def flush_print():
        nonlocal pr
        if pr is not None:
            out.append('PRINT ' + '; '.join(pr) if pr else 'PRINT')
        pr = None
    i = 0
    while i < len(toks):
        t = toks[i]; i += 1
        nt = toks[i] if i < len(toks) else ''
        if t.startswith('#') and len(t) > 1:
            st.append((t[1:], 9))
        elif t.startswith('"'):
            st.append(('\x00' + t, 9))
        elif t.startswith('DEST='):
            dest = t[5:]
        elif t == '[LD A,$41]' or re.match(r'\[LD A,\$(4[1-9A-F]|5[0-9A])\]$', t):
            letter = chr(int(t[7:9], 16))
            if nt == 'SVAR':
                i += 1
                if i < len(toks) and toks[i] == 'SDEST':
                    i += 1; dest = letter + '$'
                else:
                    st.append((letter + '$', 9))
            elif nt == 'COLOUR':
                pass
            else:
                st.append(('?A' + t, 9))
        elif re.match(r'\[LD A,\$1[0-5]\]$', t) and nt == 'COLOUR':
            i += 1; v = pop()[0]; item = '%s %s' % (COLOURS[int(t[7:9], 16)], v)
            if pr is None: pend.append(item)
            else: pr.append(item)
        elif t == '[LD A,$06]' and nt == '[RST $10]':
            i += 1
            if pr is None: pr = []
            prx().append(',')
        elif re.match(r'v[0-9A-F]{4}$|[a-z][a-z0-9]*$', t):
            st.append((t, 9))
        elif t in TOKBIN:
            op = TOKBIN[t]; p = BIN[op]
            b, a = pop(), pop()
            a = (fix(a[0]), a[1]); b = (fix(b[0]), b[1])
            fa = a[0] if a[1] >= p else '(%s)' % a[0]
            fb = b[0] if b[1] > p else '(%s)' % b[0]
            if t == 'CALL_6014': st.append(('%s AND %s' % (fa, fb), 2))
            else: st.append(('%s %s %s' % (fa, op, fb), p))
        elif t == 'NEG':
            a = pop(); st.append(('-' + (a[0] if a[1] >= 8 else '(%s)' % a[0]), 8))
        elif t in ('INT', 'LEN', 'PEEK?', 'CALL_3669'):
            a = pop(); fn = {'PEEK?': 'USR', 'CALL_3669': 'CODE'}.get(t, t)
            st.append(('%s %s' % (fn, a[0] if a[1] >= 9 else '(%s)' % a[0]), 8))
        elif t == 'CALL_5DCF':
            a = pop(); s = pop()
            if nt == '#1': i += 1
            st.append(('%s(%s)' % (s[0], a[0]), 9))
        elif t == 'RND': st.append(('RND', 9))
        elif t == 'INKEY$': st.append(('INKEY$', 9))
        elif t == '#1': st.append(('1', 9))
        elif t in ('ATTR?', 'CALL_2535'):
            b, a = pop(), pop(); st.append(('%s (%s,%s)' % ('ATTR' if t == 'ATTR?' else 'SCREEN$', a[0], b[0]), 9))
        elif t == 'LET':
            out.append('LET %s=%s' % (dest, fix(pop()[0])))
        elif t == 'LET$':
            out.append('LET %s=%s' % (dest, '; '.join(sstr(pop()[0][1:]))))
        elif t == 'PRINT(':
            pr = []
        elif t == 'CALL_207C':
            prx().append('#' + pop()[0])
        elif t == 'AT':
            b, a = pop(), pop(); prx().append('AT %s,%s' % (a[0], b[0]))
        elif t == 'PI':
            v = pop()[0]
            prx().extend(sstr(v[1:]) if v.startswith('\x00') else [fix(v)])
        elif t == 'CALL_2DE3':
            prx().append(pop()[0])
        elif t == ')PRINT':
            flush_print()
        elif t == ';':
            pass
        elif t in ('PAUSE', 'CLS', 'RANDOMIZE'):
            if t == 'CLS': out.append('CLS')
            else: out.append('%s %s' % (t, pop()[0]))
        elif t in ('BEEP', 'PLOT', 'DRAW?', 'CALL_1E80'):
            b, a = pop(), pop()
            out.append('%s %s%s,%s' % ({'DRAW?': 'DRAW', 'CALL_1E80': 'POKE'}.get(t, t), ''.join(c + '; ' for c in pend), fix(a[0]), fix(b[0]))); pend.clear()
        elif t == 'COLRESET':
            pass
        elif t == 'COLOUR':
            pass
        elif t == 'IF':
            cond = pop()[0]
            m = re.match(r'\[JP Z,\$([0-9A-F]+)\]', nt); i += 1
            tgt = int(m.group(1), 16)
            out.append('IF %s THEN' % fix(cond) + ('' if tgt == nextaddr.get(here) else ' {else %s}' % L(tgt)))
        elif t == 'GOSUB':
            m = re.match(r'CALL_([0-9A-F]+)$', nt)
            if m and nt != 'CALL_64A0':
                i += 1; out.append('GO SUB %s' % L(int(m.group(1), 16)))
            else:
                gosub_var = True        # GO SUB expression: ... CALL_64A0
        elif t == 'CALL_64A0':
            out.append('%s %s' % ('GO SUB' if gosub_var else 'GO TO', fix(pop()[0]))); gosub_var = False
        elif t == '[LD B,$1D]' and nt == 'CALL_35DE':
            i += 1; a_ = pop(); st.append(('VAL %s' % a_[0], 8))
        elif t.startswith('[JP $'):
            tgt = int(t[5:-1], 16)
            if tgt == 0x64A0: out.append('GO TO %s' % pop()[0])
            elif tgt == TABLE_END: out.append('RUN')
            else: out.append('GO TO %s' % L(tgt))
        elif t.startswith('[LD DE,$') and nt.startswith('FOR'):
            v = decomp.var(int(t[8:12], 16)); i += 1
            m = re.match(r'\[JP NC,\$([0-9A-F]+)\]', toks[i]); i += 1
            if nt == 'FOR6116':
                s, b, a = pop(), pop(), pop(); out.append('FOR %s=%s TO %s STEP %s' % (v, a[0], b[0], s[0]))
            else:
                b, a = pop(), pop(); out.append('FOR %s=%s TO %s' % (v, a[0], b[0]))
        elif t.startswith('[LD HL,$') and nt.startswith('NEXT'):
            v = decomp.var(int(t[8:12], 16)); i += 2
            out.append('NEXT %s' % v)
        elif t == '[LD HL,$%04X]' % TABLE_END and nt == 'X64C1':
            i += 1; out.append('RUN')
        elif t == '[RET]':
            out.append('RETURN')
        elif t == 'CALL_6688': out.append('OVER 1')
        elif t == 'CALL_6680': out.append('OVER 0')
        elif t == 'CALL_65A3': out.append('(INK 8?)')
        elif t == 'CALL_1CEE': out.append('STOP')
        else:
            out.append('<%s>' % t)
    flush_print()
    return out

def fix(e):
    return e[1:] if e.startswith('\x00') else e

if __name__ == '__main__':
    res = []
    for l, a in lines:
        toks = decode(a, nextaddr[a])
        try:
            res.append('%5d %s' % (l, ': '.join(stmt(toks, a)).replace('THEN: ', 'THEN ')))
        except Exception as e:                  # not understood: the raw tokens
            res.append('%5d ?? %s' % (l, ' '.join(toks)))
    head = '''# Classic Japanese Monster Castle '93 (LOKO SOFT), tape 1: the compiled
# program turned back into BASIC by tools/listing.py. Not the author's
# source: the compiler kept no names, so the variables are named by what
# they do (x y player column and bottom row; lives; jumping jumpt; i k
# loop counters; e f enemy column and row; p1-p3 platforms, rows or
# columns; d1-d4 their directions; c1-c4 their colours; bx/bdx balls;
# g1 g2 guards; by bd the guardian; back the line after the box room).
# [A]..[U] are the UDGs, [g0]..[gF] the block graphics. "RUN" is the
# compiled program's restart. {else N}: the IF jumps to line N when false.
#
'''
    txt = (head + '\n'.join(res) + '\n').replace('\x00', '')
    open(os.environ['OUT'], 'w').write(txt)
    print(txt)
