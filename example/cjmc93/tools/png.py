"""Minimal PNG reader/writer (Python standard library only).

read(path)  -> (width, height, rows) with rows[y][x] = (r, g, b, a)
write(path, width, height, rows)  rows[y][x] = (r, g, b)  -> 8-bit RGB PNG

Reads non-interlaced PNGs: greyscale, RGB, palette, grey+alpha and RGBA,
1/2/4/8/16 bits per channel. That covers what image editors save."""
import struct, zlib

def read(path):
    data = open(path, 'rb').read()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('%s: not a PNG file' % path)
    pos, idat, plte, trns = 8, b'', None, None
    while pos < len(data):
        n, typ = struct.unpack('>I4s', data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + n]
        pos += 12 + n
        if typ == b'IHDR':
            w, h, depth, ctype, _, _, inter = struct.unpack('>IIBBBBB', chunk)
        elif typ == b'PLTE':
            plte = [tuple(chunk[i:i + 3]) for i in range(0, len(chunk), 3)]
        elif typ == b'tRNS':
            trns = chunk
        elif typ == b'IDAT':
            idat += chunk
        elif typ == b'IEND':
            break
    if inter:
        raise ValueError('%s: interlaced PNGs are not supported, save it without interlacing' % path)
    chans = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
    bpp = max(1, chans * depth // 8)
    stride = (w * chans * depth + 7) // 8
    raw = zlib.decompress(idat)
    prev = bytearray(stride)
    rows = []
    p = 0
    for y in range(h):
        f = raw[p]; line = bytearray(raw[p + 1:p + 1 + stride]); p += 1 + stride
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if f == 1: line[i] = (line[i] + a) & 255
            elif f == 2: line[i] = (line[i] + b) & 255
            elif f == 3: line[i] = (line[i] + ((a + b) >> 1)) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        prev = line
        # unpack samples
        if depth == 8: s = list(line)
        elif depth == 16: s = [line[i] for i in range(0, len(line), 2)]
        else:
            s = []
            per = 8 // depth
            for byte in line:
                for k in range(per):
                    s.append((byte >> (8 - depth * (k + 1))) & ((1 << depth) - 1))
        maxv = (1 << depth) - 1 if depth < 8 else 255
        row = []
        for x in range(w):
            if ctype == 3:
                i = s[x]
                r, g, b = plte[i]
                a = trns[i] if trns and i < len(trns) else 255
            elif ctype == 0:
                v = s[x] * 255 // maxv; r = g = b = v; a = 255
            elif ctype == 4:
                r = g = b = s[2 * x]; a = s[2 * x + 1]
            elif ctype == 2:
                r, g, b = s[3 * x:3 * x + 3]; a = 255
            else:
                r, g, b, a = s[4 * x:4 * x + 4]
            row.append((r, g, b, a))
        rows.append(row)
    return w, h, rows

def write(path, w, h, rows):
    raw = b''.join(b'\0' + bytes(c for px in row for c in px[:3]) for row in rows)
    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                           + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))
