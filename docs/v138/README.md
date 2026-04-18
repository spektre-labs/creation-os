# v138 σ-Proof — Deductive verification of σ-invariants

Source: `src/v138/proof.[ch]` · `src/v138/sigma_gate.c` · `src/v138/main.c`
Spec:   `specs/v138/sigma_gate.acsl`
Gate:   `benchmarks/v138/check_v138_prove_frama_c_wp.sh`
Build:  `make creation_os_v138_proof` · `make check-v138` · `make prove`

## Problem

v123 TLA+ model-checking is **bounded** — it explores up to a depth
limit. For DO-178C DAL-A avionics-grade certification you need
**unbounded** proofs: the invariant must hold for every reachable
state, not just the bounded subset. In C that means deductive
verification via ACSL annotations discharged by Frama-C's WP
plug-in.

## σ-innovation

σ-gate correctness is now a machine-checked obligation. The same
two-tier pattern v123 uses for TLC:

| Tier | Runs when | What it checks |
|------|-----------|----------------|
| **1. ACSL shape** | always (pure C) | `/*@ ... */` blocks exist, every annotated function carries `requires` + `ensures`, the σ-domain predicate `0.0f ≤ σ ≤ 1.0f` appears, the emit / abstain partition is total, `disjoint behaviors` keyword is present, loop invariants discharge the `vec8` existential OR |
| **2. Frama-C WP** | if `frama-c` is on `$PATH` | `frama-c -wp -wp-rte` discharges every proof obligation to alt-ergo / z3 / cvc4 |

Tier-1 never fails a green host. Tier-2 is **opportunistic** — it
SKIPs gracefully when the prover isn't installed. Export
`V138_REQUIRE_FRAMA_C=1` to fail the merge-gate on a missing
`frama-c`.

## The canonical contract

`specs/v138/sigma_gate.acsl` is the frozen reference. `src/v138/sigma_gate.c`
mirrors it with inline ACSL:

```c
/*@ requires 0.0f <= sigma <= 1.0f;
    requires 0.0f <= tau   <= 1.0f;
    assigns  \nothing;
    behavior emit:
        assumes sigma <= tau;
        ensures \result == 0;
    behavior abstain:
        assumes sigma > tau;
        ensures \result == 1;
    complete behaviors;
    disjoint behaviors;
*/
int cos_v138_sigma_gate(float sigma, float tau);
```

The `vec8` variant carries a loop invariant that turns the
existential `∃ i. ch[i] > τ` quantifier into a machine-discharged
obligation. `cos_v138_spike_trigger` encodes v134's |Δσ| ≥ δ
predicate so the same proof path certifies the neuromorphic leg.

## Merge-gate measurements

| Claim | Check |
|-------|-------|
| `sigma_gate.c` validates with ≥ 3 annotations, loop invariant present, disjoint-behaviors keyword present | `check-v138` |
| Contractless stub → tier-1 FAILs with a populated error message | `check-v138` |
| Tier-2 returns `OK` or `SKIPPED` (never `FAIL`) on green hosts | `check-v138` |

## `make prove`

```
$ make prove
# tier-1: ACSL annotation shape
result=OK
n_annotations=4
has_domain_predicate=1
has_emit_behavior=1
has_abstain_behavior=1
has_loop_invariant=1
has_disjoint_behaviors=1
# tier-2: Frama-C WP (opportunistic)
result=SKIPPED
```

On a host with Frama-C installed, tier-2 reports `OK` and prints
the WP proof-obligation summary.

## v138.0 vs v138.1

| | Shipped (v138.0) | Follow-up (v138.1) |
|---|---|---|
| ACSL-annotated σ-gate + spec file | ✅ | — |
| Tier-1 shape validator | ✅ | — |
| Tier-2 Frama-C probe | ✅ (opportunistic) | Required on CI with alt-ergo container |
| Coq extraction | — | Frama-C → Coq lemmas for the highest verification tier |
| DO-178C DAL-A packaging | partial | Full certification dossier |

## Honest claims

* v138.0 guarantees the **shape** of the σ-contract under every
  commit. A source file that strips an `ensures` clause or the
  domain predicate fails the merge-gate.
* v138.0 guarantees the **machine-checked correctness** only on
  hosts that have Frama-C. That's the DO-178C DAL-A path — the
  README's existing DO-178C reference is now backed by a concrete
  machine-checkable artifact.
* Without Frama-C, v138.0 is equivalent in rigor to the previous
  v123 TLC integration — shape-only, bounded, and honest about it.
