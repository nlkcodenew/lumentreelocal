# 2026-05-19 No-PSRAM Test On 16MB ESP32-S3

## Goal

Backup the currently running device, then test a firmware build path that does
not rely on PSRAM-specific build flags before exposing that build on the public
flash site.

## Local Backup

A full 16MB flash backup was created locally before flashing:

```text
docs/evidence/device-backups/2026-05-19-esp32-device-backup/esp32s3-full-flash-16mb.bin
```

The backup is intentionally ignored by Git because it may contain NVS secrets
such as Wi-Fi credentials and API tokens.

Backup SHA-256:

```text
2c0a78790d423ff36f1b0d5b4343f1db52361100d1161fbea617916e34f8e794
```

## Test Build

Added a dedicated test environment in `firmware/platformio.ini`:

```text
env:esp32-s3-16mb-nopsram-test
```

Characteristics:

- Same dedicated Lumentree firmware behavior
- 16MB flash layout preserved
- PSRAM-specific build flag removed
- `board_build.arduino.memory_type` override removed
- Firmware version set to `0.15.0-rc1`

## Build Result

PlatformIO build passed.

Resource usage:

- RAM: `20.7%` (`67876 / 327680`)
- App partition flash: `50.8%` (`1597749 / 3145728`)

## Live Flash Test Result

The `0.15.0-rc1` no-PSRAM build was flashed to the physical ESP32-S3 device and
validated live:

- Serial `STATUS` reported firmware `lumentree-ble-bridge/0.15.0-rc1`
- Wi-Fi reconnected successfully
- Production upload stayed enabled at `10s`
- BLE read and settings snapshot uploads resumed
- Public health endpoint reported firmware `lumentree-ble-bridge/0.15.0-rc1`

This is enough evidence to continue with a broader support plan for
`ESP32-S3 >= 8MB flash` without requiring PSRAM as a hard prerequisite.

## Not Done Yet

- The public flash site still serves the current published firmware line.
- No new website manifest was published from this test build.
- Smaller boards, especially `4MB` flash devices, are still out of scope.
