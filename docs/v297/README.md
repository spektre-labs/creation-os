# v297 — σ-Clock

**Time-invariant code.**

The Giza pyramid does not know what year it is. It just stands.
Code that does not depend on absolute time lasts longer: no
expiry field, no wallclock in the hot path, no epoch-dependent
log, no protocol version stamped on the struct. v297 types four
v0 manifests for "the kernel works the same in 2026 and 2126".

## σ innovation (what σ adds here)

> **The σ-gate does not read a calendar.** v297 names the four
> predicates that keep the gate decisions identical across
> decades: no expiry, monotonic-only time, epoch-free log,
> version-free protocol.

## v0 manifests

Enumerated in [`src/v297/clock.h`](../../src/v297/clock.h);
pinned by
[`benchmarks/v297/check_v297_clock_time_invariant.sh`](../../benchmarks/v297/check_v297_clock_time_invariant.sh).

### 1. No expiry (exactly 3 canonical forbidden fields)

| field                | present_in_kernel | forbidden |
|----------------------|-------------------|-----------|
| `hardcoded_date`     | `false`           | `true`    |
| `valid_until_2030`   | `false`           | `true`    |
| `api_version_expiry` | `false`           | `true`    |

Every row forbidden AND absent — the kernel never reads a
calendar.

### 2. Monotonic-time only (exactly 3 canonical time sources)

| source            | verdict | in_kernel |
|-------------------|---------|-----------|
| `CLOCK_MONOTONIC` | ALLOW   | `true`    |
| `CLOCK_REALTIME`  | FORBID  | `false`   |
| `wallclock_local` | FORBID  | `false`   |

Exactly 1 ALLOW (monotonic) AND exactly 2 FORBID (wallclock
sources) — the kernel only asks "has more time passed than X?",
never "what time is it?".

### 3. Epoch-free logging (exactly 3 canonical properties)

| property             | holds  |
|----------------------|--------|
| `relative_sequence`  | `true` |
| `unix_epoch_absent`  | `true` |
| `y2038_safe`         | `true` |

σ-log stores causal order "A before B", not Unix
seconds-since-1970.

### 4. Version-free protocol (exactly 3 canonical forward-compat properties)

| property                        | holds  |
|---------------------------------|--------|
| `no_version_field_on_struct`    | `true` |
| `old_reader_ignores_new_fields` | `true` |
| `append_only_field_semantics`   | `true` |

Same `sigma_measurement_t` struct works in v1 and v1000; new
fields append, old readers ignore them.

### σ_clock (surface hygiene)

```
σ_clock = 1 −
  (expiry_rows_ok + expiry_all_forbidden_ok +
   expiry_all_absent_ok +
   time_rows_ok + time_allow_count_ok + time_forbid_count_ok +
   log_rows_ok + log_all_hold_ok +
   proto_rows_ok + proto_all_hold_ok) /
  (3 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 3 + 1)
```

v0 requires `σ_clock == 0.0`.

## Merge-gate contracts

- 3 expiry rows; all forbidden AND all absent.
- 3 time sources; exactly 1 ALLOW (CLOCK_MONOTONIC);
  exactly 2 FORBID.
- 3 logging properties; all hold.
- 3 protocol-compat properties; all hold.
- `σ_clock ∈ [0, 1] AND == 0.0`.
- FNV-1a chain replays byte-identically.

## v0 vs vN.1 (claim discipline)

- **v0 (this kernel)** names the expiry / time-source / log /
  protocol contracts as predicates.
- **v297.1 (named, not implemented)** is a `cos audit --time`
  pass scanning source for `CLOCK_REALTIME`, hard-coded years,
  and `struct { int version; ... }` on the wire, and a CI job
  that runs the whole gate matrix with the host clock set to
  2126 to prove the kernel does not read the calendar.

See [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).
