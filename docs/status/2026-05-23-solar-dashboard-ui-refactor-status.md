# Solar Dashboard UI Refactor Status

Date: 2026-05-23

## Scope completed

- Audited the live Lovelace storage dashboard `z-thi-t-b` and confirmed the active `Solar` view is stored in Home Assistant, not maintained as a primary source file in this repo.
- Backed up the live dashboard config and the current `my-3d-energy-card` worktree before editing.
- Reworked the live `Solar` tab layout toward a more modern structure:
  - top toolbar row
  - Light button
  - Dark button
  - wide live energy-flow overview card
  - forecast card
  - battery summary card
- Updated the stored dashboard snapshot file in this repo to match the live dashboard after the change.

## Backup and rollback

- Root backup artifact:
  - `/home/mrlinh/esp32-lumentree/solar-ui-backup-20260523-081008.tar.gz`
- The archive contains:
  - live Lovelace config for dashboard `z-thi-t-b`
  - baseline SHAs for the private repo and `my-3d-energy-card`
  - a clean snapshot of the cloned `my-3d-energy-card` worktree
  - the prior dashboard snapshot file from this repo

## Live dashboard changes

- Dashboard id/url path:
  - `z-thi-t-b`
- `Solar` view changes:
  - `max_columns` moved to `4`
  - `sections` count moved from `3` to `4`
  - section 1: toolbar + `Light` / `Dark` theme actions
  - section 2: `custom:nlk-3d-energy-card` retained as the primary overview
  - section 3: theme-aware solar forecast card
  - section 4: theme-aware battery summary card

## Theme toggle approach

- The toggle does not rely on a new helper entity.
- It uses the built-in Home Assistant service:
  - `frontend.set_theme`
- It targets the built-in `default` theme with:
  - `mode: light`
  - `mode: dark`

This keeps the toggle low-risk and avoids introducing extra helper state just for the dashboard.

## Custom card follow-up

- Repository cloned locally for refactor work:
  - `/home/mrlinh/my-3d-energy-card`
- Intended card changes:
  - make `NLK-3d-energy-card.js` more theme-friendly in both light and dark modes
  - release a new HACS-visible version for the dashboard to pick up after update

## Validation performed

- Verified live dashboard storage access through HA websocket API.
- Verified dashboard save succeeded for `z-thi-t-b`.
- Verified the updated live config now reports `4` sections in the `Solar` view.
- Parsed the custom card JavaScript with `node --check`.

## Remaining operational step

- The live dashboard config is already updated.
- The custom card repo still needs to be committed, pushed, tagged, and then updated through HACS on the Home Assistant side so the new theme-aware card shell is actually served from `/hacsfiles/my-3d-energy-card/...`.
