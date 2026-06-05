# Lumentree Private Runtime

This is the private runtime checkout for the Lumentree local stack.

Use this repo when the task touches any of these:

- ESP32 firmware
- local API server
- flash site
- Home Assistant deploy/runtime behavior
- private operational docs and evidence

Do not treat this repo as the public HACS repository. Public-safe integration
release work belongs in:

```text
/home/mrlinh/esp32-lumentree-public
```

## Read First

Follow this order at the start of every session:

1. `LUMENTREE_START_HERE.md`
2. `START_HERE.md`
3. `AGENTS.md`
4. `docs/status/2026-05-31-s3-sse-rollout-and-stability-final.md`
5. `docs/status/2026-05-31-ha-routing-fix-after-sse.md`

## Current Operational Baseline

- private runtime checkout:
  - path: `/home/mrlinh/esp32-lumentree`
  - branch: `local-only`
  - push remote: `private`
- preferred HA path:
  - `HAOS VM -> http://192.168.122.1:8787`
- remote browser path:
  - `https://lumentree.jonah.io.vn`
- current SSE-capable S3 runtime line:
  - env `esp32-s3-8mb-fastbulk-task-poll3s`
- current verified USB mapping on this host:
  - `/dev/ttyACM0` = `ESP32-C3`
  - `/dev/ttyACM1` = `ESP32-S3`

## Repository Layout

- `firmware/`: ESP32 firmware source, envs, and exported bins
- `host/local-server/`: local API server and Postgres bridge
- `host/flash-site/`: private firmware distribution site
- `custom_components/lumentreelocal/`: HA integration runtime code
- `docs/status/`: authoritative session reports and rollout evidence
- `docs/specs/`, `docs/plans/`: design and implementation references

## Common Commands

Build the current S3 runtime line:

```bash
platformio run -d /home/mrlinh/esp32-lumentree/firmware -e esp32-s3-8mb-fastbulk-task-poll3s
```

Check the local server:

```bash
curl http://127.0.0.1:8787/health
curl http://192.168.122.1:8787/health
```

Deploy the HA integration from this checkout into the local HAOS VM:

```bash
python3 tools/deploy_lumentreelocal_to_haos.py \
  --ha-url "$HA_URL" \
  --ha-token "$HA_TOKEN"
```

## Guardrails

- Prefer LAN/local-origin troubleshooting before blaming BLE or Cloudflare.
- Verify the USB chip type before every flash or erase action.
- Keep historical status reports as evidence; fix only the current entry docs
  when the baseline changes.
