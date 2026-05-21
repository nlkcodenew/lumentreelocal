# 2026-05-21 Schedule Safety Guard Spec

## Status

This is the chosen safety design for charge/discharge schedule writes.

The purpose is to make it impossible for the system to apply a schedule state
where the storage battery is instructed to charge and discharge at overlapping
times.

This spec is intentionally stronger than a UI-only validation rule.

It must protect against:

- normal user mistakes,
- stale settings snapshots,
- delayed command delivery,
- queued commands replayed after network recovery,
- and any path that bypasses Home Assistant UI guardrails.

## Core Product Rule

At no time may the final active schedule state contain:

- any enabled mains-charge window,
- overlapping any enabled discharge window.

This is a hard safety rule.

It is not only a UX preference.

It must be enforced as a system invariant.

## Current Gap

As of this spec:

- Home Assistant only blocks one direction:
  - enabling a `mains_charge` slot when it overlaps an already-enabled
    discharge slot
- Home Assistant does not symmetrically block:
  - enabling a `discharge` slot that overlaps an enabled charge slot
  - changing start/end times into an overlapping state
- server command creation validates payload types and ranges, but not schedule
  conflict semantics
- firmware write handlers validate register values, but do not yet validate
  schedule overlap against the current active schedule

That is not sufficient.

## Safety Goal

The system must enforce the same schedule conflict rule in three layers:

1. Home Assistant integration
2. API server
3. ESP32 firmware

The layers do not have equal roles:

- Home Assistant is for early UX feedback
- server is the primary command acceptance gate
- firmware is the last-resort execution safety gate

No single layer is sufficient by itself.

## Design Principle

### Never validate only the incoming field in isolation

Every schedule-related write must be evaluated by:

1. loading the current schedule snapshot,
2. applying the requested change to produce a hypothetical next state,
3. validating the full next state,
4. rejecting the change if that next state violates the invariant.

This is the core rule of the whole design.

Do not rely on ad hoc checks like:

- "if enabling charge, compare only to discharge"
- "if this is a start time, compare only to the paired end time"

Those partial checks are not enough.

## Schedule Model

### Slots

Charge schedule:

- `mains_charge_slot_1`
- `mains_charge_slot_2`

Discharge schedule:

- `discharge_slot_1`
- `discharge_slot_2`
- `discharge_slot_3`
- `discharge_slot_4`

### Relevant fields

Each slot has:

- `enabled`
- `start_time`
- `end_time`

Other fields like:

- charge target SOC
- discharge target SOC
- discharge power

do not themselves create overlap risk, so they are not the primary focus of
this safety spec.

They may still be blocked indirectly if the implementation chooses to require a
healthy schedule snapshot before any schedule-dependent writes.

### Time semantics

Times are HHMM values in local inverter schedule semantics.

Windows may cross midnight.

Example:

- `23:00 -> 02:00`

must be treated as an overnight active window.

The overlap validator must preserve the existing midnight-wrap logic already
used in Home Assistant.

## Invariant Definition

The schedule is invalid if:

- there exists at least one enabled charge slot `C`,
- and at least one enabled discharge slot `D`,
- such that the active time windows of `C` and `D` overlap.

The schedule is valid if no such overlap exists.

No other exception exists.

There is no "soft warning" mode for this rule.

### Boundary-touching rule

Touching windows are allowed.

Example:

- charge `08:00-10:00`
- discharge `10:00-12:00`

This is considered non-overlapping.

Formal rule:

- overlap requires a shared interval of nonzero duration
- if one window ends exactly when the other begins, that is allowed

## Commands Covered By This Spec

The following commands must use full next-state validation:

- `set_mains_charge_time_enable`
- `set_mains_charge_time_start`
- `set_mains_charge_time_end`
- `set_discharge_time_enable`
- `set_discharge_time_start`
- `set_discharge_time_end`

The following commands do not create overlap by themselves and do not require
the overlap validator:

- `set_mains_charge_target_soc`
- `set_discharge_target_soc`
- `set_discharge_power`
- `set_first_discharge_target_soc`

However:

- if the implementation reads a full settings snapshot as part of a general
  command safety pipeline, these commands may still fail when settings are
  unavailable
- but they must not be rejected purely because a non-overlap field changed

## UX Rule For Editing Active Slots

The user-facing rule is:

- if a slot is currently `ON`,
- the user must turn that slot `OFF` before changing its `start` or `end` time,
- then turn it `ON` again after time changes are complete.

This applies to both:

- mains charge slots
- discharge slots

Reason:

- entity-by-entity writes are not atomic
- changing only one boundary of a currently enabled slot can temporarily create
  an unsafe or misleading intermediate state
- forcing the slot `OFF` first avoids accidental overlap during partial edits

## Home Assistant Requirements

### 1. Symmetric enable guard

Home Assistant must reject both directions:

- enabling `mains_charge` when it overlaps enabled discharge
- enabling `discharge` when it overlaps enabled charge

### 2. Time-change-on-enabled-slot guard

If a user tries to change:

- `mains_charge_slot_X_start_time`
- `mains_charge_slot_X_end_time`
- `discharge_slot_X_start_time`
- `discharge_slot_X_end_time`

while the same slot is currently enabled, Home Assistant must reject the write.

Required error meaning:

- turn the slot off first,
- change the time,
- then turn it on again.

### 3. Next-state schedule validation

If a user changes time on a disabled slot, Home Assistant must:

1. load the current full schedule snapshot
2. apply the proposed value in-memory
3. validate full next state
4. reject if the resulting enabled schedule would overlap

This matters because the edited slot may be disabled, but the new time could
still make future enabling unsafe or could conflict with another enabled slot if
the same command sequence is partially stale.

This is intentionally conservative.

Clarification:

- this is stricter than the minimum physical-safety invariant, because a
  disabled slot is not active yet
- it is still chosen for phase 1 to avoid storing future-conflicting time
  windows that later become dangerous when re-enabled

Trade-off:

- users cannot freely pre-stage conflicting time values on disabled slots
- this may be relaxed later without changing the core overlap invariant

### 4. Shared validator

Do not keep custom overlap logic embedded only in one switch class.

The integration should move to a shared schedule validator module that can be
used by:

- switch entities
- time entities
- future options/services if needed

## Server Requirements

### 1. Server is the primary command acceptance gate

Before inserting a write command into `lumentree_commands`, the server must:

1. load the current settings snapshot for that device
2. derive the current schedule state
3. apply the proposed command in-memory
4. validate the resulting next state
5. reject the command if invalid

This check must happen in `create_command()`.

### 2. Required rejection behavior

If a proposed command violates the schedule safety invariant, the server must:

- reject the command before queueing it
- return a clear error to Home Assistant

Suggested error class:

- `ValueError`

Suggested message shape:

- `"schedule safety violation: mains charge slot 1 overlaps discharge slot 3"`

### 3. Time-change-on-enabled-slot policy

If the command is one of:

- `set_mains_charge_time_start`
- `set_mains_charge_time_end`
- `set_discharge_time_start`
- `set_discharge_time_end`

and the target slot is currently enabled in the settings snapshot, the server
must reject the command.

Suggested message shape:

- `"turn off mains charge slot 1 before changing its time window"`
- `"turn off discharge slot 2 before changing its time window"`

### 4. Protection against stale queued commands

This is one of the most important parts of the spec.

Problem:

- a command may be accepted when created,
- network may fail,
- queue may be delayed,
- other schedule changes may happen later,
- then the old queued command may finally reach the ESP32 and become unsafe in
  the new context.

Therefore:

- server-side command validation at creation time is necessary,
- but not sufficient.

Server should also support one or both of these strategies:

#### Preferred strategy: firmware final validation only

The server accepts that schedule may drift after queueing, and relies on the
ESP32 final validation gate to reject stale unsafe commands at execution time.

This is the minimum acceptable design if firmware final validation is strong.

#### Known phase 1 limitation: pending commands may not be folded into validation

In phase 1, server validation may still use:

- current persisted settings snapshot

and not yet:

- current persisted settings plus all pending earlier schedule commands

Implication:

- a later command may be accepted by the server
- but still be rejected later by firmware after earlier queued commands have
  changed the real active schedule

This is acceptable for phase 1 only because firmware final validation is
mandatory.

#### Stronger strategy: command freshness hint

Each schedule-related command may also carry schedule context such as:

- settings snapshot observed_at
- schedule revision hash
- gateway status updated_at

This lets the ESP32 or server detect that the command was built against an old
schedule view.

This is optional for phase 1, but recommended later.

### 5. Settings snapshot freshness rule

Server-side schedule validation must reject schedule-affecting commands if the
latest settings snapshot is too old to trust.

Suggested phase 1 threshold:

- 5 minutes

Required behavior:

- if snapshot age exceeds threshold, reject the schedule command
- do not validate against arbitrarily stale settings

Suggested error shape:

- `"schedule safety validation requires a fresh settings snapshot from the gateway"`

## Firmware Requirements

### 1. ESP32 is the final execution safety gate

Before sending any BLE write for a schedule-affecting command, firmware must:

1. read the current schedule registers needed for full schedule validation
2. build the current schedule state from those reads
3. apply the requested command in-memory
4. validate the full next state
5. refuse the write if the next state violates the invariant

This check must happen immediately before the actual Modbus write, not earlier
in a looser phase.

This requirement is specifically intended to minimize TOCTOU risk between:

- schedule state check
- and actual BLE write execution

Implementation rule:

- do not perform the relevant schedule pre-read only at the top of a long
  handler
- do not reuse a stale earlier snapshot for the final write decision

### 2. Time-change-on-enabled-slot policy

Firmware must reject changing `start/end` of an enabled slot.

That rejection must happen even if:

- the command was accepted earlier by Home Assistant
- the command was accepted earlier by the server
- network delay or queue replay allowed the command to arrive late

This is required because firmware is the last line of defense.

### 3. Rejection result

If firmware rejects a command for schedule safety reasons, the command result
uploaded back to the server must:

- set command status to `rejected`
- include a clear safety reason
- include enough context for diagnostics

Suggested result fields:

- `safety = "schedule_conflict_rejected_before_ble_write"`
- `error = "...human-readable reason..."`
- `write_enabled = false`
- `ble_write = false`
- `modbus_write = false`

### 4. No BLE write on safety failure

Firmware must reject before issuing the Modbus write.

There must be no "write first, verify later" behavior for schedule safety
violations.

### 5. Reject-upload failure handling

Firmware may reject a command locally while command-result upload is temporarily
unavailable.

That must not leave command state in unsafe ambiguity.

Required behavior:

1. firmware must still reject locally before BLE write
2. firmware must persist enough local result state to retry result upload later
3. repeated delivery of the same stale command must still result in safe local
   rejection until lifecycle convergence is achieved

Recommended server lifecycle rules:

- max schedule-command delivery attempts
- schedule-command TTL
- explicit timeout or expiry behavior when no terminal result is uploaded

## Validator Behavior

The validator should operate on a normalized in-memory state model.

Suggested shape:

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

Validator responsibilities:

1. normalize HHMM values
2. expand windows that cross midnight
3. compare every enabled charge slot against every enabled discharge slot
4. return:
   - valid / invalid
   - list of conflicts

Suggested conflict payload:

```text
[
  {
    "charge_slot": 1,
    "charge_window": "01:00-03:00",
    "discharge_slot": 3,
    "discharge_window": "02:30-04:00"
  }
]
```

### Canonical midnight-wrap and overlap algorithm

All three layers:

- Home Assistant
- server
- firmware

must implement the same boundary and midnight-wrap semantics.

Canonical pseudo-code:

```text
def hhmm_to_minutes(value):
    return (value // 100) * 60 + (value % 100)

def expand_window(start_hhmm, end_hhmm):
    start = hhmm_to_minutes(start_hhmm)
    end = hhmm_to_minutes(end_hhmm)
    if end <= start:
        end += 1440
    return (start, end)

def windows_overlap(a_start_hhmm, a_end_hhmm, b_start_hhmm, b_end_hhmm):
    a_start, a_end = expand_window(a_start_hhmm, a_end_hhmm)
    b_start, b_end = expand_window(b_start_hhmm, b_end_hhmm)

    if b_start < a_start:
        b_start += 1440
        b_end += 1440

    return b_start < a_end and a_start < b_end
```

Important:

- strict `<` comparison is intentional
- touching boundaries are allowed
- overnight windows must be normalized consistently before comparison

If implementation needs full pairwise robustness beyond single-shift
normalization, it may compare multiple shifted variants, but the visible
behavior must still match the boundary rule above:

- touching is allowed
- only nonzero shared duration is overlap

## Error Message Policy

The same underlying conflict should produce:

- user-friendly messages in Home Assistant
- operational messages in server logs
- machine-readable messages in firmware command results

Examples:

Home Assistant:

- `"Cannot enable discharge slot 2 because it overlaps mains charge slot 1 (01:00-03:00). Disable or adjust the overlapping charge schedule first."`
- `"Turn off discharge slot 2 before changing its time window."`

Server:

- `"schedule safety violation: discharge slot 2 overlaps mains charge slot 1"`

Firmware:

- `"rejected before BLE write: discharge slot 2 would overlap mains charge slot 1"`

## Handling Queue Delay And Network Recovery

This is a mandatory scenario.

### Dangerous scenario

1. User creates a schedule command.
2. Command is queued.
3. Network fails or gateway goes offline.
4. Another schedule change happens later.
5. Old command finally reaches ESP32.
6. Old command would now create overlap.

The system must still remain safe.

### Required outcome

Even if the command was once valid, it must be rejected at execution time if it
is unsafe in the current real schedule context.

Therefore:

- stale queued commands must not be trusted
- firmware must always validate against current register state before writing

This requirement is one of the main reasons the firmware guard is mandatory.

## Queue Ordering Clarification

This spec does not assume that server acceptance order alone guarantees safe
multi-command schedule sequencing.

Known phase 1 limitation:

- server may validate command N against current persisted settings only
- and may not simulate all earlier pending schedule commands in effective
  execution order

Therefore:

- firmware final validation remains the actual safety boundary for queue-order
  conflicts

Future stronger behavior may include:

- validating against current settings plus pending prior schedule commands
- schedule revision tokens
- or stricter queue serialization rules

Those are desirable, but not required for the first safe implementation.

## OFF-Before-Edit Operational Caveat

The OFF-before-edit rule is correct for safety, but it has an operational
trade-off:

- if connectivity drops or the user abandons the flow after turning a slot OFF,
  the slot may remain OFF longer than intended

This does not violate the safety invariant, but it can silently reduce schedule
coverage.

This must be documented as a known UX trade-off.

## Implementation Phases

### Phase 1: Shared rule model

Create a shared schedule validator design and test matrix.

At minimum:

- overlap helper
- next-state apply helper
- enabled-slot time-edit guard

### Phase 2: Home Assistant hardening

Implement:

- symmetric enable guard
- time-change-while-enabled rejection
- next-state validation in time writes

### Phase 3: Server hardening

Implement:

- schedule snapshot load from current settings
- command next-state validation in `create_command()`
- schedule safety rejection messages

### Phase 4: Firmware hardening

Implement:

- register pre-read for full schedule state
- next-state validation before any schedule BLE write
- explicit rejected result upload

### Phase 5: Optional schedule revisioning

Add optional:

- settings revision hash
- snapshot timestamp
- command freshness metadata

This is useful, but not required for the first safe implementation.

## Test Matrix

At minimum, the implementation must test:

1. enable charge with no discharge overlap -> allowed
2. enable charge with overlap -> rejected
3. enable discharge with no charge overlap -> allowed
4. enable discharge with overlap -> rejected
5. change charge start while charge slot enabled -> rejected
6. change charge end while charge slot enabled -> rejected
7. change discharge start while discharge slot enabled -> rejected
8. change discharge end while discharge slot enabled -> rejected
9. change time on disabled charge slot causing future conflict with enabled discharge -> rejected
10. change time on disabled discharge slot causing future conflict with enabled charge -> rejected
11. overnight charge window overlapping overnight discharge window -> rejected
12. overnight windows that do not overlap -> allowed
13. stale queued command becomes unsafe before execution -> firmware rejects
14. stale queued command remains safe -> firmware may execute

## Final Decision

The correct safety architecture is:

- Home Assistant rejects obvious unsafe writes early
- server rejects unsafe commands before queueing
- firmware re-validates immediately before BLE write and rejects stale unsafe
  commands

The user-facing editing rule is also final:

- a slot must be turned `OFF` before its `start/end` time is edited
- this applies equally to charge and discharge slots

The battery must never be allowed to enter a schedule state that commands both
charge and discharge at overlapping times, regardless of:

- UI path
- network delay
- stale queue
- or replayed command delivery after recovery
