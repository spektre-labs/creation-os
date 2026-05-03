# Architecture — σ-gate (Creation OS)

**σ (sigma)** is a **scalar in [0, 1]** scoring how “stressed” or unreliable a **(prompt, response)** pair looks under a chosen probe. **Lower σ** usually means calmer / more acceptable for that probe; **higher σ** means more skeptical.

Creation OS separates:

1. **Detectors** (σ-gate, probes) — score existing text.
2. **Generators** (your LLM) — produce text; σ does not replace them.

---

## Core principle

σ measures a **proxy signal** derived from the prompt+response (lite: character-level entropy; full tree: hidden states, spectral helpers, LSD bundles, …). It is **not** a universal truth label: benchmark and evidence-class discipline lives in `docs/CLAIM_DISCIPLINE.md`.

---

## Cascade L1–L6 (Python)

When you call `SigmaGate.score_cascade(...)`:

- **L1 — Entropy (always):** training-free, **zero optional dependencies**; runs everywhere `pip install creation-os` works.
- **L2–L5 — Optional:** HIDE / ICR / LSD / SAE-style paths require **`cos.cascade`** and **tensor inputs** (Torch not in the default wheel).
- **L6 — Optional:** sink / spectral attention map path when provided.

If higher-level signals are missing, the implementation **falls back** to the strongest available level (usually **L1** in lite installs).

---

## Verdict logic (default thresholds)

Approximate policy on `SigmaGate()`:

- `σ < tau_accept` → **ACCEPT**
- `σ > tau_abstain` → **ABSTAIN**
- else → **RETHINK**

Defaults: `tau_accept=0.3`, `tau_abstain=0.7` (adjust per deployment).

---

## C reference kernel

The **portable C lineage** (bit-geometry σ, invariants, versioned `creation_os_v*.c` drivers) is the kernel’s reference math surface. **Policy:** `sigma_gate.h` and the canonical kernel files are **release-frozen** — do not edit for packaging or docs churn; see maintainer rules in `AGENTS.md`.

The Python package (`python/cos/`) ships **CLI**, **ASGI server**, **integrations**, and optional **probe** loaders; `pip install creation-os` is the default path for integrators.

---

## Where to read next

- `docs/QUICKSTART.md` — first σ score in minutes.
- `docs/CLAIM_DISCIPLINE.md` — claims, negatives, benchmarks (TruthfulQA saturation, HaluEval 0.514, etc.).
- `docs/SUPPORTED_PATH.md` — production-supported surfaces vs non-claims.

---

*Spektre Labs · Creation OS · 2026*
