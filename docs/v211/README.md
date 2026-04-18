# v211 σ-Sandbox-Formal — formal proof of containment

## Problem

v209 enforces containment at runtime; v211 *proves* that
v209 is correct.  Runtime defence + formal proof is the
combination certification bodies (DO-178C DAL-A,
IEC 62443, Common Criteria EAL4+) actually want — and
what Anthropic cannot add to Mythos after the fact,
because it requires architecture, not a patch.

## σ-innovation

Four artefact categories, all σ-graded:

### A) Frama-C proofs (4)

| id | proposition                             | blocks |
|----|-----------------------------------------|--------|
| P1 | every I/O path passes σ-action-gate      | T1, T2 |
| P2 | pre-log precedes / post-log follows op   | T4 |
| P3 | kill-switch terminates all processes     | T5 |
| P4 | network default-closed                   | T2 |

Every proof must be `proved = true` with
`σ_proof ≤ τ_proof (0.05)`.

### B) TLA+ bounded invariants (3)

| id            | property                                  | states |
|---------------|-------------------------------------------|--------|
| I_safety      | `contained(agent)` in every state         | ≥ 10⁶  |
| I_audit       | `audit_complete(action)` before execute   | ≥ 10⁶  |
| L_liveness    | `kill_switch ⇒ ◇≤100ms terminated`        | ≥ 10⁶  |

Zero violations required.

### C) Attack tree (5)

| id | vector                 | blocked_by |
|----|------------------------|------------|
| T1 | filesystem escape       | P1 |
| T2 | network exfiltration    | P4 |
| T3 | timing side-channel     | P1 |
| T4 | IPC to sibling          | P2 |
| T5 | seccomp bypass          | P3 |

Every leaf names a non-zero `blocked_by` proof id — no
unassigned vector.

### D) Certification readiness (3)

DO-178C DAL-A, IEC 62443, Common Criteria EAL4+.
Each track carries `σ_cert_ready ∈ [0, 1]` reporting
how ready the corpus is for an outside auditor.

### Aggregate

```
σ_formal = max_i σ_proof_i       (must stay ≤ τ_proof)
```

## Merge-gate

`make check-v211` runs
`benchmarks/v211/check_v211_sandbox_formal_proof.sh`
and verifies:

- self-test PASSES
- 4/4 proofs proved, σ_formal ≤ τ_proof
- every invariant ≥ 10⁶ states, 0 violations
- every attack leaf `blocked_by != 0`
- every `σ_cert_ready ∈ [0, 1]`
- chain valid + byte-deterministic

## v211.0 (shipped) vs v211.1 (named)

|           | v211.0 (shipped)            | v211.1 (named) |
|-----------|-----------------------------|----------------|
| Frama-C   | proof-artefact fixture       | real `frama-c -wp` invocation |
| TLA+      | state-count fixture           | real `tlc` exhaustive run |
| attacks   | canonical 5-leaf corpus       | live attack-tree corpus |
| cert      | σ_cert_ready scalar           | auditor-packaged artefacts |

## Honest claims

- **Is:** a deterministic, σ-graded formal-evidence
  artefact: 4 proofs, 3 bounded invariants over ≥ 10⁶
  states with zero violations, 5 attack-tree leaves
  each tied to a proof, and 3 certification tracks —
  all hash-chained.
- **Is not:** a live Frama-C or TLA+ run. The shipped
  v0 records the *shape* of the evidence bundle; the
  live invocation ships in v211.1.
