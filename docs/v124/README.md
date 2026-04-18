# v124 σ-Continual — on-device living weights

> Living Weights / TTT-E2E frameworks keep promising "weights that
> adapt while the model is being used".  Creation OS makes it a
> policy the merge-gate enforces, not a marketing claim.

## Problem

`v111.3` ran one supervised fine-tune on boot-up.  `v120` ran one
σ-targeted distillation pass.  Neither ever ran again.  In the real
world an operator uses the model for days, σ_profile evidence
accumulates, and the model should keep learning — without sending
anything off-device and without destabilizing the baseline.

## What ships in v124.0

A **pure-C policy + buffer + forgetting-smoke** state machine.  The
kernel decides *when* to train, *what* to train on, and *whether to
roll back* when the weights drift.  It does not (yet) perform the
MLX LoRA update itself — that's v124.1.

Four pieces:

1. **σ-ring buffer** (`cos_v124_buffer_*`).  Fixed-capacity (default
   100) ring of `cos_v124_interaction_t`.  Each slot stores prompt,
   response, σ_product, and (if present) the operator's correction.
   Persisted as JSONL at `~/.creation-os/learning_buffer.jsonl`.

2. **Idle-time trigger** (`cos_v124_trigger_decide`).  Returns one
   of `skip | train | smoke`:

   | idle | buffer ≥ trigger | next epoch % 10 | verdict |
   |------|------------------|-----------------|---------|
   | < 60 s | any | any | skip |
   | ≥ 60 s | no  | any | skip |
   | ≥ 60 s | yes | ≠ 0 | train |
   | ≥ 60 s | yes | = 0 | smoke |

3. **σ-targeted batch selection** (`cos_v124_buffer_select`).  Walks
   the buffer once and fills a three-bucket count:

   - `n_preferences` — rows with `user_corrected && correction`
     (the highest-value signal; a real human correction).
   - `n_high_sigma`  — rows with σ > τ_high (default 0.60) that
     the operator did not correct (the model knows it struggled).
   - `n_anchors`     — rows with σ < τ_anchor (default 0.30)
     (confirmed answers the model must not forget).

   Mid-σ rows contribute no training signal and are skipped.

4. **Forgetting prevention**.  Every `smoke_every_n_epochs` (default
   10) epochs we re-run a frozen 50-question baseline.  If accuracy
   drops by more than `max_baseline_drop` (default 0.02) we rollback
   to the previous adapter version and flag
   `forgetting_detected=1, rolled_back=1`.  The baseline is supplied
   through a caller-provided `cos_v124_baseline_fn`; in v124.0 the
   CLI uses deterministic healthy / pathological stubs so the
   merge-gate exercises both branches without running inference.

## What lands in v124.1

- MLX LoRA trainer driven by `n_total` examples per epoch.
- Adapter saved to `models/v124/continual_epoch_<N>.safetensors`.
- v106 hot-swap: `POST /v1/adapter/load` reloads the new adapter
  without restart; a response header `X-COS-Adapter-Version:
  continual_epoch_<N>` advertises what's serving.
- Web UI (v108) "Learning…" indicator while an epoch runs.

## What does not ship here (on purpose)

- No training loop in v124.0.  We measure the **policy contract**
  (buffer semantics, trigger table, batch-selection counts, smoke
  cadence, rollback bookkeeping, JSON shape).  The actual weight
  update is v124.1 — keeping v124.0 weights-free means the merge-
  gate runs in seconds on any Mac, any CI runner, no MLX required.
- No claim about throughput or token count per epoch.  When v124.1
  lands, a measured row joins `benchmarks/v111/results/` with an
  archived host manifest per
  [docs/REPRO_BUNDLE_TEMPLATE.md](../REPRO_BUNDLE_TEMPLATE.md).

## Source

- Header    : [`src/v124/continual.h`](../../src/v124/continual.h)
- Impl      : [`src/v124/continual.c`](../../src/v124/continual.c)
- CLI       : [`src/v124/main.c`](../../src/v124/main.c)
- Smoke     : [`benchmarks/v124/check_v124_continual_learning.sh`](../../benchmarks/v124/check_v124_continual_learning.sh)

## Self-test

```
make check-v124
```

Covers:

- ring-buffer wrap at capacity (oldest element correctly evicted)
- σ-targeted selection counts across corrections / high-σ / anchors
- idle-trigger policy at all four table rows
- healthy 10-epoch run: smoke runs, no forgetting, adapter advances
- pathological 10-epoch run: 4 % baseline drop triggers rollback,
  active adapter stays pinned at the previous version
- JSON shapes for `cos_v124_epoch_t` and `cos_v124_stats_t`.

## Claim discipline

v124 is a **Lab demo (C)** in the sense of
[docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md): it proves the
*policy* is correct today, and flags the *training loop* as deferred
to v124.1 with a named ticket ("wire MLX LoRA + v106 hot-swap").
Living-weights claims in the README go through this file.

## σ-stack layer

**Training+Persistence** — alongside v111 (SFT), v120 (distill),
v115 (memory).  v124 is the first kernel that makes the *schedule*
of weight updates a governable artifact.
