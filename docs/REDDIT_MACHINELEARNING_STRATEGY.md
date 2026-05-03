# r/MachineLearning posting strategy — Creation OS

**Language:** English only (repository policy).

This file complements **[CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md)** for **community** threads: how to present σ-gate without headline gaming.

## Do not lead with a single number

- **TruthfulQA AUROC 0.982 alone** is a **disallowed** hook: the benchmark is **saturated** (2024+) and shortcut baselines exist; lead with **multi-benchmark context** instead.

## Required elements in every post

1. **Full M-tier table** (v2): `cos bench --mtier` or `SigmaBench().mtier_v2()` JSON → markdown via `SigmaReport.generate_mtier_table`.
2. **Multi-model σ table** after a host run: `cos bench --multi-model --models gemma3-4b,qwen3.6-a3b --n 30 --dataset truthfulqa[,simpleqa]` → paste the printed markdown (includes **Known failures** / HaluEval 0.514).
3. **Visible saturation flags** for TruthfulQA and length-solvability notes for HaluEval-style tasks.
4. **Negative row**: **HaluEval QA AUROC 0.514** (or current archived harness value) — **never** omitted.
5. **Pending rows**: SimpleQA, FACTS Grounding, FaithDial — say **pending** until harness JSON is archived.
6. **Detector vs generator**: σ-gate scores **existing** (prompt, response) pairs; it does **not** replace the LM.
7. **Dynamic / held-out option**: point to `cos.eval.dynamic` versioned pools + SHA-256 set id for fresh questions.

## Boilerplate line

> **NOT AGI ACHIEVED.** Lab / harness metrics are evidence-class labeled; negatives are part of the evidence ladder.

## References

- [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) — sections 6–8 (what we claim / do not claim / ladder).
- [REPRO_BUNDLE_TEMPLATE.md](REPRO_BUNDLE_TEMPLATE.md) — archiving numbers.

*Spektre Labs · Creation OS · 2026*
