# 2026-05-20 Read + Write Token Claim Flow Spec

## Status

This is the chosen target design for the next authorization architecture.

It replaces the idea that Home Assistant read access can remain open by
`Device ID` alone.

It also replaces the current portal requirement that the end user manually
enters production `API token`, `Device ID`, and `gateway_id`.

As of 2026-05-20, this is a design spec only. Current production behavior is
still the older flow until implementation starts.

## Product Goal

The desired product behavior is:

1. User flashes ESP32 from the public flash site.
2. First-time portal only asks for Wi-Fi credentials.
3. After Wi-Fi is connected, AP mode stops.
4. ESP32 keeps serving a local portal on LAN.
5. That local portal shows:
   - a required `read pairing token`
   - an optional `write pairing token`
6. User adds the `Lumentree Local` integration in Home Assistant.
7. User enters:
   - `Device ID`
   - `read pairing token`
   - optional `write pairing token`
8. If no valid read token is provided, the integration setup must fail.
9. The API must require a valid read grant for normal read endpoints.
10. Write must remain separately scoped even if claimed during the same setup.

## Core Decision

### Read and write must not share one long-lived grant

Do not use one permanent token for both read and write.

Keep separate long-lived grants:

- `read_grant_token`
- `write_grant_token`

Reason:

- read and write have different risk levels,
- write must stay revocable and stricter,
- future policy can relax read without weakening write,
- a leaked read grant must never become a write grant.

### Read and write may be claimed in the same overall setup

The user experience may expose both tokens on the same ESP32 local portal, but
they should still map to different server-side grants and scopes.

## Important Clarification About Device Identity

This design treats BLE MAC as the hard binding key.

The working decision is:

- user-facing flow uses `Device ID`,
- server-side ownership binding locks to BLE MAC,
- `Device ID` stays the product-facing selector,
- `MAC` is the internal identity anchor used for the actual bind.

Current practical assumption:

- the inverter advertises a stable BLE MAC,
- ESP32 can read that MAC directly during BLE scan,
- future vendor behavior may be revisited later if evidence changes,
- but for now we intentionally design around stable MAC.

So the intended first-time bind is:

- user chooses the inverter by `Device ID`,
- ESP32 candidate discovery resolves that inverter's BLE MAC,
- server records and locks:
  - `gateway_id`
  - `device_id`
  - `mac`
- later read/write grants are validated against that locked relation.

## Chosen User Flow

### Phase 1: Flash and Wi-Fi Provisioning

1. User flashes ESP32 from `https://flash-lumentree.jonah.io.vn/`.
2. ESP32 boots into AP mode.
3. User opens `http://192.168.4.1`.
4. Portal shows only Wi-Fi onboarding fields:
   - `SSID`
   - `Password`
5. User saves Wi-Fi credentials.
6. ESP32 connects to Wi-Fi.
7. AP mode shuts down.
8. ESP32 continues serving the same portal on LAN IP or mDNS hostname.

### Phase 2: ESP32 Local Discovery

After ESP32 has network access:

1. ESP32 scans BLE nearby.
2. ESP32 builds a local candidate list.
3. Each candidate should contain enough evidence to link to an inverter, for
   example:
   - `device_id`
   - `mac`
   - `address_type`
   - observed service/UUID match
   - name if present
   - RSSI
   - discovery timestamps
4. ESP32 uploads candidate evidence to the server.
5. The local portal shows the device candidates and the ESP32's current status.

This phase is discovery only. It does not itself grant HASS read access.

### Phase 3: Token Generation

On the LAN portal, ESP32 exposes:

- `Generate read pairing token`
- `Generate write pairing token`

Rules:

- read token is required for HASS setup,
- write token is optional,
- both tokens are short-lived,
- both tokens are one-time use,
- both tokens are scoped to:
  - `gateway_id`
  - chosen `device_id`
  - bound `mac`
- plaintext token is shown only on the ESP32 portal,
- server stores only hashes.

### Phase 4: Home Assistant Setup

1. User adds `Lumentree Local`.
2. Integration asks for:
   - `Device ID`
   - `read pairing token`
   - optional `write pairing token`
3. Integration sends claim request to the API.
4. API validates:
   - the read token exists,
   - it is not expired,
   - it is not already consumed,
   - it belongs to a gateway currently online,
   - that gateway has discovered or is associated with the requested
     `Device ID`,
   - that the matching candidate has a concrete BLE MAC,
   - that the `gateway_id <-> device_id <-> mac` relation can be locked.
5. API issues a `read_grant_token`.
6. If a valid write token is also supplied, API separately issues a
   `write_grant_token`.
7. Integration stores the grant tokens and uses them for future requests.

If the user does not provide a valid read token:

- setup must fail,
- no config entry should be created,
- there is no anonymous read mode in this design.

## API Security Outcome

If this spec is fully implemented, then the API is protected correctly for
multi-user read access.

Required result:

- knowing `Device ID` alone is not enough to read,
- knowing `Device ID` alone is not enough to write,
- read API requires `read_grant_token`,
- write API requires `write_grant_token`,
- pairing tokens are only temporary proof for claiming grants.

So the answer to "is the API still not secure?" is:

- it is secure enough only if read endpoints actually stop being public,
- if public read endpoints remain open, then this design is not complete.

## Portal Design Rules

### AP mode portal

AP mode should be limited to onboarding Wi-Fi only.

Do not ask the end user for:

- production `API token`
- `Device ID`
- `gateway_id`
- inverter MAC

at AP onboarding time.

### LAN portal

After Wi-Fi is connected, the portal should be reachable on LAN and should show:

- Wi-Fi connected state
- firmware version
- `gateway_id`
- discovered inverter candidates
- token generation buttons
- token expiry status
- current pairing/grant status if known

## ESP32 Responsibilities

The firmware should move from "manually configured backend identity" to
"self-managed gateway identity".

Required behavior:

- keep a stable `gateway_id`,
- connect to Wi-Fi after AP onboarding,
- serve portal on LAN after AP mode ends,
- scan BLE for inverter candidates,
- derive or observe `Device ID` from BLE-visible evidence,
- capture BLE MAC and address type from scan results,
- publish candidate list to the server,
- generate short-lived one-time read pairing tokens,
- generate short-lived one-time write pairing tokens,
- upload telemetry with a gateway credential managed by the firmware/system,
- never require the end user to type the production API token into the portal.

## Server Responsibilities

The server becomes the issuer of scoped HASS grants, not the place where HASS
uses a master token.

Required server responsibilities:

- accept ESP32 candidate reports,
- store candidate evidence by gateway,
- store one-time read token hashes,
- store one-time write token hashes,
- validate claim requests from HASS,
- issue:
  - `read_grant_token`
  - `write_grant_token`
- revoke grants,
- require read grant on normal read endpoints,
- require write grant on write endpoints,
- keep audit logs for token creation, claim success, claim failure, expiry,
  consumption, and revocation.

## Home Assistant Responsibilities

Config flow should become:

1. enter `Device ID`
2. enter required `read pairing token`
3. optionally enter `write pairing token`
4. claim grants
5. store returned grants
6. finish setup only if read grant exists

After setup:

- `/latest`, `/energy`, and any other normal read path use `read_grant_token`,
- write commands use `write_grant_token`,
- options flow may support later:
  - rotate read access
  - claim write later
  - revoke write later

## Candidate Matching Model

Candidate discovery should use `Device ID` for user-facing selection, but the
final binding should lock to BLE MAC.

Recommended candidate record:

- `gateway_id`
- `device_id`
- `mac`
- `address_type`
- `service_uuid_match`
- `characteristic_uuid_match`
- `name`
- `rssi`
- `first_seen_at`
- `last_seen_at`
- `pairing_status`
- raw evidence payload

Recommended ownership rule:

- a gateway may observe multiple candidates,
- user chooses the intended inverter by entering `Device ID` in HASS,
- server verifies that the gateway has recently reported that `Device ID`,
- server resolves the matching candidate MAC,
- server locks:
  - `gateway_id`
  - `device_id`
  - `mac`
- then token claim is allowed for that exact bound inverter.

This means the user does not need to select by MAC, but the system still uses
MAC as the hard internal binding key.

## Required Data Model

Recommended new tables:

- `lumentree_gateway_candidates`
- `lumentree_read_pairing_tokens`
- `lumentree_read_grants`
- `lumentree_write_pairing_tokens`
- `lumentree_write_grants`
- `lumentree_auth_audit`

Recommended `lumentree_gateway_candidates` fields:

- `id`
- `gateway_id`
- `device_id`
- `mac`
- `address_type`
- `name`
- `service_uuid`
- `characteristic_uuid`
- `rssi`
- `payload`
- `first_seen_at`
- `last_seen_at`
- `active`

Recommended `lumentree_read_pairing_tokens` fields:

- `id`
- `gateway_id`
- `device_id`
- `mac`
- `token_hash`
- `expires_at`
- `consumed_at`
- `created_at`
- `firmware`
- `payload`

Recommended `lumentree_read_grants` fields:

- `id`
- `gateway_id`
- `device_id`
- `mac`
- `grant_token_hash`
- `enabled`
- `created_at`
- `revoked_at`
- `last_used_at`

Recommended `lumentree_write_pairing_tokens` fields:

- `id`
- `gateway_id`
- `device_id`
- `mac`
- `token_hash`
- `expires_at`
- `consumed_at`
- `created_at`
- `firmware`
- `payload`

Recommended `lumentree_write_grants` fields:

- `id`
- `gateway_id`
- `device_id`
- `mac`
- `grant_token_hash`
- `scope`
- `enabled`
- `created_at`
- `revoked_at`
- `last_used_at`

## Required API Shape

Recommended API endpoints:

- `POST /api/lumentree/gateways/{gateway_id}/candidates`
  - called by ESP32
  - uploads current candidate list
- `POST /api/lumentree/gateways/{gateway_id}/read-pairing-token`
  - called by ESP32
  - registers a one-time read pairing token
- `POST /api/lumentree/gateways/{gateway_id}/write-pairing-token`
  - called by ESP32
  - registers a one-time write pairing token
- `POST /api/lumentree/devices/{device_id}/grants/claim`
  - called by HASS
  - request body:
    - `read_pairing_token` required
    - `write_pairing_token` optional
  - response:
    - `read_grant_token` required on success
    - `write_grant_token` optional on success
- `POST /api/lumentree/devices/{device_id}/read-grants/revoke`
  - revoke read grant
- `POST /api/lumentree/devices/{device_id}/write-grants/revoke`
  - revoke write grant

Read endpoints in the new model:

- `GET /api/lumentree/devices/{device_id}/latest`
  - requires `Authorization: Bearer <read_grant_token>`
- `GET /api/lumentree/devices/{device_id}/energy`
  - requires `Authorization: Bearer <read_grant_token>`
- `GET /api/lumentree/devices/{device_id}/settings`
  - requires `Authorization: Bearer <read_grant_token>`
- `GET /api/lumentree/devices/{device_id}/health`
  - requires `Authorization: Bearer <read_grant_token>`

Write endpoints in the new model:

- `POST /api/lumentree/commands`
  - `mode=write` requires `Authorization: Bearer <write_grant_token>`
- `POST /api/lumentree/commands`
  - `mode=dry_run` may use read or write grant depending on later policy, but
    default recommendation is to require write grant for command creation

## Token Rules

### Pairing tokens

For both read and write pairing tokens:

- generated on ESP32,
- short-lived,
- one-time,
- scoped to `gateway_id`, `device_id`, and bound `mac`,
- displayed in plaintext only locally on the ESP32 portal,
- stored hashed on the server,
- invalid after claim or expiry.

### Grant tokens

For both read and write grant tokens:

- generated by the server,
- random and long-lived enough for HASS use,
- stored hashed on the server,
- revocable,
- tracked with `last_used_at`.

## Recovery and Rebinding

This design needs an explicit recovery story.

Questions to solve during implementation:

- if the user replaces the ESP32, how is the old gateway detached?
- if the same user has multiple inverters nearby, how is the candidate list
  shown clearly?
- if the user loses read access but still has physical portal access, how do
  they re-claim read without DB surgery?
- should admin be able to revoke stale gateway/device bindings manually?

The recommended answer is:

- recovery should happen through fresh token claim and explicit revoke,
- not by manually deleting `Device ID` rows from Postgres in normal operation.

## Migration From Current System

Current production assumptions that must change:

- AP portal asks for too many backend secrets,
- HASS can read by `Device ID` alone,
- read and diagnostic endpoints are too public,
- ESP32 identity is too manually configured.

Migration target:

1. simplify AP onboarding to Wi-Fi only,
2. add LAN portal mode,
3. add candidate upload with BLE MAC capture,
4. add read pairing token flow,
5. keep existing write pairing concept but align naming and storage,
6. move HASS to grant-based read,
7. close public read endpoints.

## Final Decision Summary

Chosen architecture:

- AP mode: Wi-Fi onboarding only
- LAN portal: token generation and local device status
- `Device ID`: user-facing inverter selector
- `MAC`: hard internal binding key
- read token: required
- write token: optional
- read grant: mandatory for API reads
- write grant: mandatory for API writes
- no public read by `Device ID` alone
- no shared long-lived read+write token
