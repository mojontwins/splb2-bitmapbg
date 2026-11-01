# Changes from Sprite Pack 2.2

## BITMAPBG (this fork)

* `SPconfig.def`: new options `BITMAPBG` (0/1) and `BGSHADOW` (MSB of the
  6144-byte shadow screen, 256-byte aligned). Defaults: `BITMAPBG = 1`,
  `BGSHADOW = $D8`.
* `updater/SPUpdateNow.asm`: in `tileonly` and `notload`, when `BITMAPBG`
  is set, the background of a cell is read from the shadow screen at
  `screen_address + (BGSHADOW*256 - $4000)` instead of through
  `SPTileArray`. `BITMAPBG` requires `NOFLICKER` (assembly error otherwise).
* New modules in `bitmapbg/`: `SPBgCapture`, `SPBgShow`, `SPBgAddr`, with C
  wrappers `sp_BgCapture`, `sp_BgShow`, `sp_BgAddr` in `cwrapper/` and
  prototypes plus `SP_BGSHADOW` in `spritepack.h`.
* `sp.lst`, `cwrapper/sp_c.lst`, `Makefile.bat`: new modules added.
* `build.sh`: Unix equivalent of `Makefile.bat`.
* Text sources normalised to LF line endings (`.bat` files stay CRLF, see
  `.gitattributes`).
* `vector/SPVecDraw.asm` with C wrappers `sp_VecDraw` and `sp_VecConfig`:
  draws Suvleir 4 (SVL) vector screens and decodes their hardness maps.
* `tools/suvleir4`: a portable vector screen editor written in C, replacing
  `draw.exe`, plus the SVL format specification.
* `example/lhikoj`: a complete game using the library. Its screens are SVL
  files drawn with `sp_VecDraw`.
* Painting straight into the shadow, without drawing on the screen and
  capturing it: `SPBgPutTile` / `sp_BgPutTile` (8×8 tile + attribute + dirty
  bit), `SPBgAttrFill` / `sp_BgAttrFill` (all attributes + everything
  dirty), the `sp_BgThird(n)` macro, and `SPZX0Depack` / `sp_ZX0`, Einar
  Saukas' ZX0 "turbo" depacker (BSD 3-Clause).
* `example/cjmc3`: a second complete game, built only with those helpers:
  ZX0-packed screen thirds and tile layouts painted into the shadow.
* `build.sh tiles` builds `splib2t.lib`, the library with `BITMAPBG = 0`
  (stock tile backgrounds), in a temporary copy; `prebuilt/splib2t.lib`.
  `Makefile.bat` now names its output `splib2f.lib`, as documented.
* `example/eleuterio`: a third game, using the tile-mode library and the
  RAM that the shadow screen would take.
* `build.sh variant NAME SYMBOL=VALUE...`: library builds with other
  `SPconfig.def` settings (`tiles` is now one of them).
* `example/phantomasa`: a 128K game in one load (screens in RAM banks,
  the original music player kept at its address by moving splib2's
  tables with a variant build).

## Earlier Mojon Twins changes (inherited)

* `FLASHASFG` option: attribute bit 7 marks a cell as foreground. Sprites
  are not drawn there, and the bit is masked out when the attribute is written
  to the screen. Note that this makes `SPUpdateNow` use `IY`.
* Trimmed module list in `sp.lst` (e.g. hashtable and huffman not built).
