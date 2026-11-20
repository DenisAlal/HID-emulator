@echo off
cd /d "%~dp0"
go build -o server.exe .
if errorlevel 1 goto fail
go build -ldflags "-H=windowsgui -X main.trayMode=true" -o server-tray.exe .
if errorlevel 1 goto fail
echo Build OK: server.exe + server-tray.exe
goto :eof

:fail
echo Build FAILED. Check that Go is installed and available in PATH.
pause
exit /b 1
