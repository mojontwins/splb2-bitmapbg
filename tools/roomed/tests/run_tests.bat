@echo off
rem roomed self tests (Windows): binary export against tools\conv.py and a
rem scripted UI session with the headless build. Needs gcc (MinGW) and Python 3.
setlocal
cd /d "%~dp0"
call ..\build.bat >nul || exit /b 1
call ..\build.bat headless >nul || exit /b 1
set R=..\roomed.exe
set ROOMS=..\..\..\gfx\rooms.txt
set FAIL=0
del /q out_* 2>nul
copy /y %ROOMS% out_rt1.txt >nul
copy /y ..\..\..\gfx\tileset.png . >nul
copy /y ..\..\..\gfx\templ.png . >nul
if not exist preview mkdir preview
copy /y ..\..\..\gfx\preview\*.png preview >nul
%R% -export out_rt1.txt out_rt1.h
python check_export.py out_rt1.h ..\..\..\src\assets.h || set FAIL=1
copy /y %ROOMS% out_ui.txt >nul
set ROOMED_SCRIPT=ui_session.txt
..\roomed-headless.exe out_ui.txt >nul
%R% -info out_session.txt > out_info.txt
findstr /c:"room 0 \"scene 0, room 1: empty\": 8 objects" out_info.txt >nul || (echo FAIL ui session & set FAIL=1)
findstr /b /c:"rect 5 10 78 " out_session.txt >nul || (echo FAIL ui: rect & set FAIL=1)
findstr /r /b /c:"pattern 6 15 64 [0-9]* 9 18 2 2" out_session.txt >nul || (echo FAIL ui: moved pattern & set FAIL=1)
findstr /b /c:"char 20 3 33 " out_session.txt >nul || (echo FAIL ui: char & set FAIL=1)
findstr /b /c:"crate 20 15 33 " out_session.txt >nul || (echo FAIL ui: crate & set FAIL=1)
echo ui session: done (screenshots in tests\out_*.bmp)
if %FAIL%==0 echo ALL TESTS PASSED
exit /b %FAIL%
