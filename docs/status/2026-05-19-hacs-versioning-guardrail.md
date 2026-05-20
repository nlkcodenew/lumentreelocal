# HACS Versioning Guardrail

Date: 2026-05-19

## Problem

Integration code was pushed to `main`, but HACS still offered `v0.14.2`
because the integration release process was incomplete.

The missing pieces were:

- `custom_components/lumentreelocal/manifest.json` version bump
- matching Git tag
- matching GitHub Release

## Rule

For any Home Assistant integration change that the user is expected to update
through HACS, the work is not complete until:

1. `manifest.json` has the new version
2. `CHANGELOG.md` has the matching section
3. the release commit is pushed
4. `./tools/release_hacs_integration.sh` succeeds
5. the new GitHub Release exists on the remote repository

## Automation

Use:

```bash
./tools/release_hacs_integration.sh
```

This makes the release flow reproducible and avoids telling the user to update
before HACS can actually see the new version.
