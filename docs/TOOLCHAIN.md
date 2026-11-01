# Toolchain

This library is written for the **classic z88dk** toolchain of the 1.10 era:
`zcc` + `sccz80` + the old `z80asm` with `XLIB` / `LIB` / `XREF`, and the
`Z80LMF01` library format. Modern z88dk 2.x dropped splib2 in favour of
SP1, and its assembler syntax differs.

## Version used to build and test this repository

| Tool | Version |
|---|---|
| z88dk sources | GitHub `z88dk/z88dk` commit **`0ba252b41f3c0e9d3d8b907ece1b73c118501042`** (2012-07-10), the CVS-era trunk between 1.10 and the 1.99 series |
| `zcc` | v2.59 (16.4.2010, D.J. Morris) |
| `sccz80` | Small-C/Plus, Version 20100416.1 |
| `z80asm` | Z80 Module Assembler **V1.0.31** (13.6.2009), built from `src/z80asm.stable` |
| `appmake` | from the same tree (`+zx` creates a TAP with a BASIC loader that does `CLEAR org-1`) |

The Windows **z88dk 1.10** release ("z88dk10") is the same generation: same
compiler, same assembler, same library format. The original Mojon Twins
`splib2f.lib` was built with it. Rebuilding this library *without* the
BITMAPBG changes using the toolchain above gives a library of exactly the
same size as that one.

## Building that z88dk on modern Linux / macOS

```sh
git clone --filter=tree:0 --no-checkout https://github.com/z88dk/z88dk.git z88dk
cd z88dk
git checkout 0ba252b41f3c0e9d3d8b907ece1b73c118501042
mkdir -p bin
export PATH=$PWD/bin:$PATH Z80_OZFILES=$PWD/lib/ ZCCCFG=$PWD/lib/config/
make -e CC=gcc CFLAGS="-g -O1 -fcommon -std=gnu89 -w" CCOPT=-DUNIX

# IMPORTANT on 64-bit hosts: the src/z80asm of this snapshot is a work in
# progress and writes object files the linker can't read ("Cannot read
# from file" at link time). Use the stable assembler instead:
make -C src/z80asm.stable CC=gcc CFLAGS="-g -O1 -fcommon -std=gnu89 -w" CCOPT=-DUNIX
cp src/z80asm.stable/z80asm bin/z80asm

# Only the libraries a Spectrum program needs:
make -C libsrc z80_crt0.lib zx_clib.lib ndos.lib malloc.lib
make -C libsrc install
```

`-fcommon -std=gnu89` keeps modern GCC happy with 2012-era C.

While libraries are built with `zcc -make-lib` (z88dk's own and this one),
zcc also tries to assemble the startup code and prints `Cannot open file
'zcc_opt.def'`. That is harmless: the object files are still produced.
`build.sh` filters the message out. `Makefile.bat` on Windows shows it.

Then build this library with `./build.sh`, and keep the three environment
variables above set when you use `zcc`.

## Known sccz80 quirk (affects user code, not the library)

Indexing a cast constant is miscompiled: `((unsigned char *) 0xF101)[i]` reads
`*(0xF101)` and ignores `i`. Use a pointer variable:

```c
unsigned char *buf = 0xF101;   /* then buf[i] works */
```
