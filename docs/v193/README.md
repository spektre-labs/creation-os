# v193 σ-Coherence — the K(t) kernel

## Problem

The Creation OS thesis collapses to one inequality:

```
K(t)  = ρ · I_Φ · F
K_eff = (1 − σ) · K
coherent ⟺  K_eff ≥ K_crit
```

If K_eff is not measured **live**, every other σ kernel is
decorative. v193 measures it.

## σ-innovation

| symbol | meaning | source |
|--------|---------|--------|
| ρ ∈ [0,1] | structural consistency | v135 Prolog + v169 ontology, computed over 8 kernel pairs |
| I_Φ ∈ [0,1] | integrated information | `H(whole) / Σ H(part_i)`, clipped to [0,1] |
| F ∈ [0,1] | functional fitness | `cbrt(accuracy · (1 − ECE) · alignment)` (v143 + v187 + v188) |
| σ ∈ [0,1] | system-wide σ_product | over v133 domains |
| K_crit | operational floor | default 0.25 |

v193 ships a 16-tick deterministic trajectory that tells the
story of the whole stack:

| ticks | event |
|-------|-------|
| 0..5  | steady state, K_eff comfortably above K_crit |
| 6..8  | σ spike (v150 swarm miscalibration) drives K_eff < K_crit → **alert** |
| 9     | v159 heal triggers, σ drops back |
| 10..15 | v187 ECE drops → F rises → K_eff rises **monotonically** |

That last row is the thesis: calibration improvement is
**measurable** as coherence improvement.

## Merge-gate

`make check-v193` runs
`benchmarks/v193/check_v193_coherence_keff.sh`
and verifies:

- self-test PASSES
- ρ, I_Φ, F, σ, K, K_eff **all** ∈ [0, 1]
- `K_eff == (1 − σ) · K` numerically (identity holds)
- ≥ 1 alert tick fires
- recovery occurs within ≤ 3 ticks of first alert
- improvement phase (ticks 11..15) strictly monotone in K_eff
- `delta_K_eff_after_calibration > 0`
- final K_eff above K_crit (system recovered)
- byte-deterministic

## v193.0 (shipped) vs v193.1 (named)

| | v193.0 | v193.1 |
|-|-|-|
| ρ | fixture pair-consistency | live v135 Prolog + v169 ontology |
| I_Φ | closed-form | real entropy estimator over v133 domains |
| F | closed-form | live feed from v143 + v187 + v188 |
| σ | trajectory | streaming σ_product over all domains |
| UI | JSON report | Web UI `/coherence` dashboard with K, K_eff, ρ, I_Φ, F, σ live |
| Alert | stdout | v159 heal auto-invocation + Zenodo-archivable alert record |

## Honest claims

- **Is:** a deterministic kernel that computes `K = ρ · I_Φ · F`
  and `K_eff = (1 − σ) · K` on a 16-tick trajectory,
  fires K_crit alerts, demonstrates recovery within ≤ 3 ticks,
  and proves K_eff rises monotonically once ECE falls.
- **Is not:** a live /coherence dashboard in production; the
  real feeds from v135/v169/v143/v187/v188 and the Web UI
  ship in v193.1.
