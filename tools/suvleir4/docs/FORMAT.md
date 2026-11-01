# SVL: the Suvleir 4 vector screen format

An SVL file holds one 256×192 screen: a list of drawing commands (lines,
plots, patterned flood fills) plus an optional 32×24 **hardness map**. It is
compact, and it is meant to be `incbin`'d and given straight to the engine
(`sp_VecDraw` in splib2-bitmapbg) with no conversion step.

All numbers are bytes unless stated. Coordinates are in pixels, with (0,0)
at the top-left and x from 0 to 255, y from 0 to 191, like the Spectrum display.

## Layout

```
offset  size  contents
0       2     H: little-endian offset (from the start of the file) of the
              hardness section, or 0 if there is none
2       ...   command stream, ended by $FF
H       ...   hardness section (RLE)
```

`H` always equals the offset just past the `$FF`. It is stored so a game can
fetch the hardness map without walking the commands.

## Commands

The engine keeps a **pen** position, which starts at (0,0) for every screen.

| Bytes | Name | Meaning |
|---|---|---|
| `y x` (first byte `y < 192`) | LINE | Draw a line from the pen to (x,y). The pen's own pixel is **not** plotted; the last pixel is. Pen = (x,y). 2 bytes. |
| `$C0 x y` | PLOT | Set pixel (x,y). Pen = (x,y). |
| `$C1 x y p` | FILL | Patterned flood fill seeded at (x,y) with pattern number `p`. The pen doesn't change. |
| `$C2 x y` | MOVE | Pen = (x,y). Nothing is drawn. |
| `$FF` | END | End of the command stream. |
| `$C3`–`$FE` | — | Reserved. Decoders stop at an unknown command. |

A typical outline is `MOVE` (or `PLOT`) followed by `LINE`s: 3 bytes to
start and 2 bytes per vertex.

### Line algorithm (normative)

The editor and the engine must produce the same pixels, so this is part of
the format. It is the ZX Basic `DRAW` Bresenham:

```
dx = |x1-x0|, dy = |y1-y0|, xi = sign(x1-x0) or +1, yi = sign(y1-y0) or +1
if dy >= dx:
    err = dy >> 1
    while y != y1:  err -= dx; if err < 0: err += dy; x += xi
                    y += yi; plot(x, y)
else:
    err = dx >> 1
    while x != x1:  err -= dy; if err < 0: err += dx; y += yi
                    x += xi; plot(x, y)
```

Plots with y ≥ 192 are ignored.

### Fill algorithm (normative)

This is splib2's `SPPFill`. Starting at the seed, every **unset** pixel
4-connected to it is reached. Each reached pixel is then set to the pattern
bit:

```
pixel(x,y) = (pattern[p][y & 7] >> (7 - (x & 7))) & 1
```

A seed on a set pixel does nothing. Zero bits in a pattern leave holes, and a
*later* fill can flow through those holes. This happens on the Spectrum too,
so the editor shows it.

### Patterns

The pattern table is **not** stored in the file. It belongs to the engine
(8 bytes per pattern, top line first, bit 7 = leftmost pixel), and the editor
must be given the same table (Fill tool → load/save). splib2-bitmapbg has
the 8 Suvleir 3 patterns built in:

```
0  FF FF FF FF FF FF FF FF   solid
1  77 FF FF FF 77 FF FF FF
2  77 FF DD FF 77 FF DD FF
3  55 FF 55 FF 55 FF 55 FF
4  AA 55 AA 55 AA 55 AA 55   checkerboard
5  FF 10 10 10 FF 01 01 01   bricks
6  88 00 22 00 88 00 22 00
7  88 00 00 00 88 00 00 00
```

## Hardness section

There are 768 cells (32 columns × 24 rows, one per 8×8 character cell), in row
major order, run-length encoded:

```
byte = (value << 6) | count        value 0..3, count 1..63
```

The runs cover exactly 768 cells. Values 0–3 mean whatever the game wants.
The convention, and the editor's colours, are 0 = free, 1 = solid,
2 and 3 = game-defined (spikes, water, triggers…).

A completely empty map takes 13 bytes (12 × 63 + 1 × 12).

## Example

A 16×16 box at (8,8), filled with the checkerboard, with its cells marked
solid:

```
12 00                     H = 18
C2 08 08                  MOVE 8,8
08 17                     LINE 23,8
17 17                     LINE 23,23
17 08                     LINE 8,23
08 08                     LINE 8,8
C1 0C 0C 04               FILL 12,12 with pattern 4
FF                        END
21 42 1E 42               hardness: 33×0, 2×1, 30×0, 2×1,
3F 3F 3F 3F 3F 3F 3F 3F             then 701×0 = 11×63 + 8
3F 3F 3F 08
```

## Legacy Suvleir 3 (`draw.exe`) files

The old format is the same apart from these differences, which the importer
handles:

* There is no `END` byte and no `MOVE`. The first two bytes give the hardness
  offset, and `gfxParser.bas` parsed commands **while pointer < offset − 2**,
  so a final 2-byte LINE would have been read as hardness data.
* `draw.exe`'s RLE encoder never wrote cell 768. The importer copies cell 767
  into it.
* Every file ends with a spurious `PLOT 0,0` (an uninitialised command slot).
  The importer drops it; `-keep-trailing-plot` keeps it.
* A count of 0 meant 256 (the decoder's byte wrap). The importer accepts it.

`suvleir4 -legacy` writes the old format, for engines that still use
`gfxParser`. It adds a dummy trailing `PLOT 0,0`, so no real command is lost
to the `offset − 2` quirk.
