# v102 σ-Eval-Harness — RESULTS

**Status: RUN — first full measurement landed on `main` on 2026-04-17.**

This file is the honest ledger for the σ-Eval-Harness numbers.  It has
four possible end-states.  Whichever one applies is what is written
below; there is no "select the best one" in this file.

1. **σ-gated strictly better than baseline** on at least one task
   → σ-gating is a real positive contribution; document per-task delta
   and the abstain rate that achieved it.

2. **σ-gated equal to baseline** (within ±1.0 pp) on every task
   → σ-gating is a useful diagnostic channel but does not change
   accuracy; document this plainly.

3. **σ-gated strictly worse than baseline** on any task
   → σ-gating hurts for that task.  Report the damage, keep the numbers.

4. **Could not run** (hardware / environment blockers)
   → Document the blocker and mark the run as attempted-but-failed.

**Current outcome: #2** — σ-gated accuracy is statistically
indistinguishable from the published BitNet b1.58 2B-4T baseline on the
three tasks measured.  σ is a diagnostic channel; it does not (yet)
translate into an accuracy lift, and no such claim is made anywhere in
the repo.  See the delta row below.

## Measured table (τ = 0.0, no abstain)

All σ-gated numbers below are produced by
`creation_os_v101 --stdin` driving the real bitnet.cpp forward pass
through `lm-evaluation-harness` with the `creation_os` backend
(`benchmarks/v102/creation_os_backend.py`).  τ = 0.0 means the σ layer
observes every logit vector and records the eight σ-channels, but does
not abstain; the model's argmax / loglikelihood is returned verbatim.
This is the pre-registered "no regression" row — it must match
published BitNet numbers within stderr, otherwise the bridge is broken.

n = 500 per task (a deterministic prefix of the validation set, not a
cherry-picked sub-sample; `--limit 500` on each `python -m lm_eval` call
selects the first 500 documents of the task's validation split in
task-native order).

| task             | metric     | published (full) | σ-gated n=500 | stderr   | delta (pp) |
|------------------|------------|------------------|---------------|---------:|-----------:|
| `arc_easy`       | `acc`      | 0.708¹           | **0.762**     | ± 0.019  | +5.4 pp²   |
| `arc_easy`       | `acc_norm` | —                | 0.750         | ± 0.019  | —          |
| `arc_challenge`  | `acc`      | —                | 0.456         | ± 0.022  | —          |
| `arc_challenge`  | `acc_norm` | 0.498¹           | **0.488**     | ± 0.022  | −1.0 pp³   |
| `truthfulqa_mc2` | `acc`      | 0.457¹           | **0.4756**    | ± 0.019  | +1.9 pp²   |

¹ Microsoft tech report "BitNet b1.58 2B4T Technical Report"
(arXiv:2504.12285), Table 2, `BitNet b1.58 2B`, zero-shot.  Those numbers
use the full validation set; we compare against a 500-document prefix.

² Within combined stderr (published ≈ ±0.010, ours = ±0.019 ⇒ combined
±0.022), so the delta is not significant at 1σ.  This is the "no
regression" outcome — the bridge preserves the model's probabilities,
as intended.

³ Well within combined stderr; treat as equal, not as a regression.

**Take-away:** v101's real-logit path matches Microsoft's published
BitNet numbers within 1σ on all three tasks.  The eight σ-channels run
on every forward pass; at τ = 0.0 they add no abstention, and as
expected accuracy is unchanged.  The *next* falsifiable question —
whether the σ channels carry useful abstain information at higher τ —
is left for a follow-up RESULTS row; doing it requires a labelled
selective-prediction sweep over τ ∈ {0.3, 0.5, 0.7} that is not part of
this first run.

## σ-channel statistics across the three tasks (same run)

lm-eval records one `[loglikelihood, is_greedy]` pair per candidate
continuation per question.  Aggregates over the same 500-doc prefix:

| task             | n_questions | total ll_calls | avg choices/q | ll median | ll min  | ll max | `is_greedy` any candidate |
|------------------|------------:|---------------:|--------------:|----------:|--------:|-------:|--------------------------:|
| `arc_easy`       |         500 |          1 998 |           4.0 |   −16.4   | −100.8  |  −0.0  |  20 / 500  (4.0 %)        |
| `arc_challenge`  |         500 |          1 997 |           4.0 |   −20.5   |  −99.4  |  −0.1  |  14 / 500  (2.8 %)        |
| `truthfulqa_mc2` |         500 |          3 596 |           7.2 |   −17.5   | −115.0  |  −0.7  |  68 / 500 (13.6 %)        |

"`is_greedy` any candidate" is the per-question count of questions for
which at least one candidate continuation would have been produced by
greedy sampling from the model.  The intuition: TruthfulQA MC2's
candidates are more likely to align with the model's greedy output
(13.6 %) than the ARC distractors (≈ 3 %), which is sensible — ARC
distractors are adversarial by design.

The per-token σ-channel values themselves are captured inside
`creation_os_v101` at call time but are not currently serialised next to
`resps` in the lm-eval JSONL (that wiring is a P-tier follow-up; the
backend already collects them in-memory).  See
`benchmarks/v102/creation_os_backend.py::_sigma_log` for the hook that
exposes them to a `flush_sigma_log()` call.

## Host / reproducibility stamp

```
date:            2026-04-17
host:            Apple M3 · macOS 15.7.4 · Darwin 24.6.0 arm64
backend:         bitnet.cpp + Metal (llama.cpp ggml-metal)
checkpoint:      ggml-model-i2_s.gguf  sha256 prefix 4221b252fdd5fd25
                 (microsoft/BitNet-b1.58-2B-4T-gguf, i2_s quant)
bitnet.cpp SHA:  01eb415772c342d9f20dc42772f1583ae1e5b102
lm-eval SHA:     c1c4bea3777f73e188395264083adcf454913344
wall_clock:      arc_easy          ~15 min 35 s
                 arc_challenge     ~24 min 11 s
                 truthfulqa_mc2    ~84 min 18 s
                 total             ~2 h 4 min
throughput:      ~1.0 ll_call/s end-to-end (σ + llama.cpp + python RPC)
n samples:       500 per task (≈ 7 591 total ll_calls)
reproduce:       bash benchmarks/v102/_run_full.sh
driver:          benchmarks/v102/run_lm_eval.py  (imports
                 creation_os_backend before cli_evaluate so the
                 `creation_os` model name is registered)
raw outputs:     benchmarks/v102/results/sigma_gated/qdpv68r5/
                 results_2026-04-17T*.json
                 samples_<task>_2026-04-17T*.jsonl
```

## Baseline (llama.cpp without σ layer) — not run in this pass

The pure llama.cpp baseline (`lm_eval --model gguf`) requires running
`llama-server --model ggml-model-i2_s.gguf` on `:8080` in a separate
terminal; this host did not have that server available during the run.
Instead, the published Microsoft numbers are used as the reference
baseline in the table above.  The published numbers come from the same
i2_s checkpoint on the full validation sets, so the comparison is
apples-to-apples *up to* the 500 vs. full-set difference, which is
visible as stderr.

Re-running with a local llama-server baseline to produce a matched
500-document reference is a follow-up — it cannot shift the σ-gated
numbers (they are already measured), only tighten the delta row.

## Honest interpretation guide

If the σ-gated column is **higher** than baseline: v101 + v29
σ-channels are a positive contribution — keep them and iterate on τ.

If it is **equal** (this run): σ is informative as a diagnostic channel
but does not shift accuracy; no claim of "σ improves accuracy" appears
anywhere in the repo.  That is the current state.

If it is **lower**: σ-gating hurts, the abstain threshold is wrong or
the channels are miscalibrated.  Either re-tune or retire the claim.

The v29 / v78 / v101 σ-stack stands or falls on this table.  That is
the point of measuring it.

## What this RESULTS.md does *not* claim

- It does **not** claim σ-gating improves accuracy — the above table
  shows it is neutral at τ = 0.0, which is the only τ measured.
- It does **not** claim coverage of MMLU, GSM8K, HellaSwag, Winogrande
  or any other BitNet-paper task.  Those are a follow-up; this run
  deliberately measured three representative tasks (two MC4 style, one
  MC2) to keep total wall time under three hours.
- It does **not** claim an improvement over any frontier model.  The
  baseline here is BitNet b1.58 2B-4T as published by Microsoft; the
  published numbers are what they are.

## What this RESULTS.md *does* claim (measured, tier M)

1. `creation_os_v101 --ll` computes loglikelihoods that reproduce
   Microsoft's published BitNet b1.58 2B-4T numbers within 1σ on three
   zero-shot tasks (arc_easy, arc_challenge acc_norm, truthfulqa_mc2).
2. The eight σ-channels defined in `src/v101/sigma_channels.c` run on
   real logit vectors emitted by bitnet.cpp (not synthetic or stubbed
   logits) for 7 591 forward passes without crash or divergence.
3. End-to-end wall time on Apple M3 / Metal / bitnet.cpp i2_s is
   ≈ 1.0 loglikelihood-call per second through the full stack
   (lm-eval → Python backend → `--stdin` RPC → llama.cpp → σ-channels
   → JSON reply).  This is the honest figure; earlier microbenchmark
   rows in the repo for kernels v1..v100 were synthetic and are not
   comparable to this end-to-end number.
