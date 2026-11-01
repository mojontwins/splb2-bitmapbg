#!/bin/sh
# Linux / macOS equivalent of Makefile.bat.
# Needs a z88dk classic toolchain (zcc, sccz80, z80asm) in the PATH and
# ZCCCFG / Z80_OZFILES set as usual.
#
#   ./build.sh          splib2f.lib with SPconfig.def as it is (BITMAPBG = 1:
#                       backgrounds come from the 6K shadow screen)
#   ./build.sh tiles    splib2t.lib: the same configuration but BITMAPBG = 0,
#                       i.e. classic tile backgrounds. Nothing uses the
#                       shadow screen, so its 6K are free for your program.
#                       (Built in a temporary copy; SPconfig.def is untouched.)
set -e
cd "$(dirname "$0")"

if [ "$1" = "tiles" ]; then
  tmp=$(mktemp -d)
  trap 'rm -rf "$tmp"' EXIT
  # copy the library sources only (not examples, tools or docs)
  tar cf - --exclude=./example --exclude=./tools --exclude=./docs --exclude=./prebuilt \
      --exclude=./.git --exclude='*.lib' . | (cd "$tmp" && tar xf -)
  sed -i.bak 's/^\( *DEFC BITMAPBG *= *\)1/\10/' "$tmp/SPconfig.def"
  grep -q 'DEFC BITMAPBG *= *0' "$tmp/SPconfig.def" || { echo "could not set BITMAPBG = 0"; exit 1; }
  "$tmp/build.sh"
  cp "$tmp/splib2f.lib" splib2t.lib
  echo "built splib2t.lib (BITMAPBG = 0, tile backgrounds)"
  exit 0
fi

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
