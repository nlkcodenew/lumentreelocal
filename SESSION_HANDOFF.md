# Session Handoff

Continue work in:

```text
/home/mrlinh/esp32-lumentree
```

Canonical repo:

```text
https://github.com/nlkcodenew/lumentreelocal
```

## Read First

1. `docs/status/2026-05-20-standard-firmware-flash-ha-update-runbook.md`
2. `README.md`
3. `firmware/README.md`
4. `docs/status/2026-05-19-hacs-release-flow.md`

## Current Baseline

- Current repo HEAD: `e3117de`
- Production API: `https://lumentree.jonah.io.vn`
- Local API bind: `127.0.0.1:8787`
- Home Assistant domain: `lumentreelocal`
- Firmware release env:
  `firmware/platformio.ini -> [env:esp32-s3-8mb-nopsram-release]`
- Firmware release version source:
  `firmware/platformio.ini -> 0.15.1`
- Integration version source:
  `custom_components/lumentreelocal/manifest.json -> 0.14.11`

## Operational Truth

The standard runbook already reflects the verified state from 2026-05-20:

- firmware flash procedure
- function `0x04` validation with `READ_STATS_ONCE`
- Postgres verification for `today_statistics_0_7`
- HACS update check for `update.lumentree_local_update`
- HASS reload first, full restart only if needed

Do not reconstruct these steps from older status docs unless the runbook is
being intentionally updated.

## What Matters Most

- Architecture stays:

```text
Inverter -> BLE -> ESP32 -> HTTPS API -> Home Assistant
```

- BLE is only between inverter and ESP32.
- ESP32 and Home Assistant both use the HTTP API server, not MQTT.
- Write flow is guarded and semantic, not generic raw register write.

## Next-Session Rule

Before changing anything:

1. confirm which surface is in scope: firmware, server, integration, or docs
2. read the matching primary doc
3. verify live/runtime truth if the task depends on current state

After each successful task:

1. commit
2. push
3. record the result in repo docs
