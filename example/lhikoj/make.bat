@echo off
rem Build the Lhikoj example against the splib2 in this repository.
rem Needs z88dk (classic) in the PATH and ..\..\splib2f.lib built with BITMAPBG = 1
rem (run ..\..\Makefile.bat first). Needs Python 3 for the asset converter.
python tools_conv.py
cd src
zcc +zx -vn -m -unsigned -zorg=24200 -I..\..\.. -l..\..\..\splib2f -o lhikoj.bin lhikoj.c
appmake +zx -b lhikoj.bin -o ..\lhikoj.tap --org 24200
cd ..
