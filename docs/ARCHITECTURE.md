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
