# Production Write Scope

Date: 2026-05-19

## Problem

Home Assistant showed active write access, but the write status entity still
reported:

- `scope = dry_run`
- `command_mode = dry_run_only`
- `safety = no_real_inverter_write_enabled`

That no longer matched the real system behavior because guarded production
writes had already been validated and were in active use.

## Root Cause

The local server still had test-era defaults:

- `WRITE_GRANT_SCOPE = "dry_run"`
- `write_grant_status()` always returned `command_mode = "dry_run_only"`
- docs still described the claimed grant as a dry-run grant

At the same time, the Home Assistant integration was already sending real
guarded write commands with `mode = "write"`.

## Fix

- Changed server grant scope default from `dry_run` to `write`
- Auto-upgraded legacy active grants from `dry_run` to `write` when validated
- Enforced that `mode = write` commands require a write-scope grant unless the
  request uses the server token
- Updated write status output to:
  - `command_mode = guarded_write`
  - `safety = allowlisted_semantic_write_only`
- Updated firmware/integration/server docs to stop describing the current
  grant as dry-run only

## Runtime Verification

After restarting the production API server and allowing Home Assistant to poll
again, runtime state showed:

- `sensor.lumentree_local_p240819130_write_access_status = enabled`
- `scope = write`
- `command_mode = guarded_write`
- `safety = allowlisted_semantic_write_only`
- `binary_sensor.lumentree_local_p240819130_write_grant_active = on`

## Files

- `host/local-server/server.py`
- `host/local-server/README.md`
- `firmware/README.md`
- `custom_components/lumentreelocal/strings.json`
- `custom_components/lumentreelocal/translations/en.json`
