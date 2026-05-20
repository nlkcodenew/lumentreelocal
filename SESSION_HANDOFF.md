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
2. `LOCAL_ONLY_WORKFLOW.md`
3. `docs/status/2026-05-20-standard-firmware-flash-ha-update-runbook.md`

## Current Baseline

- Private runtime branch HEAD should be checked with `git whereami`
- Production API: `https://lumentree.jonah.io.vn`
- Local API bind: `127.0.0.1:8787`
- Flash site local bind: `127.0.0.1:8790`
- Home Assistant domain: `lumentreelocal`

## Operational Rule

- Commit private runtime changes here before flash/deploy.
- Push private runtime changes to `private`.
- Do not use this checkout for public-only HACS cleanup or release commits.

## Next-Session Rule

Before doing any work:

1. run `git whereami`
2. confirm whether the task is private runtime or public HACS
3. switch checkout if needed before editing
