# Lumentree BLE ESP32 Tasks

## Task Checklist

- [x] Record current branch, commit, firmware build status, and upload/rollback method before touching firmware.
- [x] Confirm the current ESP32 serial port and that serial monitor/upload still work.
- [x] Add a Lumentree BLE probe flag that is disabled by default.
- [x] Prevent Lumentree probe code from running automatically in the main loop.
- [x] Add a manual one-shot passive scan command.
- [x] Use short scan duration, initially 1-2 seconds.
- [x] Reduce scan duty cycle from near-continuous listening to a conservative interval/window.
- [x] Capture raw advertisement evidence: MAC, name, RSSI, service UUIDs, manufacturer data hex, service data hex, timestamp.
- [x] Add before/after safety telemetry: Wi-Fi status, Wi-Fi RSSI, MQTT connected state, and local uptime.
- [x] If Wi-Fi or MQTT drops after scan, disable BLE probe and require manual re-enable.
- [x] Add Lumentree candidate whitelist by MAC first; name/service UUID matching can follow after evidence exists.
- [x] Add manual GATT discovery command for a whitelisted device only.
- [x] Make GATT discovery connect once, discover services/characteristics, log properties, and disconnect immediately.
- [x] Do not write any characteristic during scan or discovery.
- [ ] Build a read-only Modbus frame only after GATT discovery identifies likely transport characteristics.
- [ ] Test exactly one small read request before any polling loop.
- [ ] Compare raw BLE response against Home Assistant Lumentree raw MQTT/register data.
- [x] Document discovered UUIDs, raw frames, and risk observations in the plan notes before implementing polling.

## Acceptance Criteria

- Initial firmware change builds successfully with PlatformIO.
- BLE probe is opt-in/manual and cannot run periodically by default.
- A scan can be triggered and produces raw candidate evidence.
- Wi-Fi and MQTT state are logged before and after every BLE action.
- Any BLE-induced connectivity drop disables future automatic BLE work.
- GATT discovery is unavailable until a candidate device is whitelisted.
- No code path writes inverter configuration or Modbus registers.

## Rollback Notes

- Keep USB serial upload available before testing BLE changes.
- If BLE causes Wi-Fi instability, revert to the last commit before BLE probe changes or disable probe through config.
- If the ESP32 becomes unreachable over Wi-Fi, use `/dev/ttyACM0` upload path to restore a known-good firmware.
