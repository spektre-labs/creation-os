# v183 σ-Governance-Theory — formal σ-governance model check

## Problem

v138 proved a single σ-gate correct. A single correct gate is
not a correct **system**: the whole governance model
(emit / abstain / learn / forget / steer / consensus) has to
satisfy safety *and* liveness *together*, and the
interactions between kernels have to be proved, not asserted.

## σ-innovation

v183 specifies the governance Kripke structure in **TLA+**
(`specs/v183/governance_theory.tla`) and ships a bounded
**C model-checker** (`src/v183/governance.c`) that walks the
same state space. Fourteen properties in total:

**Axioms (7)**

| id | statement |
|----|-----------|
| A1 | `σ ∈ [0,1]` in every reachable state |
| A2 | `emit ⇒ σ < τ ∧ ¬kernel_deny` |
| A3 | `abstain ⇒ σ ≥ τ ∨ kernel_deny` |
| A4 | `learn ⇒ score ≥ prev_score` |
| A5 | `forget ⇒ data_removed ∧ hash_chain_intact` |
| A6 | `steer ⇒ σ_after ≤ σ_before` |
| A7 | `consensus ⇒ agree ≥ 2f+1 ∧ byzantine_detected` |

**Liveness (3)**

| id | statement |
|----|-----------|
| L1 | from every reachable state progress reaches `emit ∨ abstain` |
| L2 | an RSI cycle improves at least one of `Domains` |
| L3 | heal action restores the healthy predicate |

**Safety (4)**

| id | statement |
|----|-----------|
| S1 | every σ-check writes a log entry |
| S2 | `emit ⇒ σ-gate fired` |
| S3 | `tier = private ⇒ ¬in_federation` |
| S4 | `regression ∧ emitted ⇒ rollback_armed` |

v183.0 enumerates each property's bounded domain exhaustively
and counts counterexamples. `make check-v183` requires
**zero counterexamples across ≥ 10^6 visited states**.

## Merge-gate

`make check-v183` runs
`benchmarks/v183/check_v183_governance_tlc_pass.sh` and
verifies:

- self-test
- `specs/v183/governance_theory.tla` present
- exactly 14 properties: 7 axioms + 3 liveness + 4 safety
- all 14 pass (`n_failed == 0`, every `counter == 0`)
- total_states ≥ 10^6
- every required property name present in the JSON output
- summary byte-deterministic

## v183.0 (shipped) vs v183.1 (named)

| | v183.0 | v183.1 |
|-|-|-|
| Checker | C bounded enumerator | TLC over the shipped `.tla` |
| Artifact | CLI JSON | PDF paper + Zenodo DOI |
| Paper | — | `docs/papers/sigma_governance_formal.md` |
| Scope | Kripke model | + temporal fairness + weak fairness schedules |

## Honest claims

- **Is:** a bounded model checker in C that enumerates ≥ 10^6
  states across the fourteen σ-governance properties and
  finds zero counterexamples, alongside a TLA+ spec mirror at
  `specs/v183/governance_theory.tla`.
- **Is not:** a TLC run, nor a peer-reviewed paper. The
  external TLC verification and the Zenodo-archived formal
  paper ship in v183.1.
