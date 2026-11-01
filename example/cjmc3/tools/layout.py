#!/usr/bin/env python3
"""Mini interpreter for the screen-drawing subroutines of CJMC3's sources.bas
(HiSoft BASIC / zmakebas escapes). Produces, for every screen np 0..11, the
cells the original PRINTs paint: {(row, col): (charcode, attr)}.
charcode: 32..127 = character in the current font (gfx font while drawing),
144..164 = UDG A..U."""
import re, sys

SRC = sys.argv[1] if len(sys.argv) > 1 else 'sources.bas'

def load_lines(path):
    txt = open(path, 'rb').read().decode('latin-1').replace('\r', '')
    lines = {}
    for l in txt.split('\n'):
        m = re.match(r'\s*(\d+) (.*)$', l)
        if m: lines[int(m.group(1))] = m.group(2)
    return lines

# ------------------------------------------------------------- strings
UDG = 'abcdefghijklmnopqrstu'
def decode_string(s):
    """returns list of items: ('c', code) or ('attr', key, value)"""
    out = []
    i = 0
    while i < len(s):
        ch = s[i]
        if ch == '\\':
            n = s[i + 1]
            if n == '{':
                j = s.index('}', i)
                spec = s[i + 2:j]
                k = 0
                while k < len(spec):
                    key = spec[k]; val = spec[k + 1]
                    if key in 'ipbf' and val.isdigit():
                        out.append(('attr', key, int(val)))
                    k += 2
                i = j + 1
                continue
            if n in UDG: out.append(('c', 144 + UDG.index(n))); i += 2; continue
            if n == '*': out.append(('c', 127)); i += 2; continue
            if n in '@`\\': out.append(('c', ord(n))); i += 2; continue
            if n == '#':
                m = re.match(r'#(\d+)', s[i + 1:]); out.append(('c', int(m.group(1)))); i += 2 + len(m.group(1)); continue
            raise Exception('escape ' + s[i:i + 4])
        out.append(('c', ord(ch)))
        i += 1
    return out

# ------------------------------------------------------------- tokenizer
def split_statements(line):
    st, cur, q = [], '', False
    i = 0
    while i < len(line):
        c = line[i]
        if c == '"':
            q = not q
        if c == ':' and not q:
            st.append(cur.strip()); cur = ''
        else:
            cur += c
        i += 1
    if cur.strip(): st.append(cur.strip())
    return st

def split_items(s):
    """split PRINT list by ; and , outside quotes, keeping separators"""
    items, cur, q, depth = [], '', False, 0
    for c in s:
        if c == '"': q = not q
        if not q and c == '(': depth += 1
        if not q and c == ')': depth -= 1
        if not q and depth == 0 and c == ',' and cur.strip().upper().startswith('AT') and ',' not in cur:
            cur += c; continue
        if not q and depth == 0 and c in ';,':
            items.append(cur.strip()); items.append(c); cur = ''
        else:
            cur += c
    items.append(cur.strip())
    return items

class Interp:
    def __init__(self, lines):
        self.L = lines
        self.vars = {}
        self.cells = {}
        self.perm = {'i': 1, 'p': 0, 'b': 0, 'f': 0}   # INK 1: CLS, PAPER 0
        self.bordcr = {'i': 1, 'p': 0, 'b': 0, 'f': 0} # POKE 23624,1

    def ev(self, e):
        e = e.strip()
        # string expressions: literal possibly followed by junk like \{p0}
        if e.startswith('"'):
            j = e.index('"', 1)
            return decode_string(e[1:j])
        if re.fullmatch(r'[a-z]\$', e.lower()):
            return self.vars.get(e.lower(), [])
        if e.upper().startswith('FN V'):
            raise Exception('FN')
        # numeric
        expr = e.lower()
        expr = re.sub(r'\b([a-z][a-z0-9]*)\b', lambda m: str(self.vars.get(m.group(1), 0)), expr)
        return int(eval(expr))

    def fnv(self, s, a, b):
        return 10 * (s[a][1] - 48) + s[b][1] - 48

    def put(self, row, col, code, attr):
        self.cells[(row, col)] = (code, attr)

    def do_print(self, args):
        lower = False
        if args.startswith('#0'):
            lower = True
            args = args[2:].lstrip(';')
        base = dict(self.bordcr if lower else self.perm)
        attr = dict(base)
        row, col = (22 if lower else 0), 0
        for it in split_items(args):
            if it in (';', ''): continue
            if it == ',':
                col = 16 if col < 16 else 32
                if col == 32: row += 1; col = 0
                continue
            m = re.match(r'AT\s+(.+?),(.+)$', it)
            if m:
                row = self.ev(m.group(1)) + (22 if lower else 0); col = self.ev(m.group(2)); continue
            m = re.match(r'(INK|PAPER|BRIGHT|FLASH)\s+(\d+)$', it)
            if m:
                attr[{'INK': 'i', 'PAPER': 'p', 'BRIGHT': 'b', 'FLASH': 'f'}[m.group(1)]] = int(m.group(2)); continue
            v = self.ev(it)
            if isinstance(v, int):
                v = decode_string(str(v))
            for x in v:
                if x[0] == 'attr':
                    attr[x[1]] = x[2]
                else:
                    if col >= 32: row += 1; col = 0
                    a = attr['i'] | (attr['p'] << 3) | (attr['b'] << 6) | (attr['f'] << 7)
                    self.put(row, col, x[1], a)
                    col += 1

    def run_line(self, n):
        st = split_statements(self.L[n])
        pc = 0
        loops = {}
        while pc < len(st):
            s = st[pc]
            u = s.upper()
            if u.startswith('PRINT'):
                self.do_print(s[5:].strip())
            elif u.startswith('LET'):
                m = re.match(r'LET\s+([a-zA-Z][a-zA-Z0-9]*\$?)\s*=\s*(.*)$', s)
                self.vars[m.group(1).lower()] = self.ev(m.group(2))
            elif u.startswith('GO SUB'):
                t = int(s[6:])
                if t == 410: self.run_line(410)
                elif t == 420: self.sub420()
                else: raise Exception('GO SUB %d' % t)
            elif u.startswith('FOR'):
                m = re.match(r'FOR\s+([a-zA-Z])\s*=\s*(.+?)\s+TO\s+(.+?)(\s+STEP\s+(.+))?$', s, re.I)
                v = m.group(1).lower()
                self.vars[v] = self.ev(m.group(2))
                loops[v] = (pc, self.ev(m.group(3)), self.ev(m.group(5)) if m.group(5) else 1)
            elif u.startswith('NEXT'):
                v = s[4:].strip().lower()
                start, end, step = loops[v]
                self.vars[v] += step
                if self.vars[v] <= end:
                    pc = start
            elif u.startswith('RETURN') or u.startswith('REM'):
                return
            else:
                raise Exception('unknown statement in %d: %s' % (n, s))
            pc += 1

    def sub420(self):
        b = self.vars['b$']
        p = self.vars['p$']; q = self.vars['q$']
        for i in range(0, len(b), 4):
            yy = self.fnv(b, i, i + 1); xx = self.fnv(b, i + 2, i + 3)
            self.vars['yy'] = yy; self.vars['xx'] = xx
            self.do_print('AT yy,xx;p$;AT yy+1,xx;q$')

SCREEN_LINES = {0: [500, 501, 502, 503], 1: [520, 521, 522], 2: [540, 541, 542], 3: [560, 561],
                4: [580, 581], 5: [600, 601], 6: [620, 621, 622], 7: [640, 641, 642],
                8: [660, 661, 662], 9: [680, 681, 682], 10: [700, 701, 702, 703, 704, 705],
                11: [720, 721, 722]}

def screens(path):
    L = load_lines(path)
    res = {}
    for np_, lines in SCREEN_LINES.items():
        it = Interp(L)
        for n in lines:
            it.run_line(n)
        res[np_] = it.cells
    return res

if __name__ == '__main__':
    r = screens(SRC)
    for k, v in r.items():
        print(k, len(v))
