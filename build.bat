@echo off
echo Compiling EdgeClock...
windres EdgeClock.rc -o resource.o

REM Flag notes (all measured, see CLAUDE.md):
REM   -Os ... --gc-sections -s   smaller image => fewer resident code pages
REM   -fno-exceptions -fno-rtti  nothing here throws or uses typeid/dynamic_cast;
REM                              drops most of the static C++ runtime
REM   -Wall -Wextra              the tree is warning-clean, so anything new here
REM                              is worth reading. Run check.bat for more.
g++ -o EdgeClock.exe EdgeClock.cpp resource.o ^
    -Os -fno-exceptions -fno-rtti ^
    -ffunction-sections -fdata-sections -Wl,--gc-sections -s ^
    -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers ^
    -lgdi32 -luser32 -lgdiplus -lcomdlg32 -lole32 -luuid -mwindows -ldwmapi -lcomctl32 -luxtheme -static

if %errorlevel% neq 0 (
    echo Compilation failed!
    pause
    exit /b %errorlevel%
)
echo Compilation successful! EdgeClock.exe created.
if exist resource.o del resource.o
pause
