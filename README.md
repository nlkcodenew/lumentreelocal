# Lumentree Local

Home Assistant custom integration for Lumentree inverter telemetry.

This public repository is intentionally limited to the integration surface
needed for HACS installation and Home Assistant runtime code.

## Install With HACS

Add this repository as a custom repository in HACS:

```text
https://github.com/nlkcodenew/lumentreelocal
```

Category:

```text
Integration
```

Then install `Lumentree Local` and restart Home Assistant.

## Add Integration

In Home Assistant:

```text
Settings -> Devices & services -> Add Integration -> Lumentree Local
```

The integration asks for the inverter `Device ID`.

## What The Integration Exposes

- realtime inverter metrics
- energy sensors
- health diagnostics
- optional guarded write entities when supported by the paired backend

## Repository Layout

- `custom_components/lumentreelocal/`: Home Assistant integration code
- `CHANGELOG.md`: release notes for HACS-visible versions
- `hacs.json`: HACS metadata

## Deploy To HAOS

For the local `HAOS` VM on this workstation, use the repo deploy script instead
of patching files manually inside the VM:

```bash
python3 tools/deploy_lumentreelocal_to_haos.py \
  --ha-url "$HA_URL" \
  --ha-token "$HA_TOKEN"
```

What it does:

- packages `custom_components/lumentreelocal/` from this repo
- connects to the `HAOS` libvirt VM through `virsh console`
- backs up the currently deployed runtime directory inside HAOS
- replaces `/mnt/data/supervisor/homeassistant/custom_components/lumentreelocal`
- restarts Home Assistant Core
- optionally waits for the HA API to come back

If `HA_URL` and `HA_TOKEN` are not already exported, load them from
`/home/mrlinh/hass/.env` first.

## Release

The version source of truth is:

```text
custom_components/lumentreelocal/manifest.json
```

Public release notes are kept in:

```text
CHANGELOG.md
```
