@echo off
rem Suvleir 4 - Windows build with MinGW-w64 gcc (w64devkit, MSYS2, TDM...)
rem Produces suvleir4.exe: a single file, no DLLs besides the ones in Windows.
gcc -O2 -std=c99 -o suvleir4.exe src\main.c src\app.c src\ui.c src\vec.c src\img.c src\plat_win32.c -mwindows -static -lgdi32 -lcomdlg32 -lshell32
if errorlevel 1 goto :eof
echo built suvleir4.exe
