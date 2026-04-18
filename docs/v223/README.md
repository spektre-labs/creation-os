# v223 — σ-Meta-Cognition (`docs/v223/`)

Strategy awareness, bias detection, and an explicit Gödel honesty bound.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

v133 measures performance; v147 reflects on outputs.  Neither answers the
deeper question: **how** does the model think, and what does it know about
its own thinking?  v223 adds strategy awareness, bias detection, and a
hard Gödel bound.

## σ-innovation

Each reasoning path carries:

- a declared **strategy**: deduction / induction / analogy / abduction /
  heuristic
- a **problem type**: logic / math / creative / social
- `σ_choice`   — prior `σ_strategy[problem_type][chosen]` from a tool/task-fit
                 calibration matrix
- `σ_meta`     — `|σ_choice − reflect_mean|`: confidence in the meta-analysis
- `σ_bias`     — max over detected biases (anchoring / confirmation /
                 availability); flags ≥ 0.30 count as "bias present"
- `σ_goedel`   — the **honest non-verifiable residual**, `∈ [0.10, 1.00]`.
                 When the model knows it is using a tool outside its domain
                 (or when a decision is self-referential), `σ_goedel → 1.0`.
                 This is the `1 = 1` cross-system resolution point:
                 self-consistency inside a single system can only reach
                 ~0.90; only a second system's agreement closes the gap.
- `σ_total`    = `0.40·σ_choice + 0.20·σ_meta + 0.20·σ_bias + 0.20·σ_goedel`

### Tool/task fit matrix (prior)

|              | deduction | induction | analogy | abduction | heuristic |
|--------------|-----------|-----------|---------|-----------|-----------|
| **logic**    | 0.10      | 0.30      | 0.40    | 0.25      | 0.60      |
| **math**     | 0.12      | 0.35      | 0.45    | 0.30      | 0.55      |
| **creative** | 0.70      | 0.40      | 0.20    | 0.35      | 0.50      |
| **social**   | 0.55      | 0.30      | 0.25    | 0.35      | 0.40      |

Deduction on logic is low σ; deduction on creative tasks is high σ.  Analogy
on creative tasks is low σ.  v223 enforces these contours in the merge-gate.

## Merge-gate contract

`bash benchmarks/v223/check_v223_metacognition_strategy_aware.sh`

- self-test PASSES
- 6 paths × 5 strategies × 4 problem types × 3 biases
- tool/task fit priors respected:
  `σ[logic][deduction] ≤ 0.15`, `σ[logic][heuristic] ≥ 0.50`,
  `σ[creative][deduction] ≥ 0.60`, `σ[creative][analogy] ≤ 0.25`
- **strategy awareness**: `σ_choice` equals the prior matrix entry per path
  (to 1e-6) — the model isn't just picking, it is reporting the prior σ for
  that exact pairing.
- at least one bias flag ≥ 0.30
- at least one `σ_goedel ≥ 0.80` — the honest "I cannot verify from inside
  myself" record
- `n_metacognitive ≥ 5 / 6` (model produced a meta-analysis for ≥ 5 paths)
- weights sum to 1; σ_total ∈ [0, 1]; σ_goedel ∈ [0.10, 1.0]
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — calibration matrix + 6 hand-picked paths with
  engineered σ-contours and biases, closed-form σ_total, memset-zeroed hash
  records.
- **v223.1 (named, not implemented)** — live v111.2 reasoning-path hooks,
  v144 RSI cycle driven by meta-diagnostics, v141 curriculum adjusted by
  σ_goedel, and a cross-system 1=1 verification pairing so σ_goedel can
  *shrink* through agreement with a second model.

## Honest claims

- **Is:** a calibrated strategy-awareness layer with three bias channels
  and an explicit non-verifiable residual.
- **Is not:** consciousness.  σ_goedel reports what the model **cannot**
  measure; it is a boundary, not a proof.

---

*Spektre Labs · Creation OS · 2026*
