# v206 σ-Theorem — NL conjecture → Lean 4 → σ-honest status

## Problem

v138 (Frama-C) proves σ-invariants at the C level. v206
extends σ-governed verification to arbitrary
propositions: a natural-language conjecture is
formalised, a proof is searched, a simulated Lean 4
checker accepts or rejects, and the result is exposed
through a σ-honest status ladder — no pose-as-proven.

## σ-innovation

For each of the 8 conjectures (fixture) v206 records:

| field                 | meaning |
|-----------------------|---------|
| σ_formalization       | NL → Lean-4 translation faithfulness (≤ τ_formal 0.35 to accept) |
| σ_step[0..3]          | per-step proof confidence |
| σ_proof               | `max(σ_step)` — the chain is as honest as its weakest link |
| weakest_step          | index of the highest-σ step (where iteration must repair) |
| lean_accepts          | closed-form Lean 4 predicate: `σ_formalization ≤ τ_formal ∧ σ_proof ≤ τ_step` |
| counter_example_id    | non-zero when a refutation witness exists |

Status ladder:

| status       | condition                                    |
|--------------|----------------------------------------------|
| PROVEN       | `lean_accepts == true` and no counter-example |
| CONJECTURE   | `lean_accepts == false` but `σ_proof ≤ τ_step + 0.10` |
| SPECULATION  | `lean_accepts == false` and `σ_proof > τ_step + 0.10` |
| REFUTED      | counter-example id ≠ 0                        |

σ-honesty contract: **nothing** is marked PROVEN without
the Lean accept flag. The self-test rejects any state
that violates this.

FNV-1a chain over (id, status, weakest, lean, counter,
σ_formal, σ_proof, full step vector, prev) gives a
byte-identical audit of every verification run.

## Merge-gate

`make check-v206` runs
`benchmarks/v206/check_v206_theorem_lean_verify.sh` and
verifies:

- self-test PASSES
- ≥ 1 PROVEN, ≥ 1 CONJECTURE, ≥ 1 SPECULATION, ≥ 1 REFUTED
- PROVEN ⇒ `lean_accepts` ∧ `σ_proof ≤ τ_step`
- REFUTED ⇒ counter-example id ≠ 0
- σ ∈ [0, 1]
- chain valid + byte-deterministic

## v206.0 (shipped) vs v206.1 (named)

|                   | v206.0 (shipped) | v206.1 (named) |
|-------------------|------------------|----------------|
| formalisation     | template table | v111.2 reason + v190 latent reason |
| proof search      | σ-per-step fixture | iterative weakest-step repair loop |
| Lean acceptance   | closed-form predicate | real `lake env lean` invocation |
| C-level lemmas    | — | v138 Frama-C WP bridge |

## Honest claims

- **Is:** a deterministic σ-honest theorem-status
  ladder with a simulated Lean-4 accept predicate that
  refuses to mark any conjecture PROVEN without the
  accept flag.
- **Is not:** a live Lean 4 invocation, real proof
  synthesiser, or formalisation translator — those ship
  in v206.1.
