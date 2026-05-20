# 2026-05-19 8MB No-PSRAM Public Release

## Goal

Promote the firmware line tested on the physical ESP32-S3 device to a public
flash image that works for a broader set of ESP32-S3 boards.

## Public Firmware Line

- Release environment: `esp32-s3-8mb-nopsram-release`
- Firmware version: `0.15.0`
- Flash layout: `default_8MB.csv`
- PSRAM requirement: none

## Compatibility Rule

Supported:

- ESP32-S3
- 8MB flash or larger
- PSRAM optional

Not supported:

- Boards with less than 8MB flash
- Non-ESP32-S3 families unless tested separately

## Validation Before Publish

The firmware was validated on the user's real ESP32-S3 device after:

1. Creating a full 16MB flash backup.
2. Flashing `0.15.0-rc1` no-PSRAM 16MB test build.
3. Flashing `0.15.0` 8MB-layout no-PSRAM release build.
4. Verifying production health, BLE reads, Wi-Fi reconnect, and a post-flash
   reboot.

## Flash Site Publish

The flash site should sync artifacts from:

```text
firmware/.pio/build/esp32-s3-8mb-nopsram-release
```

The sync script now defaults to that environment.

## Ongoing Publish Process

For the next firmware version:

1. Bump
   `firmware/platformio.ini -> [env:esp32-s3-8mb-nopsram-release] -> LUMENTREE_FIRMWARE_VERSION`
2. Build the same environment.
3. Flash a real ESP32-S3 test device.
4. Verify serial `STATUS`, BLE reads, Wi-Fi reconnect, and production health.
5. Run `./tools/sync_flash_site_artifacts.sh`
6. Confirm the public manifest version matches the tested firmware version.
