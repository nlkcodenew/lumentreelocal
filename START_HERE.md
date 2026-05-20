# START HERE: PUBLIC HACS Checkout

Every new session must read this file first.

Global entrypoint for new sessions:

```text
/home/mrlinh/LUMENTREE_START_HERE.md
```

This checkout is:

```text
/home/mrlinh/esp32-lumentree-public
```

Branch role:

```text
main
```

Remote role:

```text
origin -> https://github.com/nlkcodenew/lumentreelocal
```

This checkout is only for public-safe work:

- Home Assistant integration code
- HACS metadata
- public README and changelog

Do not use this checkout for:

- firmware
- flash-site
- private operational docs
- local server runtime recovery

That work belongs in:

```text
/home/mrlinh/esp32-lumentree
```

## Read Order

1. `README.md`
2. `CHANGELOG.md`
3. `custom_components/lumentreelocal/manifest.json`

## Push Rules

- For this checkout, `git push` must go to `origin main`.
- Do not copy private runtime files into this checkout.

## Session Rule

If a task mentions firmware, flash, host runtime, handoff docs, or local recovery,
stop and switch to the private checkout first.

If unsure, run:

```bash
git whereami
```
