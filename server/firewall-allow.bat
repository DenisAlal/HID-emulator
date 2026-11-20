@echo off
REM Открывает порт 8080 (TCP, входящие) для HID Emulator.
REM Запусти двойным кликом — если нужно, сам запросит права администратора.

net session >nul 2>&1
if %errorlevel% neq 0 (
  echo Запрашиваю права администратора...
  powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)

netsh advfirewall firewall delete rule name="HID Emulator" >nul 2>&1
netsh advfirewall firewall add rule name="HID Emulator" dir=in action=allow protocol=TCP localport=8080

if errorlevel 1 (
  echo.
  echo ОШИБКА: не удалось добавить правило.
) else (
  echo.
  echo Готово: порт 8080 открыт для входящих TCP.
)
pause
