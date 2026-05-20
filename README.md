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

With the current onboarding flow, initial setup also requires:

- a `read pairing token` from the ESP32 local portal
- an optional `write pairing token` if Home Assistant should be allowed to send guarded inverter setting changes

## What The Integration Exposes

- realtime inverter metrics
- energy sensors
- health diagnostics
- optional guarded write entities when supported by the paired backend

## Repository Layout

- `custom_components/lumentreelocal/`: Home Assistant integration code
- `CHANGELOG.md`: release notes for HACS-visible versions
- `hacs.json`: HACS metadata

## Release

The version source of truth is:

```text
custom_components/lumentreelocal/manifest.json
```

Public release notes are kept in:

```text
CHANGELOG.md
```
