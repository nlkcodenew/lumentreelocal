# START HERE: PRIVATE Runtime Checkout

Every new session must read this file first.

Global entrypoint for new sessions:

```text
/home/mrlinh/esp32-lumentree/LUMENTREE_START_HERE.md
```

This checkout is:

```text
/home/mrlinh/esp32-lumentree
```

Branch role:

```text
local-only
```

Remote role:

```text
private -> https://github.com/nlkcodenew/lumentreelocal-private
```

This checkout is for:

- firmware
- flash-site
- local server and host-side runtime work
- private docs, evidence, handoff, and operational scripts

Do not treat this checkout as the public HACS repo.

Public-safe work belongs in:

```text
/home/mrlinh/esp32-lumentree-public
```

## Read Order

1. `SESSION_HANDOFF.md`
2. `docs/status/2026-05-20-standard-firmware-flash-ha-update-runbook.md`
3. `README.md`
4. `firmware/README.md`

## Push Rules

- For this checkout, `git push` must go to `private`.
- Do not push this branch to `origin`.
- Before flashing new firmware, commit here first.

## Private Workflow

This checkout exists to hold runtime assets that must not return to the public
`main` branch.

Use this checkout for:

- firmware source and local build artifacts needed for production work
- flash-site source and public firmware files served from this machine
- local docs, handoff notes, evidence, and operational scripts

Public repo workflow:

1. Keep `/home/mrlinh/esp32-lumentree` on the `local-only` branch.
2. Commit local runtime changes here before flashing or deploying.
3. Use `/home/mrlinh/esp32-lumentree-public` as the clean public `main` worktree.
4. Only make public-safe changes in the public worktree, then commit/push there.

Remote mapping:

1. `private` is the push target for this checkout.
2. `origin` is the public repo and should not be used from this checkout.

Minimum rule before flashing a new firmware build:

1. Commit the firmware and any related flash-site changes on the local-only branch.
2. Verify the flash site still serves the expected manifest and binaries.

## Scope Index

- `firmware/`: ESP32 firmware
- `host/flash-site/`: flash website and firmware files served from this machine
- `host/local-server/`: local API server
- `docs/`: private notes, evidence, specs, runbooks
- `custom_components/lumentreelocal/`: integration code when a task truly spans runtime and integration

## Stable Facts

- Production API: `https://lumentree.jonah.io.vn`
- Local API bind: `127.0.0.1:8787`
- Flash site local bind: `127.0.0.1:8790`
- Home Assistant domain: `lumentreelocal`

## Session Rule

Before editing anything, confirm whether the task belongs to:

1. private runtime checkout
2. public HACS checkout

If unsure, run:

```bash
git whereami
```
