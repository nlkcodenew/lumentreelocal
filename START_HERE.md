# START HERE: PRIVATE Runtime Checkout

Every new session must read this file first.

Global entrypoint for new sessions:

```text
/home/mrlinh/esp32-lumentree/LUMENTREE_START_HERE.md
```

This checkout is:

```text
/home/mrlinh/esp32-lumentree
```

Branch role:

```text
local-only
```

Remote role:

```text
private -> https://github.com/nlkcodenew/lumentreelocal-private
```

This checkout is for:

- firmware
- flash-site
- local server and host-side runtime work
- private docs, evidence, handoff, and operational scripts

Do not treat this checkout as the public HACS repo.

Public-safe work belongs in:

```text
/home/mrlinh/esp32-lumentree-public
```

## Read Order

1. `SESSION_HANDOFF.md`
2. `docs/status/2026-05-20-standard-firmware-flash-ha-update-runbook.md`
3. `README.md`
4. `firmware/README.md`

## Push Rules

- For this checkout, `git push` must go to `private`.
- Do not push this branch to `origin`.
- Before flashing new firmware, commit here first.

## Private Workflow

This checkout exists to hold runtime assets that must not return to the public
`main` branch.

Use this checkout for:

- firmware source and local build artifacts needed for production work
- flash-site source and public firmware files served from this machine
- local docs, handoff notes, evidence, and operational scripts

Public repo workflow:

1. Keep `/home/mrlinh/esp32-lumentree` on the `local-only` branch.
2. Commit local runtime changes here before flashing or deploying.
3. Use `/home/mrlinh/esp32-lumentree-public` as the clean public `main` worktree.
4. Only make public-safe changes in the public worktree, then commit/push there.

Remote mapping:

1. `private` is the push target for this checkout.
2. `origin` is the public repo and should not be used from this checkout.

Minimum rule before flashing a new firmware build:

1. Commit the firmware and any related flash-site changes on the local-only branch.
2. Verify the flash site still serves the expected manifest and binaries.

## Scope Index

- `firmware/`: ESP32 firmware
- `host/flash-site/`: flash website and firmware files served from this machine
- `host/local-server/`: local API server
- `docs/`: private notes, evidence, specs, runbooks
- `custom_components/lumentreelocal/`: integration code when a task truly spans runtime and integration

## Stable Facts

- Production API: `https://lumentree.jonah.io.vn`
- Local API bind: `127.0.0.1:8787`
- Flash site local bind: `127.0.0.1:8790`
- Home Assistant domain: `lumentreelocal`

## Current Feature Inventory

This section is the quickest summary of what the `esp32-lumentree` system
currently does across firmware, server, flash-site, and Home Assistant.

### 1. ESP32 onboarding

- first boot enters AP mode
- AP portal asks only for Wi-Fi credentials
- after Wi-Fi join, AP mode turns off
- the ESP32 local portal continues on LAN
- preferred local URL is mDNS-based:
  - `http://lumentree-xxxx.local`
- IP access still works as fallback

### 2. BLE bridge behavior

- ESP32 scans BLE for nearby Lumentree candidates
- user can inspect/select the intended inverter from the local portal
- internal binding is based on `Device ID` plus inverter `MAC`
- gateway pairing state and candidates are uploaded to the local server

### 3. Auth and onboarding model

- Home Assistant no longer uses `Device ID` alone for first-time read access
- first-time onboarding requires:
  - `Device ID`
  - `read pairing token` from the ESP32 local portal
  - optional `write pairing token`
- token entry is case-insensitive
- Home Assistant stores scoped grant tokens after claim:
  - `read_grant_token`
  - optional `write_grant_token`

### 4. Read path

- normal device read endpoints are protected
- `Device ID` alone is no longer enough to read inverter data
- Home Assistant reads through scoped read/write grants after onboarding

### 5. Write path

- direct inverter writes stay behind explicit write authorization
- write entities in Home Assistant use guarded semantic commands only
- generic arbitrary register writing is not part of the normal runtime path

### 6. Schedule safety model

Charge/discharge scheduling now has layered protection in all three runtime
layers:

- Home Assistant integration
- local server
- ESP32 firmware

Current invariant:

- the battery must never end up in a schedule state where enabled charge and
  enabled discharge windows overlap

Current behavior:

- enabling mains charge checks all enabled discharge windows
- enabling discharge checks all enabled mains charge windows
- editing `start` or `end` time requires the target slot to be `OFF` first
- touching boundaries are allowed
  - example: `08:00-10:00` and `10:00-12:00`
- overnight overlap is rejected correctly
  - example: `23:00-02:00` and `01:00-03:00`
- firmware re-checks immediately before BLE write, so stale or replayed queued
  commands are still blocked at the last safety boundary

### 7. Energy and telemetry

- realtime telemetry upload is active
- energy counters are available
- server stores telemetry and energy history in Postgres
- Home Assistant exposes realtime sensors, energy sensors, diagnostics, and
  guarded write entities when authorized
- the Solar dashboard now prefers inverter-native battery power and status
  instead of the legacy adjusted helper
- the integration/server now expose billing-cycle load energy plus an estimated
  equivalent bill for the EVN-style cycle:
  - reset boundary: `22nd 00:00`
  - cycle end: next `22nd 00:00`

### 8. Current important caveats

- the local server service must be restarted after server code changes to make
  runtime gates live
- firmware TLS hardening is still an important future task
- full multi-user server hardening remains deferred beyond the current scoped
  grant model

## Session Rule

Before editing anything, confirm whether the task belongs to:

1. private runtime checkout
2. public HACS checkout

If unsure, run:

```bash
git whereami
```
