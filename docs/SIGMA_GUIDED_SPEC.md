# σ-guided speculative decoding (v35 positioning)

This document is **interpretive (I-tier)** for external inference literature (speculative decoding, EAGLE / Mirror-SD class work, distributed edge–cloud drafts). **M-tier** begins only where `make check-v35` and future archived harness rows exist.

## Claim discipline (paper title)

**“σ-Guided Speculative Decoding: Adaptive Draft Length and Dual-Model Abstention via Epistemic Uncertainty Decomposition”**

Treat as **aspirational** until benchmark tables + repro bundle land (`docs/WHAT_IS_REAL.md`).

## What shipped in-repo (v35 lab)

- **Adaptive draft length** from epistemic uncertainty using a monotone unit score  
  `u = σ_epistemic / (1 + σ_epistemic)` and thresholds `(0.7, 0.4, 0.2) → K ∈ {0,2,4,8}` (`src/v35/spec_decode.c`).
- **Dual-model verify policy hook**: accept draft token only if verifier agreement **and** verifier epistemic stays below a threshold; otherwise reject or abstain (`cos_spec_verify_with_dual_sigma`).
- **Progressive routing tiers** on the same unit score: local-only vs local spec-decode vs API escalation (`cos_spec_route_tier`), driven by `config/spec_routing.json`.

## Field positioning (high level)

| They do (typical) | We do (Creation OS v35) | Practical difference |
|-------------------|-------------------------|----------------------|
| Fixed draft length (e.g., K=4) | σ-adaptive K capped by `max_draft_tokens` | Fewer needless rollbacks on hard tokens (when wired to a real draft engine). |
| Binary accept / reject | Add **abstain** when verifier epistemic is high | Avoids “two wrongs confidently agree” failure mode at the policy layer. |
| Single deployment story | Same σ unit score feeds **local → spec → API** escalation | Matches Creation OS routing story; network only when needed. |
| Verifier is a black box | Verifier must expose logits (or epistemic proxy) for σ | Requires engine hooks (`N-tier` until exposed). |

## Distributed speculative decoding (edge + cloud)

Edge draft + cloud verifier is an **architectural fit** for Creation OS (BitNet local, verifier localhost/LAN/API). The **ScienceDirect / AI-RAN** line is **E-tier** unless cited with a stable URL in your public materials; do not mark it **M-tier** here.

## Config

See `config/spec_routing.json` for default thresholds. Draft/verifier executables remain in the v33 model registry / environment until a merged JSON schema is adopted.
