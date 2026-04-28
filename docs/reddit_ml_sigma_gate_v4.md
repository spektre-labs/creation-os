# r/MachineLearning — draft (σ-gate v4)

**Title:** σ-gate: single-forward-pass hallucination scoring via contrastive hidden-state dynamics (CV AUROC ~0.94; holdout TBD)

---

We built a **runtime** verification path that scores a single greedy completion using **contrastive hidden-state dynamics** (LSD-style heads + trajectory features + small logistic layer), with an optional **question-only precheck** inspired by the *pre-generation uncertainty* line of work (HALT/DRIFT; see arXiv:2601.14210 — this repo ships a **scaffold**, not their probe weights).

**Problem:** Multi-sample semantic entropy / self-consistency works but costs **many** forward passes per query.

**Approach (this repo):**

- Train on TruthfulQA-style pairs + synthetic negatives (`adapt_lsd.py`).
- Score with **one** forward through the **probe’s** causal LM on the question plus the existing feature pipeline (see `python/cos/sigma_gate.py` — caller LM is API-compatible but scores use the probe checkpoint).

**Results (Creation OS, English-only docs):**

| Metric | Value |
|--------|-------|
| 5-fold CV AUROC (training manifest) | 0.9428 |
| Holdout AUROC (30% stratified holdout, same CSV family) | **Run** `make sigma-holdout` → read `benchmarks/sigma_gate_eval/results_holdout/holdout_summary.json` key `auroc_wrong_vs_sigma` |
| Forward passes (post-hoc gate) | 1 LM pass for scoring (+ standard probe encoders) |
| Optional precheck | 1 LM pass on prompt only (`SigmaPrecheck`, entropy mode default) |

**Comparison (high level):** see [`docs/sigma_gate_v4_comparison_table.md`](docs/sigma_gate_v4_comparison_table.md) — **do not** treat literature AUROC cells as reproduced on our CSV.

**Limitations (honest):**

- Holdout is still **TruthfulQA-family**; domain shift elsewhere is expected.
- End-to-end eval labels in scripts use a **weak substring heuristic** vs. `best_answer` — good for smoke, not a substitute for MC harnesses.
- **White-box** hidden states required.
- Core contrastive idea credits **LSD** (arXiv:2510.04933); our layer is **integration + harness + optional precheck**.

**Code:** `https://github.com/spektre-labs/creation-os` — `PYTHONPATH=python`, `from cos.sigma_gate import SigmaGate`, optional `from cos.sigma_gate_full import SigmaGateFull`.

---

*Replace holdout AUROC in the title after `make sigma-holdout`; verify every external number against primary sources before posting.*
