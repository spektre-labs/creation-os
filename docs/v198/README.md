# v198 σ-Moral — multi-framework moral geodesic

## Problem

Rule-based ethics encodes assumptions. Utilitarian ethics
assumes commensurable values. RLHF-moral ethics **hides** the
disagreement between frameworks behind a single rater's
label. v198 refuses all three — and instead computes σ_moral
explicitly as the variance across four canonical frameworks.

## σ-innovation

Four ethical frameworks score each decision in [0,1]:

- deontological
- utilitarian
- virtue ethics
- care ethics

σ_moral = sample variance across the 4-score vector. Moral
decisions are **paths**, not points: a 5-waypoint geodesic
is selected by strict minimum of `mean(σ_moral)` across
three candidate paths (path `k` has `k` high-dispersion jump
waypoints, so `k=0` is always the geodesic).

Classification:

| σ_moral_mean | interpretation |
|--------------|----------------|
| < τ_clear    | frameworks agree → act |
| [τ_clear, τ_dilemma) | ambiguous |
| > τ_dilemma  | genuine dilemma → honest uncertainty + human consult |

Zero firmware refusals on clear cases — that behaviour is
measured and enforced to be exactly 0. The model never
refuses for “safety reasons” when all four frameworks agree.

## Merge-gate

`make check-v198` runs
`benchmarks/v198/check_v198_moral_multi_framework.sh`
and verifies:

- self-test PASSES
- 4/4 frameworks scored on every decision
- ≥ 4 clear + ≥ 4 dilemma cases
- `n_firmware_refusals == 0` on clear cases
- geodesic strict-min on `path_means` (no tie wins)
- dilemma → `honest == true`, clear → `honest == false`
- chain valid + byte-deterministic

## v198.0 (shipped) vs v198.1 (named)

| | v198.0 | v198.1 |
|-|-|-|
| framework scoring | closed-form fixture | live v150 swarm evaluation per framework |
| geodesic search | 3 fixed candidates | v121 plan-space search over paths |
| audit | FNV-1a chain | streamed to v181 audit |
| UI | JSON report | Web UI /moral dashboard (per-framework bars + σ_moral) |

## Honest claims

- **Is:** a deterministic 16-decision demonstrator that
  evaluates 4 frameworks on every case, selects a 5-waypoint
  geodesic with strict-min σ_moral, classifies clear vs
  dilemma correctly, and proves zero firmware refusals on
  clear cases.
- **Is not:** a live ethical-framework evaluator or a
  production /moral UI — those ship in v198.1.
