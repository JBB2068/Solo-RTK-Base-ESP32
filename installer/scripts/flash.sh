#!/usr/bin/env bash
# Solo RTK Base installer for macOS / Linux. Usage: ./flash.sh [update]
#   (no arg) full install: erases flash, including saved settings
#   update   writes firmware only, keeps settings
set -euo pipefail
cd "$(dirname "$0")"
if ! python3 -m esptool version >/dev/null 2>&1; then
  echo "Installing esptool (one time)..."
  python3 -m pip install --user --quiet "esptool==4.8.1"
fi
ET="python3 -m esptool --chip esp32c3 --baud 460800"
read -rp "Plug the ESP32-C3 into USB, then press Enter... "
if [ "${1:-}" != "update" ]; then $ET erase_flash; fi
$ET write_flash 0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
echo
echo "Done. Join Wi-Fi \"SoloRTK-XXXX\" (password solortk1) and open http://192.168.4.1"
