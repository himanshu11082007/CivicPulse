@echo off
echo =======================================================
echo   Compiling CivicPulse C++ Engine (FDS)
echo =======================================================

set GXX=C:\mingw64\mingw64\bin\g++.exe

if not exist "%GXX%" (
    echo [INFO] C:\mingw64\mingw64\bin\g++.exe not found, using g++ from PATH...
    set GXX=g++
)

echo Stopping any running server instances...
powershell -NoProfile -Command "Stop-Process -Name main_api,main -Force -ErrorAction SilentlyContinue" 2>nul
ping -n 2 127.0.0.1 >nul

echo [1/2] Compiling main_api.exe (HTTP Server + FDS Engine)...
"%GXX%" -std=c++17 -D_WIN32_WINNT=0x0A00 main_api.cpp -lws2_32 -liphlpapi -lwinhttp -O2 -o main_api.exe
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] main_api compilation failed!
    echo If you see 'Permission denied', ensure main_api.exe is not currently running in another window.
    exit /b 1
)
echo [OK] main_api.exe compiled successfully!

echo [2/2] Compiling main.exe (Console CLI + FDS Engine)...
"%GXX%" -std=c++17 main.cpp -O2 -o main.exe
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] main.exe compilation failed!
    exit /b 1
)
echo [OK] main.exe compiled successfully!

echo.
echo =======================================================
echo   Build Successful!
echo   Run main_api.exe or ..\run_server.bat to start.
echo =======================================================
