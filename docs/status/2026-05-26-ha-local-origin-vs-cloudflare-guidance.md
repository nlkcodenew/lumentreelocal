# 2026-05-26 HA Local-Origin vs Cloudflare Guidance

## What Happened

During runtime checks after the `ESP32-C3` and `lumentreelocal` stabilization
work, Home Assistant still showed intermittent warnings such as:

- `Required health fetch failed ... cannot connect to local Lumentree server`
- `Latest telemetry fetch failed ...`
- `GET ... failed: 530 ...`

At the same time, the actual local runtime remained healthy:

- `ESP32-C3` stayed on LAN and continued BLE telemetry uploads
- local server `127.0.0.1:8787` kept returning `200/201`
- Home Assistant entities usually remained fresh and online

The warning pattern matched a public-path issue rather than a local-origin
issue. A likely trigger was accidental deletion or disruption of the
Cloudflare/tunnel route used by the public hostname.

## Operational Conclusion

The correct architecture should be treated as:

- `ESP32 -> local server/Postgres`
- `Home Assistant -> local server origin`
- `remote browser/user -> Cloudflare hostname`

This avoids making Home Assistant dependent on Cloudflare/tunnel health for
routine local polling.

## Why This Is Safer

- It removes an unnecessary external dependency from the HA polling path.
- It reduces false alarms where HA warns even though firmware and local server
  are both healthy.
- It does not require changing BLE architecture, firmware telemetry grouping,
  or database design.
- It does not weaken security by itself, because bearer-token auth still
  applies on the local API path.

## Guardrail For Future Sessions

- If HA warnings mention `502`, `530`, `Cloudflare`, or proxy-style HTML
  responses, check the public/tunnel path first.
- Do not blame BLE, RF, or `ESP32-C3` first if:
  - board LAN status is healthy
  - local server health is healthy
  - local server logs still show `200/201`
- Prefer small operational fixes over broad refactors:
  - re-point HA to local origin if appropriate
  - keep Cloudflare only for remote access
  - avoid changing firmware/networking architecture unless the user explicitly
    asks for it
