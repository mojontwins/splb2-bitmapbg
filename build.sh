#!/bin/sh
# Linux / macOS equivalent of Makefile.bat.
# Needs a z88dk classic toolchain (zcc, sccz80, z80asm) in the PATH and
# ZCCCFG / Z80_OZFILES set as usual. Produces splib2f.lib.
set -e
cd "$(dirname "$0")"
for d in cwrapper heap; do
  ( cd $d
    cp ../SPconfig.def .
    for f in $(tr -d '\r' < sp_c.lst); do
      # -make-lib makes zcc also try to assemble the crt0, which complains about
      # a missing zcc_opt.def: harmless, filtered out here.
      zcc +zx -vn -make-lib -I.. $f 2>&1 | grep -v -e "zcc_opt.def" -e "^Key to" -e "^/tmp/" -e "Warning" || true
      [ -f "${f%.c}.o" ] || { echo "failed: $d/$f"; exit 1; }
    done
    rm -f SPconfig.def zcc_opt.def )
done
tr -d '\r' < sp.lst | sed 's#\\#/#g' | grep -v '^$' > sp_unix.lst
z80asm -I. -d -ns -nm -Mo -DFORzx -xsplib2f @sp_unix.lst
rm -f sp_unix.lst
find . -name '*.o' -delete
echo "built splib2f.lib"
