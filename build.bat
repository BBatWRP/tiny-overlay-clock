@echo off
echo Compiling EdgeClock...
windres EdgeClock.rc -o resource.o

REM -Wall -Wextra is deliberate: the compiler is the cheapest bug finder we
REM have, and the build is warning-clean, so anything new that appears here is
REM worth reading. Run check.bat for the stricter pass.
g++ -o EdgeClock.exe EdgeClock.cpp resource.o ^
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
