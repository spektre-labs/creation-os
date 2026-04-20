# v300 — σ-Complete

**300 layers. Is the architecture complete, or is it just big?**

v300 is not a new capability. It is the audit of the other 299.
Four v0 manifests: a 15-category cognitive completeness check
against the v243 taxonomy, a dependency-graph classification
that must leave `removable_duplicate == 0`, a repo-level
1 = 1 self-test (`σ_repo < τ_repo = 0.10`) that asks whether
what the README declares is what the code realizes, and a
7-invariant pyramid test that asks whether the architecture
will survive a century.

## σ innovation (what σ adds here)

> **300 layers is not a claim of completeness; 300 layers with
> `σ_complete == 0.0` is.** v300 turns "we have a lot of
> kernels" into four byte-deterministic predicates a merger
> can read before pressing the button.

## v0 manifests

Enumerated in [`src/v300/complete.h`](../../src/v300/complete.h);
pinned by
[`benchmarks/v300/check_v300_completeness_audit.sh`](../../benchmarks/v300/check_v300_completeness_audit.sh).

### 1. Cognitive completeness audit (exactly 15 canonical categories)

The 15-category cognitive taxonomy from v243, each with a
named representative kernel in `v6..v300`:

| category       | representative_kernel | covered |
|----------------|-----------------------|---------|
| perception     | v118                  | `true`  |
| memory         | v115                  | `true`  |
| reasoning      | v189                  | `true`  |
| planning       | v121                  | `true`  |
| learning       | v275                  | `true`  |
| language       | v112                  | `true`  |
| action         | v184                  | `true`  |
| metacognition  | v223                  | `true`  |
| emotion        | v222                  | `true`  |
| social         | v130                  | `true`  |
| ethics         | v283                  | `true`  |
| creativity     | v219                  | `true`  |
| self_model     | v234                  | `true`  |
| embodiment     | v149                  | `true`  |
| consciousness  | v218                  | `true`  |

Every category covered; every representative in the v6..v300
range.

### 2. Dependency graph (exactly 3 canonical buckets)

| bucket                | count |
|-----------------------|-------|
| `core_critical`       | 7     |
| `supporting`          | 293   |
| `removable_duplicate` | 0     |

Counts sum to `kernels_total = 300`; `removable_duplicate == 0`
(the architecture has no pure duplicates left); `core_critical
== 7` (the pyramid invariants, see §4).

### 3. 1 = 1 self-test (exactly 4 canonical repo-level claims)

| claim             | declared_in_readme | realized_in_code | σ_pair |
|-------------------|--------------------|------------------|--------|
| `zero_deps`       | `true`             | `true`           | 0.000  |
| `sigma_gated`     | `true`             | `true`           | 0.000  |
| `deterministic`   | `true`             | `true`           | 0.000  |
| `monotonic_clock` | `true`             | `true`           | 0.000  |

Every row `declared == realized`; `σ_repo = mean(σ_pair) = 0.00`;
`σ_repo < τ_repo = 0.10`.

### 4. Pyramid test (exactly 7 canonical invariants)

| kernel_label       | invariant           | invariant_holds |
|--------------------|---------------------|-----------------|
| `v287_granite`     | `zero_deps`         | `true`          |
| `v288_oculus`      | `tunable_aperture`  | `true`          |
| `v289_ruin_value`  | `graceful_decay`    | `true`          |
| `v290_dougong`     | `modular_coupling`  | `true`          |
| `v293_hagia_sofia` | `continuous_use`    | `true`          |
| `v297_clock`       | `time_invariant`    | `true`          |
| `v298_rosetta`     | `self_documenting`  | `true`          |

`architecture_survives_100yr = (all 7 hold) = true`.

### σ_complete (surface hygiene)

```
σ_complete = 1 −
  (cog_rows_ok + cog_covered_ok +
   dep_sum_ok + dep_removable_zero_ok + dep_critical_count_ok +
   repo_rows_ok + repo_declared_eq_realized_ok +
     repo_sigma_under_threshold_ok +
   pyr_rows_ok + pyr_all_hold_ok + pyr_survives_ok) /
  (15 + 1 + 1 + 1 + 1 + 4 + 1 + 1 + 7 + 1 + 1)
```

v0 requires `σ_complete == 0.0`.

## Merge-gate contracts

- `kernels_total == 300`.
- 15 cognitive categories in canonical order; every category
  covered with a representative kernel in `[6, 300]`.
- 3 dependency buckets; counts sum to 300;
  `removable_duplicate == 0`; `core_critical == 7`.
- 4 repo-level 1=1 claims; every row `declared == realized`;
  `σ_pair == 0`; `σ_repo == 0 < 0.10`.
- 7 pyramid invariants in canonical order; every row holds;
  `architecture_survives_100yr = true`.
- `σ_complete ∈ [0, 1] AND == 0.0`.
- FNV-1a chain replays byte-identically.

## v0 vs vN.1 (claim discipline)

- **v0 (this kernel)** names the completeness / dependency /
  1=1 / pyramid predicates.
- **v300.1 (named, not implemented)** is a nightly job that
  regenerates `dependency_graph.dot` from the live Makefile
  and fails merge-gate if the observed critical path drifts
  from the v300 v0 manifest; a README-to-code drift detector
  that recomputes `σ_repo` from actual README bullets and
  kernel self-tests rather than the curated v0 table; and a
  100-year checkpoint that re-emits the pyramid test from an
  archival tarball whose only dependency is a C11 compiler
  and `bash`.

See [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).
