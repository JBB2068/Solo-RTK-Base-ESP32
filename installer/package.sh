#!/usr/bin/env bash
# Collects the esp32c3 build into dist/: flash parts, merged factory.bin,
# web-installer site, and Windows / macOS-Linux zip packages.
# Usage: installer/package.sh <version>   (after `pio run -e esp32c3`)
set -euo pipefail
VER="${1:?version}"
B=.pio/build/esp32c3
BOOT_APP0=$(find ~/.platformio/packages -path '*tools/partitions/boot_app0.bin' | head -n1)
[ -f "$BOOT_APP0" ] || { echo "boot_app0.bin not found"; exit 1; }

rm -rf dist && mkdir -p dist/parts dist/site
cp "$B/bootloader.bin" "$B/partitions.bin" "$B/firmware.bin" dist/parts/
cp "$BOOT_APP0" dist/parts/boot_app0.bin

python3 -m esptool --chip esp32c3 merge_bin -o dist/solo-rtk-base-${VER}-factory.bin \
  0x0 dist/parts/bootloader.bin 0x8000 dist/parts/partitions.bin \
  0xe000 dist/parts/boot_app0.bin 0x10000 dist/parts/firmware.bin

# Web installer (ESP Web Tools). Separate parts so "don't erase" keeps NVS settings.
cp installer/web/index.html dist/site/
cp dist/parts/*.bin dist/site/
cat > dist/site/manifest.json <<JSON
{
  "name": "Solo RTK Base",
  "version": "${VER}",
  "new_install_prompt_erase": true,
  "builds": [{
    "chipFamily": "ESP32-C3",
    "parts": [
      { "path": "bootloader.bin", "offset": 0 },
      { "path": "partitions.bin", "offset": 32768 },
      { "path": "boot_app0.bin",  "offset": 57344 },
      { "path": "firmware.bin",   "offset": 65536 }
    ]
  }]
}
JSON

# Script packages
mk() {  # name, extra files...
  local d="dist/pkg/$1"; shift
  mkdir -p "$d"
  cp dist/parts/*.bin installer/scripts/README.txt "$d/"
  for f in "$@"; do cp "$f" "$d/"; done
  (cd dist/pkg && zip -qr "../$(basename "$d").zip" "$(basename "$d")")
}
mk "solo-rtk-base-${VER}-windows" installer/scripts/flash.bat installer/scripts/update-keep-settings.bat "${ESPTOOL_EXE:?set ESPTOOL_EXE}"
mk "solo-rtk-base-${VER}-mac-linux" installer/scripts/flash.sh
ls -la dist
