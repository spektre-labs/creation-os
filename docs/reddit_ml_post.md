# [DRAFT — DO NOT POST] σ-gate: 12-byte C kernel + LSD runtime score (MCP envelope)

**Status:** *Draft only.* Do **not** post to r/MachineLearning (or elsewhere) until **Gemma eval**, **HIDE eval**, and **calibration** artifacts you intend to cite are archived and checked against [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md).

Suggested title (edit after final numbers):

> σ-gate: runtime hallucination-oriented scoring for LLM completions (MCP JSON envelope + 12-byte C interrupt)

---

## Problem

Decoding stacks optimize for fluency and latency; **per-completion reliability** is usually assessed **offline** or with **expensive** judges. There is no single MCP-standard field today that exposes a **calibrated, machine-readable** epistemic risk payload alongside agent output.

## Method (Creation OS lab)

1. **LSD probe** — contrastive trajectory features + logistic head over a **frozen** causal LM checkpoint (see `benchmarks/sigma_gate_lsd/`). Produces σ ∈ [0,1] (hallucination-oriented risk) on `(prompt, completion)`.  
2. **Cognitive interrupt** — `python/cos/sigma_gate.h` + `sigma_gate_core.py`: **ACCEPT / RETHINK / ABSTAIN** from Q16.16 state (12 bytes in C).  
3. **MCP** — `python/cos/mcp_sigma_server.py` returns a **structured envelope** on every tool: `result`, `sigma` (`value`, `verdict`, `d_sigma`, `k_eff`, `signals`), `metadata` (`gate_version`, SPDX license).  
4. **Training-free channel** — `SigmaHIDE` (HSIC sketch) for scoring without the LSD pickle when configured (see `benchmarks/sigma_gate_eval/run_hide_eval.py`).

## Results (only what is already in archived JSON)

| Harness | Generator | Metric | Value |
|---------|-----------|--------|------:|
| TruthfulQA holdout | GPT-2 | AUROC (wrong vs σ) | **0.982** |
| TriviaQA validation smoke | GPT-2 | AUROC (wrong vs σ) | **0.960** |
| TruthfulQA holdout | GPT-2 | Wrong + confident | **0** |

Artifacts: `benchmarks/sigma_gate_eval/results_holdout/holdout_summary.json`, `benchmarks/sigma_gate_eval/results_cross_domain/cross_domain_summary.json`.

**Do not** paste **routing savings %** or **abstain rate** here until the exact JSON key you will cite exists in a committed repro bundle (see [REPRO_BUNDLE_TEMPLATE.md](REPRO_BUNDLE_TEMPLATE.md)).

## Limitations (explicit)

- Shipped LSD pickle is **checkpoint-specific** (manifest in pickle); other LMs need retraining / adaptation.  
- σ is **not** a cryptographic watermark and **not** a guarantee of EU legal compliance by itself — see [EU_AI_ACT_COMPLIANCE.md](EU_AI_ACT_COMPLIANCE.md) (supporting doc, not legal advice).  
- MCP audit ring is **in-process** — not a tamper-evident compliance log.

## Code & docs

- Repo: [https://github.com/spektre-labs/creation-os](https://github.com/spektre-labs/creation-os)  
- MCP: [MCP_SIGMA.md](MCP_SIGMA.md), [MCP_LISTING.md](MCP_LISTING.md)  
- σ kernel README section (install / benchmarks): root `README.md`

## License

`LicenseRef-SCSL-1.0 OR AGPL-3.0-only` — see repository `LICENSE`.
