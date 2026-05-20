# 2026-05-17 Next Development Roadmap

This note records the recommended next development direction after the ESP32
production uploader, local API, Postgres storage, and Home Assistant integration
were brought online.

## Current Baseline

- ESP32 uploads realtime BLE telemetry to `https://lumentree.jonah.io.vn`.
- Production upload interval is configured as `15s`; observed end-to-end samples
  are roughly `20s` because a BLE read and HTTPS upload take additional time.
- Home Assistant reads Lumentree Local entities from the local production API.
- Postgres stores raw telemetry in `lumentree_telemetry` and incremental daily
  energy rollups in `lumentree_energy_daily`.
- API/HASS health diagnostics now expose freshness, sample counts, sample
  interval, max gap, firmware, gateway ID, and ESP32 uptime.
- Firmware only sends Modbus function `03` read-register requests. There is no
  inverter setting write path.
- User validation confirmed the ESP32 BLE local reader and the inverter's vendor
  cloud path can run in parallel. The ESP32 reads BLE and uploads to
  `lumentree.jonah.io.vn`; the inverter can continue sending data to the vendor
  server at the same time.

## Main Product Gap

The hardest user-facing step is identifying the correct inverter BLE device
after flashing a new ESP32. A normal user should not need to know the BLE MAC,
service UUID, or low-level protocol.

Recommended target flow:

1. User flashes the firmware.
2. ESP32 has no Wi-Fi credentials, so it opens AP `Lumentree-XXXX`.
3. User opens the AP portal and enters Wi-Fi credentials.
4. ESP32 connects to Wi-Fi and stops the AP.
5. User installs the Home Assistant integration `Lumentree Local`.
6. In Home Assistant, user enters only the inverter Device ID, for example
   `P240819130`.
7. Home Assistant calls the API server to check whether that device already has
   gateway telemetry.
   - If the server has telemetry for `P240819130`, setup is complete.
   - If no telemetry exists yet, HASS reports `Waiting for ESP32 gateway
     pairing`.
8. ESP32 has Wi-Fi but no `target_mac`, so it starts BLE discovery.
   - Scan nearby BLE devices.
   - Match candidates by BLE name equal to or containing the Device ID.
   - Match candidates by the known service UUID
     `a018739b-734d-8211-cb80-c9acd39d13b4`.
   - Match candidates by vendor GATT `FFE0`/`FFE1`.
   - If exactly one device matches, store the target MAC and start uploading.
   - If multiple devices match, push the candidate list to the API.
9. Home Assistant shows pairing status:
   - `Scanning for inverter`
   - `Found candidate: P240819130, RSSI -57`
   - `Multiple candidates found, choose in ESP32 portal`
   - `Paired, receiving telemetry`
10. When ESP32 uploads the first telemetry sample:
    - API server creates or updates device `P240819130`.
    - HASS coordinator sees latest telemetry data.
    - Entities begin receiving states.

Important ownership boundary: HASS only asks for Device ID. ESP32 owns BLE scan
and MAC binding because ESP32 is the device with the BLE radio.

## Firmware Tasks

- Add BLE discovery mode when no target MAC is configured.
- Record scan candidates with:
  - MAC address
  - RSSI
  - advertised name
  - service UUID list
  - last seen timestamp
- Prefer matching candidates by Device ID/SN if it appears in advertisement or
  readable characteristics.
- Otherwise rank candidates by known Lumentree service UUIDs, vendor GATT
  `FFE0`/`FFE1`, and RSSI.
- Persist the selected target MAC in NVS after auto-pairing or user selection.
- Add serial commands for diagnostics:
  - `SCAN_BLE`
  - `BLE_CANDIDATES`
  - `SET_TARGET_MAC <mac>`
  - `CLEAR_TARGET_MAC`
- Add runtime counters:
  - `ble_read_fail_count`
  - `api_upload_fail_count`
  - `wifi_fail_count`
  - `watchdog_reboot_count`
  - `last_ble_rssi`
  - `last_ble_seen`
  - `candidate_count`
- Keep the existing safety boundary: no inverter write/settings commands unless
  explicitly approved.

## API Tasks

- Store gateway pairing/diagnostic state in Postgres. A small table such as
  `lumentree_gateway_status` is enough for:
  - gateway ID
  - firmware
  - configured device ID
  - target MAC
  - pairing status
  - last Wi-Fi status
  - last BLE status
  - failure counters
  - last candidate list
- Expose a gateway health endpoint, for example:
  `/api/lumentree/gateways/{gateway_id}/health`
- Extend device health to include pairing status when the gateway reports it.
- Allow ESP32 to publish candidate lists to the API when multiple inverter BLE
  devices match.
- Add a weekly energy aggregation section to
  `/api/lumentree/devices/{device_id}/energy`. Daily, monthly, yearly, and total
  already exist; weekly is the missing period for the user's requested set.

## Home Assistant Tasks

- Surface pairing state clearly:
  - `Waiting for ESP32 gateway pairing`
  - `Scanning for inverter`
  - `Found candidate: P240819130, RSSI -57`
  - `Multiple candidates found, choose in ESP32 portal`
  - `Paired, receiving telemetry`
- Add diagnostic sensors/entities for firmware counters and BLE RSSI.
- Keep the config flow simple: Device ID only. Do not ask the user to type UUID
  or MAC unless a manual override is explicitly needed.
- If multiple candidates are reported, direct the user to the ESP32 portal's BLE
  inverter candidates page instead of adding MAC/UUID fields to the normal HASS
  config flow.
- After integration version `0.5.0` is installed/restarted in HA, verify the new
  health diagnostic entities appear before adding more diagnostics.

## Historical Statistics

The older vendor-cloud integration proves that Lumentree cloud has historical
statistics APIs:

- Daily chart APIs:
  - `/lesvr/getPVDayData`
  - `/lesvr/getBatDayData`
  - `/lesvr/getOtherDayData`
- Monthly API:
  - `/lesvr/getMonthData`
- Yearly API:
  - `/lesvr/getYearData`

Those APIs show that the vendor cloud stores daily/monthly/yearly history. They
do not prove that the inverter exposes the same historical tables locally over
BLE. The current local BLE path has only proven realtime main-register reads.

Local-first recommendation:

- Keep Postgres as the source of truth for history generated after the local
  gateway is installed.
- Continue storing raw telemetry plus daily rollups locally.
- Compute weekly/monthly/yearly/total values from the daily rollup table.
- Do not depend on vendor cloud for normal operation.
- Add a separate optional vendor-cloud import/backfill task only if historical
  data from before the local gateway matters.

This avoids mixing cloud-derived history with local realtime data unless the user
intentionally asks for a one-time migration or reconciliation.

## Hardening Backlog

- Replace firmware `TLS_INSECURE=1` with explicit CA validation.
- Queue unsent telemetry samples in flash when Wi-Fi/API is unavailable.
- Persist watchdog reset reason and reboot count.
- Add alerting in HASS when telemetry age, max sample gap, or failure counters
  exceed thresholds.
- Add a small production monitor script that checks API health and newest sample
  age from cron/systemd timer.
