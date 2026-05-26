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
12. `docs/status/2026-05-24-session-wrapup-c3-runtime-and-s3-erase.md` for the
    end-of-session runtime summary: C3 live state, published flash-site
    outcome, and the fact that the older S3 board was erased without reflash
13. `docs/plans/2026-05-24-persistent-ble-session-refactor-plan.md` for the
    accepted direction away from reconnect-per-read BLE telemetry
14. `docs/specs/2026-05-24-persistent-ble-session-and-adaptive-polling-spec.md`
    for the intended runtime session/scheduler model
15. `docs/status/2026-05-24-persistent-ble-session-refactor-implementation.md`
    for the compile-validated implementation checkpoint and hardware test
    caveats
16. `docs/status/2026-05-24-c3-persistent-ble-refactor-rollout-and-rollback.md`
    for the real `ESP32-C3` refactor outcome, recommended firmware, and exact
    rollback procedure to both the last good refactor stage and the exact
    pre-refactor checkpoint
17. `docs/status/2026-05-24-c3-placement-and-ota-next-direction.md`
    for the recorded next-step direction: move the `ESP32-C3` near the
    inverter, use floor-3 Wi-Fi, and treat OTA as the next operational
    priority with explicit `4MB` flash-layout constraints
18. `docs/specs/2026-05-25-dual-line-firmware-strategy-c3-lan-update-s3-ota-stable.md`
    for the formal two-line firmware strategy: `ESP32-C3` first with LAN
    update after relocation, then `ESP32-S3` as the OTA-stable line after
    runtime uplift
19. `docs/status/2026-05-26-ha-local-origin-vs-cloudflare-guidance.md`
    for the current HA networking rule: prefer local origin for HA polling and
    treat Cloudflare as remote-access infrastructure, not the default HA path
20. `AGENTS.md`
    for the current operational rules around `ESP32-C3` LAN control, NVS config
    storage, telemetry-stall recovery, and the current OTA/LAN-update limits

## Current Baseline

- Private runtime branch HEAD should be checked with `git whereami`
- Production API: `https://lumentree.jonah.io.vn`
- Local API bind: `127.0.0.1:8787`
- Preferred HA polling path should be the local origin rather than the public
  Cloudflare hostname whenever HA can reach the Ubuntu host directly
- Flash site local bind: `127.0.0.1:8790`
- Home Assistant domain: `lumentreelocal`
- Current experimental runtime board after this session:
  - `ESP32-C3 Super Mini`
  - gateway `esp32-lumentree-01cc9c`
  - mDNS `lumentree-cc9c.local`
  - last confirmed LAN IP `192.168.1.245`
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
- Hardware test caveat after this session:
  - the older `ESP32-S3` board with MAC `3c:dc:75:63:47:5c` was erased only
  - it is blank now and does not have firmware on it unless reflashed later
- Persistent BLE session caveat:
  - compile-only uncertainty is no longer the current state for `ESP32-C3`
  - real hardware testing on the `ESP32-C3` board concluded with
    `9f52f5d` / `esp32-c3-4mb-debug-cmdtask-fastbulk-task` as the current
    recommended runtime
  - exact rollback instructions are recorded in
    `docs/status/2026-05-24-c3-persistent-ble-refactor-rollout-and-rollback.md`
- Current remote-control capability on the live `ESP32-C3`:
  - `GET /api/status`
  - `GET /api/logs`
  - `POST /api/reboot`
  - `POST /api/command`
  - `POST /api/configure`
  - `POST /api/ble_connection`
- Current live semantics:
  - local config is stored on the board in `NVS` via `Preferences`
  - Wi-Fi/device identity/target MAC no longer require AP-mode access for
    normal changes if the board is reachable on LAN
  - BLE can now be disabled and re-enabled independently of Wi-Fi/local web
  - `LAN firmware update` is still not an accepted operational path on the
    `ESP32-C3 4MB` line because tested dual-slot layouts boot-looped on real
    hardware
- Current `ESP32-C3` live recovery baseline after latest flash/recovery:
  - LAN reachable at `192.168.1.245`
  - `device_id = P240819130`
  - `target_mac = d8:13:2a:ee:58:d6`
  - telemetry was re-confirmed healthy with:
    - `main_telemetry_cache_valid = true`
    - `main_telemetry_cache_valid_registers = 95`
    - `telemetry_upload.last_http_status = 201`
    - `telemetry_upload.failures = 0`
- Current publish/deploy baseline after the 2026-05-26 release snapshot:
  - Home Assistant integration version in this private runtime repo:
    - `0.14.18`
  - published `ESP32-C3` website line now points to:
    - `esp32-c3-4mb-debug-cmdtask-fastbulk-task`
    - firmware version `0.15.2-exp-c3-fastbulk-task`
  - older `ESP32-S3` website line is intentionally preserved as:
    - `esp32-s3-8mb-nopsram-release`
    - firmware version `0.15.1`
  - new `ESP32-S3` website preview line is published separately as:
    - `esp32-s3-8mb-nopsram-fastbulk-preview`
    - firmware version `0.15.2-preview-s3-fastbulk-task`
  - flash website now exposes three choices:
    - `ESP32-S3 Stable Legacy`
    - `ESP32-S3 Preview`
    - `ESP32-C3 Super Mini`

## Operational Rule

- Commit private runtime changes here before flash/deploy.
- Push private runtime changes to `private`.
- Do not use this checkout for public-only HACS cleanup or release commits.

## Next-Session Rule

Before doing any work:

1. run `git whereami`
2. confirm whether the task is private runtime or public HACS
3. switch checkout if needed before editing
