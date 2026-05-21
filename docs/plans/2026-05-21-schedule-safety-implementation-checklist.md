# 2026-05-21 Schedule Safety Implementation Checklist

## Purpose

This checklist turns the schedule safety spec into a concrete implementation
sequence.

Reference spec:

- `docs/specs/2026-05-21-schedule-safety-guard-spec.md`

The goal is to make charge/discharge overlap protection reliable across:

- Home Assistant
- API server
- ESP32 firmware

while minimizing rollout risk.

## Non-Negotiable Safety Outcome

The final system must guarantee:

- an enabled mains-charge window may never overlap an enabled discharge window

and this must remain true even when:

- the Home Assistant UI is bypassed
- commands are delayed in queue
- gateway connectivity drops and later recovers
- a stale command becomes unsafe before execution

## Implementation Strategy

Do not start by patching random `if` statements into multiple files.

Implement in this order:

1. define the shared validator behavior and test matrix
2. harden Home Assistant UX guardrails
3. harden server command acceptance
4. harden ESP32 final execution gate
5. verify delayed/stale command scenarios

## Phase 0: Safety Baseline Before Edits

### 0.1 Confirm current behavior

Before changing code, verify and document the current gaps:

- enabling `mains_charge` with overlap is blocked
- enabling `discharge` with overlap is not blocked
- changing `start/end` while a slot is enabled is not blocked
- server accepts overlapping schedule commands
- firmware does not reject overlapping schedule writes before BLE write

### 0.2 Create a checkpoint commit

Before starting the implementation:

- commit current private runtime state
- push to `private`

This work touches safety-critical write behavior and must be easy to roll back.

## Phase 1: Shared Rule Model And Test Matrix

### 1.1 Define normalized schedule state

Create a canonical schedule model for validation:

```text
schedule = {
  "mains_charge": {
    1: {"enabled": bool, "start": int, "end": int},
    2: {"enabled": bool, "start": int, "end": int},
  },
  "discharge": {
    1: {"enabled": bool, "start": int, "end": int},
    2: {"enabled": bool, "start": int, "end": int},
    3: {"enabled": bool, "start": int, "end": int},
    4: {"enabled": bool, "start": int, "end": int},
  },
}
```

### 1.2 Define pure helper operations

At minimum, the implementation needs pure helpers for:

- `hhmm -> minutes`
- overnight window expansion
- overlap detection
- loading a schedule snapshot into normalized state
- applying one proposed change to a copy of state
- validating the resulting state

### 1.3 Define conflict payload

The validator should return structured conflicts, not only `True/False`.

Suggested shape:

```text
{
  "charge_slot": 1,
  "charge_start": 100,
  "charge_end": 300,
  "discharge_slot": 3,
  "discharge_start": 230,
  "discharge_end": 400
}
```

### 1.4 Build the test matrix first

Before rolling changes across runtime layers, add tests for:

1. enable charge with no overlap -> allow
2. enable charge with overlap -> reject
3. enable discharge with no overlap -> allow
4. enable discharge with overlap -> reject
5. edit charge start while slot enabled -> reject
6. edit charge end while slot enabled -> reject
7. edit discharge start while slot enabled -> reject
8. edit discharge end while slot enabled -> reject
9. disabled-slot time edit leading to overlap with an enabled opposite-side slot -> reject
10. overnight overlap -> reject
11. overnight non-overlap -> allow
12. stale queued command unsafe at execution time -> firmware reject
13. stale settings snapshot older than threshold -> server reject
14. local firmware reject while result upload is unavailable -> no BLE write and lifecycle converges later

## Phase 2: Home Assistant Guardrails

### 2.1 Move overlap logic out of `switch.py`

Current one-direction logic lives in:

- `custom_components/lumentreelocal/switch.py`

Refactor this into a shared validator helper module inside:

- `custom_components/lumentreelocal/`

That helper should be reusable by:

- switch entities
- time entities
- future schedule-related entity types

### 2.2 Add symmetric enable validation

When enabling a schedule slot:

- if `mains_charge` is being enabled, validate against enabled discharge slots
- if `discharge` is being enabled, validate against enabled charge slots

### 2.3 Add enabled-slot time edit rejection

When writing:

- `mains_charge_slot_X_start_time`
- `mains_charge_slot_X_end_time`
- `discharge_slot_X_start_time`
- `discharge_slot_X_end_time`

Home Assistant must reject the write if that same slot is currently enabled.

User-facing message should instruct:

- turn the slot off
- change time
- turn the slot on again

### 2.4 Add next-state validation for disabled-slot time edits

Even when the edited slot is currently disabled:

- build the hypothetical next state
- validate the full schedule
- reject if the resulting state is unsafe

Document explicitly:

- this is a conservative phase 1 UX rule
- it is stricter than the minimum physical-safety invariant
- it is chosen intentionally to avoid pre-staging future-conflicting schedules

### 2.5 Keep number entities outside overlap validator scope

Do not apply overlap rejection to:

- target SOC writes
- discharge power writes

unless later product policy explicitly broadens the scope.

### 2.6 Add HA-level tests

Add unit tests for:

- symmetric enable guard
- enabled-slot time edit rejection
- disabled-slot next-state conflict rejection
- overnight overlap handling

## Phase 3: Server Command Acceptance Gate

### 3.1 Add server-side schedule validator module

Inside the local server code:

- create server-side helpers for normalized schedule state
- build a function that loads the current schedule snapshot from latest settings
- build a function that applies a proposed schedule command in-memory
- validate the resulting next state

This validator must not depend on Home Assistant.

### 3.2 Load schedule from current settings snapshot

When `create_command()` receives one of these commands:

- `set_mains_charge_time_enable`
- `set_mains_charge_time_start`
- `set_mains_charge_time_end`
- `set_discharge_time_enable`
- `set_discharge_time_start`
- `set_discharge_time_end`

the server must:

1. fetch the current latest settings snapshot for the device
2. build normalized state
3. apply the candidate change
4. validate the next state

Also enforce snapshot freshness:

- if the latest settings snapshot is older than the configured threshold
- reject schedule-affecting commands instead of validating against stale data

Suggested phase 1 threshold:

- 5 minutes

### 3.3 Reject time edits on enabled slots

For the four time-edit commands:

- if the target slot is currently enabled in settings, reject immediately

Suggested server errors:

- `"turn off mains charge slot 1 before changing its time window"`
- `"turn off discharge slot 2 before changing its time window"`

### 3.4 Reject overlap before queueing

If the next state is unsafe:

- do not insert the command into `lumentree_commands`
- return a clear `400` error

### 3.4a Document phase 1 queue-order limitation

If pending schedule commands are not yet folded into validation:

- document clearly that server validates against current persisted settings
- not against a simulated future state including all queued earlier commands

This is acceptable only because firmware remains the final execution gate.

### 3.5 Preserve non-schedule write behavior

Do not regress existing allowed writes:

- target SOC
- discharge power

They still need type/range validation, but not schedule overlap validation.

### 3.6 Add server tests

Add tests covering:

- charge enable rejected by overlapping discharge
- discharge enable rejected by overlapping charge
- time edit on enabled slot rejected
- disabled-slot time edit that creates overlap rejected
- non-overlap schedule commands accepted
- non-schedule writes still accepted

## Phase 4: ESP32 Final Execution Gate

### 4.1 Build firmware-side schedule state loader

Before executing any schedule-affecting write command, firmware must read the
registers needed to reconstruct:

- charge slot enabled/start/end for slots 1..2
- discharge slot enabled/start/end for slots 1..4

Important:

- the pre-read used for safety validation must happen immediately before the
  final write decision
- do not leave a broad TOCTOU gap between schedule validation and BLE write

### 4.2 Build firmware-side normalized schedule state

Implement firmware helpers for:

- HHMM validity
- overnight expansion
- overlap detection
- applying candidate schedule command to current state
- validating the next state

### 4.3 Reject time edits on enabled slots in firmware

If firmware receives:

- `set_mains_charge_time_start`
- `set_mains_charge_time_end`
- `set_discharge_time_start`
- `set_discharge_time_end`

and the target slot is currently enabled according to the pre-read state:

- reject the command
- do not send BLE write

### 4.4 Reject overlap before BLE write

For all six schedule-affecting commands:

- compute next state
- if overlap exists, reject before any Modbus write

Required:

- `ble_write = false`
- `modbus_write = false`
- command result marked rejected

### 4.5 Upload explicit rejected results

Command results should clearly communicate:

- rejection happened before BLE write
- exact reason
- conflict details if practical

Suggested result conventions:

- `status = "rejected"`
- `safety = "schedule_conflict_rejected_before_ble_write"`
- human-readable `error`

### 4.5a Handle reject-result upload failure

If firmware rejects locally but cannot upload the rejected result:

- persist the rejected result locally for retry
- do not execute the command
- ensure repeated delivery of the same command still re-rejects safely

Also define lifecycle rules on the server side:

- max delivery attempts
- schedule-command TTL
- timeout or expiry status when terminal result upload never arrives

### 4.6 Add firmware test strategy

If full automated firmware tests are impractical, at minimum add a reproducible
manual verification checklist for:

- enable overlap rejection
- time-edit-on-enabled-slot rejection
- delayed command stale-conflict rejection

## Phase 5: Stale Queue / Recovery Safety

### 5.1 Simulate delayed command scenario

Test this explicitly:

1. queue a schedule command
2. prevent immediate delivery
3. change schedule context so the old command becomes unsafe
4. allow delivery
5. confirm firmware rejects before BLE write

This is one of the highest-risk scenarios in the whole system.

### 5.1a Simulate queue-order conflict

Explicitly test:

1. queue schedule command A that is valid on its own
2. queue schedule command B that is also valid against current persisted settings
3. allow A to execute first so B becomes unsafe
4. confirm firmware rejects B before BLE write

### 5.2 Confirm no unsafe replay path remains

Verify that:

- Home Assistant rejection is not the only protection
- server rejection is not the only protection
- firmware rejection still happens when context changed after queue creation

## Phase 6: Optional Hardening

These are optional after the required safety baseline works.

### 6.1 Schedule revision metadata

Add optional metadata to schedule commands:

- source settings observed_at
- schedule revision hash

This can improve diagnostics for stale-command rejection.

### 6.1a Command lifecycle convergence

Add stronger lifecycle handling so schedule commands do not remain ambiguous:

- explicit expired status
- explicit abandoned status
- bounded retry policy
- clear operator-visible reason when a command was never safely applied

### 6.2 Atomic schedule update commands

Longer-term improvement:

- add commands that update full slot state atomically:
  - enabled
  - start
  - end

This could reduce partial-edit friction.

This is not required for the first safe rollout because the chosen UX rule is:

- OFF first
- edit time
- ON again

### 6.3 Better user-facing diagnostics

Potential improvements:

- explicit conflicting slot names in HA toast/errors
- richer command safety sensor attributes
- clearer rejected-command history in diagnostics
- warning that OFF-before-edit can temporarily reduce schedule coverage if the
  user does not complete the ON step

## Rollout Order

Recommended order:

1. checkpoint commit and push
2. add shared validator tests
3. implement HA guardrails
4. implement server validation
5. implement firmware final gate
6. run delayed-command recovery test
7. only then consider public/user-facing documentation updates

Do not roll out firmware-side schedule writes without the final firmware gate.

Do not consider the feature safe if only Home Assistant checks exist.

## Definition Of Done

This work is done only when all of the following are true:

- Home Assistant rejects unsafe enable operations in both directions
- Home Assistant rejects time edits on enabled slots
- server rejects unsafe schedule commands before queueing
- server rejects schedule commands when settings snapshot is too stale
- firmware re-validates current state before BLE write
- stale queued commands are rejected if they became unsafe
- command lifecycle converges even if rejected-result upload is delayed
- manual or automated verification confirms that no overlapping active
  charge/discharge schedule can be written through any normal supported path
