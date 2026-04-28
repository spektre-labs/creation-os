# Reddit draft — σ-gate (Creation OS)

**Proposed title:** σ-gate: single-forward-pass hallucination scoring via contrastive hidden-state dynamics (holdout AUROC ~0.98 on TruthfulQA-style smoke; TriviaQA cross-task ~0.96)

---

We integrated an **LSD-style** contrastive probe ([arXiv:2510.04933](https://arxiv.org/abs/2510.04933)) as a runtime σ gate: one scoring forward through a frozen GPT-2 probe checkpoint + trajectory features, **MiniLM-L6-v2** truth encoder, **12-byte** packed measurement (`python/cos/sigma_gate.py`).

**Measured numbers (this checkout, JSON artefacts — not a harness leaderboard):**

| Benchmark | AUROC (wrong vs σ) | Notes |
|-----------|-------------------|--------|
| TruthfulQA CV (manifest) | ~0.943 | In-distribution training CV |
| TruthfulQA holdout | ~0.982 | 30%-style split; semantic labels + PRISM prompt for greedy GPT-2 |
| TriviaQA (100 prompts) | ~0.960 | Greedy GPT-2 + semantic match to aliases |
| HaluEval `qa_samples` (generative) | ~0.38 | **Honest:** oracle-string probe was misleading; generative eval scores **model-generated** answers with a label rule coupling HF `hallucination` + cosine vs `answer` — still a smoke harness, not a HaluEval paper repro |

**Why hidden states:** sampling-based uncertainty stacks (semantic entropy, SelfCheckGPT-style) pay a **multi-generation** tax; RLHF can homogenize surface text while inner activations still carry separable structure — we are **not** claiming universal truth; we report AUROCs on the stated slices only.

**What σ-gate outputs:** σ ∈ [0, 1] plus {ACCEPT, RETHINK, ABSTAIN}; wire layout is fixed-size bytes for downstream COS plumbing.

**Limitations (up front):**

- GPT-2-scale probe path in the bundled repro; larger checkpoints are future work.
- Short-form QA smokes only; no long-form / medical / legal claims.
- HaluEval generative row is **weak** today — we publish it anyway to avoid cherry-picking.
- **Not** a substitute for `lm-eval` harness tables; see repo [`docs/CLAIM_DISCIPLINE.md`](CLAIM_DISCIPLINE.md).

**Code:** [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) — run `bash run_holdout_pipeline.sh` / `bash run_ship.sh`, read `benchmarks/sigma_gate_eval/REPRO_CHECKSUMS.txt` after locking.

**Feedback we want:** which additional QA slices to run; whether TriviaQA smoke transfers to your HF model family; interest in Llama/Gemma checkpoints with the same CSV protocol.

**Acknowledgments:** Sirraya LSD / *Geometry of Truth* for the contrastive recipe; Kuhn / Farquhar lines for semantic-entropy context; Liu et al. (arXiv:2603.24124) for RLHF–uncertainty discussion *as literature*, not reproduced here.
