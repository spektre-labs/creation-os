# Reddit draft — r/LocalLLaMA (v2) — **DO NOT POST** — Lauri review

**Title:** I built a local AI runtime that measures its own uncertainty — and it actually separates easy from hard questions. Here's the data.

**Body:**

9 days ago I posted about Creation OS. Since then:

- Switched from Qwen3.5 to gemma3:4b (non-thinking)
- Built semantic entropy σ measurement into the pipeline
- Verified separation: easy questions σ_combined ≈ 0.32, hard (P vs NP) σ_combined ≈ 0.55 on the same pipeline revision

The core idea: every AI answer gets a confidence score (σ). Low σ = model is confident. High σ = model is uncertain. When σ is too high, the system can **ABSTAIN** instead of hallucinating.

Results on **gemma3:4b** (4B model), **MacBook Air M4 8GB**, **Ollama** (`localhost:11434`):

| Category | σ_mean (15-prompt pipeline) | Interpretation |
|----------|----------------------------|------------------|
| FACTUAL | 0.384 | Tight cluster with CREATIVE |
| CREATIVE | 0.383 | Tight cluster with FACTUAL |
| REASONING | 0.497 | Mid band |
| SELF_AWARE | 0.608 | Higher uncertainty |
| IMPOSSIBLE | 0.643 | Highest uncertainty |

**Category spread (max − min mean):** **0.260** (chart threshold check: > 0.15).

**Two-prompt spot check (σ_combined):** EASY **0.318**, HARD **0.553**, gap **0.235**.

How it works:

- Generate 3 answers at different temperatures (primary + extras at **0.1** and **1.5**, short direct-answer system on extras)
- Compare answers (**Jaccard** on **first-sentence** extraction after normalization)
- If answers agree → lower semantic σ → more confident
- If answers disagree → higher semantic σ → more uncertain  
- Shadow line in verbose output: **σ_combined = 0.7·σ_semantic + 0.3·σ_prior** (see `cos` verbose receipts)

Try it (clone + build + demo):

```bash
git clone https://github.com/spektre-labs/creation-os
cd creation-os && make cos cos-demo && ./cos demo
```

What I'd love feedback on:

1. Does this approach generalize to larger models?
2. Better similarity metrics than Jaccard on first sentences?
3. Would you use σ in your local setup?

Pure C kernel focus; local stack details in-repo. Monthly electricity note for hybrid inference is cited in **`README.md`** (€4.27/month at stated call volume — see repo; not re-derived here). Proof badges (**Lean 6/6**, Frama-C scope) are **repository claims** on `README.md` — not re-proven in this draft.

**GitHub:** https://github.com/spektre-labs/creation-os

---

*Draft only. Lauri decides if/when to post. Numbers above match `benchmarks/sigma_separation/chart.txt`, `SEPARATION_REPORT.md`, and the evolve `sigma_trajectory.csv` from the 2026-04-25 run.*
