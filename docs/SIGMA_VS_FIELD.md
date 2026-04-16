# σ vs the broader UQ / abstention field (positioning)

This note is **interpretive (I-tier)**: it maps external literature themes to Creation OS primitives. It is **not** an in-repo benchmark claim until archived repro rows exist (see `docs/WHAT_IS_REAL.md`).

## Core framing

Creation OS treats **σ** as a single interface for “should we trust this step / route / abstain?” Many adjacent lines of work name the same underlying question (uncertainty quantification, selective prediction, calibration, routing). The engineering goal is to keep **one coherent gate** while swapping in better estimators as they are validated **in-repo (M-tier)**.

The invariant story for coupling to control gain remains **`K_eff = (1 − σ) · K`** at the systems layer; estimators below feed scalar(s) that drive σ.

## Mapping table (literature ↔ shipped / planned hooks)

| Their theme (examples) | Our σ surface (repo) | Gap / discipline |
|--------------------------|----------------------|-------------------|
| Predictive entropy over a sequence (Kadavath et al. 2022) | `sigma_predictive_entropy_mean` in `src/sigma/channels_v34.c` | Needs per-token NLL stream from the engine; **M-tier** only once wired to real logits traces. |
| Semantic entropy via multi-sample clustering (Kuhn et al., ICLR 2023) | `sigma_semantic_entropy_cluster_equal` (exact-match cluster proxy) | Full embedding clustering is **not** shipped; proxy is a deterministic toy for harness / CI. |
| LogTokU / Dirichlet logit evidence (Ma et al. 2025) | `sigma_decompose_dirichlet_evidence` + `sigma_decompose_softmax_mass` in `src/sigma/decompose.c` | Shipped math is evidence-parameter style; paper-specific constants may differ — document when tightening. |
| EigenScore / representation spectral cues (Chen et al. 2024) | `sigma_eigenscore_hidden_proxy` (needs hidden states) | Returns **0** without hidden vectors; a future `--expose-hidden` style hook is **N-tier** until implemented. |
| Critical-token weighting (Duan et al. 2024; Bigelow et al. 2024) | `sigma_critical_token_entropy_weighted` | Requires a mask stream (heuristic today). |
| Selective abstention surveys (e.g., TACL “Know Your Limits” framing) | `sigma_abstain_gate` (`src/sigma/channels.c`) + v33/v34 routers | Survey is **E-tier** evidence; gate behavior is **M-tier** only where self-tested. |
| Uncertainty-aware routing / SLM–LLM fallback (e.g., `arXiv:2510.03847`) | `cos_route_from_logits` / `cos_route_from_logits_v34` (`src/v33/router.c`) | **M-tier** for routing self-tests; not a claim about external API models. |
| Entropy → calibration → RL pipelines (`arXiv:2603.06317` narrative) | `sigma_platt_*` (`src/sigma/calibrate.c`) + `config/sigma_calibration.json` | Coefficients are **placeholders** until fit from archived validation pairs `(σ_raw, was_correct)`. |

## Planned paper title (CLAIM_DISCIPLINE)

**“σ as Universal Uncertainty: Unifying Selective Abstention, Epistemic Decomposition, and Uncertainty-Aware Routing in a Single Invariant”**

Treat this as **aspirational copy**, not an **M-tier** publication claim, until benchmark tables ship with a repro bundle.

## v34 router semantics (epistemic vs aleatoric)

`cos_route_from_logits_v34`:

1. **Epistemic-like trip** (fallback/abstain): Platt `P(correct)` on raw epistemic scalar when calibration is valid, else raw epistemic threshold; **or** top-margin trip (auxiliary “first alert”).
2. **Aleatoric-only branch**: high aleatoric without (1) → `COS_ROUTE_PRIMARY_AMBIGUOUS` (“multiple valid interpretations” path).

This is deliberately conservative: ambiguous answers should not automatically look like “model ignorance” abstentions.
