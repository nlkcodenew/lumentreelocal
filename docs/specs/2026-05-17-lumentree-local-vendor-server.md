# Lumentree Local Vendor Server Spec

## Goal

Replace the Lumentree vendor-cloud role with a production API server running on
this Ubuntu laptop and exposed through Cloudflare Tunnel at
`https://lumentree.jonah.io.vn`.

ESP32-S3 BLE gateways may be on the same LAN or at other sites. They read
inverters over BLE and upload telemetry over Wi-Fi/Internet to the production
API hostname. Home Assistant reads the same production API through the
`lumentreelocal` custom integration.

MQTT is not required for this path.

## Topology

```text
Remote or local Lumentree inverter
  -> BLE
  -> ESP32-S3 dedicated Lumentree gateway
  -> Wi-Fi / Internet
  -> https://lumentree.jonah.io.vn
  -> Cloudflare Tunnel
  -> local server bound on this laptop
  -> Postgres
  -> Home Assistant lumentreelocal integration
```

Internal host binding:

```text
Cloudflare Tunnel ingress
  -> http://127.0.0.1:8787
```

`127.0.0.1:8787` is an implementation detail behind the tunnel, not the
integration URL for Home Assistant or remote ESP32 gateways.

## Non-Goals

- Do not add MQTT unless a later integration choice explicitly needs it.
- Do not put Home Assistant tokens on the ESP32.
- Do not implement inverter writes.
- Do not implement BLE Modbus write/notify behavior until the BLE protocol is
  proven and explicitly approved.
- Do not replace the current Home Assistant custom integration before the local
  API has stable decoded data.
- Do not use LAN-only addressing as the production API contract.

## Components

### ESP32-S3 BLE Gateway

Responsibilities:

- Connect to Wi-Fi after the BLE protocol is proven.
- Read Lumentree telemetry over BLE.
- POST telemetry to `https://lumentree.jonah.io.vn/api/lumentree/events`.
- Include a device id, BLE MAC, firmware version, uptime, raw payload evidence,
  and decoded metrics.
- Use a gateway token that can be rotated independently from Home Assistant.

The current dedicated firmware remains in evidence mode until a safe read path
is known.

### Local Lumentree Server

Responsibilities:

- Accept telemetry from ESP32 gateways through Cloudflare Tunnel.
- Store raw and decoded payloads in Postgres.
- Provide a stable HTTP API for Home Assistant.
- Provide health/debug endpoints for production verification.
- Require bearer-token auth for telemetry upload and debug event history.
- Allow Home Assistant read endpoints without a bearer token for simpler setup:
  `/api/lumentree/devices` and `/api/lumentree/devices/{device_id}/latest`.

### Postgres

Required tables:

- `lumentree_devices`: one row per inverter/gateway device.
- `lumentree_telemetry`: append-only decoded telemetry events.
- `lumentree_ble_events`: existing raw BLE event table from the serial collector.
- `lumentree_energy_daily`: incremental daily energy aggregates calculated from
  adjacent telemetry power samples.
- `lumentree_gateway_status`: latest ESP32 gateway pairing/diagnostic state,
  including target MAC, pairing status, Wi-Fi status, and BLE candidates.

### Home Assistant Custom Integration

Responsibilities:

- Poll `https://lumentree.jonah.io.vn`.
- Create/update sensor entities from `metrics`.
- Use domain `lumentreelocal` while testing side by side with the old
  `lumentree` integration.
- Treat production API unavailability as `unavailable`, not as a config failure.

## API Contract

### `GET /health`

Returns server and database health.

Response:

```json
{
  "ok": true,
  "database": "ok"
}
```

### `POST /api/lumentree/events`

Used by ESP32 gateways or the host collector to submit telemetry.

Headers:

```text
Authorization: Bearer <token>
Content-Type: application/json
```

Payload:

```json
{
  "device_id": "P240819130",
  "mac": "d8:13:2a:ee:58:d6",
  "gateway_id": "esp32-s3-lumentree-01",
  "observed_at": "2026-05-17T05:00:00Z",
  "firmware": "lumentree-ble-bridge/0.1.0",
  "uptime_ms": 123456,
  "raw": {
    "transport": "ble",
    "service_uuid": "0000ffe0-0000-1000-8000-00805f9b34fb",
    "characteristic_uuid": "0000ffe1-0000-1000-8000-00805f9b34fb",
    "payload_hex": "..."
  },
  "metrics": {
    "battery_soc": 82,
    "pv_power_w": 450,
    "load_power_w": 120,
    "grid_power_w": 0
  }
}
```

For production, `device_id`, `gateway_id`, and `mac` should be present. During
USB evidence capture, `device_id` or `mac` is enough. `metrics` may be empty
while BLE decoding is still in progress.

### `GET /api/lumentree/devices`

Used by Home Assistant config flow and diagnostics. This endpoint is readable
without a bearer token.

Response:

```json
{
  "devices": [
    {
      "device_id": "P240819130",
      "mac": "d8:13:2a:ee:58:d6",
      "gateway_id": "esp32-s3-lumentree-01",
      "first_seen_at": "2026-05-17T05:00:00Z",
      "last_seen_at": "2026-05-17T05:01:00Z"
    }
  ]
}
```

### `GET /api/lumentree/devices/{device_id}/latest`

Used by Home Assistant polling. This endpoint is readable without a bearer
token.

Response:

```json
{
  "device_id": "P240819130",
  "mac": "d8:13:2a:ee:58:d6",
  "observed_at": "2026-05-17T05:01:00Z",
  "gateway_id": "esp32-s3-lumentree-01",
  "firmware": "lumentree-ble-bridge/0.1.0",
  "metrics": {
    "battery_soc": 82,
    "pv_power_w": 450
  },
  "raw": {
    "transport": "ble",
    "payload_hex": "..."
  }
}
```

### `GET /api/lumentree/events?device_id=...&limit=100`

Debug endpoint for recent telemetry. This endpoint requires a bearer token.

### `GET /api/lumentree/devices/{device_id}/energy`

Used by Home Assistant energy sensors.

Response:

```json
{
  "device_id": "P240819130",
  "timezone": "Asia/Ho_Chi_Minh",
  "daily_reset_time": "00:00",
  "daily": {
    "pv_kwh": 1.23,
    "load_kwh": 2.34,
    "grid_in_kwh": 0.45,
    "grid_out_kwh": 0,
    "battery_charge_kwh": 0.5,
    "battery_discharge_kwh": 0.2,
    "ac_input_kwh": 0.45,
    "ac_output_kwh": 0,
    "sample_count": 120,
    "covered_seconds": 3600
  },
  "monthly": {},
  "yearly": {},
  "total": {}
}
```

Energy aggregation uses adjacent samples and clamps intervals larger than five
minutes so long telemetry gaps do not create false kWh totals.

Daily, monthly, and yearly periods use the local production timezone
`Asia/Ho_Chi_Minh` (GMT+7). Daily counters reset at `00:00` GMT+7. If one sample
interval crosses local midnight, the server splits that interval across the two
local days instead of assigning all energy to one side.

### `POST /api/lumentree/gateways/status`

ESP32 gateway diagnostic endpoint. This endpoint requires a bearer token.

Request:

```json
{
  "gateway_id": "esp32-lumentree",
  "device_id": "P240819130",
  "firmware": "lumentree-ble-bridge/0.4.0",
  "target_mac": "d8:13:2a:ee:58:d6",
  "pairing_status": "paired",
  "wifi_connected": true,
  "wifi_rssi": -58,
  "ip": "192.168.1.151",
  "uptime_ms": 8210,
  "candidates": []
}
```

### `GET /api/lumentree/gateways/{gateway_id}/health`

Returns latest gateway pairing and Wi-Fi diagnostics.

### `GET /api/lumentree/devices/{device_id}/health`

Returns telemetry health and, when available, embeds the latest matching gateway
status in `gateway_status`. If no telemetry exists yet, HASS can still use this
endpoint to show `waiting_for_gateway_pairing`, `scanning`, or
`multiple_candidates`.

### `POST /api/lumentree/gateways/{gateway_id}/write-pairing-code`

Gateway-only endpoint. Requires the production bearer token. Registers a
short-lived one-time write pairing code generated by ESP32. The server stores
only a scoped hash, not the plaintext code.

### `POST /api/lumentree/devices/{device_id}/write-grants/claim`

Home Assistant endpoint. Accepts the one-time code from the ESP32 portal and
returns a scoped `dry_run` write grant token if the code is valid, unused, not
expired, and the gateway is online for the requested Device ID.

### `GET /api/lumentree/devices/{device_id}/write-grants/status`

Returns write authorization diagnostics:

```json
{
  "device_id": "P240819130",
  "write_available": true,
  "write_enabled": false,
  "grant_active": false,
  "active_grant_count": 0,
  "gateway_id": "esp32-lumentree",
  "gateway_online": true,
  "command_mode": "dry_run_only",
  "safety": "no_real_inverter_write_enabled"
}
```

### `POST /api/lumentree/devices/{device_id}/write-grants/revoke`

Revokes the scoped write grant supplied as bearer token.

### `POST /api/lumentree/commands`

Creates a command. Only `mode=dry_run` and `command=dry_run_noop` are accepted.
Home Assistant must authenticate with a scoped write grant token. The production
server token is also accepted for administrative validation. No real inverter
write command exists.

## Security

- Use long random bearer tokens.
- Require the production token for telemetry upload, gateway status, gateway
  write-code registration, command polling, command result upload, and debug
  event history.
- Do not give the production token to Home Assistant in normal use. HASS claims
  a scoped write grant through the ESP32 one-time write pairing code flow.
- Expose only the HTTP API through Cloudflare Tunnel at
  `lumentree.jonah.io.vn`.
- Do not expose raw Postgres directly.
- Do not expose local server ports publicly outside the tunnel.

## Implementation Phases

### Phase 1: Production API Skeleton

- Add a small host-side HTTP server.
- Add Postgres schema initialization.
- Support telemetry POST, devices list, latest telemetry, and health.
- Verify with curl and psql.

### Phase 2: Cloudflare Tunnel Production Hostname

- Add `lumentree.jonah.io.vn` ingress to `/etc/cloudflared/config.yml`.
- Route the hostname to tunnel `ubuntu-home`.
- Keep the server bound to `127.0.0.1:8787` behind the tunnel.
- Validate `https://lumentree.jonah.io.vn/health`.
- Validate authenticated API calls through the public hostname.

### Phase 3: Collector Bridge

- Let the USB serial collector optionally POST events to the production API.
- Keep JSONL and Postgres raw capture as fallback evidence.

### Phase 4: Decoder

- Decode known BLE payloads into stable `metrics`.
- Compare metrics against the existing Lumentree Home Assistant integration.
- Store raw payloads alongside decoded metrics.

### Phase 5: Home Assistant Integration

- Add config flow for production API URL and token.
- Fetch devices from `/api/lumentree/devices`.
- Poll `/latest` for each configured device.
- Create sensor entities from the `metrics` object.
- Default API URL should be `https://lumentree.jonah.io.vn`.

### Phase 6: ESP32 Wi-Fi Upload

- Add Wi-Fi configuration to the dedicated firmware.
- Add HTTPS POST upload to `https://lumentree.jonah.io.vn/api/lumentree/events`.
- Keep BLE write operations blocked.
- Add retry/backoff and local status telemetry.

The read-only BLE implementation may send Modbus function 03 read-register
requests through FFE1. Firmware must not send Modbus write functions or change
inverter settings without explicit approval.

## Current Next Step

Promote the current API skeleton to a production service behind
`https://lumentree.jonah.io.vn`, then point Home Assistant and future ESP32
Wi-Fi firmware at that hostname.
