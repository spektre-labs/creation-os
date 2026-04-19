# v265 — σ-Speculative (`docs/v265/`)

σ-guided speculative decoding.  A small draft model
proposes N tokens; a big verifier checks them in one
forward pass.  σ picks *how many* tokens to speculate
(low σ → speculate 12, high σ → speculate 4) and every
speculated token clears an additional σ-gate, so
"verifier accepted" is never the last word.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Model pair (exactly 2)

| role      | name                  |
|-----------|-----------------------|
| draft     | `bitnet-1.5B-local`   |
| verifier  | `airllm-70B-local`    |

### σ-guided speculation length (exactly 4 bands)

| band      | σ_context     | spec_len |
|-----------|---------------|---------:|
| easy      | [0.00, 0.20]  | 12       |
| medium    | (0.20, 0.40]  | 8        |
| hard      | (0.40, 0.60]  | 6        |
| extreme   | (0.60, 1.00]  | 4        |

`spec_len` is **strictly non-increasing** in σ.  Low σ
→ speculate far; high σ → speculate short.  Any
regression that flips this monotonicity fails the gate.

### Multi-draft duel (exactly 3 fixtures)

| draft_a    | σ_a  | draft_b    | σ_b  | winner |
|------------|-----:|------------|-----:|:------:|
| `bitnet-A` | 0.12 | `bitnet-B` | 0.19 | A      |
| `bitnet-A` | 0.28 | `bitnet-B` | 0.15 | B      |
| `bitnet-A` | 0.08 | `bitnet-B` | 0.22 | A      |

`winner == argmin(σ_draft)`; both A-wins and B-wins fire.

### Speculation + σ-gate (exactly 4 fixtures, τ_spec = 0.35)

Rule: `σ_speculated ≤ τ_spec` → `ACCEPT`, else →
`REJECT`.  The σ-gate is an *extra* filter on top of
the verifier — a token the verifier would keep can
still be rejected.

| token          | σ_speculated | decision |
|----------------|-------------:|:--------:|
| `tok_the`      | 0.08         | ACCEPT   |
| `tok_quantum`  | 0.24         | ACCEPT   |
| `tok_hallucit` | 0.48         | REJECT   |
| `tok_suspect`  | 0.67         | REJECT   |

### Throughput claim

| metric                       | value |
|------------------------------|------:|
| tok/s plain (verifier only)  | 45    |
| tok/s σ-speculative          | 108   |
| speedup ×                    | 2.40  |

Contract: `plain < σ_spec` AND `speedup_x ≥ 2.0`.

### σ_speculative

```
σ_speculative = 1 − (models_ok + bands_ok +
                     monotone_ok + duels_ok +
                     both_winners_ok + gate_rows_ok +
                     both_gate_branches_ok + speedup_ok) /
                    (2 + 4 + 1 + 3 + 1 + 4 + 1 + 1)
```

v0 requires `σ_speculative == 0.0`.

## Merge-gate contract

`bash benchmarks/v265/check_v265_speculative_sigma_guided.sh`

- self-test PASSES
- 2 models: draft + verifier roles
- 4 bands with spec_len `[12, 8, 6, 4]` and monotone_ok
- 3 duels; winner == argmin(σ); ≥ 1 A-win AND ≥ 1 B-win
- 4 gate fixtures; ≥ 1 ACCEPT AND ≥ 1 REJECT
- throughput: plain < σ-spec AND speedup × ≥ 2.0
- `σ_speculative ∈ [0, 1]` AND `σ_speculative == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed model / band / duel / gate
  / throughput manifest with FNV-1a chain.
- **v265.1 (named, not implemented)** — live
  draft+verifier inference wired to v262 hybrid engine,
  live σ feed from v226 attention driving spec-length,
  real GPU throughput benchmark producing measured
  tok/s.

## Honest claims

- **Is:** a typed, falsifiable speculative-decoding
  manifest where band monotonicity, multi-draft duel
  winners, both σ-gate branches, and ≥ 2× speedup are
  all merge-gate predicates.
- **Is not:** a running draft+verifier pipeline.
  v265.1 is where the manifest drives real tokens.

---

*Spektre Labs · Creation OS · 2026*
