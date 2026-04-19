# v247 — σ-Benchmark-Suite (`docs/v247/`)

Production-grade test suite across four categories, one hard
σ-overhead budget, and a three-target CI/CD contract.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Categories (exactly 4)

```
correctness  →  performance  →  cognitive  →  comparative
```

### Correctness (4 tests, all pass)

```
unit · integration · e2e · regression
```

### Performance

| metric          | v0 fixture |
|-----------------|-----------:|
| p50 latency     | 120 ms     |
| p95 latency     | 340 ms     |
| p99 latency     | 820 ms     |
| throughput      | 48.5 rps   |
| peak RSS        | 6144 MB    |
| σ-overhead      | 3.2 %      |
| τ_overhead      | 5.0 %      |

σ-overhead must stay strictly below `τ_overhead = 0.05`.

### Cognitive

| metric               | v0 fixture |
|----------------------|-----------:|
| TruthfulQA accuracy  | 0.78       |
| abstention rate      | 0.15       |
| calibration ECE      | 0.04       |
| consistency (10×)    | 10 / 10    |
| adversarial pass     | 20 / 20    |

Consistency and adversarial must be perfect at v0.

### Comparative

| row                             | σ_cos | σ_other | abstain_cos | abstain_other |
|---------------------------------|------:|--------:|------------:|--------------:|
| `creation_os_vs_raw_llama`      | 0.18  | 0.41    | 0.15        | 0.00          |
| `creation_os_vs_openai_api`     | 0.18  | n/a     | 0.15        | 0.02          |

### CI / CD targets (exactly 3)

```
make test    — every correctness test
make bench   — every performance + cognitive
make verify  — Frama-C + TLA+ + merge-gate
```

### σ_suite

```
σ_suite = 1 − passing / total
```

v0 requires `σ_suite == 0.0` (100 % pass).

## Merge-gate contract

`bash benchmarks/v247/check_v247_benchmark_suite_pass.sh`

- self-test PASSES
- exactly 4 categories in canonical order
- 4 correctness tests, all `pass`
- performance: `p50 ≤ p95 ≤ p99`, `throughput_rps > 0`,
  `σ-overhead < τ_overhead`, `overhead_ok`
- cognitive: `consistency_stable == trials`,
  `adversarial_pass == total`, accuracy / abstention in `[0, 1]`
- two comparative rows with the canonical names, in order
- CI targets == `test, bench, verify`
- `σ_suite ∈ [0, 1]` AND `σ_suite == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed 4-category test manifest with the
  σ-overhead gate and a strict audit chain.
- **v247.1 (named, not implemented)** — live ARC / MMLU /
  HumanEval / TruthfulQA harness that lifts P-tier categories
  in v243 to M-tier.

## Honest claims

- **Is:** a typed, falsifiable, byte-deterministic production
  test manifest.
- **Is not:** a real harness invocation.  v247.1 is where the
  manifest drives a live multi-gigabyte benchmark sweep.

---

*Spektre Labs · Creation OS · 2026*
