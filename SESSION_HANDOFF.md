# Session Handoff

Continue private runtime work in:

```text
/home/mrlinh/esp32-lumentree
```

This checkout must stay on:

```text
branch = local-only
remote = private
```

Public HACS work is separate:

```text
/home/mrlinh/esp32-lumentree-public
branch = main
remote = origin
```

## Read First

1. `START_HERE.md`
2. `docs/status/2026-05-20-standard-firmware-flash-ha-update-runbook.md`
3. `docs/status/2026-05-20-read-write-token-claim-implementation-status.md`
4. `docs/specs/2026-05-20-multi-user-read-claim-spec.md` if the next session
   discusses shared-server or multi-user access control
5. `docs/specs/2026-05-20-read-write-token-claim-flow-spec.md` for the chosen
   AP-Wi-Fi-only + LAN-portal token onboarding design

## Current Baseline

- Private runtime branch HEAD should be checked with `git whereami`
- Production API: `https://lumentree.jonah.io.vn`
- Local API bind: `127.0.0.1:8787`
- Flash site local bind: `127.0.0.1:8790`
- Home Assistant domain: `lumentreelocal`
- Read auth is now live:
  - normal device read endpoints require server token or scoped read/write
    grant
  - unauthenticated `/api/lumentree/devices/{device_id}/latest` now returns
    `401`
- Current onboarding direction is implemented in private runtime:
  - AP mode is Wi-Fi-only onboarding
  - LAN portal generates required read token and optional write token
  - HASS claims `read_grant`/`write_grant` with `Device ID`
- Multi-user hardening beyond this remains deferred and recorded in
  `docs/specs/2026-05-20-multi-user-read-claim-spec.md`
- Ownership reset caveat:
  - do not delete rows from `lumentree_devices` to unlink a device
  - deleting that row still cascades historical telemetry and energy data
  - current safe reset scope is grants/tokens/candidates only

## Operational Rule

- Commit private runtime changes here before flash/deploy.
- Push private runtime changes to `private`.
- Do not use this checkout for public-only HACS cleanup or release commits.

## Next-Session Rule

Before doing any work:

1. run `git whereami`
2. confirm whether the task is private runtime or public HACS
3. switch checkout if needed before editing
