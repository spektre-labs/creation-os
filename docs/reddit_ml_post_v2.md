# r/MachineLearning draft — sigma-gate v4 (LSD contrastive, single forward pass)

**Title:** sigma-gate: runtime hallucination screening with ~0.94 AUROC (TruthfulQA-style probe) using a **single** LM forward pass for the detector

**Body:**

Large language models still fabricate plausible false statements. Practical stacks often rely on **self-consistency**, **semantic entropy**, or similar checks that require **many extra generations** per query — fine for offline eval, painful for latency- and cost-sensitive production.

We ship a **contrastive hidden-state probe** inspired by **Layer-wise Semantic Dynamics (LSD)** ([arXiv:2510.04933](https://arxiv.org/abs/2510.04933) — *The Geometry of Truth*): train lightweight projection heads so trajectory statistics over hidden states separate factual vs. fabricated answer strings paired with the same question, then add a small logistic layer on hand-crafted trajectory features.

**What we measured (Creation OS repo, English-only docs):**

- **Training / CV (held-out folds on curated pairs):** AUROC **0.9428** (5-fold CV mean on TruthfulQA-derived + synthetic negatives; see `benchmarks/sigma_gate_lsd/results_full/manifest.json`).
- **End-to-end script:** `benchmarks/sigma_gate_eval/run_eval.py` scores **greedy GPT-2** completions on the bundled **TruthfulQA200** CSV and writes `benchmarks/sigma_gate_eval/results/eval_summary.json` (AUROC there is **not** the same harness as MC2; labels use a conservative substring heuristic vs. `best_answer`).

**Why “single forward pass” matters:** at scoring time the detector runs **one** forward through the **probe’s frozen causal LM** (same family as training — mismatch otherwise) plus the probe’s fixed encoders/heads. That is **one** heavy LM pass **per scored completion**, not 5–20 sampling rounds.

**Wire-ish output:** the bundled packer emits **12 bytes** (versioned header + `float` sigma + `float` tau slot) compatible with the repo’s `cos_sigma_measurement_t` layout — see `python/cos/sigma_gate.py` and `benchmarks/sigma_probe_lsd/sigma_gate_lsd.py`.

**Honest limitations (please read before roasting):**

- The probe is trained on **TruthfulQA-style** factual/refusal/incorrect contrasts; **out-of-domain** topics, long-context chat, tool-augmented agents, and different model families need **fresh calibration or retraining**.
- The end-to-end AUROC on free-form greedy text **depends on the weak automatic label** (substring vs. reference); treat it as a **smoke harness**, not a replacement for human eval or MC2 harness numbers.
- RLHF / instruction tuning shifts the hidden-state manifold; **porting the same heads to a new base model without retraining is unsupported.**

**Code:** [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) — integration entrypoint `python/cos/sigma_gate.py` (`PYTHONPATH=python`), probe pickle under `benchmarks/sigma_gate_lsd/results_full/`.

**Acknowledgment:** Sirraya / LSD authors for the public contrastive trajectory framing; any mistakes in our adapter layer are ours.

---

*Draft for editorial pass; numbers must match `manifest.json` / `eval_summary.json` at post time.*
