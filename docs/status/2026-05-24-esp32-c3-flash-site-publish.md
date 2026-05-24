# ESP32-C3 Flash-Site Publish Status

Date: 2026-05-24

## Scope

Publish the new `ESP32-C3 Super Mini` experimental firmware line to the
Lumentree WebSerial flash site without disturbing the existing stable
`ESP32-S3` line.

## What Changed

1. Added multi-board firmware sync in:
   - `tools/sync_flash_site_artifacts.sh`
2. Updated flash-site operator docs in:
   - `host/flash-site/README.md`
3. Updated the public flash page in:
   - `host/flash-site/public/index.html`

## Flash-Site Behavior After This Change

- `ESP32-S3` remains the stable default line.
- `ESP32-C3 Super Mini` is now exposed as a separate experimental board
  choice.
- Users choose the firmware line explicitly based on their hardware.
- The page now fetches and displays board-specific manifest versions from:
  - `/firmware/esp32-s3/flash-manifest.json`
  - `/firmware/esp32-c3/flash-manifest.json`
- A combined manifest is also published at:
  - `/firmware/flash-manifest.json`

## Artifact Layout

Stable S3 artifacts:

- `host/flash-site/public/firmware/esp32-s3/bootloader.bin`
- `host/flash-site/public/firmware/esp32-s3/partitions.bin`
- `host/flash-site/public/firmware/esp32-s3/boot_app0.bin`
- `host/flash-site/public/firmware/esp32-s3/lumentree-ble-bridge.bin`
- `host/flash-site/public/firmware/esp32-s3/flash-manifest.json`

Experimental C3 artifacts:

- `host/flash-site/public/firmware/esp32-c3/bootloader.bin`
- `host/flash-site/public/firmware/esp32-c3/partitions.bin`
- `host/flash-site/public/firmware/esp32-c3/lumentree-ble-bridge.bin`
- `host/flash-site/public/firmware/esp32-c3/flash-manifest.json`

## Validation Completed

Build validation:

- `pio run -e esp32-s3-8mb-nopsram-release`
- `pio run -e esp32-c3-4mb-experimental`

Artifact sync:

- `./tools/sync_flash_site_artifacts.sh`

Manifest verification:

- local `http://127.0.0.1:8790/firmware/flash-manifest.json`
- local `http://127.0.0.1:8790/firmware/esp32-s3/flash-manifest.json`
- local `http://127.0.0.1:8790/firmware/esp32-c3/flash-manifest.json`

Page verification:

- local flash page contains both:
  - `ESP32-S3`
  - `ESP32-C3 Super Mini`
- public flash page also serves both board choices after sync

## Support Position

- `ESP32-S3` remains the only stable production line.
- `ESP32-C3 Super Mini` is now user-selectable on the website because it has
  already passed real-world build/flash/boot/AP/Wi-Fi/LAN-portal/end-to-end
  integration validation.
- It must still stay labeled `experimental` until longer soak testing confirms
  stability comparable to the `ESP32-S3` line.
