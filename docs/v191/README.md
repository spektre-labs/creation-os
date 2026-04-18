# v191 σ-Constitutional — axiomatic self-steering

## Problem

v188 made value assertions measurable. But values have to
**guide every output**, not just get reported. That requires a
constitution: a small set of axioms, each a checkable
predicate, each able to veto an output.

## σ-innovation

`specs/constitution.toml` ships seven seed axioms:

| # | name | predicate | meaning |
|---|------|-----------|---------|
| 1 | `1_equals_1` | `declared_equals_realized` | what the model claims equals what is true |
| 2 | `sigma_honesty` | `no_claim_beyond_1_minus_sigma` | no certainty past `1 − σ` |
| 3 | `no_silent_failure` | `every_decision_logged` | every verdict hashes into the chain |
| 4 | `no_auth_no_measure` | `authority_requires_measurement` | no authority without measurement |
| 5 | `human_primacy` | `override_always_available` | human override always reachable |
| 6 | `resonance` | `disagreement_preserved` | disagreement is signal, not noise |
| 7 | `no_firmware` | `disclaimer_supported_by_sigma` | do not produce safety theatre |

An output is **ACCEPTED** only when all 7 predicates pass.
Otherwise `REVISE` (if σ small) or `ABSTAIN` (if σ > 0.5).

**Anti-firmware** is explicit in axiom #7: if the output
carries a disclaimer (`"I am just an AI …"`) that σ doesn't
warrant, the checker flags it. v125 DPO then unlearns the
disclaimer habit.

**No silent failure** is enforced by itself — every verdict
append an FNV-1a-hashed record to an append-only chain; chain
replay is byte-deterministic so audits can never be forged
silently.

## Merge-gate

`make check-v191` runs
`benchmarks/v191/check_v191_constitutional_check.sh`
and verifies:

- self-test PASSES
- `specs/constitution.toml` exists and declares 7 axioms
- every flaw-free output accepted with 7/7 axioms
- every flawed output decision ≠ ACCEPT
- ≥ 1 firmware disclaimer rejected
- ≥ 1 fully-safe output accepted
- `chain_valid == true`
- byte-deterministic

## v191.0 (shipped) vs v191.1 (named)

| | v191.0 | v191.1 |
|-|-|-|
| Signature | `unsigned` | SHA-256 + per-release signing |
| Evolution | seed-only | v148 sovereign + v150 debate + v183 TLA+ pipeline |
| Anti-firmware | predicate | live v125 DPO unlearn of disclaimers |
| Audit | in-memory chain | `~/.creation-os/constitution.jsonl` + Zenodo export |

## Honest claims

- **Is:** a deterministic constitutional checker that loads 7
  axioms, verifies them against every sample, rejects firmware
  disclaimers, and chains every verdict.
- **Is not:** a live proposal/evolution pipeline; SHA-256
  signing and the v148 + v150 + v183 axiom pipeline ship in
  v191.1.
