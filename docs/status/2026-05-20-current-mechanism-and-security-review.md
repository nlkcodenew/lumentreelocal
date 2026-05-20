# 2026-05-20 Current Mechanism And Security Review

## Scope

This note reviews the current implemented behavior and current security posture
of the private runtime stack in `/home/mrlinh/esp32-lumentree`.

It is intentionally about the system that exists today.

It does not switch production to the deferred multi-user design recorded in:

- `docs/specs/2026-05-20-multi-user-read-claim-spec.md`

## Review Method

This review used:

- code inspection of firmware, local server, integration, and flash site,
- current public endpoint probes against `https://lumentree.jonah.io.vn`,
- current public flash-site probe against `https://flash-lumentree.jonah.io.vn/`.

Live probe time:

- `2026-05-20 17:22 +0700`

## Executive Summary

Current architecture is coherent and already split into two security levels:

- `read` is intentionally easy and currently mostly public by `Device ID`,
- `write` is intentionally protected by ESP32-generated one-time pairing code
  and scoped write grant tokens.

That means the real current posture is:

- the system is acceptable for single-owner or trusted-user deployments,
- it is not suitable as-is for true multi-user privacy,
- the write path is materially better protected than the read path,
- some read-adjacent endpoints reveal more than just simple telemetry.

The most important current fact is not theoretical:

- public `GET /api/lumentree/devices/P240819130/latest` returned `200`,
- public `GET /api/lumentree/devices/P240819130/energy` returned `200`,
- public `GET /api/lumentree/devices/P240819130/health` returned `200`,
- public `GET /api/lumentree/devices/P240819130/settings` returned `200`,
- public `GET /api/lumentree/devices/P240819130/write-grants/status` returned
  `200`,
- public `GET /api/lumentree/devices` returned `401`.

So current runtime truth is:

- device enumeration is protected,
- direct read by known `Device ID` is not protected,
- write command creation is protected,
- write-grant status metadata is publicly queryable by `Device ID`.

## Current Mechanism

### 1. Flash And Provisioning

Current intended first-use flow:

1. User flashes firmware from `https://flash-lumentree.jonah.io.vn/`.
2. ESP32 reboots and opens AP `Lumentree-XXXX`.
3. User opens `http://192.168.4.1`.
4. User enters Wi-Fi, API URL, API token, `Device ID`, optional target MAC,
   and `gateway_id`.
5. ESP32 stores those values locally and reboots.

Evidence:

- flash-site instructions: `host/flash-site/public/index.html`
- portal save path: `firmware/src/main.cpp`
- local config persistence: `firmware/src/main.cpp`

Important implementation detail:

- the flash site explicitly tells the user to enter production parameters in the
  portal after flashing.
- the firmware portal currently accepts and stores `api_token`, `device_id`,
  `gateway_id`, Wi-Fi credentials, and target MAC.

## 2. Firmware Runtime Behavior

After provisioning, the ESP32 firmware does the following:

- reads inverter data over BLE,
- uploads telemetry to `/api/lumentree/events`,
- periodically uploads gateway status to `/api/lumentree/gateways/status`,
- polls the server for pending commands,
- can generate a short-lived write pairing code and register it with the API.

Current relevant implementation facts:

- config is loaded from `Preferences`,
- telemetry upload uses `Authorization: Bearer <api_token>`,
- gateway status upload uses the same bearer token,
- write pairing code registration uses the same bearer token,
- TLS certificate verification is currently optional and defaults to insecure.

That last point matters:

- `LUMENTREE_DEFAULT_TLS_INSECURE` defaults to `1`,
- when the endpoint is `https://`, the firmware calls `secureClient.setInsecure()`
  if `tlsInsecure` is enabled.

So current firmware trust model is:

- transport may be HTTPS,
- but certificate verification can be disabled by default,
- which weakens the protection of the ESP32 -> server token path.

## 3. Server Runtime Behavior

Current local server roles:

- accept telemetry events,
- accept gateway status,
- store everything in Postgres,
- expose current read APIs for Home Assistant,
- expose write grant and write command APIs.

Current auth model in code:

- server token authenticates ESP32 uploads and privileged API usage,
- scoped write grant token authenticates write-capable HASS actions,
- most read APIs do not require any token,
- some read-adjacent diagnostics do require a token.

## 4. Home Assistant Integration Behavior

Current HASS setup flow is simple:

1. user enters `Device ID`,
2. integration calls `/health`,
3. integration tries `/latest`,
4. if not found, integration tries `/health` for that device,
5. config entry is created without any long-lived general API token.

Write flow is separate:

1. user opens ESP32 portal,
2. user generates one-time write pairing code,
3. user enters it in integration options,
4. integration claims a scoped write grant,
5. integration stores only the returned write grant token.

This is a meaningful security improvement over the older pattern where HASS
would carry the full production API token.

## Current Endpoint/Auth Matrix

Based on code review and live probes:

- `GET /health`
  - public
  - live result: `200`
- `GET /api/lumentree/devices`
  - server token required
  - live result without token: `401`
- `GET /api/lumentree/devices/{device_id}/latest`
  - currently public
  - live result without token: `200`
- `GET /api/lumentree/devices/{device_id}/energy`
  - currently public
  - live result without token: `200`
- `GET /api/lumentree/devices/{device_id}/health`
  - currently public
  - live result without token: `200`
- `GET /api/lumentree/devices/{device_id}/settings`
  - currently public
  - live result without token: `200`
- `GET /api/lumentree/devices/{device_id}/write-grants/status`
  - currently public
  - live result without token: `200`
- `GET /api/lumentree/devices/{device_id}/commands/status`
  - server token or valid write grant required
  - live result without token: `401`
- `GET /api/lumentree/events`
  - server token required
- `GET /api/lumentree/devices/{device_id}/commands`
  - server token required
- `GET /api/lumentree/gateways/{gateway_id}/commands/next`
  - server token required
- `POST /api/lumentree/events`
  - server token required
- `POST /api/lumentree/gateways/status`
  - server token required
- `POST /api/lumentree/gateways/{gateway_id}/write-pairing-code`
  - server token required
- `POST /api/lumentree/devices/{device_id}/write-grants/claim`
  - public claim endpoint, but requires valid one-time ESP32-generated code
- `POST /api/lumentree/devices/{device_id}/write-grants/revoke`
  - valid write grant required
- `POST /api/lumentree/commands`
  - server token or valid write grant required
- `POST /api/lumentree/commands/{id}/result`
  - server token required

## What Is Protected Well Enough Today

### 1. Production upload path is not open

ESP32 telemetry upload, gateway status upload, and command result reporting all
need the server bearer token.

That is the right split for the current architecture.

### 2. HASS does not need the full production token for write control

The integration stores a scoped write grant instead of the main server token
during normal use.

That reduces blast radius if a Home Assistant instance is compromised.

### 3. Write creation is scoped and guarded

Current write protection has several good properties:

- one-time write pairing codes,
- code stored hashed on the server,
- grant token stored hashed on the server,
- command creation requires a valid grant or server token,
- write commands are allowlisted semantic commands only,
- payload validation exists for supported write commands,
- audit table exists for write authorization events.

### 4. Device enumeration is not fully open

`GET /api/lumentree/devices` currently returns `401` without the server token,
so casual anonymous listing of all known devices is blocked.

## Current Security Weaknesses

### 1. Read privacy is weak by design

This is the biggest current gap.

If someone already knows a valid `Device ID`, they can currently read:

- latest live telemetry,
- energy aggregates,
- device health,
- settings snapshots,
- write-grant availability metadata.

This is not a future or theoretical concern. It was confirmed live on
`2026-05-20`.

Implication:

- the current system protects against public device enumeration,
- but not against public read access once a `Device ID` is known.

### 2. Settings exposure is broader than simple telemetry exposure

Public `settings` currently reveals operational inverter configuration such as:

- discharge slot enable flags,
- charge/discharge time windows,
- target SOC values,
- mapped register numbers.

That is materially more sensitive than simple power telemetry.

If the intended current policy is "easy read-only metrics", `settings` is more
open than that policy suggests.

### 3. Write-grant status endpoint leaks operational metadata

Public `write-grants/status` currently discloses values such as:

- `gateway_id`,
- whether the gateway appears online,
- whether write is available,
- active grant count,
- gateway heartbeat timestamp.

This is not direct write access, but it does leak useful operational metadata
for anyone who knows a valid `Device ID`.

### 4. Firmware TLS is weak by default

Current firmware defaults to insecure TLS behavior:

- `LUMENTREE_DEFAULT_TLS_INSECURE = 1`
- `secureClient.setInsecure()` is used when that flag is enabled.

Implication:

- the ESP32 may accept a TLS endpoint without certificate verification,
- the production API token sent by the ESP32 is less protected than it should
  be against active network interception.

This is a bigger issue than the HASS write grant design because it affects the
main server token on the device side.

### 5. Portal configuration currently includes the server token

The provisioning portal explicitly asks for:

- API URL,
- API token,
- `Device ID`,
- `gateway_id`.

Those values are then stored in `Preferences`.

This is operationally convenient, but it means the ESP32 remains a holder of the
main server bearer token. That is acceptable in the current architecture, but it
should be treated as a sensitive credential path.

### 6. Some docs/specs drift from actual implementation

Current spec text still suggests certain read endpoints are public by design,
while the actual runtime split is more nuanced:

- `/api/lumentree/devices` is no longer public in runtime,
- but `/latest`, `/energy`, `/health`, `/settings`, and
  `/write-grants/status` are public.

This should be kept in mind during future auth refactors so decisions are made
from runtime truth, not stale assumptions.

## Practical Risk Rating

For current single-owner use on your own server:

- write risk: moderate and reasonably controlled,
- read privacy risk: moderate but probably acceptable if you treat `Device ID`
  as private-ish and do not share the system broadly,
- firmware transport risk: the most important technical weakness because of
  insecure TLS default.

For future shared-server multi-user use:

- current read model: not acceptable,
- current write model: directionally acceptable,
- current firmware TLS default: should be tightened before calling it
  production-grade for other users.

## What Should Stay Unchanged Right Now

Per current decision, do not start the multi-user read-claim redesign yet.

So this review does not recommend immediate auth churn on the read path today.

Current explicit hold:

- keep the current simple read flow,
- keep the current write-grant flow,
- keep the multi-user redesign only as deferred spec.

## Highest-Value Future Hardening, In Order

When you decide to improve security later, the highest-value order is:

1. tighten firmware TLS verification first,
2. decide whether `settings` and `write-grants/status` should remain public,
3. only then move read APIs to a real read-grant or claim model for multi-user.

Reason:

- TLS tightening protects the server token that already exists today,
- reducing public `settings` exposure gives an immediate privacy win,
- full multi-user read authorization is a larger product flow change and should
  be done deliberately, not piecemeal.

## Bottom Line

Current system summary:

- simple and workable for your own deployment,
- write path already has meaningful scoped authorization,
- read path is intentionally convenient but publicly readable by known
  `Device ID`,
- firmware-side TLS is currently weaker than ideal and is the most important
  pure security weakness in the live implementation today.
