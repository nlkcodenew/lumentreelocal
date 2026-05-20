# 2026-05-17 Production Rollout

This file tracks the production promotion tasks for the standalone
`/home/mrlinh/esp32-lumentree` workspace.

## Completed

- Cloudflare Tunnel ingress added for `lumentree.jonah.io.vn`.
  - Local service target: `http://127.0.0.1:8787`
  - Config file: `/etc/cloudflared/config.yml`
  - Backup created before edit: `/etc/cloudflared/config.yml.backup-lumentree-*`
  - Validation: `cloudflared tunnel --config /etc/cloudflared/config.yml ingress validate`
  - Runtime: `cloudflared` restarted and active
  - DNS route: `lumentree.jonah.io.vn` CNAME added to tunnel
- Production API token and environment file created outside Git.
  - Env file: `/etc/lumentree/local-server.env`
  - Permissions: root-owned, mode `600`
  - Contains: server bind, Postgres DSN, and bearer token
- `lumentree-local-server` systemd service created and enabled.
  - Unit file: `/etc/systemd/system/lumentree-local-server.service`
  - Working directory: `/home/mrlinh/esp32-lumentree/host/local-server`
  - Bind address: `127.0.0.1:8787`
  - Database: local Postgres database `lumentree`
  - Local health check: `http://127.0.0.1:8787/health` returned OK
- Production Cloudflare route validated.
  - Dashboard service target `http://localhost:8787` works with the server bound
    to `127.0.0.1:8787`.
  - Public health check: `https://lumentree.jonah.io.vn/health` returned OK.
  - Authenticated event POST through the public hostname returned OK.
  - Authenticated `devices` and `latest` reads through the public hostname
    returned telemetry for `P240819130`.
  - The accidental unused `lumentree-local` Cloudflare tunnel was deleted; the
    production route uses the existing `ubuntu-home` tunnel.
- Home Assistant HACS integration files added to the canonical repo.
  - Integration path: `custom_components/lumentreelocal`
  - HACS metadata: `hacs.json`
  - Install source: `https://github.com/nlkcodenew/lumentreelocal`
  - Default API URL: `https://lumentree.jonah.io.vn`
- Home Assistant `lumentreelocal` configured and validated.
  - HA URL: `http://192.168.122.230:8123`
  - Config entry: `Lumentree Local P240819130`
  - API URL configured in HA: `https://lumentree.jonah.io.vn`
  - Device ID: `P240819130`
  - Current ESP32 path used for validation: BLE read through `/dev/ttyACM0`,
    posted by `host/ble-collector` to the local production server.
  - HA sensors confirmed populated from a real BLE function `03` read:
    grid power `770 W`, load power `747 W`, battery SOC `40%`, battery voltage
    `53.1 V`, grid voltage `229 V`, AC output voltage `226 V`, frequency
    `49.7 Hz`, device temperature `37 C`.
- Home Assistant config flow simplified.
  - The user form now asks only for `Device ID`.
  - `https://lumentree.jonah.io.vn` is the integration's internal default API
    URL.
  - There is no default device ID; the user must enter the desired inverter ID.
  - Public read endpoints needed by HA are tokenless:
    `/api/lumentree/devices` and `/api/lumentree/devices/{device_id}/latest`.
  - Telemetry upload and debug event history remain token-protected.
- Home Assistant realtime entity coverage expanded.
  - The integration now exposes all realtime metrics currently decoded from the
    BLE main-register response: power, voltage, frequency, battery/grid status,
    PV1/PV2, AC input/output, device SN, master/slave status, and UPS mode.
  - Historical vendor-cloud statistics are not copied; local statistics are
    calculated from telemetry stored in Postgres.
- Lumentree Local icon added.
  - Root asset: `icon.png`
  - Integration asset: `custom_components/lumentreelocal/icon.png`
- Local energy aggregation added.
  - Postgres table: `lumentree_energy_daily`
  - API endpoint: `/api/lumentree/devices/{device_id}/energy`
  - Daily/monthly/yearly periods use `Asia/Ho_Chi_Minh` (GMT+7); daily counters
    reset at `00:00` GMT+7.
  - HA sensors now include daily, monthly, yearly, and total kWh values for PV,
    load, grid import/export, battery charge/discharge, and AC input/output.
  - Current expected entity coverage is 59 entities: 57 sensors and 2 binary
    sensors.
- Polling guidance.
  - Current Home Assistant polling interval is `10s`.
  - A full BLE read through the current firmware commonly takes a few seconds,
    so `2s` is too aggressive for long-running production.
  - Production ESP32 upload interval is now standardized at `10s`; use `5s`
    only for short tests.
- Short `5s` BLE polling test.
  - Ran the USB collector against `/dev/ttyACM0` with
    `--read-main-interval 5` for a short validation window.
  - BLE responses completed and the public energy endpoint advanced to
    `sample_count=7` with `covered_seconds=200.9`.
  - The BLE read cycle nearly saturates a `5s` interval, so keep `5s` for
    debug bursts only; production guidance remains `15s` to `30s`.
- Home Assistant `Solar` dashboard migrated to Lumentree Local entities.
  - Dashboard storage path: `z-thi-t-b`
  - View: `Solar`
  - Replaced old `sensor.device_p240819130_*` and `sensor.grid_export_daily`
    references with `sensor.lumentree_local_p240819130_*` references.
  - Backup snapshots:
    `docs/status/ha-dashboard-z-thi-t-b-before-lumentreelocal.json` and
    `docs/status/ha-dashboard-z-thi-t-b-after-lumentreelocal.json`
  - Read-back validation confirmed no old LumentreeHASS entity references remain
    in the `Solar` view, and all 14 replacement entities exist in HA.
- USB BLE collector public POST compatibility fixed.
  - Collector now sends a clear `User-Agent` header.
  - Manual urllib validation through `https://lumentree.jonah.io.vn` returned
    HTTP `201`.
- ESP32 firmware production Wi-Fi upload implemented.
  - Firmware version: `0.3.1`
  - Default API URL: `https://lumentree.jonah.io.vn`
  - Default device ID: `P240819130`
  - Default target MAC: `d8:13:2a:ee:58:d6`
  - Default upload interval: `10s`
  - Runtime serial commands added for provisioning Wi-Fi, API token, API URL,
    device ID, gateway ID, upload interval, TLS mode, and production mode.
  - Wi-Fi password and API token are stored in ESP32 NVS; the optional local
    compile-time config header `firmware/src/lumentree_config.h` is ignored by
    Git.
  - `UPLOAD_ONCE` performs one BLE function `03` read and POSTs the raw Modbus
    response to `/api/lumentree/events`; the server remains responsible for
    metric decoding and Postgres aggregation.
  - Captive portal provisioning added from the OpenClaw firmware pattern:
    if no Wi-Fi SSID is stored, the ESP32 opens `Lumentree-XXXX` at
    `http://192.168.4.1`; `START_AP` can also start the portal manually.
  - Wi-Fi/BLE coexistence uses `WIFI_PS_MIN_MODEM`; disabling modem sleep caused
    an ESP-IDF abort when Wi-Fi and Bluetooth were enabled together.
  - Serial `SCAN_WIFI` added for direct ESP32-side SSID diagnostics.
  - Validation on the attached ESP32:
    firmware `0.3.1` flashed, BLE function `03` still returns a 195-byte main
    register response, and provisioning AP `Lumentree-475C` starts.
  - Production Wi-Fi validated with SSID `Devices`; ESP32 connected at
    `192.168.1.151` and uploaded multiple telemetry samples with HTTP `201`
    from `gateway_id=esp32-lumentree`.
  - The earlier `NLK` association failure was consistent with an unsuitable
    network for ESP32 production use; ESP32-side `SCAN_WIFI` did see both
    `NLK` and `Devices`, and `Devices` was selected for production.
  - Provisioning AP is stopped automatically after STA Wi-Fi connects, so the
    setup portal is not left open during normal production operation.
  - Final validation after reboot: public latest telemetry returned
    `gateway_id=esp32-lumentree`, firmware `lumentree-ble-bridge/0.3.1`, and
    Home Assistant `lumentreelocal` realtime sensors updated from the new
    ESP32 production uploader.
  - User validation confirmed the local ESP32 BLE read/upload path can run in
    parallel with the inverter's vendor cloud connection. The ESP32 path does
    not replace or intentionally disconnect the inverter vendor path.
- Health diagnostics and monitor hardening added.
  - API endpoint: `/api/lumentree/devices/{device_id}/health`
  - Health response includes latest telemetry age, online status, sample counts,
    average sample interval, max/min sample gap, gateway ID, firmware, and ESP32
    uptime.
  - Home Assistant integration version bumped to `0.5.0` and adds diagnostic
    sensors for telemetry age, sample interval, max gap, sample counts, uptime,
    gateway ID, and firmware.
  - `online_status` now uses backend health freshness instead of only coordinator
    connectivity.
  - Monitor check found a real stale period: ESP32 stopped uploading after
    telemetry id `553`; the health endpoint correctly reported `online=false`
    and a stale sample age.
  - Firmware was flashed again with a task watchdog and serial heartbeat. After
    reboot, uploads resumed at telemetry ids `554+`.
  - Latest validation after hardening: health returned `online=true`,
    latest age about `1s`, average sample interval about `20s`, and the outage
    remained visible as `max_sample_gap_seconds=2579.1`.
- BLE pairing flow foundation added.
  - Firmware version bumped to `lumentree-ble-bridge/0.4.0`.
  - New firmware default target MAC is empty for new devices; existing ESP32 NVS
    configuration still preserves the already paired MAC.
  - If `target_mac` is empty, `UPLOAD_ONCE` or production upload runs BLE
    discovery before reading telemetry.
  - Candidate matching uses the configured Device ID in BLE name, service UUID
    `a018739b-734d-8211-cb80-c9acd39d13b4`, and vendor GATT `FFE0`/`FFE1`.
  - If exactly one candidate matches, ESP32 stores its MAC and starts uploading.
    If multiple candidates match, ESP32 reports the candidate list to the API
    and the user should choose in the ESP32 portal.
  - ESP32 portal now has a BLE scan action and candidate list.
  - API endpoint added: `POST /api/lumentree/gateways/status`
  - API endpoint added: `GET /api/lumentree/gateways/{gateway_id}/health`
  - Device health now includes `pairing_status` and `gateway_status`.
  - Home Assistant integration version bumped to `0.6.0`; new diagnostic
    sensors include pairing status, target BLE MAC, and BLE candidate count.
  - Production validation after flashing `0.4.0`: public device health returned
    `online=true`, `pairing_status=paired`, gateway firmware
    `lumentree-ble-bridge/0.4.0`, target MAC `d8:13:2a:ee:58:d6`, Wi-Fi RSSI
    about `-58`, and telemetry continued updating.
- Home Assistant pairing UX improved.
  - Integration version bumped to `0.7.0`.
  - Initial setup no longer requires `/latest` telemetry to already exist. If
    the server is reachable and device health can be read, the entry can be
    created and HASS will wait for ESP32 pairing.
  - Options flow added so the user can change Device ID without deleting and
    recreating the integration.
  - Coordinator creates a one-shot persistent notification when
    `pairing_status=scanning_no_candidate`: "No inverter candidate found. Check
    the Device ID, move ESP32 closer to the inverter, then rescan from the ESP32
    portal."
  - Coordinator also notifies when multiple BLE candidates are found and directs
    the user to choose in the ESP32 portal.
- Firmware version source centralized.
  - Firmware name/version now come from PlatformIO build flags
    `LUMENTREE_FIRMWARE_NAME` and `LUMENTREE_FIRMWARE_VERSION`.
  - Serial status, HTTP telemetry payload, gateway status, API health, and the
    HASS `Firmware` entity use the same firmware build string after flashing.
- Dry-run command queue added and validated.
  - Firmware version bumped and flashed to `lumentree-ble-bridge/0.5.0`.
  - Home Assistant integration version bumped to `0.8.1`.
  - API table added: `lumentree_commands`.
  - API endpoints added:
    - `POST /api/lumentree/commands`
    - `GET /api/lumentree/devices/{device_id}/commands`
    - `GET /api/lumentree/gateways/{gateway_id}/commands/next`
    - `POST /api/lumentree/commands/{id}/result`
  - Only `mode=dry_run` and `command=dry_run_noop` are accepted.
  - ESP32 polls commands and returns dry-run results without BLE write calls.
  - Production validation: command id `1` moved from `requested` to
    `dry_run_completed`; result included `ble_write=false`,
    `modbus_write=false`, `write_enabled=false`, and
    `safety=no_ble_write_function_called`.
  - Reject validation: `set_charge_current_limit` returned HTTP `400` with
    `unsupported dry-run command`.
  - HA service exists as `lumentreelocal.dry_run_command`.
  - Command creation is authenticated. Lumentree Local options now include API
    token storage for command service use. Read-only entities still work without
    a token.
  - If the service is called without a token, HASS now returns a clear
    token-required error instead of an internal server error.
- Write authorization flow added and validated.
  - Firmware version bumped and flashed to `lumentree-ble-bridge/0.6.0`.
  - Home Assistant integration version bumped to `0.9.0`.
  - API tables added: `lumentree_write_pairing_codes`,
    `lumentree_write_grants`, and `lumentree_write_audit`.
  - ESP32 portal/serial can generate a short-lived one-time write pairing code.
  - HASS options accept the one-time code and store only the returned scoped
    write grant token.
  - `/api/lumentree/commands` now requires either server auth or a valid scoped
    write grant.
  - Unauthenticated command creation was validated to return HTTP `401`.
  - Dry-run through a scoped write grant completed with `ble_write=false`,
    `modbus_write=false`, and `write_enabled=false`.
  - Temporary validation grants were revoked after testing.

## Pending

- Update/restart Home Assistant from the pushed `lumentreelocal` integration to
  load version `0.9.0` health/pairing diagnostics, write access diagnostics,
  one-time write pairing code options flow, and dry-run service.
- Continue monitoring continuous uploads from `gateway_id=esp32-lumentree` in
  Home Assistant.
- Next development roadmap recorded in
  `docs/plans/2026-05-17-next-development-roadmap.md`.
- Current recommended next feature is BLE auto-discovery/pairing so a new user
  only enters Device ID in HASS and does not need to know the inverter BLE MAC
  or service UUID.
- Pairing flow is now explicitly defined in the roadmap: HASS enters Device ID
  only, the API reports whether telemetry exists, and ESP32 performs BLE
  discovery/binding when `target_mac` is empty.
- Vendor cloud exposes daily/monthly/yearly history APIs, but the current local
  BLE path has only proven realtime main-register reads. Keep local Postgres
  aggregation as the source of truth for local-first history, and treat vendor
  import/backfill as optional.

## Git Push Status

- Remote configured: `git@github.com:nlkcodenew/lumentreelocal.git`
- Local `main` tracks remote branch `origin/main`.
- The standalone ESP32 Lumentree workspace is now the canonical content for
  `origin/main`.
