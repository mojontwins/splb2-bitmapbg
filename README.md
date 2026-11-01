# splib2 BITMAPBG — Sprite Pack 2.2 with bitmapped backgrounds

A fork of **Sprite Pack 2.2** (`splib2`) by Alvin Albrecht for the ZX Spectrum
and z88dk. It is based on the Mojon Twins' working copy (with the
`FLASHASFG` addition). It adds one compile-time option, **`BITMAPBG`**.

With `BITMAPBG = 1`, splib2 no longer builds the background out of 8×8 tiles.
Each cell's background is taken from a **6144-byte shadow screen**: a copy of
the Spectrum bitmap, with exactly the display-file layout, at a fixed
256-byte-aligned address. You can put **any bitmap** behind splib2's masked,
pixel-positioned sprites: vector art drawn at run time, a decompressed
`.scr`, a full-screen picture, or tiles you draw yourself. You get to keep
everything splib2 is good at: differential (dirty-cell) updates, flicker-free
compositing, masked/OR/XOR/load sprites, 0–7 px rotation tables, planes and
clipping.

```
            your drawing code                 sp_BgCapture()
  (ROM, lines, fills, depackers, ...) ──► $4000 ───────────────► shadow screen ($D800)
                                                  attrs ───────► display list byte 0
                                                                       │
  sp_MoveSprAbs(...) ──► sprite lists ──┐                              │
                                        ▼                              ▼
                         SPUpdateNow: for every dirty cell:  background from the shadow
                                     + sprites → buffer → screen, attribute → screen
```

* The library change is small: two blocks in `updater/SPUpdateNow.asm`, the
  option in `SPconfig.def`, and a few helpers (`sp_BgCapture`, `sp_BgShow`,
  `sp_BgAddr`, and for painting straight into the shadow `sp_BgPutTile`,
  `sp_BgAttrFill`, `sp_BgThird` and the `sp_ZX0` depacker).
* With `BITMAPBG = 0` it builds the original tile-based library.
* **Vector screens**: `sp_VecDraw` draws Suvleir 4 (SVL) screens (lines,
  patterned fills and a hardness map) made with the bundled editor,
  [`tools/suvleir4`](tools/suvleir4). It runs on Windows and Linux, and on
  macOS through XQuartz.
* Two complete games:
  * [`example/lhikoj`](example/lhikoj): vector backgrounds, pixel-by-pixel
    masked sprites, 25 fps.
  * [`example/cjmc3`](example/cjmc3): ZX0-packed bitmap thirds plus tiles,
    all painted straight into the shadow (no screen capture), pixel-by-pixel
    platform action at 25 fps.

**Documentation:**

* [`docs/BITMAPBG.md`](docs/BITMAPBG.md): how it works, memory layout, and how
  to get bitmaps into the buffer (with code). **Start here.**
* [`docs/VECTOR.md`](docs/VECTOR.md): vector screens with `sp_VecDraw`, and
  [`tools/suvleir4/README.md`](tools/suvleir4/README.md) for the editor.
* [`docs/TOOLCHAIN.md`](docs/TOOLCHAIN.md): the exact z88dk version used, and
  how to build it on modern systems.
* [`docs/SP22-original-readme.txt`](docs/SP22-original-readme.txt): Alvin
  Albrecht's original readme. Everything in it about the API still applies.
* [`CHANGES.md`](CHANGES.md): what differs from stock Sprite Pack 2.2.

---

## Quick start

### 1. Configure

`SPconfig.def` ships with these settings:

```
DISP_SPECTRUM = 1        ; normal Spectrum display (required by BITMAPBG)
NOFLICKER     = 1        ; required by BITMAPBG
SP_ROWSTART   = 0, SP_ROWEND = 24
IM2TABLE      = $F000, SProtatetbl = $F000   (rotation tables at $F200-$FFFF)
FLASHASFG     = 1        ; attr bit 7 = "don't draw sprites in this cell"
BITMAPBG      = 1
BGSHADOW      = $D8      ; shadow screen at $D800-$EFFF
```

If you move the shadow, change `BGSHADOW` here **and** define `SP_BGSHADOW`
(the full address, e.g. `0xC000`) before including `spritepack.h`.

### 2. Build the library

* **Windows** (z88dk 1.10-style setup): `Makefile.bat`. It needs
  `GenCompile.exe`, which is included in `cwrapper/` and `heap/`.
* **Linux / macOS**: `./build.sh`.

Either one produces `splib2f.lib`. Copy it to `z88dk/lib/clibs/` and
`spritepack.h` to `z88dk/include/`, or point `zcc` at them as the example
does. A prebuilt library with the default settings is in
`prebuilt/splib2f.lib`.

### 3. Use it

```c
#include <spritepack.h>

void *my_malloc(uint b) { return sp_BlockAlloc(0); }
void *u_malloc = my_malloc;
void *u_free   = sp_FreeBlock;

struct sp_Rect clip = {0, 0, 24, 32};

main() {
    #asm
        di
    #endasm
    sp_AddMemory(0, 30, 14, 0xD800 - 30*15);   /* BEFORE sp_RegisterHook */
    sp_InitIM2(0xf1f1);
    sp_CreateGenericISR(0xf1f1);
    /* sp_RegisterHook(255, my_hook);   (hook needs 2 spare bytes before it) */
    #asm
        ei
    #endasm
    sp_Initialize(INK_BLACK | PAPER_WHITE, 0);

    draw_my_background();      /* anything that draws on the real screen */
    sp_BgCapture();            /* screen -> shadow + attrs, everything dirty */
    /* ...or paint the shadow directly, without touching the screen:      */
    /*   sp_ZX0(packed_third, sp_BgThird(1));                             */
    /*   sp_BgAttrFill(INK_CYAN | PAPER_BLACK);                           */
    /*   sp_BgPutTile(row, col, attr, tile_gfx);                          */

    /* create sprites as usual, then each frame: */
    /*   sp_MoveSprAbs(spr, &clip, anim, y>>3, x>>3, x&7, y&7); */
    /*   sp_UpdateNow();                                         */
}
```

### API added

| C | asm | What it does |
|---|---|---|
| `void sp_BgCapture(void)` | `SPBgCapture` | Real screen → background. Copies the 6144-byte bitmap into the shadow, copies the attributes into display-list byte 0, and marks every cell dirty. The screen must not contain sprite pixels at that point. |
| `void sp_BgShow(void)` | `SPBgShow` | Background → real screen. Copies the bitmap and attributes. The screen then shows the background with no sprites, which is the clean state for drawing over it before another `sp_BgCapture()`. |
| `void *sp_BgAddr(uchar row, uchar col)` | `SPBgAddr` (A=row, C=col → HL) | Shadow address of the top pixel line of a cell. The other 7 lines are at +256 each. |
| `void sp_BgPutTile(uchar row, uchar col, uchar attr, void *gfx)` | `SPBgPutTile` (A=row, C=col, B=attr, DE=gfx) | 8×8 tile straight into the shadow, plus the cell's attribute; marks the cell dirty. |
| `void sp_BgAttrFill(uchar attr)` | `SPBgAttrFill` (A) | Every cell's attribute = `attr`; everything dirty. |
| `void sp_ZX0(void *src, void *dst)` | `SPZX0Depack` (HL, DE) | ZX0 depacker (Einar Saukas): e.g. a packed third straight into `sp_BgThird(n)`. |
| `SP_BGSHADOW` | `BGSHADOW*256` | Base address of the shadow. |
| `sp_BgThird(n)` | `BGSHADOW*256 + n*2048` | Address of screen third `n` in the shadow. |
| `void sp_VecDraw(void *svl, uchar *hard, uchar stride)` | `SPVecDraw` (HL, DE, A) | Draws an SVL vector screen on the real screen. Optionally decodes its hardness map, with `stride` bytes between cells (4 = straight into the display list's free tile byte). |
| `void sp_VecConfig(void *pats, void *fillstack, uint depth)` | `SPVecPatterns`, `SPVecFillSP`, `SPVecFillDepth` | Fill pattern table (NULL = built-in), the stack the fill runs on (NULL = current), and the fill queue depth. |

Everything else is standard splib2. Functions that only deal with tile
*graphics* (`sp_TileArray`, `sp_PrintAt*`'s tile number, `SPTileArray`) still
work, but they no longer affect what is drawn. See
[docs/BITMAPBG.md](docs/BITMAPBG.md#what-changes-and-what-doesnt).

---

## Repository layout

```
SPconfig.def, sp.lst, Makefile.bat, build.sh, spritepack.h   library config / build
bitmapbg/          new: SPBgCapture, SPBgShow, SPBgAddr, SPBgPutTile,
                   SPBgAttrFill, SPZX0Depack
vector/            new: SPVecDraw (Suvleir 4 vector screens)
tools/suvleir4/    the vector screen editor (C, Win32/X11)
updater/           SPUpdateNow.asm (BITMAPBG patch) and the rest of the updater
cwrapper/          C wrappers (now including sp_Bg*, sp_ZX0, sp_Vec*)
...                all other Sprite Pack 2.2 modules, unchanged
prebuilt/          splib2f.lib built with the default SPconfig.def
docs/              documentation
example/lhikoj/    complete game using the library (vector screens)
example/cjmc3/     complete game using the library (packed thirds + tiles)
```

## Credits and licence

Sprite Pack 2.2 © Alvin Albrecht 2002–2003. His terms: *"You have the right to
use and abuse this software as you see fit but please include an
acknowledgement in your projects."* The same applies to this fork. See
[`NOTICE.md`](NOTICE.md).

Fork maintenance, `FLASHASFG` and `BITMAPBG`: The Mojon Twins.

The ZX0 depacker (`bitmapbg/SPZX0Depack.asm`) is by Einar Saukas and
introspec, BSD 3-Clause licence; see [`NOTICE.md`](NOTICE.md).
