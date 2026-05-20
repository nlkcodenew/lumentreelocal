# Session Handoff

Continue public-safe HACS work in:

```text
/home/mrlinh/esp32-lumentree-public
```

This checkout must stay on:

```text
branch = main
remote = origin
```

Private runtime work is separate:

```text
/home/mrlinh/esp32-lumentree
branch = local-only
remote = private
```

## Read First

1. `START_HERE.md`
2. `README.md`
3. `CHANGELOG.md`

## Allowed Scope

- `custom_components/lumentreelocal/`
- `README.md`
- `CHANGELOG.md`
- `hacs.json`

## Stop Conditions

If the task needs any of these, switch to the private checkout:

- `firmware/`
- `host/flash-site/`
- `host/local-server/`
- `docs/`
- session handoff or operational evidence outside this public worktree
