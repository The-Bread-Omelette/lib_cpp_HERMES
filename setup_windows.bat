@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   Hermes Build System (Windows/MinGW)
echo ========================================
echo.

REM Detect make command
set MAKE_CMD=
for %%m in (mingw32-make make gmake) do (
    where %%m >nul 2>&1
    if !errorlevel! equ 0 (
        set MAKE_CMD=%%m
        goto :make_found
    )
)
:make_found

if "!MAKE_CMD!"=="" (
    echo [ERROR] No make command found (tried mingw32-make, make, gmake^)
    echo.
    echo Please ensure MinGW is installed and in PATH
    echo Download: https://www.mingw-w64.org/
    pause
    exit /b 1
)

echo [INFO] Using make: !MAKE_CMD!
echo.

REM Check dependencies
call :check_boost || exit /b 1
call :check_pugixml || exit /b 1

REM Patch if needed
call :patch_senderenvelope

REM Build library
echo [BUILD] Compiling Hermes library...
cd src\Hermes
!MAKE_CMD! -f Makefile.mingw clean
!MAKE_CMD! -f Makefile.mingw
if !errorlevel! neq 0 (
    echo [FAILED] Library build failed
    cd ..\..
    pause
    exit /b 1
)
copy /Y Hermes.dll ..\..\Hermes.dll >nul
cd ..\..

REM Build and run tests
call :run_tests

echo.
echo ========================================
echo   Build Complete!
echo ========================================
pause
exit /b 0

REM ============================================================================
REM Functions
REM ============================================================================

:check_boost
if exist "References\boost\version.hpp" (
    echo [OK] Boost found
    exit /b 0
)
echo [ERROR] Boost not found in References\boost\
echo Download: https://archives.boost.io/release/1.78.0/source/boost_1_78_0.zip
pause
exit /b 1

:check_pugixml
if exist "References\pugixml\pugixml.hpp" (
    echo [OK] Pugixml found
    exit /b 0
)
echo [ERROR] Pugixml not found in References\pugixml\
echo Download: https://github.com/zeux/pugixml
pause
exit /b 1

:patch_senderenvelope
cd src\Hermes
findstr /C:"#ifdef _WIN32" SenderEnvelope.cpp >nul 2>&1
if !errorlevel! neq 0 (
    echo [PATCH] Fixing SenderEnvelope.cpp...
    powershell -Command "$c = Get-Content SenderEnvelope.cpp -Raw; $c = $c -replace 'localtime_r\(&cnow, &local_tm\);', ('#ifdef _WIN32' + [char]10 + '            localtime_s(&local_tm, &cnow);' + [char]10 + '#else' + [char]10 + '            localtime_r(&cnow, &local_tm);' + [char]10 + '#endif'); Set-Content SenderEnvelope.cpp -Value $c -NoNewline"
)
cd ..\..
exit /b 0

:run_tests
if exist "simple_test.cpp" (
    echo [TEST] Building simple_test...
    g++ -std=c++17 -I src/include -L . -o simple_test.exe simple_test.cpp -lHermes -lws2_32 -lmswsock -static-libgcc -static-libstdc++
    if !errorlevel! equ 0 (
        simple_test.exe
    )
)

if exist "References\boost_libs\libboost_unit_test_framework*.a" (
    echo [TEST] Building official test suite...
    cd test\BoostTestHermes
    !MAKE_CMD! -f Makefile.mingw clean
    !MAKE_CMD! -f Makefile.mingw
    if !errorlevel! equ 0 (
        copy /Y BoostTestHermes.exe ..\..\BoostTestHermes.exe >nul
        cd ..\..
        BoostTestHermes.exe --run_test=* --detect_memory_leaks=0
    ) else (
        cd ..\..
    )
)
exit /b 0