# 2026-05-31 HA Routing Fix After SSE Rollout

## Problem Observed

After SSE rollout, Home Assistant still logged intermittent:

- `Latest telemetry fetch failed ... cannot connect to local Lumentree server`
- `Required health fetch failed ... cannot connect to local Lumentree server`
- `Optional command_status fetch failed ... cannot connect to local Lumentree server`

## Root Cause

Two routing mismatches happened in sequence:

1. HA config entry still pointed to Cloudflare API URL (`https://lumentree.jonah.io.vn`), so tunnel/proxy instability could still trigger fetch failures.
2. After switching HA to local URL, local-server process was still bound to `127.0.0.1`, which HAOS VM could not reach.

## Fix Applied

- Updated HA `lumentreelocal` config entry `api_url` to:
  - `http://192.168.122.1:8787`
- Updated local server systemd environment:
  - `/etc/lumentree/local-server.env`
  - `LUMENTREE_SERVER_HOST=0.0.0.0`
- Restarted service:
  - `sudo systemctl restart lumentree-local-server.service`
- Confirmed service listen:
  - `listening on http://0.0.0.0:8787`
- Reloaded HA config entry when needed from `setup_retry` back to `loaded`.

## Validation

- Journal showed concurrent successful requests from both lanes:
  - HA VM (`192.168.122.230`) -> `/latest`, `/health`, `/stream`
  - ESP upload path (`127.0.0.1`) -> `/api/lumentree/events`
- HA entities recovered from `unavailable`.
- `latest_telemetry_age` returned to low values (seconds range).

## Operational Rule

For this single-host + HAOS VM setup:

- Keep local server bound to `0.0.0.0` on host.
- Keep HA integration URL on VM-reachable local bridge address.
- Keep Cloudflare route only for remote browser access, not HA internal polling path.
