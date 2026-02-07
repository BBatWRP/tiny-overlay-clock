@echo off
echo Compiling EdgeClock...
g++ -o EdgeClock.exe EdgeClock.cpp -lgdi32 -luser32 -lgdiplus -mwindows -static
if %errorlevel% neq 0 (
    echo Compilation failed!
    pause
    exit /b %errorlevel%
)
echo Compilation successful! EdgeClock.exe created.
pause
