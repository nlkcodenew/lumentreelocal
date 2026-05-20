# Flash Site UX And Localization

Date: 2026-05-19

## Scope

Finalize the public flash website UX after the dedicated flash site deployment.

## Changes Recorded

### 1. Default language

The public page at `https://flash-lumentree.jonah.io.vn/` now defaults to
Vietnamese.

### 2. Language toggle

The page now exposes a direct `VI / EN` toggle in the header so users can
switch between Vietnamese and English without reloading the page.

### 3. Post-flash provisioning guidance

The `After Flash` section now documents the real first-run sequence:

1. Wait for the ESP32 to reboot.
2. Connect to `Lumentree-XXXX`.
3. Open `http://192.168.4.1`.
4. Enter Wi-Fi and production settings.
5. Confirm telemetry reaches the production API before adding Home Assistant
   controls.

### 4. Write-access pairing guidance

The page now documents the required write flow explicitly:

1. Open `http://192.168.4.1` again after the ESP32 is online.
2. Go to `Write Access`.
3. Press `Generate write pairing code`.
4. Open `Lumentree Local` integration options in Home Assistant.
5. Enter the one-time `Write pairing code` there to claim write access.

Without a valid pairing code, Home Assistant should remain read-only for write
entities.

### 5. Vietnamese copy

The Vietnamese copy on the public page was upgraded from ASCII-only text to
fully accented Vietnamese text.

## Files

- `host/flash-site/public/index.html`
- `host/flash-site/README.md`

## Commits

- `dfc2581 docs: update flash site provisioning guidance`
- `12da10a docs: add accented Vietnamese flash site copy`
