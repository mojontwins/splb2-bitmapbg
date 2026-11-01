@echo off
rem Build Eleuterio against the splib2 in this repository, TILE MODE variant.
rem Needs z88dk (classic) in the PATH, Python 3, and ..\..\splib2t.lib: the
rem library built with BITMAPBG = 0. To make it on Windows: set
rem "DEFC BITMAPBG = 0" in ..\..\SPconfig.def, run ..\..\Makefile.bat, rename
rem the resulting splib2f.lib to splib2t.lib and set BITMAPBG back to 1.
rem Or copy ..\..\prebuilt\splib2t.lib to ..\..
if not exist ..\..\splib2t.lib copy ..\..\prebuilt\splib2t.lib ..\..
python tools\conv.py
if errorlevel 1 goto end
cd src
zcc +zx -vn -m -unsigned -zorg=24200 -I..\..\.. -l..\..\..\splib2t -o eleuterio.bin eleuterio.c
appmake +zx -b eleuterio.bin -o ..\eleuterio.tap --org 24200
cd ..
:end
