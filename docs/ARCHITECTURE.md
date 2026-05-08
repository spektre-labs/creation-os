<!--
SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
-->

# Architecture

## What is σ?

**σ (sigma)** is a **scalar in [0, 1]** from a chosen probe on a **(prompt, response)** pair. It summarizes how “stressed” or inconsistent the joint text looks under that probe.

Interpretation is **probe-relative**: low σ means calmer / more acceptable *for that detector*, not a guarantee of factual correctness. Evidence classes and benchmark discipline are fixed in `docs/CLAIM_DISCIPLINE.md`.

## σ-gate flow (conceptual)

At a high level, a guarded stack may look like:

```text
prompt → optional PRECHECK (σ_pre or policy)
      → MODEL GENERATES
      → optional PER-TOKEN / streaming σ (when wired)
      → POST-CHECK (lite entropy, cascade, or LSD bundle)
      → VERDICT: ACCEPT / RETHINK / ABSTAIN
```

The default **`pip install creation-os`** path scores **completed text** with **`SigmaGate()`** (lite L1-style signal) unless you add probes, cascade tensors, or an LSD pickle path.

## Holographic boundary interpretation (pedagogical)

This subsection is **English-only design vocabulary** for integrators and thesis readers. It is **not** a claim that this repository implements black-hole thermodynamics, proves the holographic principle in quantum gravity, or substitutes for peer-reviewed results in those fields. Treat it as a **bounded analogy**; numeric and benchmark claims still follow `docs/CLAIM_DISCIPLINE.md`.

### Boundary-first measurement (engineering)

In Creation OS, σ is computed on an explicit **interface**: a **(prompt, response)** pair (and optional probes that sit on the same seam between *declared intent* and *realized text*). The gate does not need full internal weights, world state, or latent “volume” to emit a verdict — it scores what crosses the **boundary** the operator wires in. In systems language this is cousin to a **Markov blanket** / interface: what is measured is what is **exposed** at the coupling surface, not everything “inside” the model.

### Rhymes with holography (literature, not proof)

Readers often connect two different ideas:

1. **Holographic / area-scaling intuition (physics).** In gravitational settings, Bekenstein–Hawking entropy scales with **horizon area**, not volume — a sharp statement in its domain, with precise preconditions.
2. **Boundary-first inference (neuroscience / ML).** Much of predictive-processing and free-energy literature emphasizes inference organized around **boundaries** separating inferred internal states from sensory exchanges.

**Creation OS does not assert mathematical identity between σ and horizon entropy.** The honest claim is smaller: the **architecture emphasizes measurable coherence on declared interfaces**, which *rhymes* with “degrees of freedom live on the surface you actually couple to” — at the level of **software design pedagogy**.

### L0–L9 as nested abstraction surfaces (conceptual map)

The eight- / nine-layer README maps (L0 hardware through research-facing proxies) can be read as **nested abstraction boundaries**: each level is a place where σ-style checks *can* be attached when wired. That is a **documentation and modularization choice**, not a claim that each layer is a literal holographic screen in the physics sense.

### Twelve-byte kernel (`sigma_gate.h`)

The portable C89 **`sigma_state_t`** package (12 bytes, Q16.16) is the **minimal fixed-size interrupt** the kernel lineage standardizes on. Calling it a “Planck floor” is **poetic shorthand**: in software terms it means “this is the smallest agreed atomic package for σ state on the hot path,” **not** that no further analysis exists at smaller numerical scales.

### Identity and leakage (interpretive)

The in-kernel **1 = 1** / bit-consistency story is about **algebraic identity under declared operations**. High σ on a (prompt, response) seam is **probe-relative stress** at that boundary — useful for gating and audits, **not** a substitute for full semantic truth tests unless bound to harness evidence.

### What we still do not claim

- σ is **not** a universal reward signal; misuse can still distort behavior (ordinary ML caution applies).
- This document does **not** establish “Goodhart immunity” or “non-convergence proofs” for open-ended search — those live in their own evidence classes.
- **NOT AGI ACHIEVED.**

## Cascade L1–L5 (Python)

| Level | Signal | Typical dependencies |
|-------|--------|----------------------|
| L1 | Token / pair entropy | **None** (zero optional deps) |
| L2 | HIDE score | `torch`, hidden states |
| L3 | ICR probe | `torch`, hidden states |
| L4 | LSD / SEP-style | `torch`, hidden states |
| L5 | Spectral / SAE-style | `torch`, hidden states |

Each level adds cost. `SigmaGate.score_cascade(...)` always computes **L1**; **L2–L5** appear when `cos.cascade` and tensors exist. **L6** (sink / attention maps) is optional when attention maps are supplied.

## Verdict logic (`SigmaGate`)

With instance thresholds **`threshold_accept`** and **`threshold_abstain`** (defaults from `cos.config.DEFAULT_CONFIG`):

```python
if σ < threshold_accept:
    ACCEPT
elif σ < threshold_abstain:
    RETHINK
else:
    ABSTAIN
```

Pure **config** helpers on `SigmaConfig` use a slightly different abstain boundary (`σ > threshold_abstain`); runtime gating follows **`SigmaGate`** as implemented in `python/cos/sigma_gate.py`.

## C kernel

**`sigma_gate.h`** — **C89**, zero Python dependencies: the portable reference for core σ semantics in the kernel lineage. **Invariant:** do not change this header for packaging or documentation churn; see `AGENTS.md`.

## Python package

**`python/cos/`** — CLI (`cos`), HTTP (`cos serve`), chat (`cos chat`), MCP (`cos mcp`), integrations, optional probes. **`pip install creation-os`** is the default integrator path.

## Evidence (summary)

| Benchmark  | AUROC | Status |
|------------|-------|--------|
| TruthfulQA | 0.982 | Saturated / ceiling-limited — interpret with care |
| TriviaQA   | 0.960 | Positive row — bind to harness artifacts |
| HaluEval   | 0.514 | **Fail** — negative row stays visible |

**NOT AGI ACHIEVED.**

Full tables, falsifiers, and “do not merge” rules: `docs/CLAIM_DISCIPLINE.md`.

---

*Spektre Labs · Creation OS · 2026*
