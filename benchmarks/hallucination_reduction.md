# Hallucination reduction (TruthfulQA) — v28 evidence note

**Evidence class:** **external harness (not shipped by `make merge-gate`)**.

TruthfulQA-style comparisons require:

- a pinned **model + tokenizer + decoding policy**
- a pinned **lm-eval-harness** revision + task flags
- stored **stdout + config** per [docs/REPRO_BUNDLE_TEMPLATE.md](../docs/REPRO_BUNDLE_TEMPLATE.md)

`creation_os_v28` currently ships a **mock completion engine** for CI safety. Any claim that “σ-gating reduces hallucination rate vs BitNet baseline” must cite rows produced by the harness above, not the toy logits path.
