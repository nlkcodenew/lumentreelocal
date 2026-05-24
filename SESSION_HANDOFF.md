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
4. `docs/status/2026-05-20-auth-refactor-wrapup-and-future-work.md`
5. `docs/specs/2026-05-20-multi-user-read-claim-spec.md` if the next session
   discusses shared-server or multi-user access control
6. `docs/specs/2026-05-20-read-write-token-claim-flow-spec.md` for the chosen
   AP-Wi-Fi-only + LAN-portal token onboarding design
7. `docs/specs/2026-05-21-schedule-safety-guard-spec.md` for mandatory
   charge/discharge overlap protection across HA, server, and firmware
8. `docs/status/2026-05-21-schedule-safety-implementation-status.md` for the
   implementation result, validation, and rollout caveats
9. `docs/status/2026-05-23-battery-card-and-billing-cycle-refactor.md` for the
   Solar dashboard battery-source cleanup and EVN billing-cycle sensors
10. `docs/status/2026-05-24-esp32-c3-experimental-first-boot.md` for the first
    real `ESP32-C3 4MB` experimental build/flash/boot evidence
11. `docs/status/2026-05-24-esp32-c3-flash-site-publish.md` for the point
    where the experimental `ESP32-C3 Super Mini` line became selectable on the
    flash website

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
- Detailed wrap-up of what changed and what should happen next is recorded in
  `docs/status/2026-05-20-auth-refactor-wrapup-and-future-work.md`
- Schedule write safety is not finished until the invariant in
  `docs/specs/2026-05-21-schedule-safety-guard-spec.md` is implemented across
  all three layers
- As of 2026-05-21 the code implementation is complete across HA, server, and
  firmware:
  - HA blocks overlapping enable commands both ways
  - HA requires turning a slot OFF before editing its start/end time
  - server rejects unsafe schedule commands against a fresh settings snapshot
  - firmware re-validates immediately before BLE write and rejects stale or
    replayed unsafe commands
  - the overnight-overlap algorithm is canonicalized and test-covered
  - follow-up latency tuning also landed:
    - firmware polls commands before the periodic read/upload loop
    - firmware uploads settings immediately after successful writes
    - firmware fetches the next queued command without waiting the full normal
      poll interval
    - HA temporarily fast-polls while command status is `requested` or `sent`
- Rollout caveat:
  - firmware was rebuilt and flashed to `/dev/ttyACM0`
  - local `lumentree-local-server.service` could not be restarted from this
    session because `systemctl restart` required interactive authentication
  - read `docs/status/2026-05-21-schedule-safety-implementation-status.md`
    before assuming the live service has already picked up the new server gate
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
