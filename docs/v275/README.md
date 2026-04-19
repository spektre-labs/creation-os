# v275 — σ-TTT (`docs/v275/`)

Test-time training gated by σ.  Stanford / NVIDIA's
TTT-E2E (12/2025) showed that updating the last ~25 %
of MLP layers during inference yields 35× throughput
over full attention on 2 M contexts while matching
accuracy — academic validation of the v124 σ-continual
*living weights* thesis.

v275 does not execute TTT.  It types the σ-layer on top
of TTT as four falsifiable manifests: σ-gated weight
update, dual-track drift, sliding-window σ-eviction,
and a validation manifest that cites v124 and TTT-E2E
as convergent evidence.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### σ-gated update (exactly 4 fixtures, τ_update = 0.30)

`decision = LEARN iff σ_update ≤ τ_update` else `SKIP`.
Both branches fire.

| step | σ_update | decision |
|-----:|---------:|:--------:|
|  0   | 0.08     | LEARN    |
|  1   | 0.22     | LEARN    |
|  2   | 0.44     | SKIP     |
|  3   | 0.71     | SKIP     |

### Dual-track (exactly 3 fixtures, τ_sync = 0.15, τ_reset = 0.50)

Cascade: `σ_drift < τ_sync → SYNCED`; else
`σ_drift < τ_reset → DIVERGING`; else `RESET`.  Every
branch fires.

| label          | σ_drift | status     |
|----------------|--------:|:----------:|
| `cold-start`   | 0.06    | SYNCED     |
| `drifting`     | 0.32    | DIVERGING  |
| `noisy-burst`  | 0.68    | RESET      |

### Sliding-window σ-eviction (exactly 6 tokens)

`evict_rank` is a permutation of `[1..6]` matching
descending σ order (rank 1 = highest σ evicted first,
rank 6 = lowest σ kept longest).

| token_id | σ_token | evict_rank |
|---------:|--------:|-----------:|
| 0        | 0.41    | 3          |
| 1        | 0.12    | 5          |
| 2        | 0.78    | 1          |
| 3        | 0.03    | 6          |
| 4        | 0.55    | 2          |
| 5        | 0.29    | 4          |

### Validation manifest (exactly 2 citations)

This is a **citation contract**, not a throughput
claim.  v275 does not measure tok/s; v275.1 does.

| source                    | evidence                             |
|---------------------------|--------------------------------------|
| `v124_sigma_continual`    | living-weights thesis (this codebase)|
| `ttt_e2e_2025`            | Stanford/NVIDIA TTT-E2E, ICML 2025   |

### σ_ttt

```
σ_ttt = 1 − (update_rows_ok + update_both_ok +
             dualtrack_rows_ok + dualtrack_all_branches_ok +
             window_rows_ok + window_order_ok +
             validation_rows_ok + validation_distinct_ok) /
            (4 + 1 + 3 + 1 + 6 + 1 + 2 + 1)
```

v0 requires `σ_ttt == 0.0`.

## Merge-gate contract

`bash benchmarks/v275/check_v275_ttt_sigma_gated_update.sh`

- self-test PASSES
- 4 update rows; LEARN iff σ_update ≤ 0.30; both
  branches
- 3 dual-track rows; cascade fires all three
  (SYNCED / DIVERGING / RESET)
- 6 tokens; `evict_rank` permutation of `[1..6]`
  matching descending-σ order
- 2 validation citations present and distinct
- `σ_ttt ∈ [0, 1]` AND `σ_ttt == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed update / dualtrack /
  window / validation manifest with FNV-1a chain.
- **v275.1 (named, not implemented)** — live TTT
  kernel wired into v262, real weight updates on the
  MLP tail with σ-gated admission, on-device dual-
  track storage + snapshot reset, measured throughput
  vs full attention on 2 M contexts.

## Honest claims

- **Is:** a typed, falsifiable σ-TTT manifest where
  σ-gated update, dual-track state cascade,
  sliding-window σ-eviction permutation, and a
  two-source validation citation are merge-gate
  predicates.
- **Is not:** an inference-time weight-update engine,
  nor a reproduction of TTT-E2E's 35× throughput.
  v275.1 is where the manifest meets real MLP layers.

---

*Spektre Labs · Creation OS · 2026*
