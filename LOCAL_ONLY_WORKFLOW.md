# Local-Only Workflow

This checkout is intended to hold local runtime assets that must not return to
the public `main` branch.

Use this branch for:

- firmware source and local build artifacts needed for production work
- flash-site source and public firmware files served from this machine
- local docs, handoff notes, evidence, and operational scripts

Public repo workflow:

1. Keep `/home/mrlinh/esp32-lumentree` on the local-only branch.
2. Commit local runtime changes here before flashing or deploying.
3. Use `/home/mrlinh/esp32-lumentree-public` as the clean public `main` worktree.
4. Only make public-safe changes in the public worktree, then commit/push there.

Remote mapping:

1. `private` is the push target for this checkout.
2. `origin` is the public repo and should not be used from this checkout.

Minimum rule before flashing a new firmware build:

1. Commit the firmware and any related flash-site changes on the local-only branch.
2. Verify the flash site still serves the expected manifest and binaries.
