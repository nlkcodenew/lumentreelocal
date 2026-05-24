# 2026-05-24 Persistent BLE Session Task Run

## Phase 0: Documentation And Safety

- create plan/spec/task files
- create pre-refactor backup
- create pre-refactor checkpoint commit and push

## Phase 1: Session Manager

- identify current duplicated connect / service / characteristic resolution code
- create one shared BLE session path
- add explicit runtime state and reconnect backoff
- keep direct `target_mac` reconnect path separate from discovery

Exit criteria:

- code has one normal read/write session acquisition path

## Phase 2: Cached Read Model

- introduce cached snapshots for:
  - fast telemetry
  - settings
  - statistics
- timestamp each cache
- convert upload loop to use cache freshness rather than direct reconnect reads

Exit criteria:

- upload loop does not directly imply a fresh BLE connect for each payload

## Phase 3: Adaptive Poll Scheduler

- define fast tier cadence
- keep settings/statistics on slower cadence
- ensure writes preempt periodic reads
- ensure failed reads move session into reconnect/backoff logic

Exit criteria:

- periodic read cadence is tiered and session-aware

## Phase 4: Write Path Preservation

- route semantic writes through the shared session path
- preserve pre-read / write / post-read verification
- preserve schedule safety checks
- preserve forced settings refresh after successful write

Exit criteria:

- no regression in guarded write behavior

## Phase 5: Validation

- build `esp32-s3-8mb-nopsram-release`
- build `esp32-c3-4mb-experimental`
- verify compile result and note binary sizes
- if hardware flash happens later, verify:
  - stable initial connect
  - repeated fast reads without reconnect churn
  - clean reconnect after power/link drop
  - post-write settings refresh still works

## Runtime Evidence To Capture

- serial logs showing:
  - first connect
  - notify registration
  - repeated reads without disconnect
  - reconnect after forced failure
- if flashed:
  - timestamps for observed telemetry cadence
  - proof that HA faster refresh did not force more BLE reconnects

## Follow-Up If Phase 1-5 Succeed

- update `firmware/README.md`
- update `SESSION_HANDOFF.md`
- write one rollout/status note with measured behavior and any board-specific
  caveats
