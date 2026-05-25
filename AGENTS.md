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
  firmware already exposes one practical remote recovery path:
  - `POST /api/save` on the local portal triggers `ESP.restart()`
  - this works even for same/no-op Wi-Fi config payloads because the handler
    always responds with `rebooting=true` and then restarts
- The local portal also has:
  - `POST /api/ble_scan`
  - `POST /api/select_candidate`
- Re-selecting a known candidate resets the Modbus session via
  `resetModbusSession("portal_candidate_selected")`, which is a lighter-weight
  recovery step than full reboot when BLE telemetry appears stuck.

## Placement / OTA Direction

- The next practical deployment direction is:
  - move `ESP32-C3` physically close to the inverter
  - connect it to floor-3 `2.4 GHz` Wi-Fi
  - then prioritize OTA design because the board will be harder to reach
- For `ESP32-C3 4MB`, safe dual-slot OTA is still blocked by current firmware
  size and partition layout, so do not assume OTA is already solved.
