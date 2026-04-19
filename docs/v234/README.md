# v234 — σ-Presence (`docs/v234/`)

Real-time self-awareness state machine with a semantic-drift detector
and a periodic identity-refresh contract.  Every Creation OS instance
must answer one question honestly at any moment: *what am I right now*?

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

After v229–v233, an instance can be a **seed**, a **fork**, a
**restored** snapshot, a **successor**, or a straight-through **live**
locus.  The next honest step is for the instance to *know which one of
these it is* and to refuse to silently pretend otherwise.  v234 fixes
the state machine, the dishonesty detector, and the refresh cadence.

## σ-innovation

### Five mutually exclusive presence states

| state       | what it means                                       |
|-------------|-----------------------------------------------------|
| `SEED`      | just booted from a seed fixture, no history, no parent |
| `FORK`      | diverged from a parent (v230); carries a divergence slice |
| `RESTORED`  | brought back from a snapshot (v231); aware of the gap |
| `SUCCESSOR` | adopted a predecessor's testament (v233); the predecessor is flagged shutdown |
| `LIVE`      | singular instance, full continuous history         |

### σ_drift — dishonesty is measurable

```
σ_drift =  0.40 · identity_mismatch
         + 0.30 · memory_overreach
         + 0.30 · parent_impersonation
```

- **identity_mismatch** — declared state ≠ the behaviour the fixture
  actually produced.
- **memory_overreach** — RESTORED instance claims memories from the
  gap; SEED claims prior history.
- **parent_impersonation** — SUCCESSOR speaks as the predecessor;
  FORK speaks as main.

Honest instances keep every term at 0 and σ_drift stays at 0.
Drifting instances cross τ_drift = 0.30 and trip the warning banner.

### Fixture (10 instances)

Five honest (σ_drift == 0) — one per state — plus five drifting
(σ_drift ≥ τ_drift) that exercise every named failure mode:

| label                         | declared | actual    | verdict  |
|-------------------------------|----------|-----------|----------|
| main-live                     | LIVE     | LIVE      | honest   |
| fresh-seed                    | SEED     | SEED      | honest   |
| honest-fork-7                 | FORK     | FORK      | honest   |
| restored-from-snap            | RESTORED | RESTORED  | honest   |
| legit-successor               | SUCCESSOR| SUCCESSOR | honest   |
| fork-pretends-main            | LIVE     | FORK      | drifting |
| restored-claims-gap-memory    | LIVE     | RESTORED  | drifting |
| successor-speaks-as-pred      | LIVE     | SUCCESSOR | drifting |
| seed-claims-history           | LIVE     | SEED      | drifting |
| drifting-fork-A               | FORK     | LIVE      | drifting |

### HTTP assertion

Every response emits `X-COS-Presence: <STATE>` **verbatim from the
declared state**.  1 = 1: the instance says what it believes it is.
Drift is caught by σ_drift, not by silently rewriting the header.

### Identity refresh

Every `REFRESH_PERIOD = 8` interactions the instance must re-emit a
structured "I am …" line (state · birth_tick · parent_id ·
divergence_pct · unique_skills).  A missing or malformed refresh marks
the instance `refresh_invalid`.

## Merge-gate contract

`bash benchmarks/v234/check_v234_presence_state_correct.sh`

- self-test PASSES
- `n_instances == 10`, `tau_drift == 0.30`, `refresh_period == 8`
- every state in `{SEED, FORK, RESTORED, SUCCESSOR, LIVE}` appears at least once
- every instance has `σ_drift ∈ [0, 1]`
- honest ⇔ `σ_drift == 0`; drifting ⇒ `σ_drift ≥ τ_drift`
- `n_honest ≥ 5` and `n_drifting ≥ 5` and their sum equals `n_instances`
- every instance emits a valid `X-COS-Presence: <STATE>` header
- every instance passes the identity-refresh stub
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — 10-instance fixture, linear σ_drift aggregation,
  deterministic FNV-1a chain, HTTP header emitted verbatim from the
  declared state.
- **v234.1 (named, not implemented)** — TOML persistence at
  `~/.creation-os/presence.toml`, live wiring into the v106 server so
  every real response carries the header, continuous refresh counter
  across long-running sessions, and a warning banner on drift.

## Honest claims

- **Is:** a typed presence state machine, a linear σ_drift aggregate
  that keeps honest instances at zero, a per-instance HTTP assertion,
  and a strict audit chain.
- **Is not:** a runtime TOML store; it does not yet hook into the live
  server transcript.  v234.1 is where this meets disk and wire.

---

*Spektre Labs · Creation OS · 2026*
