# Domain analysis — where σ-gating helps (and where it does not)

**Audience:** maintainers shipping σ-gated chat, benchmarks, or papers.  
**Evidence class:** conceptual map + pointers to archived harness rows — not a substitute for `lm-eval` JSON. See [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md).

---

## 1. What this document is for

Creation OS separates **control-plane policy** (when to answer, rethink, abstain, or escalate) from **generator quality** (what the model knows). A σ-gate can be mechanically correct while the underlying LM is weak on a task. This note names **domains**, not headline accuracy claims.

---

## 2. Domains where a σ-style gate tends to align with user value

- **Factual / closed-form prompts** where correctness is checkable (e.g. arithmetic, short definitions, code syntax) — uncertainty often tracks **epistemic** gaps (missing fact) more than random label noise.
- **Code completion–shaped tasks** where the model can be scored against tests or static checks — abstention is preferable to silently shipping broken code.
- **Deterministic stub harnesses** (`cos_cli_stub_generate`, `make check-cos-chat`) — these prove wiring, not LM SOTA.

---

## 3. Domains where σ alone does not solve the problem

- **Commonsense and open-domain chat** — high variance, shallow correctness signals, and sycophancy pressure; σ must be paired with task-specific evaluation.
- **Adversarial / instruction-mixing prompts** — the failure mode is often **policy** and **input filtering**, not a scalar uncertainty readout.
- **Long-horizon reasoning under distribution shift** — aleatoric noise dominates; selective prediction must be validated per task, not assumed from a single σ aggregate.

---

## 4. Epistemic vs aleatoric (operational gloss)

- **Epistemic uncertainty:** the model lacks knowledge or evidence; abstain-or-verify behaviour is often appropriate.
- **Aleatoric uncertainty:** inherent ambiguity or label noise; a gate may calibrate risk but cannot invent signal. Treat harness rows as **task-specific** evidence, not universal transfer.

---

## 5. Harness anchors already documented in-tree (do not merge evidence classes)

| Topic | Where it lives | Takeaway |
|--------|----------------|----------|
| TruthfulQA MC2, Bonferroni-corrected σ vs entropy | [WHAT_IS_REAL.md](WHAT_IS_REAL.md), [docs/v104/RESULTS.md](v104/RESULTS.md) | Pre-registered operator search: **p = 0.0005** class results on `truthfulqa_mc2` for several σ summaries; **channel-level** effects can **help or hurt** on other tasks in the same study. |
| HellaSwag (negative transfer example) | [docs/v111/THE_FRONTIER_MATRIX.md](v111/THE_FRONTIER_MATRIX.md) | Reported **Δ = +0.0496** (σ-sidecar worse than baseline on that row) with honest p-value — use as a **falsifier template**, not a slogan. |
| MMLU / multi-choice floors | [ANALYSIS.md](ANALYSIS.md) (MMLU / parity protocol forward refs) | Closed-set MC near random for small models is a **design effect**; do not confuse microbench throughput with harness rows. |
| v102 harness scope | [docs/v102/RESULTS.md](v102/RESULTS.md) | States explicitly which tasks were **not** claimed in a given pass. |

---

## 6. Repository artefacts for “Codex on vs off” on the **stub** harness

- `cos benchmark --emit-comparison benchmarks/codex_comparison.json`  
- Evidence label inside the JSON: **stub fixture**, not frontier accuracy. When the Codex bytes are absent, `pipeline_codex` matches `pipeline_no_codex` by construction.

---

## 7. Honest bottom line

σ-gating is a **governance and risk** layer: it makes doubt visible and composes with engrams, sovereign cost, and escalation policy. It is **not** a guarantee of commonsense, truthfulness, or harness leaderboard dominance without per-task measurement archived under the repro bundle rules in [REPRO_BUNDLE_TEMPLATE.md](REPRO_BUNDLE_TEMPLATE.md).
