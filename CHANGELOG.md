# Changelog

All notable changes to this repository should be recorded here.

This project follows a simple release flow for HACS:

- `custom_components/lumentreelocal/manifest.json` is the source of truth for the integration version.
- Each published HACS update should have a matching Git tag such as `v0.14.0`.
- Each GitHub Release should copy the matching section from this file so HACS can show "what's new".

## [Unreleased]

- No unreleased changes.

## [0.14.14] - 2026-05-21

### Added

- Added schedule-safety guardrails so Home Assistant blocks enabling overlapping mains-charge and discharge windows in both directions.

### Changed

- Schedule start/end time edits now require the target slot to be turned OFF before the time window can be changed.

### Fixed

- Canonicalized overnight overlap detection so touching boundaries remain allowed while true cross-midnight overlap is rejected consistently.

## [0.14.12] - 2026-05-20

### Added

- Initial setup now claims a required read grant from an ESP32 portal pairing token, with an optional separate write pairing token for guarded inverter writes.

### Fixed

- Write grant status now uses the active write grant token so Home Assistant diagnostics reflect granted write access correctly.

## [0.14.11] - 2026-05-20

### Added

- Added six Home Assistant sensors for the inverter's vendor-reported daily energy counters: PV generation, essential load, total load, grid import, battery charge, and battery discharge.

## [0.14.10] - 2026-05-19

### Fixed

- Corrected the slot 1 discharge target SOC entity migration so Home Assistant uses the consistent `number.lumentree_local_*` entity ID pattern instead of the temporary `number.lumentreelocal_*` variant.

## [0.14.9] - 2026-05-19

### Fixed

- Reordered discharge number entities so Home Assistant shows each slot's discharge power before its target SOC in the writable controls list.

## [0.14.8] - 2026-05-19

### Fixed

- Migrated the slot 1 discharge target SOC entity from the old `first_discharge_target_soc` entity identity to `discharge_slot_1_target_soc` so Home Assistant can group and sort it consistently with the other discharge slot entities.

## [0.14.7] - 2026-05-19

### Fixed

- Writable switch entities now keep a short pending-confirmation state after a toggle so the UI does not bounce back to the old value before inverter confirmation arrives.
- Repeated switch toggles are blocked while a previous switch change is still waiting for confirmation from the ESP32/API round-trip.

## [0.14.6] - 2026-05-19

### Fixed

- Renamed the slot 1 discharge target SOC entity to `Discharge slot 1 target SOC` so it groups consistently with slots 2-4 in Home Assistant.

## [0.14.5] - 2026-05-19

### Changed

- Updated the integration write-access wording to describe production write access instead of dry-run authorization.

### Fixed

- Aligned the production API write-grant scope with real guarded inverter writes.
- Legacy active grants now upgrade from `dry_run` to `write` automatically when validated by the production API.
- Write status now reports `command_mode = guarded_write` with allowlisted semantic write safety instead of the old dry-run-only status.

## [0.14.4] - 2026-05-19

### Fixed

- Restored `Direct inverter write access` and `Direct inverter write status` so Home Assistant can add them normally again.
- Removed the invalid `config` entity category from the write-access binary sensor and sensor, which caused Home Assistant to reject those entities during setup.

## [0.14.3] - 2026-05-19

### Changed

- Marked write-capable switch, number, and time entities as Home Assistant config entities so they stand out from telemetry.
- Made write-access status more visible with direct-inverter wording in entity names.
- Strengthened the integration options warning that write-capable entities directly change inverter settings.

### Added

- Added warning attributes on write-capable entities to clarify that they write directly to inverter schedules and settings.

## [0.14.2] - 2026-05-19

### Changed

- Added a config-entry title migration so existing Home Assistant installs stop showing the old `Lumentree Local <device_id>` title after reload.

## [0.14.1] - 2026-05-19

### Changed

- Removed the Home Assistant device `configuration_url` so the production API hostname is no longer exposed in the device UI.
- Stopped appending the configured device ID to the integration title and device name for new entries.

### Documentation

- Sanitized the public README placeholders used by HACS-facing documentation.

## [0.14.0] - 2026-05-19

### Added

- Home Assistant control entities for `Mains charge the battery`.
- Guarded semantic write commands for mains-charge time enable, start, end, and target SOC.
- Overlap validation in Home Assistant before enabling mains-charge schedules.

### Changed

- Home Assistant polling interval reduced to `10s`.
- ESP32 runtime/default upload interval aligned to `10s`.
- Control ordering updated so writable schedule entities appear in a consistent group order.

### Documentation

- Added verified evidence and mapping notes for discharge and mains-charge writable entities.
- Documented the overlap rule: mains-charge enable must be blocked when it collides with any enabled discharge window.
