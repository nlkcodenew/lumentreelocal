# Lumentree Local Vendor Server Implementation Plan

## Decision

Build a production HTTP API server backed by Postgres and expose it through
Cloudflare Tunnel at `https://lumentree.jonah.io.vn`.

Home Assistant and ESP32 gateways should use this production hostname, even when
they happen to be on the same LAN. Do not make LAN addressing part of the
production contract. Do not add MQTT unless a later requirement needs MQTT
Discovery or broker-based fanout.

## Task List

- [x] Write the local vendor server spec.
- [x] Add a minimal Postgres-backed host API server.
- [x] Add setup and curl verification docs.
- [x] Run the server against the real Postgres DSN.
- [x] POST decoded BLE telemetry through the collector.
- [x] Verify `devices` and `latest` endpoints.
- [x] Extend the serial collector to POST raw BLE serial events to the API.
- [x] Add decoded metrics in the API when a Modbus response payload is present.
- [x] Add firmware commands for manual read-only BLE Modbus requests.
- [x] Implement a read-only Modbus response decoder for known Lumentree registers.
- [x] Create separate Home Assistant integration repo `lumentreelocal`.
- [x] Add `lumentree.jonah.io.vn` Cloudflare Tunnel ingress.
- [x] Run the local server as a systemd service bound to `127.0.0.1:8787`.
- [x] Generate and persist a production API token outside Git.
- [x] Validate `https://lumentree.jonah.io.vn/health`.
- [x] Validate authenticated `devices` and `latest` endpoints through the
      production hostname.
- [x] Configure Home Assistant `lumentreelocal` with
      `https://lumentree.jonah.io.vn`.
- [x] Simplify Home Assistant setup so the user enters only `Device ID`.
- [x] Add local Postgres energy aggregation endpoints and Home Assistant energy
      entities.
- [ ] Add ESP32 Wi-Fi HTTPS upload to the production hostname.

## Production Service Commands

Expected production-style local server command:

```bash
cd /home/mrlinh/esp32-lumentree/host/local-server
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
export LUMENTREE_POSTGRES_DSN='postgresql://USER:PASSWORD@HOST:5432/DBNAME'
export LUMENTREE_API_TOKEN="$(openssl rand -hex 32)"
python server.py --init-db --host 127.0.0.1 --port 8787
```

Cloudflare Tunnel should map:

```yaml
- hostname: lumentree.jonah.io.vn
  service: http://127.0.0.1:8787
```

Public verification:

```bash
curl https://lumentree.jonah.io.vn/health

curl -H "Authorization: Bearer $LUMENTREE_API_TOKEN" \
  https://lumentree.jonah.io.vn/api/lumentree/devices

curl -H "Authorization: Bearer $LUMENTREE_API_TOKEN" \
  https://lumentree.jonah.io.vn/api/lumentree/devices/P240819130/latest
```

## Guardrails

- Firmware changes in this phase are limited to read-only BLE Modbus function 03
  commands.
- This phase does not add MQTT.
- Do not expose Postgres or the raw local server port directly.
- Do not use LAN-only URLs for production Home Assistant or ESP32 gateway
  configuration.
- Any function that changes inverter settings must be explicitly approved first.

## 2026-05-17 Runtime Validation

- Flashed `firmware` firmware `0.2.0` to `/dev/ttyACM0`.
- `READ_MAIN_ONCE` sent Modbus function 03 read registers `0-94` through BLE
  FFE0/FFE1 and received a reassembled 195-byte response.
- Decoder validated the response with CRC OK and device SN `P240819130`.
- Local API server stored decoded metrics in Postgres and returned them from
  `GET /api/lumentree/devices/P240819130/latest`.
- Battery-cell read command returned a response, but it did not match valid
  cell-voltage data yet; keep it as raw diagnostics until the register meaning
  is confirmed.

## 2026-05-17 Production Direction Update

- Production API hostname is `https://lumentree.jonah.io.vn`.
- Same-LAN access is no longer the target deployment path.
- Cloudflare Tunnel is required before configuring Home Assistant for the
  long-lived setup.
- ESP32 Wi-Fi firmware must upload to the production hostname so remote
  inverters use the same path as local inverters.
