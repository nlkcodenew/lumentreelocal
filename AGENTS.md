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

## HA Routing Rule

- For Home Assistant polling, prefer `HA -> local origin` over
  `HA -> Cloudflare public hostname`.
- The current local origin is `http://127.0.0.1:8787` on the Ubuntu host, and
  the public hostname `https://lumentree.jonah.io.vn` should be treated as the
  remote-access path, not the preferred HA-internal path.
- A real observed failure mode is:
  - the local server stays healthy and keeps returning `200/201`
  - `ESP32-C3` keeps uploading telemetry normally
  - but Home Assistant logs intermittent `530/502` or
    `cannot connect to local Lumentree server`
  - the warning turns out to be on the public/tunnel/proxy path rather than in
    BLE, firmware, or Postgres
- This means future sessions should verify tunnel/public reachability before
  blaming the board, BLE cadence, or the local origin.

## HAOS VM Local-Origin Detail

- In the current single-host setup with HAOS VM on `192.168.122.230`, the
  practical local-origin route is:
  - `HAOS VM -> http://192.168.122.1:8787`
- Local server service must bind host-wide (not loopback-only):
  - `/etc/lumentree/local-server.env`
  - `LUMENTREE_SERVER_HOST=0.0.0.0`
- Do not set HA integration back to Cloudflare URL for internal polling unless
  local-origin route is intentionally unavailable.

## Safe Direction For HA Networking

- The stable architecture going forward is:
  - `ESP32 -> local server/Postgres`
  - `Home Assistant -> local server origin`
  - `remote browser/user -> Cloudflare hostname`
- Do not make Home Assistant depend on Cloudflare tunnel health if the local
  origin is available.
- Do not widen this into a larger auth or server refactor unless the user asks.
- Keep bearer-token auth on the local API; changing HA routing from public to
  local origin is an operational hardening step, not a security rollback.

## ESP32-S3 Current Runtime Line (Latest Session)

- Latest validated active line is now on `ESP32-S3` (test/runtime line), not
  replacing the `ESP32-C3` LAN baseline yet.
- Current S3 runtime profile:
  - env: `esp32-s3-8mb-fastbulk-task-poll3s`
  - command poll: `3000 ms`
  - upload interval: `3 s`
  - main full refresh: `60000 ms`
  - settings poll: `180000 ms`
- Timing knobs are now env-driven for this line (do not re-hardcode fixed
  timing values in firmware).

## SSE Push Baseline (Server -> HA)

- Local server now supports SSE telemetry stream:
  - `GET /api/lumentree/devices/{device_id}/stream`
- HA integration `lumentreelocal` now subscribes to SSE in background and keeps
  polling fallback.
- If future sessions touch this area:
  - keep SSE reconnect behavior
  - keep polling fallback path
  - avoid startup blocking by using HA background task APIs for long-running
    stream listeners.

## Known Warning Artifact (Not Runtime Bug)

- A prior warning burst in HA:
  - `homeassistant.components.http.ban`
  - invalid auth requests from `192.168.122.1` to `/api/`
  came from manual unauthenticated curl probes during HA reboot checks.
- Treat that as operator test artifact, not inverter/BLE/SSE failure.
