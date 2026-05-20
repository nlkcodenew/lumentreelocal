#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLATFORMIO_ENV="${PLATFORMIO_ENV:-esp32-s3-8mb-nopsram-release}"
BUILD_DIR="$ROOT_DIR/firmware/.pio/build/$PLATFORMIO_ENV"
SITE_FIRMWARE_DIR="$ROOT_DIR/host/flash-site/public/firmware"
BOOT_APP0="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
PLATFORMIO_INI="$ROOT_DIR/firmware/platformio.ini"

mkdir -p "$SITE_FIRMWARE_DIR"

cp "$BUILD_DIR/bootloader.bin" "$SITE_FIRMWARE_DIR/bootloader.bin"
cp "$BUILD_DIR/partitions.bin" "$SITE_FIRMWARE_DIR/partitions.bin"
cp "$BUILD_DIR/firmware.bin" "$SITE_FIRMWARE_DIR/lumentree-ble-bridge.bin"
cp "$BOOT_APP0" "$SITE_FIRMWARE_DIR/boot_app0.bin"

VERSION="$(
python3 - <<'PY' "$PLATFORMIO_INI" "$PLATFORMIO_ENV"
from pathlib import Path
import re
import sys

text = Path(sys.argv[1]).read_text()
env_name = sys.argv[2]
pattern = re.compile(
    rf'^\[env:{re.escape(env_name)}\](.*?)(?=^\[env:|\Z)',
    re.MULTILINE | re.DOTALL,
)
env_match = pattern.search(text)
if not env_match:
    raise SystemExit(f"Could not find env {env_name} in platformio.ini")
match = re.search(r'LUMENTREE_FIRMWARE_VERSION=\\"([^"]+)\\"', env_match.group(1))
if not match:
    raise SystemExit(f"Could not parse LUMENTREE_FIRMWARE_VERSION for env {env_name}")
print(match.group(1))
PY
)"

cat > "$SITE_FIRMWARE_DIR/flash-manifest.json" <<EOF
{
  "name": "Lumentree Local BLE Bridge",
  "version": "$VERSION",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        {
          "path": "bootloader.bin",
          "offset": 0
        },
        {
          "path": "partitions.bin",
          "offset": 32768
        },
        {
          "path": "boot_app0.bin",
          "offset": 57344
        },
        {
          "path": "lumentree-ble-bridge.bin",
          "offset": 65536
        }
      ]
    }
  ]
}
EOF

echo "Synced flash-site artifacts from $PLATFORMIO_ENV to $SITE_FIRMWARE_DIR"
