@echo off
rem Build the CJMC3 example against the splib2 in this repository.
rem Needs z88dk (classic) in the PATH, Python 3, and ..\..\splib2f.lib built with
rem BITMAPBG = 1 (run ..\..\Makefile.bat first). zx0.exe is only needed if a
rem background or a screen layout changes; editing gfx\sprites.png needs nothing else.
python tools\conv.py
if errorlevel 1 goto end
cd src
zcc +zx -vn -m -unsigned -zorg=24200 -I..\..\.. -l..\..\..\splib2f -o cjmc3.bin cjmc3.c
appmake +zx -b cjmc3.bin -o ..\cjmc3.tap --org 24200
cd ..
:end
