@echo off
rem Flasht die Custom-Firmware auf eine GreenBox (UART, Bootloader/Partitionstabelle/App).
rem NVS (0x9000) wird NICHT angefasst: WLAN-Zugang der Werks-Firmware bleibt erhalten.
rem Rueckweg: esptool write_flash 0 berlingreen.bin  (Werks-Dump)
rem Aufruf: flash.cmd COM3 [verzeichnis]   (Standard: .pio\build\greenbox, oder entpackte Release-Dateien)
rem Voraussetzung: Python + "pip install esptool"
setlocal
if "%~1"=="" (
  echo usage: flash.cmd COM3 [verzeichnis]
  exit /b 1
)
set "PORT=%~1"
set "B=%~2"
if "%B%"=="" set "B=.pio\build\greenbox"
set "BOOT_APP0=%B%\boot_app0.bin"
if not exist "%BOOT_APP0%" set "BOOT_APP0=%USERPROFILE%\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin"
for %%F in (firmware.bin bootloader.bin partitions.bin) do (
  if not exist "%B%\%%F" (
    echo fehlt: %B%\%%F ^(erst bauen: pio run, oder Release-Dateien in ein Verzeichnis legen^)
    exit /b 1
  )
)
if not exist "%BOOT_APP0%" (
  echo fehlt: boot_app0.bin
  exit /b 1
)
where py >nul 2>nul && (set "PY=py -3") || (set "PY=python")
%PY% -m esptool --chip esp32 --port %PORT% --baud 460800 write_flash -z --flash_mode dio --flash_size 8MB ^
  0x1000 "%B%\bootloader.bin" 0x8000 "%B%\partitions.bin" 0xe000 "%BOOT_APP0%" 0x10000 "%B%\firmware.bin"
exit /b %errorlevel%
