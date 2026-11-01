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
* `example/lhikoj`: a complete game using the library.

## Earlier Mojon Twins changes (inherited)

* `FLASHASFG` option: attribute bit 7 marks a cell as foreground. Sprites
  are not drawn there, and the bit is masked out when the attribute is written
  to the screen. Note that this makes `SPUpdateNow` use `IY`.
* Trimmed module list in `sp.lst` (e.g. hashtable and huffman not built).
