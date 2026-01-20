@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   Hermes Build (Direct Compilation)
echo ========================================
echo.

call :check_boost || exit /b 1
call :check_pugixml || exit /b 1
call :patch_senderenvelope

echo [BUILD] Compiling Hermes library...
cd src\Hermes

del /Q *.o Hermes.dll 2>nul

set CXXFLAGS=-std=c++17 -O2 -Wall -Wno-unknown-pragmas -DHERMES_EXPORTS
set INCLUDES=-I. -I../include -I../../References -I../../References/pugixml

for %%f in (*.cpp) do (
    if not "%%f"=="stdafx.cpp" (
        echo   %%f
        g++ !CXXFLAGS! !INCLUDES! -c %%f
        if !errorlevel! neq 0 exit /b 1
    )
)

echo   pugixml.cpp
g++ -std=c++17 -O2 -I../../References/pugixml -c ../../References/pugixml/pugixml.cpp
if !errorlevel! neq 0 exit /b 1

g++ -shared -o Hermes.dll *.o -lws2_32 -lmswsock -liphlpapi -static-libgcc -static-libstdc++ -Wl,--export-all-symbols
if !errorlevel! neq 0 exit /b 1

copy /Y Hermes.dll ..\..\Hermes.dll >nul
cd ..\..

call :run_tests
echo [OK] Build complete!
pause
exit /b 0

:check_boost
if exist "References\boost\version.hpp" (echo [OK] Boost & exit /b 0)
echo [ERROR] Boost not found
exit /b 1

:check_pugixml
if exist "References\pugixml\pugixml.hpp" (echo [OK] Pugixml & exit /b 0)
echo [ERROR] Pugixml not found
exit /b 1

:patch_senderenvelope
cd src\Hermes
findstr /C:"#ifdef _WIN32" SenderEnvelope.cpp >nul 2>&1 || (
    powershell -Command "$c=gc SenderEnvelope.cpp -Raw; $c=$c -replace 'localtime_r\(&cnow, &local_tm\);','#ifdef _WIN32`n            localtime_s(&local_tm, &cnow);`n#else`n            localtime_r(&cnow, &local_tm);`n#endif'; sc SenderEnvelope.cpp $c -NoNewline"
)
cd ..\..
exit /b 0

:run_tests
if exist simple_test.cpp (
    g++ -std=c++17 -I src/include -L . -o simple_test.exe simple_test.cpp -lHermes -lws2_32 -lmswsock -static-libgcc -static-libstdc++ && simple_test.exe
)
exit /b 0