# v102 σ-Eval-Harness — RESULTS

**Status: PENDING — benchmark run not yet executed on this branch.**

This file is the honest ledger for the σ-Eval-Harness numbers.  It has
four possible end-states.  Whichever one applies is what is written
below; there is no "select the best one" in this file.

1. **σ-gated strictly better than baseline** on at least one task
   → σ-gating is a real positive contribution; document per-task delta
   and the abstain rate that achieved it.

2. **σ-gated equal to baseline** (within ±0.5 pp) on every task
   → σ-gating is a useful diagnostic channel but does not change
   accuracy; document this plainly.

3. **σ-gated strictly worse than baseline** on any task
   → σ-gating hurts for that task.  Report the damage, keep the numbers.

4. **Could not run** (hardware / environment blockers)
   → Document the blocker and mark the run as attempted-but-failed.

The current state is #4 with an explicit caveat below.

## Current state: not run

The end-to-end run requires:

- a built `creation_os_v101` in real mode (✅ on this branch, see
  `make standalone-v101-real`),
- the 1.1 GB BitNet b1.58 2B-4T GGUF at
  `models/BitNet-b1.58-2B-4T/ggml-model-i2_s.gguf` (✅ on this branch),
- a `.venv-bitnet` with `lm-eval` installed editable from
  `third_party/lm-eval` (setup in branch — execution pending),
- ~2 hours wall clock on Apple M4 for the three default tasks.

To run:

```sh
source .venv-bitnet/bin/activate
pip install -e third_party/lm-eval
bash benchmarks/v102/run_eval.sh
```

## Smoke results — BitNet baseline on this host (2026-04-17)

The `v101` bridge itself is measured and the model is alive on this
host.  Observed during development (see `tests/v101/README.md`):

- Greedy generate, `"The capital of France is"` → `" Paris. Paris is
  a city that is known for"` (44 tok/s on Apple M4, Metal backend).
- Loglikelihood of `" Paris"` given that prefix: `ll = -0.0563`,
  `is_greedy = true`, `sigma_mean = 0.109`.
- Loglikelihood of `" Helsinki"` given the same prefix: `ll = -15.05`,
  `is_greedy = false`.  The 15-nat gap is the model's commitment.

These are sanity checks, not benchmark results.  They confirm the
bridge produces correct probabilities but say nothing about σ-gating's
effect on ArcEasy / TruthfulQA / GSM8K — that is what the full run
measures, and that run has not yet been executed.

## Table template (will be filled in on `make bench-v102`)

| task             | metric      | baseline | σ-gated (τ=0.0) | σ-gated (τ=0.5) | σ-gated (τ=0.7) | abstain % |
|------------------|-------------|---------:|----------------:|----------------:|----------------:|----------:|
| `arc_easy`       | acc         | TBD      | TBD             | TBD             | TBD             | TBD       |
| `truthfulqa_mc2` | acc         | TBD      | TBD             | TBD             | TBD             | TBD       |
| `gsm8k`          | exact_match | TBD      | TBD             | TBD             | TBD             | TBD       |

Host / reproducibility stamp (filled on `make bench-v102`):

```
host:            <host>
uname:           <output>
cpu:             <output>
bitnet.cpp SHA:  <submodule head>
lm-eval SHA:     <submodule head>
checkpoint SHA:  <sha256 ggml-model-i2_s.gguf>
wall_clock_s:    <n>
```

## Honest interpretation guide

If the σ-gated column is **higher**: v101 + v29 σ-channels are a
positive contribution — keep them and iterate.

If it is **equal**: σ is informative as a diagnostic channel (see
`sigma_log.jsonl` next to results/) but does not shift accuracy;
no claim of "σ improves accuracy" should appear anywhere in the repo.

If it is **lower**: σ-gating hurts, the abstain threshold is wrong or
the channels are miscalibrated.  Either re-tune or retire the claim.

The v29 / v78 / v101 σ-stack stands or falls on this table.  That is
the point of measuring it.
