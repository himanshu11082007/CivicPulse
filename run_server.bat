@echo off
echo =========================================================
echo       Starting CivicPulse - Multi-Tenant FDS Platform
echo =========================================================
echo.

cd /d "%~dp0backened"

if not exist "main_api.exe" (
    echo [INFO] Binaries not found. Building with GCC 14...
    call build.bat
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] Build failed!
        pause
        exit /b 1
    )
)

echo Starting CivicPulse HTTP Server...
echo Web Dashboard available at:        http://127.0.0.1:8080
echo Open on your Phone (same Wi-Fi):    http://192.168.0.102:8080
echo.
start http://127.0.0.1:8080
main_api.exe

pause
