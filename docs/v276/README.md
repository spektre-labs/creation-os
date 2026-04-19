# v276 — σ-Gated-DeltaNet (`docs/v276/`)

Linear attention with a σ-gate on every gate.  Gated
DeltaNet competes with TTT and Mamba on long contexts
while offering `O(n)` cost.  v276 types the σ-layer:
two canonical backends (deltanet linear · transformer
quadratic), four σ-gate fallback fixtures at
`τ_gate = 0.35`, a three-component combo stack
(`deltanet · ttt · sigma_gate`), and a three-task
benchmark where v267 σ-Mamba, v275 σ-TTT, and v276
σ-DeltaNet compete and at least two backends win at
least one task.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Backends (exactly 2, canonical order)

| name          | exponent | gate_mechanism | throughput_rel |
|---------------|---------:|:--------------:|---------------:|
| `deltanet`    |  1       | ✅             | 3.10           |
| `transformer` | 2       | —              | 1.00           |

Contract: `deltanet.throughput_rel > transformer.throughput_rel`
AND `deltanet.exponent == 1` AND
`transformer.exponent == 2`.

### σ-gate fallback (exactly 4 fixtures, τ_gate = 0.35)

Rule: `σ_gate ≤ τ_gate → LINEAR` else
`FALLBACK_FULL` (full attention for this token).  Both
branches fire.

| token_id | σ_gate | decision       |
|---------:|-------:|:--------------:|
|  0       | 0.12   | LINEAR         |
|  1       | 0.28   | LINEAR         |
|  2       | 0.47   | FALLBACK_FULL  |
|  3       | 0.71   | FALLBACK_FULL  |

### Combo stack (exactly 3 components, canonical order)

| component     | enabled | layer_slot |
|---------------|:-------:|-----------:|
| `deltanet`    | ✅      | 1          |
| `ttt`         | ✅      | 2          |
| `sigma_gate`  | ✅      | 3          |

### Tri-backend tasks (exactly 3 fixtures)

Each task exposes σ per backend
(`sigma_mamba`, `sigma_deltanet`, `sigma_ttt`);
`chosen == argmin(σ)`.  Contract: `sigma_chosen ≤
sigma_rival` for every task AND ≥ 2 distinct chosen
backends across the 3 tasks.

| task              | σ_mamba | σ_deltanet | σ_ttt | chosen     |
|-------------------|--------:|-----------:|------:|:----------:|
| `long-context`    | 0.31    | 0.12       | 0.22  | deltanet   |
| `icl-arith`       | 0.38    | 0.29       | 0.11  | ttt        |
| `code-retrieve`   | 0.15    | 0.27       | 0.33  | mamba      |

### σ_deltanet

```
σ_deltanet = 1 − (backend_rows_ok + backend_exponents_ok +
                  backend_throughput_ok +
                  gate_rows_ok + gate_both_ok +
                  combo_rows_ok + combo_order_ok +
                  task_rows_ok + task_chosen_ok +
                  task_diversity_ok) /
                 (2 + 1 + 1 + 4 + 1 + 3 + 1 + 3 + 1 + 1)
```

v0 requires `σ_deltanet == 0.0`.

## Merge-gate contract

`bash benchmarks/v276/check_v276_deltanet_linear_attention_sigma.sh`

- self-test PASSES
- 2 canonical backends with typed exponents AND
  `deltanet.throughput_rel > transformer.throughput_rel`
- 4 σ-gate fixtures; decision matches σ_gate vs
  τ_gate = 0.35; both branches fire
- 3 combo components canonical with `layer_slot` in
  canonical order
- 3 tri-backend tasks; `chosen == argmin(σ)`;
  `sigma_chosen ≤ sigma_rival`; ≥ 2 distinct chosen
  backends
- `σ_deltanet ∈ [0, 1]` AND `σ_deltanet == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed backend / gate / combo /
  tri-backend-task manifest with FNV-1a chain.
- **v276.1 (named, not implemented)** — live Gated
  DeltaNet kernel wired into v262, per-token σ from
  the gate, live fallback to full attention, measured
  AURCC vs v267 σ-Mamba and v275 σ-TTT on matched
  workloads.

## Honest claims

- **Is:** a typed, falsifiable σ-DeltaNet manifest
  where backend exponents, throughput ordering, gate
  fallback, combo ordering, and tri-backend task
  diversity are merge-gate predicates.
- **Is not:** a running Gated DeltaNet kernel.
  v276.1 is where the manifest meets CUDA / Metal /
  NEON.

---

*Spektre Labs · Creation OS · 2026*
