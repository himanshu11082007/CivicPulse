@echo off
echo =========================================================
echo       Starting CivicPulse - Smart Civic Platform
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

set LOCAL_IP=192.168.0.101
for /f "tokens=*" %%a in ('powershell -NoProfile -Command "(Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.InterfaceAlias -notlike '*Loopback*' -and ($_.IPAddress -like '192.168.*' -or $_.IPAddress -like '10.*' -or $_.IPAddress -like '172.*') }).IPAddress | Select-Object -First 1"') do set LOCAL_IP=%%a

echo Starting CivicPulse HTTP Server...
echo Web Dashboard available at:        http://127.0.0.1:8080
echo Open on your Phone (same Wi-Fi):    http://%LOCAL_IP%:8080
echo.
start http://127.0.0.1:8080
main_api.exe

pause
