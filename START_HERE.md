# START HERE: ESP32 Lumentree

Every new Codex session must read this file first.

Open Codex sessions in:

```text
/home/mrlinh/esp32-lumentree
```

Canonical repo:

```text
https://github.com/nlkcodenew/lumentreelocal
```

Do not continue active Lumentree work in:

```text
/home/mrlinh/esp32-s3-openclaw
```

## Read Order

1. `docs/status/2026-05-20-standard-firmware-flash-ha-update-runbook.md`
2. `SESSION_HANDOFF.md`
3. `README.md`
4. `firmware/README.md`

Then only open deeper docs that match the task:

- firmware / function `0x04`:
  `docs/specs/2026-05-20-function-04-firmware-integration-spec.md`
- server / production API:
  `docs/specs/2026-05-17-lumentree-local-vendor-server.md`
- write safety:
  `docs/plans/2026-05-17-write-safety-plan.md`
- HACS release flow:
  `docs/status/2026-05-19-hacs-release-flow.md`

## Repo Index

- `firmware/`: ESP32 firmware
- `custom_components/lumentreelocal/`: Home Assistant integration
- `host/local-server/`: local API server
- `host/flash-site/`: public flash site
- `docs/status/`: runbooks and rollout notes
- `docs/specs/`: implementation specs

## Stable Facts

- Production API: `https://lumentree.jonah.io.vn`
- Local API bind: `127.0.0.1:8787`
- Home Assistant domain: `lumentreelocal`
- Standard firmware release env:
  `firmware/platformio.ini -> [env:esp32-s3-8mb-nopsram-release]`

Avoid treating old session snapshots as current truth. For flash, function
`0x04`, HA update, and HASS reload/restart decisions, the 2026-05-20 runbook
is the operational source of truth.

## Working Rules

- Verify runtime truth before changing behavior.
- Keep firmware, server, and integration scope separate unless the task
  explicitly crosses them.
- Do not add inverter-setting writes without explicit approval.
- After each successful task, commit, push, and record the result in repo docs.
