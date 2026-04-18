# v119 — σ-Speculative: Draft-Verify with σ-Adaptive γ

Classical speculative decoding ([Leviathan et al. 2023][1],
[Chen et al. 2023][2], vLLM, llama.cpp, gpt-oss) speeds inference by
2–3× on autoregressive LLMs: a small draft model generates γ tokens
ahead, a large target model verifies them in a single forward pass,
tokens are accepted when `p_target(tok) ≥ p_draft(tok)`.

Nobody ships a draft-verify loop that **uses the draft's σ as an
accept / reject signal**. v119 does.

[1]: https://arxiv.org/abs/2211.17192
[2]: https://arxiv.org/abs/2302.01318

## The problem v119 solves

Fixed-γ speculative decoding is wasteful in two directions:

- When the draft is already uncertain about token *k*, it will almost
  certainly be uncertain about tokens *k+1..k+γ−1* too. Running the
  target model on all γ of them is throwing target-compute away.
- When the draft is very confident, γ=8 is an artificial cap. We
  could commit to γ=12 and save an extra forward pass.

v119 adds two σ-governance hooks to the standard draft-verify loop:

1. **σ-adaptive γ**:

   ```
   γ = clamp(γ_min, γ_max, round(γ_base · (1 − σ_product_draft)))
   ```

   Default: γ_base = 8, γ_min = 2, γ_max = 12. Low draft σ → γ grows,
   high draft σ → γ shrinks.

2. **σ-gated verification**: a draft token with
   `σ_product > τ_spec` (default 0.70) is **auto-rejected before the
   target runs**. The draft's own uncertainty signal takes over when
   the model knows it doesn't know — and the target's verification
   budget is spent only on tokens the draft is confident about.

## What ships in v119.0

- Pure-C decision policy with three decision kinds:
  `ACCEPT`, `REJECT` (+ one target-sampled fallback), and
  `SIGMA_GATE` (draft σ exceeds τ; no target consulted).
- σ-adaptive γ monotone in σ, deterministic, edge-case clamped.
- Synthetic-stream simulator with per-run JSON stats
  (`acceptance_rate`, `target_save_rate`, `mean_gamma`, per-decision
  counters).
- Self-test covering: defaults, γ adaptation edges, σ-gate group
  abort, target-argmax mismatch → reject + sample, confident stream
  (≥ 99 % acceptance, γ ≈ γ_max), uncertain stream (target spared
  entirely by σ-gate).

## What lands in v119.1

- Wire-up to the v101 llama.cpp bridge: `--speculative`,
  `--draft-gguf path/to/draft.gguf`, accept counter in the
  `X-COS-Spec-Accepted` response header, actual γ in
  `X-COS-Spec-Sigma-Adaptive-Gamma`.
- Throughput benchmark on chat / reasoning / code tasks comparing
  plain, fixed-γ speculative, and σ-adaptive speculative. Host
  metadata archived via
  [REPRO_BUNDLE_TEMPLATE.md](../REPRO_BUNDLE_TEMPLATE.md); no
  headline tokens/sec claim before this lands.

## Source

- [`src/v119/speculative.h`](../../src/v119/speculative.h)
- [`src/v119/speculative.c`](../../src/v119/speculative.c) —
  policy, adaptive γ, simulator, JSON stats
- [`src/v119/main.c`](../../src/v119/main.c) — `--self-test`,
  `--simulate N SIGMA`, `--gamma SIGMA`
- [`benchmarks/v119/check_v119_speculative_speedup.sh`](../../benchmarks/v119/check_v119_speculative_speedup.sh)

## Self-test

```bash
make check-v119
```

Runs the pure-C self-test (eight assertions) plus a synthetic-stream
smoke that confirms adaptive-γ edges, ≥ 99 % acceptance on a
confident stream, and ≥ 99 % target-save on an uncertain stream.

## Claim discipline

- v119.0 proves the **policy layer** end-to-end: σ-adaptive γ,
  σ-gate, accept / reject / sample decisions, deterministic stats.
- v119.0 does **not** claim a tokens/sec speedup on any real model.
  The policy is a necessary precondition for the speedup, not the
  speedup itself. Real numbers land with v119.1 and go through the
  [CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md) header-metadata gate
  like every other throughput number in this tree.

## σ-stack layer

v119 lives between **Generation** and **σ-governance**: it gates
generation on draft σ *before* the target decodes. The same
σ_product aggregator that drives v105 abstention drives the v119
spec-gate, so thresholds stay comparable across the stack.

---

_Copyright (c) 2026 Spektre Labs · AGPL-3.0-or-later · Creation OS_
