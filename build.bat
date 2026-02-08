@echo off
echo Compiling EdgeClock...
windres EdgeClock.rc -o resource.o
g++ -o EdgeClock.exe EdgeClock.cpp resource.o -lgdi32 -luser32 -lgdiplus -lcomdlg32 -mwindows -ldwmapi -lcomctl32 -static
if %errorlevel% neq 0 (
    echo Compilation failed!
    pause
    exit /b %errorlevel%
)
echo Compilation successful! EdgeClock.exe created.
if exist resource.o del resource.o
pause
