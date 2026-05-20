# 2026-05-19 HACS Release Flow

## Goal

Prepare the repository so HACS updates can show a meaningful "what's new"
description once GitHub Releases are published.

## Changes

- Added root `CHANGELOG.md` as the canonical source for release notes.
- Recorded integration version `0.14.0` with the current HACS-visible changes.
- Updated `README.md` with the release procedure for future HACS updates.
- Created the actual GitHub Release `v0.14.0` on
  `github.com/nlkcodenew/lumentreelocal`.

## Release Rule

For every future Home Assistant integration update:

1. Bump `custom_components/lumentreelocal/manifest.json`.
2. Add a matching section to `CHANGELOG.md`.
3. Commit and push the release commit.
4. Run `./tools/release_hacs_integration.sh`.
5. Confirm the Git tag `vX.Y.Z` exists on origin.
6. Confirm the matching GitHub Release exists before telling the user to update
   from HACS.

This is now the mandatory release flow. A plain push to `main` is not enough
for HACS-visible integration updates.

Public HACS-facing content must stay sanitized:

- Do not put real production API hostnames in `README.md` or GitHub Release
  notes.
- Do not put real device IDs, inverter MAC addresses, or tokens in
  `README.md` or GitHub Release notes.
- Use placeholders such as `<your device id>` and
  `https://<your public api hostname>` in public documentation.

HACS can read the repository version from `manifest.json`, but the visible
"what's new" experience depends on having a real GitHub Release with release
notes on the remote repository.

## Local GitHub Capability

As of 2026-05-19, this machine is authenticated for GitHub Releases:

- `gh` installed and working
- `gh auth status` logged in as `nlkcodenew`
- Git protocol configured by `gh` as `https`
- Auth state stored at `~/.config/gh/hosts.yml`

This means future Codex sessions can create GitHub Releases directly without
asking the user to open the web UI first.

## Standard Release Script

The repository now includes:

```bash
./tools/release_hacs_integration.sh
```

The script enforces:

- clean worktree
- version source of truth from `custom_components/lumentreelocal/manifest.json`
- matching changelog section in `CHANGELOG.md`
- no duplicate local tag
- no duplicate remote tag
- no duplicate GitHub Release
- push current branch
- create and push the Git tag
- create the GitHub Release from the matching changelog section

## Failure That Triggered This Rule

On 2026-05-19, integration code was pushed to `main` but `manifest.json` still
reported `0.14.2`, so HACS still offered `v0.14.2` instead of the new code.

The fix was to release `v0.14.3` properly with:

- manifest version bump
- changelog entry
- tag push
- GitHub Release

This is the specific class of mistake the script is meant to prevent.
