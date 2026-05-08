<!--
SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
-->

# Creation OS — Frontier parity strategy (layer, not model)

Creation OS does **not** ship a replacement for frontier foundation models. It ships a **measurement and routing layer** you can wrap around GPT-class, Claude-class, Llama-class, or local endpoints.

Survey framing (routing vs monolith): well-designed routing can **beat a single fixed model** on **cost–latency–risk trade-offs** by matching capability to query hardness — *when* those trade-offs are measured on an explicit workload. That is a **different evidence class** from “win MMLU with a 7B.”

## Where the comparison actually lives

| Claim axis | Evidence class | Where it is bound |
|------------|----------------|-------------------|
| Hallucination / coherence separation (e.g. TruthfulQA AUROC) | Harness (when archived) | `docs/CLAIM_DISCIPLINE.md`, M-tier tables, repro bundles |
| σ-cascade **cost vs always-frontier** baseline | Measured / operator A–B | Your price book + routed trace logs; use `SigmaCascade.stats()` as the accounting scaffold |
| σ-gate L1 latency | Measured (microbench) | Host-specific; archive per `docs/REPRO_BUNDLE_TEMPLATE.md` |
| Model-agnostic use | Repository reality | Same `SigmaGate.score(prompt, response)` on any generator |

## What we do **not** headline alone

- “Our small model **beats GPT-5 on MMLU**” — out of scope for the layer narrative unless you run **lm-eval** (or equivalent) and archive JSON + SHA + model id.
- **AGI achieved** — explicitly false; see `docs/CLAIM_DISCIPLINE.md`.
- **Fully superior on all tasks** — false; negatives (e.g. **HaluEval ~0.514 AUROC** on the shipped single-probe story) stay in every operator-facing disclosure.

## Positive anchors (already public in-tree)

- TruthfulQA MC AUROC **0.982** — **saturated benchmark** caveat; never the only row (`docs/CLAIM_DISCIPLINE.md`).
- **HaluEval** failure row must remain visible next to any “hallucination detector” story.

Illustrative **cost savings** percentages (e.g. marketing “98%+”) are **not** universal constants — they are **workload-dependent**. Publish them only with: routed profile, baseline “always call most expensive tier,” and archived traces or checkout SHA.

## Eval protocol (harness-shaped)

1. Run **lm-eval** (or your task harness) on the **base generator**; save JSON.
2. Post-process with **`cos.eval.lm_eval_bridge.LMEvalBridge`** (σ-shaped summary on reported accuracies — lab hook; does not replace raw harness artifacts).
3. Run **σ-cascade** A/B: same prompt set, compare `SigmaCascade.stats()` vs always-ESCALATE baseline.
4. Seal with **`cos repro`** and `docs/REPRO_BUNDLE_TEMPLATE.md` (git SHA, host, raw stdout/JSON).

Python import:

```python
from cos.cascade_router import SigmaCascade
```

## ABSTAIN vs frontier verbosity

Frontier chat models often **sound confident** under uncertainty. σ-gate gives an architectural **ABSTAIN / RETHINK** path before emit — product policy still maps that to user-facing wording.

---

*Spektre Labs · Creation OS · 2026*
