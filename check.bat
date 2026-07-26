@echo off
REM ============================================================
REM  EdgeClock static checks — free, deterministic, no network.
REM  Run this before tagging a release. Everything it reports is
REM  produced by the compiler, so there are no false "opinions".
REM ============================================================
setlocal

echo.
echo [1/3] Full warning set (-Wall -Wextra)
echo ------------------------------------------------------------
g++ -fsyntax-only EdgeClock.cpp ^
    -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
if %errorlevel% neq 0 goto :failed

echo.
echo [2/3] Stricter pass (shadowing, format strings, null derefs)
echo ------------------------------------------------------------
REM -O2 is required for -Wnull-dereference to see through the data flow.
g++ -c EdgeClock.cpp -o nul -O2 ^
    -Wshadow -Wformat=2 -Wnull-dereference -Wcast-align ^
    -Wno-unused-parameter -Wno-missing-field-initializers
if %errorlevel% neq 0 goto :failed

echo.
echo [3/3] GDI/handle pairing audit
echo ------------------------------------------------------------
echo   Each create must have a matching release. Investigate any
echo   pair whose counts do not line up.
echo.
call :count "GetDC("               "ReleaseDC("
call :count "CreateCompatibleDC("  "DeleteDC("
call :count "CreatePopupMenu("     "DestroyMenu("
echo.
echo   (CreatePopupMenu may legitimately exceed DestroyMenu:
echo    DestroyMenu is recursive and frees attached submenus.)

echo.
echo All checks passed.
endlocal
pause
exit /b 0

:count
for /f %%A in ('findstr /c:%~1 EdgeClock.cpp ^| find /c /v ""') do set n1=%%A
for /f %%B in ('findstr /c:%~2 EdgeClock.cpp ^| find /c /v ""') do set n2=%%B
echo   %~1 = %n1%   vs   %~2 = %n2%
exit /b 0

:failed
echo.
echo CHECKS FAILED - see the output above.
endlocal
pause
exit /b 1
