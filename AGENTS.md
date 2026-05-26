# AGENTS Notes

## ESP32-C3 Runtime Follow-Up

- Current experimental runtime line for the real board is
  `0.15.1-exp-c3-debug-cmdtask-fastbulk-task`.
- A real observed failure mode is:
  - local `/api/status` stays reachable
  - command polling keeps working
  - but BLE telemetry/upload can stall for a long time
  - Home Assistant then shows very large `latest_telemetry_age` and
    `online=false`
- This means a future session must not assume "board alive" implies
  "telemetry alive".

## Watchdog / Recovery Reminder

- The current main loop watchdog can keep the board alive even if the telemetry
  lane is wedged.
- Future firmware work should add explicit stalled-session recovery:
  - if BLE reads or uploads stop succeeding for repeated cycles
  - invalidate the BLE session and reconnect with backoff
  - only consider `ESP.restart()` after repeated recovery failure

## LAN-Only Recovery Reminder

- If the `ESP32-C3` is no longer on USB but still reachable on LAN, the current
  firmware already exposes practical remote recovery and control paths:
  - `POST /api/reboot` for a direct reboot
  - `POST /api/ble_scan`
  - `POST /api/select_candidate`
  - `GET /api/logs`
  - `POST /api/command`
  - `POST /api/configure`
  - `POST /api/ble_connection`
- Re-selecting a known candidate resets the Modbus session via
  `resetModbusSession("portal_candidate_selected")`, which is a lighter-weight
  recovery step than full reboot when BLE telemetry appears stuck.

## Config Storage Rule

- The current `ESP32-C3` runtime stores local operational config in `NVS`
  through `Preferences` namespace `lumentree`, not in Postgres.
- This includes:
  - `wifi_ssid`
  - `wifi_pass`
  - `device_id`
  - `target_mac`
  - `api_url`
  - `api_token`
  - `gateway_id`
  - `ble_en`
- Server/Postgres holds backend data like telemetry, events, and command state,
  but not the board's local boot/runtime config.

## LAN Control Rule

- Current private runtime line now supports structured LAN configuration without
  AP-mode onboarding for normal maintenance:
  - `POST /api/configure` can change `ssid`, `password`, `device_id`,
    `target_mac`, `api_url`, `api_token`, `gateway_id`, and
    `production_enabled`
  - `POST /api/ble_connection` can disable or re-enable BLE reads to the
    inverter without taking down Wi-Fi or local web
- The local portal in STA mode now includes:
  - Wi-Fi config form
  - BLE enable/disable buttons
  - log viewer
  - command runner
- This means future sessions should prefer LAN control first, and only fall
  back to AP mode or USB when the board is unreachable on LAN or needs a full
  reflash.

## Placement / OTA Direction

- The next practical deployment direction is:
  - move `ESP32-C3` physically close to the inverter
  - connect it to floor-3 `2.4 GHz` Wi-Fi
  - then prioritize OTA design because the board will be harder to reach
- For `ESP32-C3 4MB`, safe dual-slot OTA is still blocked by current firmware
  size and partition layout, so do not assume OTA is already solved.
- `LAN firmware update` UI/API exists conceptually, but it is still not a
  supported operational path on `ESP32-C3 4MB` because tested dual-slot layouts
  boot-looped on real hardware.
