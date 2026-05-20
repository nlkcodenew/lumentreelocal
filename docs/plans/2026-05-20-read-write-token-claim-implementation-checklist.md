# 2026-05-20 Read + Write Token Claim Implementation Checklist

## Goal

Implement the authorization architecture from:

- `docs/specs/2026-05-20-read-write-token-claim-flow-spec.md`

This checklist is ordered for safe execution, explicit rollback, and minimal
chance of getting stuck between incompatible firmware/server/HASS states.

## Hard Rule

Do not start code changes until the backup and rollback steps below are
completed.

## Phase 0: Backup And Rollback First

### 0.1 Create a full pre-change snapshot

- Back up the current private runtime repo state before touching code.
- Record current branch, HEAD commit, and worktree state.
- Save a copy of all runtime docs/specs related to the old flow.
- Save a copy of current deployed service/environment files if they are part of
  the rollout path.

Minimum artifacts to preserve:

- current repo HEAD SHA
- `git status --short`
- `SESSION_HANDOFF.md`
- current spec/docs for:
  - server auth
  - write grant flow
  - current security review
- current firmware config assumptions
- current flash-site guidance

Recommended commands:

```bash
cd /home/mrlinh/esp32-lumentree
git status --short
git rev-parse HEAD
git branch --show-current
git diff > /tmp/lumentree-pre-auth-change.diff
git ls-files -m -o --exclude-standard > /tmp/lumentree-pre-auth-change-files.txt
```

### 0.2 Commit the current known-good state before implementation

- Make a dedicated checkpoint commit in the private runtime repo before any auth
  migration work.
- This commit must represent the last known-good old architecture.
- Push that checkpoint to `private` before proceeding.

Commit intent:

- "pre read/write token claim migration checkpoint"

### 0.3 Back up live runtime surfaces needed for rollback

Before changing runtime behavior, preserve current live truth:

- current systemd service unit for local server
- current local server env file
- current Cloudflare tunnel config if routing may change
- current Home Assistant installed integration version
- current firmware version currently running on the real ESP32

Recommended paths to capture if they are part of the rollout:

- `/etc/systemd/system/lumentree-local-server.service`
- `/etc/lumentree/local-server.env`
- `/etc/cloudflared/config.yml`

### 0.4 Define rollback triggers before doing any migration

Rollback immediately if any of these happen:

- ESP32 can no longer upload telemetry
- HASS loses all read entities for the active production device
- claim flow works only for write but breaks read
- AP onboarding becomes unusable
- LAN portal cannot be reached after Wi-Fi onboarding
- existing device becomes orphaned without a clean manual recovery path

### 0.5 Define rollback actions before doing any migration

Rollback path must be explicit:

- revert to the checkpoint commit
- redeploy previous local server code
- restore previous service/env/config backups
- reflash previous firmware if firmware behavior changed incompatibly
- restore previous HASS integration build if necessary

## Phase 1: Freeze Old-Flow Truth

- Confirm exactly which current endpoints are public and which require auth.
- Confirm current firmware portal fields and runtime commands.
- Confirm current production token path used by ESP32.
- Confirm the active device binding now:
  - `device_id`
  - `mac`
  - `gateway_id`
  - `address_type` if available

Outputs:

- current endpoint/auth matrix
- current firmware portal behavior notes
- confirmed MAC stability note for the active inverter

## Phase 2: Finalize Migration Contract

- Treat `MAC` as hard internal binding key.
- Treat `Device ID` as user-facing selector.
- Keep AP mode limited to Wi-Fi onboarding only.
- Require LAN portal after Wi-Fi.
- Require `read pairing token` for HASS setup.
- Keep `write pairing token` optional.
- Keep `read_grant` and `write_grant` separate.

Before implementation, explicitly freeze:

- token TTLs
- one-time token format rules
- LAN portal reachability plan
- recovery story for replacing ESP32
- policy for dry-run command auth

## Phase 3: Data Model And DB Migrations

Add new tables and avoid overloading old semantics blindly.

Required schema work:

- `lumentree_gateway_candidates`
- `lumentree_read_pairing_tokens`
- `lumentree_read_grants`
- update or replace write-pairing tables only if naming consistency is needed
- add or extend auth audit table

Required binding fields in new auth tables:

- `gateway_id`
- `device_id`
- `mac`

Migration safety:

- do not drop old tables in the first rollout
- do not remove old write-grant support until the new path is proven
- make migrations additive first

## Phase 4: Server Implementation

### 4.1 Candidate intake

- add endpoint for ESP32 candidate uploads
- store candidate rows with:
  - `device_id`
  - `mac`
  - `address_type`
  - BLE evidence
- add lookup logic by `gateway_id + device_id`

### 4.2 Read pairing token intake

- add ESP32 endpoint to register one-time read pairing token
- hash token server-side
- store token scoped to:
  - `gateway_id`
  - `device_id`
  - `mac`

### 4.3 Unified HASS claim endpoint

- accept `device_id`
- require `read_pairing_token`
- optionally accept `write_pairing_token`
- validate candidate existence and MAC binding
- lock:
  - `gateway_id`
  - `device_id`
  - `mac`
- issue:
  - `read_grant_token`
  - optional `write_grant_token`

### 4.4 Read endpoint protection

Move normal read endpoints behind `read_grant_token`:

- `/latest`
- `/energy`
- `/settings`
- `/health`

Do not leave mixed public/private read semantics once migration is complete.

### 4.5 Write endpoint alignment

- keep existing semantic write validation
- align write pairing naming/storage with the new model if needed
- require `write_grant_token` for write command creation

### 4.6 Audit and revoke

- add claim success/failure audit events
- add revoke endpoints for read and write grants
- add `last_used_at` tracking

## Phase 5: Firmware Implementation

### 5.1 AP onboarding simplification

- remove portal fields for:
  - production API token
  - `Device ID`
  - `gateway_id` if it can be self-managed
  - target MAC manual entry in the normal first-use path
- keep only Wi-Fi onboarding in AP mode

### 5.2 LAN portal support

- keep portal alive on LAN after Wi-Fi onboarding
- expose stable access method:
  - LAN IP and/or mDNS
- verify this works from a normal browser on the same network

### 5.3 Candidate discovery

- scan BLE and collect:
  - `device_id`
  - `mac`
  - `address_type`
  - UUID evidence
  - RSSI
- upload candidate list to server

### 5.4 Read token generation

- add `Generate read pairing token`
- token must be one-time and short-lived
- register it with the server
- show plaintext token only locally on portal

### 5.5 Write token generation

- adapt current write code flow to the new naming/claim model as needed
- keep write token separate from read token

### 5.6 Runtime auth changes

- stop depending on user-entered production API token in normal onboarding
- move to a firmware-managed gateway credential path
- confirm telemetry upload still works before touching HASS

## Phase 6: HASS Integration Implementation

### 6.1 Config flow

Replace first setup form with:

- `Device ID`
- required `read pairing token`
- optional `write pairing token`

Behavior:

- fail setup if no valid read token
- store returned `read_grant_token`
- store returned `write_grant_token` only if claimed

### 6.2 API client

- send `read_grant_token` for normal read requests
- send `write_grant_token` for write requests
- treat expired/revoked grants clearly in UI

### 6.3 Entity behavior

- normal read entities depend on `read_grant`
- write entities stay disabled/unavailable without `write_grant`
- diagnostics should clearly show auth state

## Phase 7: Rollout Order

Do not deploy all layers blindly at once.

Recommended order:

1. additive DB migration
2. server support for new schema and new endpoints
3. firmware candidate upload + LAN portal + token generation
4. manual claim test outside HASS
5. HASS config-flow migration
6. only then close public read endpoints

Critical rule:

- public read endpoints should be closed only after the new `read_grant` path is
  verified end-to-end.

## Phase 8: Verification Checklist

### Firmware

- AP mode only asks for Wi-Fi
- Wi-Fi onboarding succeeds
- LAN portal reachable after Wi-Fi
- BLE candidate list shows `device_id`, `mac`, `address_type`
- read token generation works
- write token generation works
- telemetry upload still works

### Server

- candidate rows persist correctly
- claim endpoint locks `gateway_id <-> device_id <-> mac`
- read grant issuance works
- optional write grant issuance works
- revoke works
- audit rows are written

### HASS

- setup fails without read token
- setup succeeds with valid read token
- setup can optionally claim write at the same time
- read entities work with `read_grant`
- write entities only work with `write_grant`

### Security

- anonymous `/latest` returns unauthorized
- anonymous `/energy` returns unauthorized
- anonymous `/settings` returns unauthorized
- anonymous `/health` returns unauthorized
- known `Device ID` alone is no longer enough to read

## Phase 9: Recovery And Supportability

Before calling rollout complete, document:

- how to replace an ESP32
- how to re-claim read access
- how to re-claim write access
- how to revoke stale bindings
- how to inspect candidate rows and grant rows in Postgres
- how to roll back to the checkpoint commit

## Final Release Gate

Do not declare this migration complete until all of these are true:

- checkpoint commit and backup artifacts exist
- rollback procedure has been tested or at least dry-run verified
- current production device has re-claimed read successfully
- current production device has optional write claim working
- public read endpoints are closed
- HASS reads are fully grant-based
- no user-entered production API token is needed in the normal onboarding flow
