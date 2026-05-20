# 2026-05-20 Read/Write Token Claim Implementation Status

## Scope Completed

- Server now creates additive auth tables for:
  - `lumentree_gateway_candidates`
  - `lumentree_read_pairing_tokens`
  - `lumentree_read_grants`
  - `lumentree_auth_audit`
- Device read endpoints now require auth.
- ESP32 portal onboarding is split into:
  - AP mode: Wi-Fi only
  - LAN portal: BLE scan, candidate select, read token, optional write token
- Home Assistant integration initial setup now requires:
  - `Device ID`
  - `read_pairing_token`
  - optional `write_pairing_token`

## Live Runtime Check

Verified on `2026-05-20` against the running private runtime system:

- local API process restarted on `127.0.0.1:8787` with the new code
- new auth tables exist in Postgres
- unauthenticated `GET /api/lumentree/devices/P240819130/latest` now returns `401`
- authenticated read with server token still returns `200` for backward compatibility
- firmware `0.15.1` was flashed successfully to `/dev/ttyACM0`
- LAN portal on `http://192.168.1.151` responds in STA mode
- `POST /api/write_code` on the ESP32 portal succeeds
- `POST /api/read_token` succeeds after the ESP32 has uploaded a matching BLE candidate
- read grant claim, read access, write grant claim, dry-run command, and grant revoke all succeeded in live tests

## Important Behavior Notes

- Read token generation depends on a matching candidate row on the server.
- In practice this means the user must BLE-scan from the LAN portal before claiming read access.
- The current server implementation treats `mac` as the hard internal binding key and `device_id` as the user-facing selector.

## Current Caveat

- There is still no dedicated durable `device_binding` table for ownership transfer/reset.
- Do not `DELETE` from `lumentree_devices` to "unlink" a device. That still cascades and deletes historical telemetry and daily energy rows.
- Current safe reset scope is:
  - revoke/delete read grants
  - revoke/delete write grants
  - optionally clear pairing tokens/candidates
  - keep `lumentree_devices`, `lumentree_telemetry`, and `lumentree_energy_daily`

## Follow-Up Candidate

- If ownership transfer or "factory re-claim" becomes important, add a separate binding table instead of encoding that workflow through `lumentree_devices` rows.
