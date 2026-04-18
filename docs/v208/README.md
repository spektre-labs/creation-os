# v208 σ-Manufacture — DFM + process sim + quality + closed loop

## Problem

A design is a plan; v208 turns the plan into a product.
The kernel integrates Design for Manufacturing (DFM)
analysis, process simulation, and σ-graded quality
prediction — and it explicitly closes the scientific
loop back to v204.

## σ-innovation

Four manufacturing runs, one per v207 Pareto design.
Every run carries:

### DFM findings (5 canonical rules)

| rule                    | σ_dfm |
|-------------------------|-------|
| TOL_TOO_TIGHT           | measured |
| MATERIAL_COST           | measured |
| ASSEMBLY_COMPLEX        | measured |
| PART_COUNT_HIGH         | measured |
| SPECIAL_PROCESS         | measured |

Findings with `σ_dfm > τ_dfm` (0.50) are flagged and
auto-carry a concrete `suggestion_id` so operators get an
actionable fix.

### Process simulation

Four stages per run, each with σ_process. The summary
`σ_process_max` is the worst stage — the pipeline moves
only as reliably as its weakest link.

### Quality prediction

```
σ_quality = 0.6 · σ_process_max + 0.4 · mean(σ_dfm)
```

Bounded to [0, 1]. When `σ_quality > τ_quality` (0.40)
v159-style heal kicks in: extra checkpoints proportional
to the overshoot. The self-test proves the monotonicity
`higher σ_quality ⇒ ≥ checkpoints`.

### Closed loop

Every run emits a non-zero `feedback_hypothesis_id` that
feeds the next v204 generation, closing:

```
hypothesis → experiment → theorem → design →
manufacture → observed quality → new hypothesis
```

This is the science / design / build loop on σ-graded
evidence — no mystery boxes.

## Merge-gate

`make check-v208` runs
`benchmarks/v208/check_v208_manufacture_dfm_check.sh` and
verifies:

- self-test PASSES
- 4 runs, ≥ 1 DFM issue flagged fleet-wide
- every flagged finding carries a non-zero
  `suggestion_id`
- σ_process_max, σ_quality, σ_dfm ∈ [0, 1]
- monotone (σ_quality, checkpoints) ordering
- every run emits a non-zero feedback hypothesis id
- chain valid + byte-deterministic

## v208.0 (shipped) vs v208.1 (named)

|                   | v208.0 (shipped) | v208.1 (named) |
|-------------------|------------------|----------------|
| BOM               | `n_parts` scalar | real bill of materials |
| process simulator | closed-form σ    | v176 simulator |
| heal              | extra checkpoint count | v159 online rework |
| audit             | per-run hash chain | v181 streaming audit |

## Honest claims

- **Is:** a deterministic digital-manufacturing
  pipeline that flags DFM issues with actionable
  suggestions, predicts quality, adds checkpoints
  monotonically, and closes the loop back to v204 with
  a non-zero feedback hypothesis id per run.
- **Is not:** a real CAM/MES, BOM solver, or process
  simulator — those ship in v208.1.
