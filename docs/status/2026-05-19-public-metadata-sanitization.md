# 2026-05-19 Public Metadata Sanitization

## Goal

Reduce public exposure of production identifiers in HACS-facing and
Home-Assistant-facing metadata.

## Changes In `0.14.1`

- Removed the Home Assistant device `configuration_url` from the integration
  device info so the production API hostname is no longer shown in the device
  UI.
- Changed new config entry titles from `Lumentree Local <device_id>` to
  `Lumentree Local`.
- Changed device names from `Lumentree Local <device_id>` to
  `Lumentree Local`.
- Sanitized public README examples to use placeholders instead of the real
  production hostname, device ID, and inverter MAC.

## Follow-up In `0.14.2`

- Added a config-entry title migration so existing Home Assistant installs
  update old titles such as `Lumentree Local <device_id>` to
  `Lumentree Local` after integration reload.

## Scope

This change does not remove the internal default API URL from source code.
That value still exists in code because the integration needs a working backend
target. The user requirement here is to keep public-facing HACS and standard
Home Assistant UI surfaces from exposing production details by default.
