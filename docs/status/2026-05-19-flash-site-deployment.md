# 2026-05-19 Flash Site Deployment

## Goal

Provide a dedicated public flash page for fresh ESP32-S3 Lumentree devices
without reviving the old `esp32agent` application.

## Local Service

- Local service bind: `http://127.0.0.1:8790`
- Public hostname intended for Cloudflare route:
  `https://flash-lumentree.jonah.io.vn`

## Files

- Static site: `host/flash-site/public/index.html`
- Static server: `host/flash-site/server.py`
- Firmware sync script: `tools/sync_flash_site_artifacts.sh`

## Firmware Packaging

The flash manifest uses the standard ESP32-S3 four-part layout:

- `bootloader.bin` at `0x0000`
- `partitions.bin` at `0x8000`
- `boot_app0.bin` at `0xE000`
- `lumentree-ble-bridge.bin` at `0x10000`

Artifacts are copied from the PlatformIO build output and the local PlatformIO
framework package.

## Deployment Rule

After a firmware rebuild, rerun:

```bash
./tools/sync_flash_site_artifacts.sh
```

Then restart the local flash-site service if needed.

## Version Contract

The public website version must stay synchronized with the real firmware line.

Rules:

- Do not edit the flash manifest version by hand.
- The source of truth is
  `firmware/platformio.ini -> [env:esp32-s3-8mb-nopsram-release] -> LUMENTREE_FIRMWARE_VERSION`.
- The sync script reads that value and regenerates
  `host/flash-site/public/firmware/flash-manifest.json`.
- Publish to the website only after the matching firmware build was flashed and
  verified on real hardware.
