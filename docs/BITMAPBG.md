# BITMAPBG: how it works and how to use it

This document covers the bitmap-background mode only. For the rest of Sprite
Pack (sprites, block allocator, interrupts, input…) see
`SP22-original-readme.txt` and `spritepack.h`.

1. [splib2's model in 60 seconds](#1-splib2s-model-in-60-seconds)
2. [What BITMAPBG changes](#2-what-bitmapbg-changes)
3. [Setting it up](#3-setting-it-up)
4. [Memory layout](#4-memory-layout)
5. [Getting a bitmap into the buffer](#5-getting-a-bitmap-into-the-buffer)
6. [Changing the background while sprites are live](#6-changing-the-background-while-sprites-are-live)
7. [Attributes](#7-attributes)
8. [What changes and what doesn't](#what-changes-and-what-doesnt)
9. [Costs](#9-costs)
10. [Gotchas](#10-gotchas)
11. [Design notes](#11-design-notes)

---

## 1. splib2's model in 60 seconds

* **Display list** (`SPDisplayList`): 4 bytes per character cell, in left to
  right, top to bottom order (3072 bytes for 32×24):
  * byte 0: **attribute**
  * byte 1: **tile number**
  * bytes 2–3: pointer (big endian) to the first sprite *char struct* in that
    cell, or 0.
* **Dirty chars** (`SPDirtyChars`): 1 bit per cell, 96 bytes.
* **`SPUpdateNow`** walks the dirty bits. For each dirty cell (in `NOFLICKER`
  mode) it:
  1. copies the cell's background (8 bytes) into an 8-byte buffer;
  2. composes every sprite in the cell on top (mask/OR/XOR/load, rotated
     0–7 px using the tables at `SProtatetbl`);
  3. copies the buffer to the screen and writes the attribute.

  If the first sprite in the cell is a *load* sprite, the background step is
  skipped. If there are no sprites, the background goes straight to the
  screen.
* Sprite functions (`sp_MoveSprAbs` and friends) and `sp_Invalidate` set
  the dirty bits.

## 2. What BITMAPBG changes

In stock splib2 the background bytes come from
`SPTileArray[tile#]` → pointer → 8 bytes. With `BITMAPBG = 1` they come
from the **shadow screen**: 6144 bytes at `BGSHADOW*256` laid out *exactly*
like the display file (`$4000`).

Because the layout is identical, the background of a cell is at the same
offset as the cell on screen. `SPUpdateNow` always knows the screen address
of the cell it is drawing, so:

```
shadow = screen_address + (BGSHADOW*256 - $4000)   ; ADD on the high byte only
line n = shadow + n*256                            ; INC H, just like the screen
```

The two places in `SPUpdateNow.asm` that read the tile array now do this:

* **`tileonly`** (cell has no sprites): the screen address is in `DE`.
  `H = D + (BGSHADOW-$40)`, `L = E`, then 8× `ld a,(hl) / ld (de),a / inc h
  / inc d`, straight from the shadow to the screen.
* **`notload`** (cell has sprites, the first one isn't a load sprite): the
  screen address was pushed at the start of `drawchar`. It is read from
  `SP+2`, offset the same way, and 8 bytes go into the `NOFLICKER` buffer.

Nothing else changes. Both blocks are inside `IF BITMAPBG … ELSE … ENDIF`, so
`BITMAPBG = 0` gives you the original code.

## 3. Setting it up

`SPconfig.def`:

```
DEFC DISP_SPECTRUM = 1     ; BITMAPBG only supports the Spectrum display
DEFC DISP_HICOLOUR = 0
DEFC DISP_HIRES    = 0
DEFC DISP_TMXDUAL  = 0
DEFC NOFLICKER     = 1     ; required (assembly fails otherwise)
DEFC BITMAPBG      = 1
DEFC BGSHADOW      = $D8   ; MSB of the shadow address, 256-byte aligned
```

The helpers respect `SP_ROWSTART` / `SP_ROWEND` for attributes and dirty
bits. The shadow always holds the full 192 lines, because the address mapping
needs it. Only the full-screen setting (0–24) has been tested.

In C, `spritepack.h` defines `SP_BGSHADOW` as `0xD800`. If you change
`BGSHADOW`, `#define SP_BGSHADOW <BGSHADOW*256>` before the include.

Then rebuild the library (`Makefile.bat` / `build.sh`).

Initialisation is the usual splib2 sequence. **Order matters**:

```c
sp_AddMemory(0, NBLOCKS, 14, ADDR);    /* 1. block memory (hooks use it too!) */
sp_InitIM2(0xf1f1);                    /* 2. IM2 table at IM2TABLE           */
sp_CreateGenericISR(0xf1f1);           /* 3. ISR in the gap above the table   */
sp_RegisterHook(255, hook);            /* 4. optional frame counter, etc.     */
sp_Initialize(attr, 0);                /* 5. dlist (attr everywhere), dirty,  */
                                       /*    rotation tables                  */
```

`sp_Initialize`'s tile argument is irrelevant in this mode. Its colour
argument sets every cell's attribute.

## 4. Memory layout

The shadow screen needs 6144 bytes at a 256-aligned address. It must not
overlap splib2's fixed areas. With the default `IM2TABLE = SProtatetbl = $F000`
the top of RAM looks like this:

```
$D800-$EFFF  shadow screen (BGSHADOW = $D8)            6144
$F000-$F100  IM2 vector table                           257
$F101-$F1F0  free (240 bytes)
$F1F1-       generic ISR (if you use sp_CreateGenericISR)
$F200-$FFFF  horizontal rotation tables                3584
```

Your program, the display list (3 KB, part of the binary), the stack and the
sprite block memory go below `$D800`. On a 48K machine with `org 24200`, that
leaves about 29 KB for code + data + display list + stack.

**Tip: the shadow is a big scratch buffer.** Between `sp_BgShow()` and
`sp_BgCapture()` its contents don't matter, because `sp_BgCapture()`
overwrites them. The Lhikoj example uses that window to run `SPPFill` (which
keeps its queue on the stack) with the stack pointer moved to the top of the
shadow. That gives it 3 KB of fill queue for free.

**Tip: the tile byte is yours.** Byte 1 of every display-list entry is no
longer read by the updater: 768 bytes in a handy per-cell layout. The example
stores its collision map there (`SPCompDListAddr(row,col) + 1`). Only
`sp_Initialize`, `sp_PrintAt*`, `sp_PutTiles` and `sp_ClearRect` (tiles)
write to it.

## 5. Getting a bitmap into the buffer

### 5.1 Draw on the screen, then capture (simplest)

Use any routine that draws to `$4000`: ROM `PLOT`/`DRAW`, your own line code,
`sp_PFill`, a `.scr` depacker, printing…

```c
sp_BgShow();            /* or clear the screen: no sprite pixels must be visible */
draw_scene();           /* draw on $4000 / $5800                                */
sp_BgCapture();         /* bitmap -> shadow, attrs -> dlist byte 0, all dirty    */
sp_UpdateNow();         /* sprites reappear on top                               */
```

`sp_BgCapture()`:

```
ldir $4000 -> BGSHADOW*256, 6144 bytes
for each managed cell: dlist[cell*4+0] = attr[cell]
SPDirtyChars = $FF...  (all managed cells dirty)
```

Between `sp_BgShow()` and `sp_UpdateNow()` the sprites are not on screen.
That is usually 2–4 frames: fine for screen changes and pop-up messages.

### 5.2 Write directly into the shadow

The shadow behaves exactly like the display file at a different address, so
anything that writes screen data with a configurable base works on it
directly: a depacker targeting `SP_BGSHADOW`, a char printer, a tile blitter.

```c
/* put an 8x8 graphic in cell (row, col) of the background */
uchar *p = sp_BgAddr(row, col);
for (i = 0; i < 8; i++) { *p = gfx[i]; p += 256; }

*((uchar *) sp_CompDListAddr(row, col)) = attr;   /* colour of that cell */

struct sp_Rect r; r.row_coord = row; r.col_coord = col; r.height = 1; r.width = 1;
sp_Invalidate(&r, &clip_fullscreen);               /* redraw it next update */
```

To depack a whole `.scr` (6912 bytes):

```c
depack(scr_data, (void *) SP_BGSHADOW);            /* bitmap part lands in the shadow  */
/* the attribute part (at SP_BGSHADOW + 6144, 768 bytes) is then copied
   into byte 0 of each display-list cell, then call sp_Invalidate on all */
```

Watch out: in the default layout, `SP_BGSHADOW + 6144` is the IM2 table. If
the data includes attributes, depack to the real screen and use
`sp_BgCapture()` instead, or move the shadow.

### 5.3 Saving and restoring parts of the background

The layout is the display file's, so **each screen third is one contiguous
2048-byte block**:

```
third 0 (rows 0-7):   SP_BGSHADOW + 0
third 1 (rows 8-15):  SP_BGSHADOW + 2048
third 2 (rows 16-23): SP_BGSHADOW + 4096
```

A message box over the middle third can be saved and restored with one 2 KB
copy, plus the 256 attribute bytes from the display list (every 4th byte from
`sp_CompDListAddr(8,0)`). After restoring, invalidate rows 8–15.

## 6. Changing the background while sprites are live

* **Small changes** (a tile, a char, a counter): write into the shadow
  (§5.2) and invalidate those cells. The sprites in them are recomposed on
  top at the next `sp_UpdateNow()`. You never touch the screen.
* **Big changes** (new room, overlays drawn with screen routines):
  `sp_BgShow()` → draw on screen → `sp_BgCapture()`. If the sprites would get
  in the way of the overlay (e.g. a message box), hide them first by moving
  them outside the clip rectangle, then `sp_UpdateNow()`.
* **Effects directly on the screen**: splib2 only rewrites *dirty* cells. You
  can animate an area of the real screen yourself (parallax sky, scrolling
  text) as long as no sprite enters it and you don't invalidate it. The next
  invalidation of that area restores the shadow's contents.

## 7. Attributes

Attributes are **not** taken from the shadow. They stay in display-list byte
0, as in stock splib2. `SPUpdateNow` writes each redrawn cell's attribute to
`$5800`, and sprites take the colour of the cell. `sp_BgCapture()` copies the
screen attributes in, and `sp_BgShow()` copies them out.

With `FLASHASFG = 1`, attribute bit 7 means *"don't draw sprites in this
cell"* (foreground cells). The bit is masked out when the attribute is written
to the screen, both by `SPUpdateNow` and by `sp_BgShow()`.

<a name="what-changes-and-what-doesnt"></a>
## 8. What changes and what doesn't

| Unchanged | Changed / no longer meaningful |
|---|---|
| Sprite creation, movement, animation (`BC` displacement), planes, types, clipping | Background graphics come from the shadow, not `SPTileArray` |
| Dirty-cell tracking, `sp_Invalidate`, `sp_Validate`, `sp_UpdateNow` | Tile numbers (dlist byte 1) are ignored by the updater |
| Attributes (dlist byte 0), `FLASHASFG` | `sp_TileArray`, `sp_PrintAt*` tile graphics, `sp_PrintString`, `sp_PutTiles`/`sp_GetTiles` graphics: they still update the display list (and attributes still work) but the picture comes from the shadow |
| Block allocator, interrupts, input, AY, misc | Only `DISP_SPECTRUM` + `NOFLICKER` are supported with `BITMAPBG` |

## 9. Costs

* `SPUpdateNow` per dirty cell: about the same as stock. The tile lookup
  (`add`/`adc` into `SPTileArray`, two loads) is replaced by an `add` on the
  high byte. In `notload` it also reads the screen address back from the
  stack (~40 T).
* `sp_BgCapture()`: 6144-byte `ldir` (~129,000 T) + 768 attribute copies
  (~34,000 T) + dirty fill (~2,000 T). That's about **165,000 T, ~2.4 frames**,
  followed by a full `sp_UpdateNow()`.
* `sp_BgShow()`: about **185,000 T, ~2.6 frames**.
* Memory: +6144 bytes for the shadow. `SPTileArray` (512 bytes) isn't linked
  unless you call something that references it.

## 10. Gotchas

1. **`NOFLICKER` is required.** The `notload` path writes into the composition
   buffer, and assembly stops with an error otherwise.
2. **Never capture sprite pixels.** `sp_BgCapture()` copies whatever is on
   the screen. Call `sp_BgShow()` (or draw on a clean screen) first.
3. **Interrupts and IY.** `SPUpdateNow` with `FLASHASFG = 1` uses `IY`, as do
   several other splib2 functions (see the original readme). Use IM2 (as
   above), not the ROM's IM1 handler.
4. **`sp_AddMemory` before `sp_RegisterHook`.** Hook registration allocates
   through `u_malloc`.
5. **A hook needs 2 spare bytes in front of it.** The generic ISR uses them as
   the list link. Write hooks in asm after a `defw 0`:
   ```c
   extern void my_hook(void);
   #asm
       defw 0
   ._my_hook
       ld hl,_frames
       inc (hl)
       ret
   #endasm
   ```
6. **`SPNullSprPtr` is recognised by its page.** `SPMoveSprAbs` doesn't
   animate graphic pointers whose high byte equals `SPNullSprPtr/256`. Pointing
   the empty right-hand column of a sprite at `SPNullSprPtr` saves data (no
   blank column per frame). Never place real sprite graphics in that 256-byte
   page.
7. **Shadow placement.** It must be 256-byte aligned and must not overlap
   `IM2TABLE`, the ISR or `SProtatetbl+$200…+$FFF`. `BGSHADOW` (asm) and
   `SP_BGSHADOW` (C) must agree.

## 11. Design notes

Another way to do this is to keep the display list but **reinterpret its
bytes**: take the attribute from a separate attribute map (or the bitmap's own
attributes) and use bytes 0–1 as a **pointer to the cell's 8 background
bytes**. That's more flexible: cells can share graphics, you need only 8
bytes per *unique* cell, and you can scroll or flip by rewriting pointers. The
cost is 768 pointers to maintain.

The screen-layout shadow was chosen because it needs **no per-cell data at
all**. The address comes from the screen address the updater already has, it
is as fast as the tile path, any screen-drawing code can be reused, screen
thirds can be saved with one block copy, and display-list byte 1 becomes
free. Both approaches touch only the same two spots in `SPUpdateNow`.
