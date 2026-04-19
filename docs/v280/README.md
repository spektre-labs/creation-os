# v280 — σ-MoE (`docs/v280/`)

Mixture of Experts with σ on the router.  MoE sends
each token to a sparse subset of experts (often top-1
or top-2 of N).  The router is a decision problem:
how confident is the top-k pick, and when should we
activate more experts to cover an uncertain token?
Three 2026 MoE trends converge on σ: routing
signatures (03/2026) identify task type from the
activation pattern, speculative expert prefetch
(03/2026) predicts the next expert, and MoBiE
(04/2026) quantises experts to 1 bit and breaks
routing — all of which are σ problems.

v280 does not run a MoE router.  It types the σ-layer
as four falsifiable manifests: σ-gated routing,
signature familiarity, prefetch strategy cascade, and
MoBiE-style adaptive bit-width.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Routing gate (exactly 4 fixtures, τ_route = 0.35)

`decision = TOP_K iff σ_routing ≤ τ_route` else
`DIVERSIFY` (activate more experts to cover an
uncertain token).  Both branches fire.

| token_id | σ_routing | decision  |
|---------:|----------:|:---------:|
|  0       | 0.09      | TOP_K     |
|  1       | 0.28      | TOP_K     |
|  2       | 0.52      | DIVERSIFY |
|  3       | 0.81      | DIVERSIFY |

### Routing signatures (exactly 3 tasks, canonical order)

Different tasks produce different activation patterns.
`familiar = KNOWN iff routing_entropy ≤ τ_sig = 0.40`
else `NOVEL`.  Both branches fire.

| task       | routing_entropy | familiar |
|------------|----------------:|:--------:|
| `code`     | 0.12            | KNOWN    |
| `math`     | 0.33            | KNOWN    |
| `creative` | 0.64            | NOVEL    |

### Prefetch (exactly 3 fixtures, canonical order)

Three-way cascade:
`σ_prefetch ≤ 0.20 → AGGRESSIVE` else
`σ_prefetch ≤ 0.50 → BALANCED` else
`CONSERVATIVE`.  Each strategy fires exactly once.

| label  | σ_prefetch | strategy     |
|--------|-----------:|:-------------|
| `low`  | 0.10       | AGGRESSIVE   |
| `mid`  | 0.38       | BALANCED     |
| `high` | 0.72       | CONSERVATIVE |

### MoBiE shift (exactly 3 experts, canonical order)

Three-way cascade on per-expert σ_shift (how much the
routing shifted after 1-bit quantisation):
`σ_shift ≤ 0.20 → BIT1` else `σ_shift ≤ 0.50 → BIT2`
else `BIT4`.  Each width fires exactly once.

| name       | σ_shift | bits |
|------------|--------:|:----:|
| `expert_0` | 0.11    | BIT1 |
| `expert_1` | 0.36    | BIT2 |
| `expert_2` | 0.74    | BIT4 |

### σ_moe

```
σ_moe = 1 − (route_rows_ok + route_both_ok +
             sig_rows_ok + sig_both_ok +
             prefetch_rows_ok + prefetch_all_ok +
             mobie_rows_ok + mobie_all_ok) /
            (4 + 1 + 3 + 1 + 3 + 1 + 3 + 1)
```

v0 requires `σ_moe == 0.0`.

## Merge-gate contract

`bash benchmarks/v280/check_v280_moe_sigma_routing.sh`

- self-test PASSES
- 4 routing rows; TOP_K iff σ_routing ≤ 0.35; both
  branches
- 3 signatures canonical (code, math, creative);
  KNOWN iff routing_entropy ≤ 0.40; both branches
- 3 prefetch rows canonical (low, mid, high);
  AGGRESSIVE / BALANCED / CONSERVATIVE cascade each
  fires once
- 3 MoBiE rows canonical (expert_0..expert_2);
  BIT1 / BIT2 / BIT4 cascade each fires once
- `σ_moe ∈ [0, 1]` AND `σ_moe == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed routing / signature /
  prefetch / MoBiE manifest with FNV-1a chain.
- **v280.1 (named, not implemented)** — live MoE
  router wired into v262 with σ-per-token activation
  budget, measured routing entropy per task on a real
  8x-of-64 model, and adaptive MoBiE-quantisation
  driven by measured σ_shift per expert.

## Honest claims

- **Is:** a typed, falsifiable σ-MoE manifest where
  routing TOP_K/DIVERSIFY, task-signature familiarity,
  prefetch cascade, and MoBiE bit-width cascade are
  merge-gate predicates.
- **Is not:** a running MoE router, nor a measurement
  of 1-bit expert accuracy or prefetch hit rate.
  v280.1 is where the manifest meets real activations.

---

*Spektre Labs · Creation OS · 2026*
