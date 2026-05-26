#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SITE_FIRMWARE_DIR="$ROOT_DIR/host/flash-site/public/firmware"
BOOT_APP0="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
PLATFORMIO_INI="$ROOT_DIR/firmware/platformio.ini"
S3_LEGACY_ENV="${S3_LEGACY_ENV:-esp32-s3-8mb-nopsram-release}"
S3_PREVIEW_ENV="${S3_PREVIEW_ENV:-esp32-s3-8mb-nopsram-fastbulk-preview}"
C3_ENV="${C3_ENV:-esp32-c3-4mb-debug-cmdtask-fastbulk-task}"

mkdir -p "$SITE_FIRMWARE_DIR"

parse_version() {
  local env_name="$1"
  python3 - <<'PY' "$PLATFORMIO_INI" "$env_name"
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
}

sync_s3_legacy() {
  local build_dir="$ROOT_DIR/firmware/.pio/build/$S3_LEGACY_ENV"
  local out_dir="$SITE_FIRMWARE_DIR/esp32-s3"
  mkdir -p "$out_dir"
  # Keep legacy flat paths in sync with the stable S3 line for backward
  # compatibility with older links and tooling.
  cp "$build_dir/bootloader.bin" "$SITE_FIRMWARE_DIR/bootloader.bin"
  cp "$build_dir/partitions.bin" "$SITE_FIRMWARE_DIR/partitions.bin"
  cp "$build_dir/firmware.bin" "$SITE_FIRMWARE_DIR/lumentree-ble-bridge.bin"
  cp "$BOOT_APP0" "$SITE_FIRMWARE_DIR/boot_app0.bin"
  cp "$build_dir/bootloader.bin" "$out_dir/bootloader.bin"
  cp "$build_dir/partitions.bin" "$out_dir/partitions.bin"
  cp "$build_dir/firmware.bin" "$out_dir/lumentree-ble-bridge.bin"
  cp "$BOOT_APP0" "$out_dir/boot_app0.bin"
  local version
  version="$(parse_version "$S3_LEGACY_ENV")"
  cat > "$out_dir/flash-manifest.json" <<EOF
{
  "name": "Lumentree Local BLE Bridge",
  "version": "$version",
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
}

sync_s3_preview() {
  local build_dir="$ROOT_DIR/firmware/.pio/build/$S3_PREVIEW_ENV"
  local out_dir="$SITE_FIRMWARE_DIR/esp32-s3-preview"
  mkdir -p "$out_dir"
  cp "$build_dir/bootloader.bin" "$out_dir/bootloader.bin"
  cp "$build_dir/partitions.bin" "$out_dir/partitions.bin"
  cp "$build_dir/firmware.bin" "$out_dir/lumentree-ble-bridge.bin"
  cp "$BOOT_APP0" "$out_dir/boot_app0.bin"
  local version
  version="$(parse_version "$S3_PREVIEW_ENV")"
  cat > "$out_dir/flash-manifest.json" <<EOF
{
  "name": "Lumentree Local BLE Bridge",
  "version": "$version",
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
}

sync_c3() {
  local build_dir="$ROOT_DIR/firmware/.pio/build/$C3_ENV"
  local out_dir="$SITE_FIRMWARE_DIR/esp32-c3"
  mkdir -p "$out_dir"
  cp "$build_dir/bootloader.bin" "$out_dir/bootloader.bin"
  cp "$build_dir/partitions.bin" "$out_dir/partitions.bin"
  cp "$build_dir/firmware.bin" "$out_dir/lumentree-ble-bridge.bin"
  local version
  version="$(parse_version "$C3_ENV")"
  cat > "$out_dir/flash-manifest.json" <<EOF
{
  "name": "Lumentree Local BLE Bridge",
  "version": "$version",
  "builds": [
    {
      "chipFamily": "ESP32-C3",
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
          "path": "lumentree-ble-bridge.bin",
          "offset": 65536
        }
      ]
    }
  ]
}
EOF
}

sync_s3_legacy
sync_s3_preview
sync_c3

legacy_version="$(parse_version "$S3_LEGACY_ENV")"
preview_version="$(parse_version "$S3_PREVIEW_ENV")"
c3_version="$(parse_version "$C3_ENV")"

cat > "$SITE_FIRMWARE_DIR/flash-manifest.json" <<EOF
{
  "name": "Lumentree Local BLE Bridge",
  "version": "legacy-s3-$legacy_version preview-s3-$preview_version c3-$c3_version",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        {
          "path": "esp32-s3/bootloader.bin",
          "offset": 0
        },
        {
          "path": "esp32-s3/partitions.bin",
          "offset": 32768
        },
        {
          "path": "esp32-s3/boot_app0.bin",
          "offset": 57344
        },
        {
          "path": "esp32-s3/lumentree-ble-bridge.bin",
          "offset": 65536
        }
      ]
    },
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        {
          "path": "esp32-s3-preview/bootloader.bin",
          "offset": 0
        },
        {
          "path": "esp32-s3-preview/partitions.bin",
          "offset": 32768
        },
        {
          "path": "esp32-s3-preview/boot_app0.bin",
          "offset": 57344
        },
        {
          "path": "esp32-s3-preview/lumentree-ble-bridge.bin",
          "offset": 65536
        }
      ]
    },
    {
      "chipFamily": "ESP32-C3",
      "parts": [
        {
          "path": "esp32-c3/bootloader.bin",
          "offset": 0
        },
        {
          "path": "esp32-c3/partitions.bin",
          "offset": 32768
        },
        {
          "path": "esp32-c3/lumentree-ble-bridge.bin",
          "offset": 65536
        }
      ]
    }
  ]
}
EOF

echo "Synced flash-site artifacts for:"
echo "  - $S3_LEGACY_ENV -> $SITE_FIRMWARE_DIR/esp32-s3"
echo "  - $S3_PREVIEW_ENV -> $SITE_FIRMWARE_DIR/esp32-s3-preview"
echo "  - $C3_ENV -> $SITE_FIRMWARE_DIR/esp32-c3"
