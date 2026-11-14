#!/usr/bin/env python3
"""A tiny interpreter for the PRINT statements of the decompiled listing
(orig/cjmc93.bas.txt): runs the lines that paint a room and returns the
32x24 grid of (character, attribute, inverse) the original would show.

Used by extract.py to turn every room into data. Handles only what the
game's static drawing uses: PRINT [#0;] AT/INK/PAPER/FLASH/BRIGHT/INVERSE/
OVER items, strings, commas, FOR/NEXT inside a line, LET, CLS."""
import re, math

import os
LISTING = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'orig', 'cjmc93.bas.txt')
SRC = {}

def listing():
    if not SRC:
        for l in open(LISTING, encoding='latin-1'):
            m = re.match(r'\s*(\d+) (.*)$', l.rstrip('\n'))   # (# lines don't match)
            if m:
                SRC[int(m.group(1))] = m.group(2)
    return SRC

def zxround(v):
    return int(math.floor(v + 0.5))

class Screen:
    def __init__(self):
        self.clear()
        self.vars = {}
        self.solid = set()          # SOLID cells (conv.py)

    def clear(self, untouched=False):
        # (char code, attr, inverse) ; 32 = space; permanent PAPER 0 INK 7.
        # untouched=True: None in every cell, to see what a room paints.
        v = None if untouched else (32, 7, 0)
        self.cell = [[v for c in range(32)] for r in range(24)]

    def ev(self, e):
        e = e.strip()
        def rep(m):
            n = m.group(0)
            if n in ('LEN', 'INT', 'RND', 'AND', 'OR', 'CODE'): return n
            return repr(self.vars[n])
        e = re.sub(r'[A-Za-z][A-Za-z0-9]*\$?', rep, e)
        e = re.sub(r"LEN ('[^']*'|\"[^\"]*\")", lambda m: str(len(eval(m.group(1)))), e)
        return eval(e)

    def items(self, s):
        """split a PRINT's items on '; ' outside quotes"""
        out, cur, q = [], '', False
        i = 0
        while i < len(s):
            ch = s[i]
            if ch == '"': q = not q
            if not q and s.startswith('; ', i):
                out.append(cur); cur = ''; i += 2; continue
            cur += ch; i += 1
        out.append(cur)
        return out

    def put_text(self, t):
        """characters of a string literal body: [X] UDG, [gN] block"""
        i = 0
        while i < len(t):
            m = re.match(r'\[g([0-9A-F])\]|\[([A-U])\]|\{([0-9A-F]{2})\}', t[i:])
            if m:
                if m.group(1): code = 0x80 + int(m.group(1), 16)
                elif m.group(2): code = 0x90 + ord(m.group(2)) - 65
                else: code = int(m.group(3), 16)
                i += len(m.group(0))
            else:
                code = ord(t[i]); i += 1
            self.put_char(code)

    def put_char(self, code):
        r, c = self.r, self.c
        if c >= 32:
            r += 1; c = 0
        if 0 <= r < 24:
            attr = (self.flash << 7) | (self.bright << 6) | (self.paper << 3) | self.ink
            self.cell[r][c] = (code, attr, self.inverse)
        c += 1
        self.r, self.c = r, c

    def do_print(self, s):
        self.ink, self.paper, self.flash, self.bright, self.inverse = 7, 0, 0, 0, 0
        self.lower = False
        self.r, self.c = getattr(self, 'r', 0), getattr(self, 'c', 0)
        for it in self.items(s):
            it = it.strip()
            if it == '#0': self.lower = True; continue
            if it == ',':                       # spaces to the next tab stop
                self.put_char(32)
                while self.c not in (16, 32): self.put_char(32)
                continue
            m = re.match(r'AT (.*)$', it)
            if m:
                a = m.group(1)
                # split "row,col" at the top-level comma
                depth = 0
                for k, ch in enumerate(a):
                    depth += ch == '('; depth -= ch == ')'
                    if ch == ',' and depth == 0: break
                self.r = zxround(self.ev(a[:k])) + (22 if self.lower else 0)
                self.c = zxround(self.ev(a[k + 1:]))
                continue
            m = re.match(r'(INK|PAPER|FLASH|BRIGHT|INVERSE|OVER) (.*)$', it)
            if m:
                v = zxround(self.ev(m.group(2)))
                if m.group(1) != 'OVER':
                    setattr(self, m.group(1).lower(), v)
                continue
            if it.startswith('"'):
                self.put_text(it[1:-1]); continue
            if it.startswith('ATTR') or not it: continue
            # a value (lives...)
            self.put_text(str(self.ev(it)))
        # PRINT without ';' at the end: new line (doesn't matter here)

    def run_line(self, n):
        self.run_text(listing()[n])

    def run_text(self, text):
        # statements: split on ': ' outside quotes
        stmts, cur, q = [], '', False
        for k, ch in enumerate(text):
            if ch == '"': q = not q
            if not q and text.startswith(': ', k):
                stmts.append(cur); cur = ''; continue
            if not q and cur == '' and ch == ' ' and stmts and text[k - 1] == ':':
                continue
            cur += ch
        stmts.append(cur)
        k = 0
        loops = {}
        while k < len(stmts):
            s = stmts[k]
            if s.upper().startswith('PRINT'):
                self.do_print(s[6:])
            elif s == 'CLS':
                self.clear()
            elif s.startswith('FOR '):
                m = re.match(r'FOR (\w+)=(.*?) TO (.*?)( STEP (.*))?$', s)
                v = m.group(1)
                self.vars[v] = self.ev(m.group(2))
                loops[v] = (k, self.ev(m.group(3)), self.ev(m.group(5)) if m.group(5) else 1)
            elif s.startswith('SOLID '):
                # collision only (gfx/rooms.txt): row, col, width
                a = s[6:].split(',')
                r0, c0, n = (zxround(self.ev(x)) for x in a)
                for cc in range(c0, c0 + n):
                    self.solid.add((r0, cc))
            elif s.startswith('NEXT '):
                v = s[5:]
                start, end, step = loops[v]
                self.vars[v] += step
                if (step > 0 and self.vars[v] <= end) or (step < 0 and self.vars[v] >= end):
                    k = start + 1; continue
            elif s.startswith('LET '):
                m = re.match(r'LET (\w+\$?)=(.*)$', s)
                if m.group(1).endswith('$'):
                    v = re.match(r'"([^"]*)"$', m.group(2))
                    if v: self.vars[m.group(1)] = v.group(1)
                else:
                    try: self.vars[m.group(1)] = self.ev(m.group(2))
                    except Exception: pass
            k += 1

    def run(self, nums, **v):
        self.vars.update(v)
        for n in nums:
            self.run_line(n)
        return self
