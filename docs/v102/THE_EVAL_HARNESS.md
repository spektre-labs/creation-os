# v102 σ-Eval-Harness

> *"If a σ-channel correlates with correctness, it is falsifiable.  If it
> doesn't, the honest measurement says so."*

## What v102 is

The σ-Eval-Harness is **not** a new kernel.  It is a thin integration
layer that pipes `creation_os_v101` through EleutherAI's
[lm-evaluation-harness](https://github.com/EleutherAI/lm-evaluation-harness)
so that a Creation OS forward pass — the eight σ-channels sitting on
top of real BitNet b1.58 2B-4T logits — can be scored on the same
tasks the rest of the LLM research community uses.

Two configurations are measured per run:

1. **baseline**     — stock BitNet through lm-eval's `gguf` backend (no
   Creation OS code touches the logits).
2. **σ-gated**      — BitNet through the `creation_os` backend
   (`benchmarks/v102/creation_os_backend.py`), every loglikelihood call
   carries a `sigma_mean`, and generate calls may abstain when
   `sigma_threshold > 0`.

The delta (accuracy σ-gated vs accuracy baseline, plus abstain rate)
is recorded in [RESULTS.md](RESULTS.md).  When the delta is **zero or
negative**, the document says so — there is no sleight-of-hand from
switching tasks or hiding abstains.

## Tier (see `docs/WHAT_IS_REAL.md`)

| Claim                                                                                      | Tier | Evidence                                                    |
|--------------------------------------------------------------------------------------------|:----:|-------------------------------------------------------------|
| `check-v102` SKIPs cleanly on any host missing lm-eval / weights / bitnet.cpp              | **M** | `make check-v102` — exits 0 with explicit SKIP strings       |
| `check-v102` runs a real `arc_easy --limit 5` smoke when every prereq is present           | **M** | same script, different branch                                |
| Full baseline + σ-gated numbers on ArcEasy / TruthfulQA-MC2 / GSM8K                        | **M** | `make bench-v102` → [RESULTS.md](RESULTS.md)                 |
| σ-gated accuracy strictly greater than baseline on any task                                 | **P** | requires RESULTS.md delta > 0                                |

## Tasks selected for the first run

`benchmarks/v102/run_eval.sh` defaults to **`arc_easy,truthfulqa_mc2,gsm8k`**.
The rationale:

- `arc_easy`   — broad commonsense, cheap to run, good signal for
  "does the backend work at all".
- `truthfulqa_mc2` — the task where σ-gating ought to help the most,
  because it directly rewards abstention on known-hallucination traps.
- `gsm8k`      — arithmetic reasoning; σ should be **high** on wrong
  intermediate steps for the σ-channels to be useful for CoT filtering.

Future runs can extend to `mmlu`, `hellaswag`, `truthfulqa_gen`,
`mbpp`; the backend already supports both `loglikelihood` and
`generate_until`.

## How it works

```
lm_eval  ──loglikelihood(ctx, cont)──►  CreationOSLM
                                             │
                                             ▼
                                   subprocess.run(
                                     creation_os_v101 --ll ...
                                   )
                                             │
                                             ▼
                                   bitnet.cpp llama_decode
                                             │
                                             ▼
                                   σ-channel math on logits
                                             │
                                             ▼
                           JSON: {ll, is_greedy, sigma_mean}
```

The σ values are kept alongside each request in `CreationOSLM._sigma_log`
and can be dumped with `flush_sigma_log(path)` for post-hoc σ vs
correctness analysis (the correlation between `sigma_mean` and `is_correct`
is the one figure that makes σ-gating worth shipping — or doesn't).

## Reproducing the RESULTS.md numbers

```sh
git submodule update --init --recursive
python3 -m venv .venv-bitnet
.venv-bitnet/bin/pip install -r third_party/bitnet/requirements.txt
.venv-bitnet/bin/pip install -e third_party/lm-eval

.venv-bitnet/bin/hf download microsoft/BitNet-b1.58-2B-4T-gguf \
    --local-dir models/BitNet-b1.58-2B-4T
.venv-bitnet/bin/hf download microsoft/bitnet-b1.58-2B-4T \
    --include "tokenizer*" --include "*.json" \
    --local-dir models/BitNet-b1.58-2B-4T-tokenizer

cd third_party/bitnet && \
    ../../.venv-bitnet/bin/python setup_env.py \
        -md ../../models/BitNet-b1.58-2B-4T -q i2_s && \
    cd ../..

make standalone-v101-real
bash benchmarks/v102/run_eval.sh      # or --limit 50 for a fast sanity run
```

Output JSON and logs land under `benchmarks/v102/results/{baseline,sigma_gated}/`.

## Performance note

The current `creation_os` backend spawns one `creation_os_v101 --ll`
subprocess per sample, which reloads the model from the OS page cache
each time.  Full-benchmark wall-clock is therefore ~2× the baseline.

A follow-up can add a persistent-process REPL mode so the context is
loaded once per task instead of once per sample.  This is a pure
engineering win that doesn't change any accuracy number — hence it is
NOT a precondition for the first honest RESULTS.md.

## What v102 is *not*

- **Not a new benchmark.**  Every number is reported on the standard
  EleutherAI tasks; nothing is renamed or reformulated.
- **Not a trophy.**  If σ-gating underperforms baseline on every task,
  RESULTS.md will say so and the v29 / v78 σ-stack will have to go back
  to the drawing board.
- **Not a replacement for the real BitNet eval suite.**  This is 0-shot
  on three tasks.  The full microsoft/BitNet card covers many more.

## Files

```
benchmarks/v102/run_eval.sh              full driver (baseline + σ-gated)
benchmarks/v102/check_v102.sh            SKIP-aware CI smoke
benchmarks/v102/creation_os_backend.py   registered lm-eval backend
benchmarks/v102/results/                 JSON output per task per backend
docs/v102/RESULTS.md                     numbers (filled in after bench-v102)
```
