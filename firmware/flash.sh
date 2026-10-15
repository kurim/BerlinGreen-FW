#!/usr/bin/env bash
# Flasht die Custom-Firmware auf eine GreenBox (UART, Bootloader/Partitionstabelle/App).
# NVS (0x9000) wird NICHT angefasst: WLAN-Zugang der Werks-Firmware bleibt erhalten und wird beim ersten Start uebernommen.
# Rueckweg: esptool write_flash 0 berlingreen.bin  (Werks-Dump)
set -euo pipefail
PORT="${1:?usage: ./flash.sh /dev/ttyUSB0}"
B=.pio/build/greenbox
BOOT_APP0="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
[ -f "$B/firmware.bin" ] || { echo "erst bauen: pio run"; exit 1; }
python3 -m esptool --chip esp32 --port "$PORT" --baud 460800 write_flash -z --flash_mode dio --flash_size 8MB \
  0x1000 "$B/bootloader.bin" 0x8000 "$B/partitions.bin" 0xe000 "$BOOT_APP0" 0x10000 "$B/firmware.bin"
