# Vector screens: `sp_VecDraw`

splib2-bitmapbg can draw **Suvleir 4 (SVL)** vector screens: lines, plots and
patterned flood fills, plus an optional 32×24 hardness map. Make the screens
with the Suvleir 4 editor in `tools/suvleir4/`. The byte format is specified
in [`tools/suvleir4/docs/FORMAT.md`](../tools/suvleir4/docs/FORMAT.md).

A whole room fits in a few hundred bytes, and combined with
[BITMAPBG](BITMAPBG.md) splib2's sprites run on top of it.

## API

```c
void sp_VecDraw(void *svl, uchar *hardness, uchar stride);
void sp_VecConfig(void *patterns, void *fillstack, uint filldepth);
```

| | |
|---|---|
| `svl` | the SVL data (`incbin` the `.svl`, or use the editor's `.asm` / `.h` export) |
| `hardness` | where to write the 768 hardness values, or `NULL` to skip them |
| `stride` | bytes between consecutive cells in `hardness`: `1` for a plain `uchar[768]`, `4` for byte 1 of each display list entry (`sp_CompDListAddr(0,0) + 1`; BITMAPBG doesn't use that byte) |
| `patterns` | fill pattern table, 8 bytes per pattern, or `NULL` for the built-in Suvleir set |
| `fillstack` | if not `NULL`, `SPPFill` runs with SP set to this address. `NULL` uses the current stack |
| `filldepth` | `SPPFill` queue entries. It needs `3*filldepth + 30` bytes of stack. Default 300 |

`sp_VecDraw` draws on the **real screen** (`$4000`). The pen starts at (0,0).

From assembly: `HL` = data, `DE` = hardness destination (0 = none),
`A` = stride, `call SPVecDraw`. `IX` is preserved and `IY` is not used. The
configuration variables `SPVecPatterns`, `SPVecFillSP` and `SPVecFillDepth`
(all words) can be written directly, and `SPVecDefPatterns` is the built-in
table.

## Typical use with BITMAPBG

```c
/* once, after sp_Initialize */
dlist = sp_CompDListAddr(0, 0);
sp_VecConfig(0, (void *) (SP_BGSHADOW + 6144), 1000);

/* every room */
clear_screen_bitmap_and_attrs();       /* sprites gone from the display      */
sp_VecDraw(room_svl, dlist + 1, 4);    /* draw + hardness into the dlist      */
sp_BgCapture();                        /* screen -> shadow; all cells dirty   */
sp_UpdateNow();                        /* sprites back on top                 */

/* collisions */
uchar hard_at(uchar col, uchar row) { return dlist[((row << 5) + col) * 4 + 1]; }
```

### Why the fill stack goes in the shadow

`SPPFill` is a breadth-first fill that keeps its queue **on the stack**. Big
or ragged areas need hundreds of entries at 3 bytes each, which is more stack
than a 48K game usually has free. While a new room is being drawn, the shadow
screen's contents don't matter, because `sp_BgCapture()` overwrites them right
afterwards. So `sp_VecConfig(..., SP_BGSHADOW + 6144, 1000)` gives the fill
about 3 KB of queue at no cost. Interrupts stay enabled.

Don't do this if you draw vectors while the shadow holds something you need.
Pass `NULL` as `fillstack` and size the stack yourself.

If the queue fills up, `SPPFill` bails out and patterns what it has reached
so far. The editor doesn't model that case, so make the depth generous.

## Cost

Lines cost about 150–200 T-states per pixel; the plot uses `SPGetScrnAddr`.
Fills cost what `SPPFill` costs: they are the expensive part. A typical
Lhikoj room (about 200 lines and 20 fills) takes a second or two. The original
ZX Basic version drew at a similar speed, and players see the room being
drawn. Draw while the screen is blacked out if you prefer.

## Pixel exactness

The editor's renderer and `SPVecDraw` use the same line decisions and the same
fill rules (see FORMAT.md). This was checked by rendering the Lhikoj screens
with the editor and with the Z80 code (in an emulator), then comparing the
shadow screens: 0 pixels differ.
