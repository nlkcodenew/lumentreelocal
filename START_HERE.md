# START HERE: PRIVATE Runtime Checkout

Every new session must read this file first.

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

1. `LOCAL_ONLY_WORKFLOW.md`
2. `SESSION_HANDOFF.md`
3. `docs/status/2026-05-20-standard-firmware-flash-ha-update-runbook.md`
4. `README.md`
5. `firmware/README.md`

## Push Rules

- For this checkout, `git push` must go to `private`.
- Do not push this branch to `origin`.
- Before flashing new firmware, commit here first.

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
