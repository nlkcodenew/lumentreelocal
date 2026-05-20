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
3. `docs/specs/2026-05-20-multi-user-read-claim-spec.md` if the next session
   discusses shared-server or multi-user access control
4. `docs/specs/2026-05-20-read-write-token-claim-flow-spec.md` if the next
   session discusses the chosen replacement for AP-portal API token entry and
   grant-based read/write onboarding

## Current Baseline

- Private runtime branch HEAD should be checked with `git whereami`
- Production API: `https://lumentree.jonah.io.vn`
- Local API bind: `127.0.0.1:8787`
- Flash site local bind: `127.0.0.1:8790`
- Home Assistant domain: `lumentreelocal`
- Current production decision: keep the existing simple read flow for now;
  multi-user read protection is deferred and recorded in
  `docs/specs/2026-05-20-multi-user-read-claim-spec.md`
- Chosen future auth direction: AP mode should become Wi-Fi-only onboarding,
  LAN portal should generate required read token and optional write token, and
  HASS should claim `read_grant`/`write_grant` with `Device ID`; see
  `docs/specs/2026-05-20-read-write-token-claim-flow-spec.md`

## Operational Rule

- Commit private runtime changes here before flash/deploy.
- Push private runtime changes to `private`.
- Do not use this checkout for public-only HACS cleanup or release commits.

## Next-Session Rule

Before doing any work:

1. run `git whereami`
2. confirm whether the task is private runtime or public HACS
3. switch checkout if needed before editing
