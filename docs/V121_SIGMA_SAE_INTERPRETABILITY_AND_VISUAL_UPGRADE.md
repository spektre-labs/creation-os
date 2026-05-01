# Creation OS v121 — σ-SAE interpretability + visual upgrade (plan)

**Language:** English-only maintainer prose per [LANGUAGE_POLICY.md](LANGUAGE_POLICY.md).

**Hard rules:** `sigma_gate.h` is **not** modified by this track. Python/Rust/TS are fair game. SPDX headers required on new files — `LicenseRef-SCSL-1.0 OR AGPL-3.0-only`.

**Evidence:** SAE paths are **lab / demo** class per [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md). Do not headline-merge toy σ with harness AUROC. Paper IDs below are **orientation** for API shape, not in-tree reproduction claims.

---

## Part 1 — σ-SAE stack (implemented scaffolds)

| Area | Module / surface | Notes |
|------|------------------|------|
| SAE + CB-shaped audit | `python/cos/sigma_sae.py` | `aggregate_sigma_sae`, `clarify_scores`, `plan_cb_sae_prune`, `SIGMA_CONCEPT_BOTTLENECK` |
| Steering + correlation | `python/cos/sigma_steering.py` | Direction steering, Pearson correlation series, recurrence ledger for patch hints |
| Cascade L5 JSON | `python/cos/sigma_gate_sae_signal.py` | `compute_l5_sae_signal` — gated abstain hint + attribution |
| Cascade labels | `python/cos/sigma_cascade.py` | `CASCADE_LEVELS` L1–L5 metadata |
| CLI | `cos interpret` | `--decompose`, `--steer`, `--steer-permanent`, `--feature-map`, `--explain`, `--correlate`, `--mock` |

**Bibliography (orientation):** sparse-autoencoder steering / validation papers (e.g. training-free steering, task-activation correlation, clarification behaviors, concept bottleneck + prune, persistent edits, RAG-friendly feature interfaces) inform **names and JSON fields**, not shipped benchmarks.

**Follow-on (not claimed done):** train real probes, SALVE-style weight surgery, SAFE/Engram fusion, CorrSteer-scale corpora, Puppeteer PNG export parity with README.

---

## Part 2 — React / shadcn-style diagrams

Target stack: React + Tailwind (dark Spektre tokens) + Recharts + Framer Motion + Lucide. **Dark only.** Prefer Radix primitives when adding shadcn components; no MUI/Chakra/Ant.

Layout: `web/spektre-diagrams/` — Vite + Vitest; one suite per diagram component. Optional Playwright/Puppeteer script for static PNGs (README/social) once visuals match.

Components (spec): `HeroOverview`, `SigmaCore`, `GateFlow`, `SignalCascade`, `OmegaLoop`, `EngramMemory`, `EvidenceLadder`, `EvalDashboard`.

---

## Verification

- Python: `PYTHONPATH=python python3 -m pytest tests/test_sigma_*.py` (torch required for SAE tests).
- CLI smoke: `PYTHONPATH=python python3 -m cos interpret --mock --decompose`.
