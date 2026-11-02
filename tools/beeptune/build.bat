@echo off
rem Beeptune - Windows build with MinGW-w64 gcc (w64devkit, MSYS2...)
gcc -O2 -Wall -std=c99 -o beeptune.exe src\main.c src\app.c src\ui.c src\song.c src\bmp.c src\audio.c src\plat_win32.c -mwindows -static -lgdi32 -lcomdlg32 -lshell32 -lwinmm
