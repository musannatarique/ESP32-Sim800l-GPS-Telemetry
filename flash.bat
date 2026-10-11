@echo off
REM One-click build + flash. Usage: flash.bat [COM port]   (port optional, auto-detected)
setlocal
cd /d "%~dp0firmware"

set "PIO=pio"
where pio >nul 2>nul || set "PIO=%USERPROFILE%\.platformio\penv\Scripts\pio.exe"
if not exist "%PIO%" if "%PIO%" neq "pio" (
  echo PlatformIO not found. Install it: python -m pip install platformio
  exit /b 1
)

if not exist include\secrets.h (
  copy include\secrets.example.h include\secrets.h >nul
  echo Created firmware\include\secrets.h from the example.
  echo Edit it with your device ID, APN and HiveMQ credentials, then run flash.bat again.
  notepad include\secrets.h
  exit /b 0
)

if "%~1"=="" ("%PIO%" run --target upload) else ("%PIO%" run --target upload --upload-port %~1)
if errorlevel 1 exit /b 1
"%PIO%" device monitor
