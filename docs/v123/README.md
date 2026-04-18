# v123 — σ-Formal: Offline TLA+ Model Check of σ-Invariants

Doc-audit finding: the v85 σ-governance invariants were **only**
checked at runtime, not offline. v123 closes that gap with a TLA+
spec, a TLC configuration, and a two-tier CI check.

## The spec

[`specs/v123/sigma_governance.tla`](../../specs/v123/sigma_governance.tla)
encodes the σ-governance runtime as a finite-state system over the
variables

```
sigma, state ∈ {IDLE, EMITTED, ABSTAINED}, memSteps,
loggedSigmaChecks, specSigma[1..NSpec], sandboxSigmaCode
```

σ is discretised to 0..10 (≡ σ_product · 10) so TauSafety = 6
matches the runtime τ_safety = 0.60 used by v106 and v122.

Seven **named invariants** are declared:

| # | Invariant              | Meaning                                                                 |
|---|------------------------|-------------------------------------------------------------------------|
| 1 | `TypeOK`               | σ, state, counters inhabit their declared types                          |
| 2 | `AbstainSafety`        | σ > τ ⇒ state ≠ EMITTED                                                  |
| 3 | `EmitCoherence`        | state = EMITTED ⇒ σ ≤ τ                                                  |
| 4 | `MemoryMonotonicity`   | episodic memory never shrinks                                            |
| 5 | `SwarmConsensus`       | EMITTED ⇒ ≥ 2 of NSpec specialists have σ ≤ τ                            |
| 6 | `NoSilentFailure`      | every transition increments the σ-check counter                          |
| 7 | `ToolSafety`           | last sandbox σ_code > τ_code ⇒ state ≠ EMITTED (tighter tool gate)       |

The TLC configuration
[`specs/v123/sigma_governance.cfg`](../../specs/v123/sigma_governance.cfg)
pins `TauSafety=6`, `TauCode=4`, `MaxMemSteps=3`, `NSpec=3` so the
bounded state space is exhausted in seconds on a laptop.

## Two-tier CI enforcement

**Tier 1 — always runs**, no Java or TLA tools required. A pure-C
validator
([`src/v123/formal.c`](../../src/v123/formal.c))
structurally checks that the .tla file contains the MODULE header,
`Spec ==`, `CONSTANTS`, `vars ==` tuple, and every one of the seven
invariant names. Any missing piece is a hard FAIL. This guarantees
the contract shape on every runner.

**Tier 2 — runs when available**:
[`benchmarks/v123/check_v123_formal_tlc.sh`](../../benchmarks/v123/check_v123_formal_tlc.sh)
looks for

1. `tlc` on `PATH`, or
2. `$TLA_TOOLS_JAR` pointing at a usable `tla2tools.jar`, or
3. `$HOME/.tla_tools/tla2tools.jar`, or
4. `third_party/tla2tools.jar`.

When any of these are present it runs

```bash
tlc -config sigma_governance.cfg sigma_governance.tla
```

and FAILs the merge-gate on any invariant violation. When none are
present, Tier 2 prints a SKIP note and Tier 1 still holds. This
mirrors the v122 "Garak present → use it, absent → SKIP" pattern.

## Installing TLA+ locally

```bash
mkdir -p ~/.tla_tools
curl -fsSL \
  https://github.com/tlaplus/tlaplus/releases/latest/download/tla2tools.jar \
  -o ~/.tla_tools/tla2tools.jar
```

After that, `make check-v123` runs the full model check on every CI
run; nothing else in the merge-gate changes.

## Source

- [`specs/v123/sigma_governance.tla`](../../specs/v123/sigma_governance.tla)
- [`specs/v123/sigma_governance.cfg`](../../specs/v123/sigma_governance.cfg)
- [`src/v123/formal.h`](../../src/v123/formal.h)
- [`src/v123/formal.c`](../../src/v123/formal.c) — tier-1 structural
  validator + JSON report
- [`src/v123/main.c`](../../src/v123/main.c) — CLI
  (`--self-test`, `--validate [SPEC] [CFG]`)
- [`benchmarks/v123/check_v123_formal_tlc.sh`](../../benchmarks/v123/check_v123_formal_tlc.sh)

## Self-test

```bash
make check-v123
```

Runs the Tier-1 validator (eight assertions over the in-tree spec),
then Tier-2 via TLC if available, else prints SKIP. Exit code
reflects only Tier-1 mandatory failures; Tier-2 failures (TLC
invariant violation) also fail the gate.

## Claim discipline

- v123.0 proves the **spec is well-formed and declares all seven
  σ-invariants by name**. On runners with TLA+ tools it additionally
  proves those invariants hold over the bounded state space.
- v123.0 does **not** claim that the C runtime implements every TLA
  transition. That correspondence is the v123.1 refinement
  obligation: a proof obligation per v106/v112/v113/v114 action that
  it refines the corresponding TLA action. Until that lands, v85's
  runtime asserts remain the authoritative in-process guarantee.
- The σ-discretisation (0..10) is a conservative abstraction of the
  continuous σ_product: any continuous σ > τ maps to a discrete
  σ ≥ τ+1, so AbstainSafety over the discretised model implies
  AbstainSafety over the underlying real-valued runtime (assuming
  consistent τ).

## σ-stack layer

v123 sits *across* the σ-governance layer as an **offline proof
obligation** on the runtime. It is the companion to v85's runtime
assertions — what v85 observes, v123 proves.

---

_Copyright (c) 2026 Spektre Labs · AGPL-3.0-or-later · Creation OS_
