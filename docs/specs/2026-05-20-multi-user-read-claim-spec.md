# 2026-05-20 Multi-User Read Claim Spec

## Status

This is a deferred design for future multi-user support.

Current production behavior remains unchanged:

- read access stays on the current simple `Device ID` flow,
- no new read-claim protection is implemented yet,
- existing write-grant flow remains the stronger protected path.

Do not partially implement this spec. If this work starts later, apply it as a
coherent server + firmware + Home Assistant change set.

## Why This Exists

The current read model is acceptable for a single-owner deployment, but it is
too open for a shared server where different users may connect their own ESP32
gateways and inverters.

Main distinction:

- knowing a `Device ID` is not enough to pair to the correct inverter,
- but if read APIs stay open by `Device ID`, it may still be enough to read
  another user's telemetry.

Therefore, future multi-user support should protect read access at the API
layer, not only the write path.

## Current Decision

Keep the current read behavior for now.

Do not implement multi-user read protection yet.

Record the intended direction now so the next implementation session does not
restart the architecture discussion.

## Future Direction

The future multi-user design should add a lightweight first-time gateway claim,
then issue a scoped read grant after successful pairing.

Recommended target flow:

1. User flashes ESP32 from the flash site.
2. ESP32 boots, connects to Wi-Fi, and registers `gateway_id` on the server as
   `unclaimed`.
3. ESP32 exposes a short-lived one-time `gateway_claim_code`.
4. User opens Home Assistant and adds the `lumentreelocal` integration.
5. During first setup, HASS enters:
   - `Device ID`
   - `gateway_claim_code`
6. Integration calls `claim/init`.
7. Server binds the setup session to the claimed `gateway_id`.
8. Server tells only that gateway to scan for the requested inverter.
9. If the scan finds one valid match, pair automatically.
10. If the scan is ambiguous or returns multiple candidates, HASS shows the
    list and the user selects one.
11. Server finalizes the relation `gateway_id <-> device_id`.
12. Server issues a long-lived `read_grant_token` for that HASS config entry.
13. HASS uses the `read_grant_token` for read APIs.
14. Write access remains a separate stronger pairing flow using the existing
    write pairing code model.

## Why Gateway Claim Is Needed

In a real multi-user server, `Device ID` alone is not enough routing
information.

If more than one ESP32 gateway is online, the server must know exactly which
gateway is allowed to perform the first scan. Otherwise the server would need to
guess, which is unsafe and can bind the wrong hardware.

The claim step solves this:

- it proves the user has access to the correct ESP32,
- it lets the server route scan/pair work to exactly one gateway,
- it keeps the first-time read flow much lighter than the write flow.

## Security Model

### Read

Future read protection should require:

- a valid `read_grant_token`,
- a scope tied to one `device_id`,
- server-side revocation support,
- no open read API by `Device ID` alone in multi-user mode.

### Write

Write remains separate and stronger:

- do not reuse the read grant as a write credential,
- keep the current ESP32-generated write pairing code,
- keep write grants device-scoped and revocable,
- keep semantic write validation and audit requirements unchanged.

## UX Goals

The intended multi-user UX should still be simple:

- first setup asks for `Device ID` and `gateway_claim_code`,
- read then works normally after the first successful claim/pair,
- write remains optional and separately authorized.

This is intentionally not a portal-heavy design.

## Non-Goals

- Do not derive any read token from `Device ID`.
- Do not make write depend on read-token possession alone.
- Do not let the server broadcast first-time scan commands to arbitrary online
  gateways.
- Do not add MQTT for this feature.
- Do not implement a partial hybrid where some read endpoints are public and
  some require a grant without a clear migration plan.

## Required Data Model

If this work starts later, add dedicated read-authorization tables rather than
overloading the write-grant tables.

Recommended tables:

- `lumentree_gateway_claim_codes`
- `lumentree_gateway_claims`
- `lumentree_read_claim_sessions`
- `lumentree_read_grants`
- optional `lumentree_read_audit`

Recommended `lumentree_gateway_claim_codes` fields:

- `id`
- `gateway_id`
- `code_hash`
- `expires_at`
- `consumed_at`
- `created_at`
- `firmware`
- `payload`

Recommended `lumentree_gateway_claims` fields:

- `id`
- `gateway_id`
- `claimed_by`
- `claimed_at`
- `revoked_at`
- `status`

Recommended `lumentree_read_claim_sessions` fields:

- `id`
- `device_id`
- `gateway_id`
- `status`
- `requested_at`
- `scan_started_at`
- `scan_completed_at`
- `selected_candidate`
- `resolved_device_id`
- `error`
- `payload`

Recommended `lumentree_read_grants` fields:

- `id`
- `gateway_id`
- `device_id`
- `grant_token_hash`
- `enabled`
- `created_at`
- `revoked_at`
- `last_used_at`

## Required API Shape

Suggested future API endpoints:

- `POST /api/lumentree/gateways/{gateway_id}/claim-code`
  - called by ESP32
  - creates or rotates a one-time gateway claim code
- `POST /api/lumentree/read-claims/init`
  - called by HASS
  - input: `device_id`, `gateway_claim_code`
  - output: claim session state
- `GET /api/lumentree/read-claims/{claim_id}`
  - called by HASS
  - polls scan/pair status
- `POST /api/lumentree/read-claims/{claim_id}/select-candidate`
  - called by HASS when more than one inverter candidate is returned
- `POST /api/lumentree/read-claims/{claim_id}/complete`
  - optional explicit finalization step if the server does not auto-complete
- `GET /api/lumentree/devices/{device_id}/latest`
  - future multi-user mode should require `Authorization: Bearer <read_grant>`
- `GET /api/lumentree/devices/{device_id}/energy`
  - future multi-user mode should require `Authorization: Bearer <read_grant>`

## Required Firmware Behavior

Future firmware work should add only the minimum read-claim support:

- register `gateway_id` as `unclaimed`,
- generate/display a one-time `gateway_claim_code`,
- accept a targeted scan request from the server,
- report scan candidates,
- finalize the chosen inverter binding,
- report online claim/pair status for diagnostics.

Do not couple this to new write behavior.

## Required HASS Behavior

Future HASS config flow should become a small first-time wizard:

1. enter `Device ID`
2. enter `gateway_claim_code`
3. wait for scan status
4. if needed, choose a candidate
5. store the returned `read_grant_token`

After first setup:

- polling uses the read grant automatically,
- options flow may expose grant status and revoke/reclaim actions later,
- write options stay separate.

## Migration Note

When this work begins, there must be a clear migration plan from the current
open read model to the grant-based model.

Questions to answer at implementation time:

- whether single-owner mode and multi-user mode both need to exist,
- whether existing HASS users should auto-migrate or re-claim,
- whether public read endpoints should be removed immediately or only after a
  compatibility window,
- how to detect and diagnose stale or revoked read grants cleanly.

## Recommended Implementation Order

1. Update specs and threat model first.
2. Add DB schema for gateway claim and read grant lifecycle.
3. Add server claim/session/grant endpoints.
4. Add firmware gateway-claim and targeted-scan support.
5. Add HASS config-flow wizard for first-time read claim.
6. Migrate read endpoints to grant-required mode.
7. Add revoke/recovery tooling and diagnostics.

## Explicit Deferral

As of 2026-05-20:

- current production read behavior stays as-is,
- no read grant is required yet,
- no gateway claim flow is implemented yet,
- this document is only the recorded future direction.
