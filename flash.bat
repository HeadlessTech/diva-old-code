@echo off
rem Builds the Diva firmware from All_in_One\config.h, flashes it to the board
rem on the given COM port and shows the machine's log.
rem
rem   flash.bat COM5
rem
rem Needs arduino-cli.exe on the PATH or next to this file:
rem https://arduino.github.io/arduino-cli/latest/installation/
setlocal
cd /d "%~dp0"

set FQBN=esp32:esp32:esp32
set CORE_VERSION=2.0.17
set INDEX_URL=https://espressif.github.io/arduino-esp32/package_esp32_index.json

if "%~1"=="" (
  echo Usage: flash.bat COM5
  echo.
  echo Boards connected now:
  arduino-cli board list
  exit /b 1
)
set PORT=%~1

where arduino-cli >nul 2>nul
if errorlevel 1 (
  if exist "%~dp0arduino-cli.exe" (
    set "PATH=%~dp0;%PATH%"
  ) else (
    echo arduino-cli not found. Put arduino-cli.exe next to flash.bat.
    exit /b 1
  )
)

rem The firmware is built and tested with exactly this ESP32 board package.
arduino-cli core list | findstr /R /C:"esp32:esp32  *%CORE_VERSION% " >nul
if errorlevel 1 (
  echo Installing ESP32 board package %CORE_VERSION% ...
  arduino-cli core update-index --additional-urls %INDEX_URL%
  if errorlevel 1 exit /b 1
  arduino-cli core install esp32:esp32@%CORE_VERSION% --additional-urls %INDEX_URL%
  if errorlevel 1 exit /b 1
)

echo.
echo Settings in All_in_One\config.h:
echo.
findstr /B /C:"#define" All_in_One\config.h
echo.
choice /C YN /M "Flash this machine with these settings"
if errorlevel 2 (
  echo Cancelled. Edit All_in_One\config.h and run flash.bat again.
  exit /b 1
)

echo.
echo Building and flashing to %PORT% ...
arduino-cli compile --fqbn %FQBN% --upload --port %PORT% All_in_One
if errorlevel 1 (
  echo.
  echo *** BUILD OR FLASH FAILED - nothing was changed on the machine if the build failed ***
  exit /b 1
)

echo.
echo Flashed. The machine's log follows. Check the machine ID and "MQTT broker Connected".
echo Close this window or press Ctrl+C to stop.
arduino-cli monitor --port %PORT% --config baudrate=115200
