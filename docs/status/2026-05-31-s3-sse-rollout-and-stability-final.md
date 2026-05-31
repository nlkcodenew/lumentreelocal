# 2026-05-31 S3 SSE Rollout And Stability Final

## Scope

- Stabilize `ESP32-S3` telemetry/runtime after OTA branch regressions.
- Keep `ESP32-C3` line untouched (LAN-only, no USB flash this session).
- Improve end-to-end latency path:
  - inverter BLE -> ESP32-S3 -> local server -> Home Assistant.
- Deploy and validate SSE push from local server to HA integration.

## Firmware / Runtime Changes (S3)

- Restored working pre-OTA baseline path for S3 runtime behavior.
- Fixed HTTP lane starvation by reducing inter-lane HTTPS gap.
- Converted key timing knobs to env-driven behavior in firmware:
  - `LUMENTREE_UPLOAD_INTERVAL_SECONDS`
  - `LUMENTREE_MIN_TELEMETRY_INTERVAL_MS`
  - `LUMENTREE_COMMAND_POLL_INTERVAL_MS`
  - `LUMENTREE_MAIN_FULL_REFRESH_INTERVAL_MS`
  - `LUMENTREE_SETTINGS_POLL_INTERVAL_MS`
  - `LUMENTREE_STATS_UPLOAD_INTERVAL_MS`
- Confirmed env `esp32-s3-8mb-fastbulk-task-poll3s` effective runtime:
  - `upload_interval_seconds = 3`
  - `command_poll_interval_ms = 3000`
  - `main_full_refresh_interval_ms = 60000`
  - `settings_poll_interval_ms = 180000`

## Server / HA Changes

- Added SSE endpoint on local server:
  - `GET /api/lumentree/devices/{device_id}/stream`
  - emits `telemetry` events and periodic `heartbeat`.
- Added HA integration SSE client path with polling fallback:
  - background stream listener subscribes to SSE
  - on telemetry event, coordinator requests refresh
  - reconnect loop retained for transient failures
  - normal polling still present as fallback safety
- Reduced HA default polling interval from `10s` to `5s` (still with dynamic fast poll on pending commands).

## Bug Found And Fixed During Rollout

- Bug: initial SSE listener task used non-background startup path and caused HA startup blocking warning (`Setup timed out ... _stream_loop`).
- Fix: switched to HA background task API for stream listener.
- Result: no further startup-blocking warnings from this integration after redeploy.

## Validation Evidence (Session)

- Local server health:
  - `GET /health` returns `{"ok":true,"database":"ok"}`.
- SSE stream:
  - telemetry and heartbeat events observed on live endpoint.
- Postgres telemetry freshness:
  - repeated checks showed low lag (typically seconds, no >2 minute stalls after fixes).
- HA update cadence after SSE deploy:
  - measured `latest_telemetry_age` `last_updated` interval in test windows ~sub-10s, average around ~4-5s in sampled windows.

## Home Assistant HTTP Ban Warning Note

- `homeassistant.components.http.ban` warnings around `15:17:50` to `15:19:47` were caused by unauthenticated `/api/` curl probes from host `192.168.122.1` during HA reboot checks.
- This was an operator test artifact, not firmware/SSE runtime auth logic.
- No continuing burst of new ban warnings after stopping those probes.

## Final S3 Firmware Artifact

- Built env:
  - `esp32-s3-8mb-fastbulk-task-poll3s`
- Final bin:
  - `firmware/bin/s3/lumentree-s3-poll3s-sse-envdriven-a180f64.bin`
- SHA256:
  - `46fb4e2b979f5aa431c3ca44f2d6dafa94d6990545a4f4bded2ddb619ed5e0f2`

## Git References (private runtime repo)

- `f475c12` fix(s3): reduce HTTP inter-lane gap to prevent telemetry starvation
- `01900ba` perf: make telemetry intervals env-driven and reduce HA poll latency
- `16fcfc9` feat: add SSE push path from server to HA with polling fallback
- `a180f64` fix(ha): run SSE listener as background task to avoid startup blocking

## Current Operational Baseline (End Of Session)

- Active board focus:
  - `ESP32-S3` on USB/COM test line, poll3s env, SSE-enabled stack.
- `ESP32-C3`:
  - remains LAN-only line, no new flash in this session.
- Recommended architecture now:
  - `ESP32 -> local server/Postgres`
  - `HA -> local server (local origin)`
  - SSE push for fast UI refresh, polling as safety fallback.
