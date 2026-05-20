# 2026-05-20 Standard Firmware Flash + HA Update Runbook

## Goal

One standard operational procedure for:

1. flashing the current ESP32 firmware,
2. validating function `0x04` statistics upload,
3. updating the Home Assistant `lumentreelocal` integration,
4. restarting Home Assistant only when it is actually needed.

This runbook reflects the current production path verified on 2026-05-20.

## Scope

- Repo root: `/home/mrlinh/esp32-lumentree`
- ESP32 serial path used in current validation: `/dev/ttyACM0`
- Production API: `https://lumentree.jonah.io.vn`
- Local API bind: `127.0.0.1:8787`
- Home Assistant integration domain: `lumentreelocal`

## Preconditions

- Work from the canonical repo:

```bash
cd /home/mrlinh/esp32-lumentree
```

- Confirm the worktree is clean before release/flash:

```bash
git status --short
```

- For Home Assistant API operations in this environment, credentials live in:

```text
/home/mrlinh/hass/.env
```

Expected keys:

```text
HA_URL=http://192.168.122.230:8123
HA_TOKEN=...
```

## A. Build And Flash Firmware

Current public release environment:

```text
firmware/platformio.ini -> [env:esp32-s3-8mb-nopsram-release]
```

### 1. Build

```bash
cd /home/mrlinh/esp32-lumentree/firmware
pio run -e esp32-s3-8mb-nopsram-release --project-conf platformio.ini
```

### 2. Flash

```bash
cd /home/mrlinh/esp32-lumentree/firmware
pio run -e esp32-s3-8mb-nopsram-release --project-conf platformio.ini -t upload --upload-port /dev/ttyACM0
```

### 3. Verify Serial Boot

```bash
cd /home/mrlinh/esp32-lumentree
python3 -m serial.tools.miniterm /dev/ttyACM0 115200
```

Minimum checks after boot:

- firmware emits normal JSONL logs,
- target MAC is still configured,
- production upload resumes,
- no boot loop or repeated BLE/HTTP fatal failures.

## B. Validate Function 0x04 After Flash

### 1. Trigger a live statistics read

Inside the serial session:

```text
READ_STATS_ONCE
```

Expected log shape:

- `ble_modbus_read_start` with `label=today_statistics_0_7`
- `command_hex=010400000008F1CC`
- `ble_modbus_response` with `payload_hex` starting `010410`
- `ble_modbus_read_done` with
  `safety=function_04_read_only_input_registers`
- `api_upload_ok`

### 2. Verify the latest statistics row in Postgres

The local server runs under systemd and already has the DSN in its process
environment. Use the running PID to avoid retyping secrets:

```bash
SERVER_PID="$(ps -eo pid,cmd | rg '/host/local-server/server.py --init-db' | awk 'NR==1{print $1}')"
export DSN="$(tr '\0' '\n' < /proc/$SERVER_PID/environ | sed -n 's/^LUMENTREE_POSTGRES_DSN=//p')"

/home/mrlinh/esp32-lumentree/host/local-server/.venv/bin/python - <<'PY'
import os, psycopg
from psycopg.rows import dict_row
conn = psycopg.connect(os.environ["DSN"], row_factory=dict_row)
with conn, conn.cursor() as cur:
    cur.execute("""
      SELECT id, observed_at, metrics,
             raw->>'safety' AS safety,
             raw->'decode'->>'data_type' AS data_type
      FROM lumentree_telemetry
      WHERE COALESCE(raw->>'label', '') = 'today_statistics_0_7'
      ORDER BY id DESC
      LIMIT 1
    """)
    print(cur.fetchone())
PY
```

Expected result:

- `data_type = statistics`
- `safety = function_04_read_only_input_registers`
- `metrics` contains:
  - `today_pv_generation_kwh`
  - `today_essential_load_kwh`
  - `today_grid_import_kwh`
  - `today_total_load_kwh`
  - `today_battery_charge_kwh`
  - `today_battery_discharge_kwh`

## C. Update Home Assistant Integration

### 1. Check whether HA already has the latest released integration

```bash
HA_URL="$(grep '^HA_URL=' /home/mrlinh/hass/.env | cut -d= -f2-)"
HA_TOKEN="$(grep '^HA_TOKEN=' /home/mrlinh/hass/.env | cut -d= -f2-)"

curl -sS -H "Authorization: Bearer $HA_TOKEN" \
  "$HA_URL/api/states/update.lumentree_local_update"
```

If:

- `installed_version == latest_version`
  then **do not update or restart Home Assistant**.

If:

- `installed_version != latest_version`
  then install the update.

### 2. Install the HACS update when needed

```bash
curl -sS -X POST \
  -H "Authorization: Bearer $HA_TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"entity_id":"update.lumentree_local_update","backup":false}' \
  "$HA_URL/api/services/update/install"
```

To install a specific release:

```bash
curl -sS -X POST \
  -H "Authorization: Bearer $HA_TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"entity_id":"update.lumentree_local_update","version":"v0.14.11","backup":false}' \
  "$HA_URL/api/services/update/install"
```

Then poll:

```bash
curl -sS -H "Authorization: Bearer $HA_TOKEN" \
  "$HA_URL/api/states/update.lumentree_local_update"
```

Expected final state:

- `in_progress = false`
- `installed_version == latest_version`

## D. Restart Or Reload Home Assistant Only When Needed

### Do not restart HASS by default

If the integration is already on the latest version and the expected entities
exist, leave HASS alone.

### Preferred order

1. HACS install finishes
2. check whether expected entities exist
3. if missing, first try integration reload
4. only then do a full HA restart

### 1. Reload the `lumentreelocal` config entry

Find the config entry id from a known entity:

```bash
python3 - <<'PY'
import asyncio, json, os, websockets
ha_url = os.popen("grep '^HA_URL=' /home/mrlinh/hass/.env | cut -d= -f2-").read().strip()
token = os.popen("grep '^HA_TOKEN=' /home/mrlinh/hass/.env | cut -d= -f2-").read().strip()
ws_url = ha_url.replace('http://', 'ws://').replace('https://', 'wss://') + '/api/websocket'
async def main():
    async with websockets.connect(ws_url) as ws:
        await ws.recv()
        await ws.send(json.dumps({"type": "auth", "access_token": token}))
        await ws.recv()
        await ws.send(json.dumps({
            "id": 1,
            "type": "config/entity_registry/get_entries",
            "entity_ids": ["sensor.lumentree_local_p240819130_pv_power"]
        }))
        print(await ws.recv())
asyncio.run(main())
PY
```

Use the returned `config_entry_id` with:

```bash
curl -sS -X POST \
  -H "Authorization: Bearer $HA_TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"entry_id":"<config_entry_id>"}' \
  "$HA_URL/api/services/homeassistant/reload_config_entry"
```

### 2. Restart Home Assistant only if reload is not enough

Use this when:

- HACS finished installing a new integration version,
- expected new entities still do not exist after reload,
- or the module clearly did not reload cleanly.

```bash
curl -sS -X POST \
  -H "Authorization: Bearer $HA_TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{}' \
  "$HA_URL/api/services/homeassistant/restart"
```

Wait for both web UI and API to come back:

```bash
curl -fsS http://192.168.122.230:8123/
curl -fsS -H "Authorization: Bearer $HA_TOKEN" "$HA_URL/api/"
```

## E. Verify Home Assistant Entities

For the current `0x04` statistics rollout, expected entity ids are:

```text
sensor.lumentree_local_today_pv_generation
sensor.lumentree_local_today_essential_load
sensor.lumentree_local_today_total_load
sensor.lumentree_local_today_grid_import
sensor.lumentree_local_today_battery_charge
sensor.lumentree_local_today_battery_discharge
```

Check states:

```bash
for eid in \
  sensor.lumentree_local_today_pv_generation \
  sensor.lumentree_local_today_essential_load \
  sensor.lumentree_local_today_total_load \
  sensor.lumentree_local_today_grid_import \
  sensor.lumentree_local_today_battery_charge \
  sensor.lumentree_local_today_battery_discharge
do
  echo "=== $eid ==="
  curl -sS -H "Authorization: Bearer $HA_TOKEN" "$HA_URL/api/states/$eid"
  echo
done
```

Expected:

- non-`unknown` numeric `kWh` states after a fresh statistics upload,
- `device_class = energy`,
- `state_class = total_increasing`.

## F. If Local Server Code Changed

If only `host/local-server` code changed and `systemctl restart` asks for
interactive authentication, use the existing service behavior:

- unit: `lumentree-local-server.service`
- runs as user `mrlinh`
- `Restart=always`

So this is enough:

```bash
SERVER_PID="$(ps -eo pid,cmd | rg '/host/local-server/server.py --init-db' | awk 'NR==1{print $1}')"
kill "$SERVER_PID"
```

Then confirm the new PID is up and health passes:

```bash
ps -eo pid,cmd | rg '/host/local-server/server.py --init-db'
curl -sS http://127.0.0.1:8787/health
```

## Current Verified Outcome

As of 2026-05-20 this runbook was verified against:

- firmware commit `e3117de`
- integration release `v0.14.11`
- ESP32 `READ_STATS_ONCE` returning a live `010410...` frame
- latest statistics row stored with:
  - `data_type = statistics`
  - `safety = function_04_read_only_input_registers`
- Home Assistant `today_*` statistics sensors showing non-`unknown` values
