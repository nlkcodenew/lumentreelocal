# Lumentree Firmware

Firmware for the ESP32 gateway that reads the Lumentree inverter over BLE and
uploads telemetry to the local production API.

## Primary References

Use these first:

1. `../docs/status/2026-05-20-standard-firmware-flash-ha-update-runbook.md`
2. `../START_HERE.md`
3. `../SESSION_HANDOFF.md`

## Release Baseline

- Standard release env:
  `platformio.ini -> [env:esp32-s3-8mb-nopsram-release]`
- Current release version:
  `LUMENTREE_FIRMWARE_VERSION="0.15.1"`
- Target class:
  - `ESP32-S3`
  - `8MB flash or larger`
  - `PSRAM optional`

## Safety Model

- BLE is the inverter transport.
- HTTPS API is the upload path.
- Telemetry is read-only by default.
- Write behavior must stay guarded and semantic; no generic Modbus write path.
- Do not add new inverter-setting writes without explicit approval.

## Build

```bash
cd /home/mrlinh/esp32-lumentree/firmware
pio run -e esp32-s3-8mb-nopsram-release --project-conf platformio.ini
```

## Flash

```bash
cd /home/mrlinh/esp32-lumentree/firmware
pio run -e esp32-s3-8mb-nopsram-release --project-conf platformio.ini -t upload --upload-port /dev/ttyACM0
```

After flash, validate with the runbook:

- serial boot is healthy
- `READ_STATS_ONCE` works
- function `0x04` statistics upload reaches Postgres

## Provisioning

Core serial setup commands:

```text
SET_WIFI your-ssid your-password
SET_API_URL https://lumentree.jonah.io.vn
SET_API_TOKEN your-production-token
SET_DEVICE_ID P240819130
SET_GATEWAY_ID esp32-lumentree
SET_UPLOAD_INTERVAL 10
SET_TLS_INSECURE 1
UPLOAD_ONCE
SET_PRODUCTION 1
CONFIG
```

If `target_mac` is empty, the firmware can auto-pair when exactly one matching
inverter candidate is discovered.

## Common Serial Commands

```text
HELP
STATUS
CONFIG
SCAN_WIFI
SCAN_BLE
BLE_CANDIDATES
START_AP
READ_MAIN_ONCE
READ_STATS_ONCE
READ_RANGE start_register register_count
UPLOAD_ONCE
WRITE_STATUS
GENERATE_WRITE_CODE
```

## Operational Notes

- Captive portal default: `http://192.168.4.1`
- Production API default: `https://lumentree.jonah.io.vn`
- Firmware build flags in `platformio.ini` are the source of truth for the
  reported firmware name/version.

## Deeper References

- `../docs/specs/2026-05-20-function-04-firmware-integration-spec.md`
- `../docs/specs/2026-05-17-lumentree-local-vendor-server.md`
- `../docs/plans/2026-05-17-write-safety-plan.md`
