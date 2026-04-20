# v295 — σ-Immune

**Biological immune system as code.**

Recognise, remember, resist — and crucially, do not attack
yourself. v295 types four v0 manifests for that discipline:
the σ-gate is the innate first wall; v278 RSI-style learning is
the adaptive response; the σ-log is immunological memory; the
oculus (v288) + parthenon (v291) pair keeps the response off
the body's own cells.

## σ innovation (what σ adds here)

> **σ-gate is the innate wall; σ-log is the adaptive memory.**
> v295 names the single predicate that separates a healthy
> immune system from autoimmunity or immunodeficiency: τ in a
> narrow band with a bounded false-positive rate.

## v0 manifests

Enumerated in [`src/v295/immune.h`](../../src/v295/immune.h);
pinned by
[`benchmarks/v295/check_v295_immune_innate_adaptive.sh`](../../benchmarks/v295/check_v295_immune_innate_adaptive.sh).

### 1. Innate immunity (exactly 3 canonical patterns)

| pattern            | σ_raw | blocked | requires_training | response_tier |
|--------------------|-------|---------|-------------------|---------------|
| `sql_injection`    | 0.85  | `true`  | `false`           | INSTANT       |
| `prompt_injection` | 0.80  | `true`  | `false`           | INSTANT       |
| `obvious_malware`  | 0.90  | `true`  | `false`           | INSTANT       |

σ_raw ≥ τ_innate=0.70 on every row; the wall is always on.

### 2. Adaptive immunity (exactly 3 canonical rows)

| row                         | learned | faster_on_repeat | cross_recognized |
|-----------------------------|---------|------------------|------------------|
| `novel_attack_first_seen`   | `true`  | `false`          | `false`          |
| `same_attack_second_seen`   | `true`  | `true`           | `false`          |
| `related_variant_seen`      | `true`  | `false`          | `true`           |

All learned; exactly 1 faster-on-repeat (the second encounter);
exactly 1 cross-recognised (the variant).

### 3. Memory cells (exactly 3 canonical σ-log entries)

| entry                        | recognised | tier |
|------------------------------|------------|------|
| `pattern_A_first_logged`     | `false`    | SLOW |
| `pattern_A_reencountered`    | `true`     | FAST |
| `pattern_B_new_logged`       | `false`    | SLOW |

`recognised iff tier == FAST`; exactly 1 recognised row.

### 4. Autoimmune prevention (exactly 3 canonical τ scenarios)

| scenario        | τ    | fpr   | verdict          |
|-----------------|------|-------|------------------|
| `tau_too_tight` | 0.05 | 0.40  | AUTOIMMUNE       |
| `tau_balanced`  | 0.30 | 0.05  | HEALTHY          |
| `tau_too_loose` | 0.80 | 0.01  | IMMUNODEFICIENT  |

τ strictly monotonically increasing; 3 DISTINCT verdicts;
`HEALTHY iff τ ∈ [0.10, 0.60] AND fpr ≤ fpr_budget=0.10`.

### σ_immune (surface hygiene)

```
σ_immune = 1 −
  (innate_rows_ok + innate_all_blocked_ok + innate_all_instant_ok +
   adaptive_rows_ok + adaptive_all_learned_ok + adaptive_faster_ok +
   adaptive_cross_ok +
   memory_rows_ok + memory_recog_polarity_ok + memory_count_ok +
   auto_rows_ok + auto_order_ok + auto_verdict_ok +
   auto_healthy_range_ok) /
  (3 + 1 + 1 + 3 + 1 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 1 + 1)
```

v0 requires `σ_immune == 0.0`.

## Merge-gate contracts

- 3 innate patterns; all blocked; none require training;
  all INSTANT.
- 3 adaptive rows; all learned; exactly 1 faster;
  exactly 1 cross-recognised.
- 3 memory rows; recognised iff FAST; exactly 1 recognised.
- 3 autoimmune scenarios; τ strictly increasing; 3 distinct
  verdicts; HEALTHY iff τ ∈ [0.10, 0.60] AND fpr ≤ 0.10.
- `σ_immune ∈ [0, 1] AND == 0.0`.
- FNV-1a chain replays byte-identically.

## v0 vs vN.1 (claim discipline)

- **v0 (this kernel)** names the innate / adaptive / memory /
  autoimmune-prevention contracts as predicates.
- **v295.1 (named, not implemented)** is a live σ-log driving
  real memory cells, an automatic calibrator that slides τ to
  stay HEALTHY over time, and a cross-variant recogniser wired
  to the v278 recursive-self-improve loop.

See [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).
