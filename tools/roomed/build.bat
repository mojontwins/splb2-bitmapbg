@echo off
rem roomed - Windows build with MinGW-w64 gcc (w64devkit, MSYS2, TDM...)
rem Produces roomed.exe: a single file, no DLLs besides the ones in Windows.
rem   build.bat            the editor
rem   build.bat headless   roomed-headless.exe, the scripted test build
cd /d "%~dp0"
if "%1"=="headless" goto headless
gcc -O2 -std=c99 -o roomed.exe src\main.c src\app.c src\rooms.c src\ui.c src\img.c src\plat_win32.c -mwindows -static -lgdi32 -lcomdlg32 -lshell32
if errorlevel 1 exit /b 1
echo built roomed.exe
exit /b 0
:headless
gcc -O2 -std=c99 -o roomed-headless.exe src\main.c src\app.c src\rooms.c src\ui.c src\img.c src\plat_headless.c
if errorlevel 1 exit /b 1
echo built roomed-headless.exe
