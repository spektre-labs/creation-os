<!--
SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
-->

# σ-Coherence in Wolfram Language

Creation OS includes a **Wolfram Language** lab notebook-style script that implements a **σ-coherence persistence** toy model (elementary cellular automaton + post-step noise). It is **pedagogy and analogy**, not a replacement for the Python σ-gate, harness benchmarks, or silicon claims — see [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md).

## Run in Wolfram Cloud (free, no install)

1. Go to [https://www.wolframcloud.com](https://www.wolframcloud.com).
2. Create a free account (or sign in).
3. Upload [`scripts/wolfram/sigma_persistence.wl`](../scripts/wolfram/sigma_persistence.wl).
4. Evaluate the package and demo cells (or run the entire file).

## What it demonstrates

- Coherent-looking configurations can **persist** under low noise coupling **k**.
- **Higher distortion** (larger **k**) drives the lattice toward absorbing all-0 / all-1 states ("collapse").
- A **Monte Carlo sweep** over **k** shows a **sharp crossover** in mean survival; the exact location depends on rule, lattice size, and horizon — literature-style critical couplings on related models are often **O(10⁻¹)**; tune `demoRule`, `demoN`, and ranges in the script to explore **K_crit ≈ 0.127**-class behavior.
- **Observer window** functions (`observe`, `sigmaMeta`) illustrate **finite observation** without changing the underlying CA dynamics.
- **Different Wolfram rules** yield different persistence profiles (change `demoRule`).

## Connection to Creation OS

| Wolfram Language | Python (`cos`) | Concept |
|------------------|----------------|---------|
| `sigma[state]` | `SigmaGate().score(...)` (scalar readout) | σ measurement |
| `coherence[state]` | `1 - σ` (informal) | **K(t)**-style coherence |
| `verdict[state]` | `SigmaConfig().verdict(σ)` | banded outcome (here: PERSIST / UNSTABLE / COLLAPSE) |
| `observe[state, w]` | `SigmaConscious.predict_own_σ(...)` | bounded observer (different mechanism; same *role*) |
| `sigmaMeta[state, w]` | `SigmaConscious.σ_meta()` | second-level spread / calibration stress |

Verdict strings in the script are **Wolfram-lab labels** mapped to the same accept / rethink / abstain **bands** as `SigmaConfig` defaults (0.15 / 0.85), not a second gate implementation.

## Papers

- Paper #81: Coherence Produces Resonance
- Paper #82: What Is σ?
- Paper #83: Coherence Not Reasoning
- Full corpus: [https://github.com/spektre-labs/corpus](https://github.com/spektre-labs/corpus)
