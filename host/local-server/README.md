# Lumentree Local Server

This service is the production replacement for the Lumentree vendor-cloud role.

It accepts telemetry from a future ESP32-S3 BLE/Wi-Fi gateway, stores events in
Postgres, and exposes a small HTTP API for the `lumentreelocal` Home Assistant
custom integration.

Production API hostname:

```text
https://lumentree.jonah.io.vn
```

MQTT is not required.

## Install

```bash
cd host/local-server
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
```

## Configure

```bash
export LUMENTREE_POSTGRES_DSN='postgresql://USER:PASSWORD@HOST:5432/DBNAME'
export LUMENTREE_API_TOKEN='change-this-to-a-long-random-token'
```

`LUMENTREE_API_TOKEN` is required for production. Generate a long token:

```bash
openssl rand -hex 32
```

## Run

```bash
python server.py --init-db --host 127.0.0.1 --port 8787
```

The server should stay bound to `127.0.0.1` behind Cloudflare Tunnel. Do not
expose the raw port directly to the network.

Cloudflare Tunnel ingress:

```yaml
- hostname: lumentree.jonah.io.vn
  service: http://127.0.0.1:8787
```

## Test Production Route

```bash
curl https://lumentree.jonah.io.vn/health
```

Post a sample event:

```bash
curl -X POST https://lumentree.jonah.io.vn/api/lumentree/events \
  -H "Authorization: Bearer $LUMENTREE_API_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "P240819130",
    "mac": "d8:13:2a:ee:58:d6",
    "gateway_id": "host-test",
    "metrics": {"battery_soc": 82},
    "raw": {"transport": "manual-test"}
  }'
```

Read devices and latest telemetry:

```bash
curl -H "Authorization: Bearer $LUMENTREE_API_TOKEN" \
  https://lumentree.jonah.io.vn/api/lumentree/devices

curl -H "Authorization: Bearer $LUMENTREE_API_TOKEN" \
  https://lumentree.jonah.io.vn/api/lumentree/devices/P240819130/latest
```

## Write Authorization

Read-only endpoints are available without a Home Assistant token. Command
creation requires either the server token or a scoped write grant.

The intended user flow is:

1. ESP32 generates a one-time write pairing code from its portal or serial
   command `GENERATE_WRITE_CODE`.
2. ESP32 registers the code with
   `/api/lumentree/gateways/{gateway_id}/write-pairing-code` using the server
   token.
3. Home Assistant claims a scoped write grant through
   `/api/lumentree/devices/{device_id}/write-grants/claim`.
4. Home Assistant uses that grant for `POST /api/lumentree/commands`.

The server stores only hashes for pairing codes and grant tokens. Write commands
are allow-listed semantic commands only: target SOC, discharge slot enable,
discharge power, and discharge time start/end. There is no generic register write
API.
