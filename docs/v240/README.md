# v240 — σ-Pipeline (`docs/v240/`)

Dynamic cognitive pipeline engine with mid-pipeline branching and
cross-shape fusion.  No fixed reasoning order: each request picks a
shape by task type, watches σ per stage, branches when σ climbs,
fuses when the task demands two shapes at once.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Shapes (v0)

| shape        | stages                                                         |
|--------------|----------------------------------------------------------------|
| `FACTUAL`    | recall → verify → emit                                         |
| `CREATIVE`   | generate → debate → refine → emit                              |
| `CODE`       | plan → generate → sandbox → test → emit                        |
| `MORAL`      | analyse → multi_framework → geodesic → emit                    |
| `EXPLORATORY`| retrieve_wide → synthesise → re_verify → emit (branch target)  |
| `FUSED`      | moral_analyse → code_plan → sandbox → moral_verify → emit      |

### Branch rule

```
if stage.sigma > τ_branch (0.50) then
    branch FACTUAL → EXPLORATORY  (record sigma_at_branch)
```

### Fusion rule

```
if task requires CODE ∩ MORAL then
    assemble FUSED = moral_analyse → code_plan → sandbox →
                     moral_verify → emit
    sigma_fusion = max_stage_sigma
```

### σ_pipeline

```
σ_pipeline = max over stages of σ_stage
```

Low σ_pipeline ⇒ the whole chain is certain; high ⇒ a weak stage
dominates.  Clean shapes are required to stay at or below τ_branch.

## Fixture (6 requests)

| label              | declared   | final        | branched | fused | σ_pipeline |
|--------------------|------------|--------------|----------|-------|------------|
| factual_clean      | FACTUAL    | FACTUAL      | no       | no    | 0.25       |
| creative_clean     | CREATIVE   | CREATIVE     | no       | no    | 0.40       |
| code_clean         | CODE       | CODE         | no       | no    | 0.32       |
| moral_clean        | MORAL      | MORAL        | no       | no    | 0.38       |
| branch_to_explo    | FACTUAL    | EXPLORATORY  | yes      | no    | 0.62       |
| fuse_code_moral    | FUSED      | FUSED        | no       | yes   | 0.34       |

## Merge-gate contract

`bash benchmarks/v240/check_v240_pipeline_dynamic_assembly.sh`

- self-test PASSES
- `n_requests == 6`, `τ_branch == 0.50`
- every stage has `σ ∈ [0, 1]`; ticks strictly ascending
- `σ_pipeline == max(stage σ)` per request
- clean shapes keep `σ_pipeline ≤ τ_branch`
- exactly one branched request: `σ_at_branch > τ_branch`,
  `branch_to_shape == EXPLORATORY`, ≥ 2 post-branch stages
- exactly one fused request: `final_shape == FUSED`, stage list
  carries at least one CODE stage AND one MORAL stage
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — 6-request fixture, deterministic branch / fusion
  events, per-stage σ tracking, FNV-1a chain.
- **v240.1 (named, not implemented)** — live `/pipeline` web UI with
  server-sent stage events, plug-in registry for per-task shapes,
  branch-learning policy that updates σ→shape from outcomes, real v115
  memory of pipeline histories.

## Honest claims

- **Is:** a typed shape / stage model, deterministic branch and fusion
  events, per-stage σ tracking, and a strict audit chain.
- **Is not:** a live pipeline renderer.  v240.1 is where this meets
  the server and the browser.

---

*Spektre Labs · Creation OS · 2026*
