# v101 σ-BitNet-Bridge

> *"Stub → real, without replacing the σ-gate."*

## What v101 is

The σ-BitNet-Bridge is the first Creation OS kernel that runs **real**
language-model logits.  Every earlier kernel (v1…v100) works on one of the
following surrogates:

- synthetic logits in a self-test (`src/nn/bitnet_forward_stub.c`),
- bipolar hypervectors (`core/cos_bsc.h`, `hypervec/`),
- branchless integer decision tables (`src/v60`…`src/v100`), or
- hardcoded n-gram corpora (`oracle/`).

v101 replaces the `bitnet_forward_stub` façade with a thin llama.cpp
wrapper linked against **microsoft/BitNet** (bitnet.cpp — a llama.cpp fork
that ships i2_s / TL1 / TL2 kernels for the BitNet b1.58 ternary weight
format).  On top of the real logits we compute eight σ-channels, and the
result is piped to either a loglikelihood scorer or a greedy generator.

This is the first Creation OS kernel whose output can **be wrong** in the
LLM sense — the model can hallucinate, the σ-channels can disagree with
correctness, and the evaluation is measured by
[EleutherAI lm-evaluation-harness](../v102/THE_EVAL_HARNESS.md) rather
than a self-test assertion.

## Tier (see `docs/WHAT_IS_REAL.md`)

| Claim                                                                                                                                    | Tier | Evidence                                               |
|------------------------------------------------------------------------------------------------------------------------------------------|:----:|--------------------------------------------------------|
| Eight σ-channels from one row of float logits, bit-for-bit deterministic, all in [0, 1]                                                  | **M** | `make check-v101` → `v101 σ-BitNet-Bridge: 19 PASS / 0 FAIL` |
| `--self-test` is independent of weights (stub + real builds produce identical σ values for the same synthetic input)                     | **M** | Same self-test binary runs in both modes                |
| Real BitNet b1.58 2B-4T forward pass via llama.cpp, Metal backend on Apple Silicon                                                       | **M** | `make standalone-v101-real && make bench-v101-smoke`    |
| Greedy generation + σ-gated early-stop with abstain bit                                                                                  | **M** | `--gen` JSON carries `abstained: true/false`            |
| Loglikelihood scoring of `cont` given `ctx` with per-token σ aggregation                                                                 | **M** | `--ll` JSON carries `sigma_mean`                        |
| σ-gated accuracy on TruthfulQA / MMLU / GSM8K exceeds vanilla BitNet baseline                                                            | **P** | requires `make bench-v102` (see RESULTS.md)             |

## Build

### Stub mode (default; no weights required)

```sh
make standalone-v101
make check-v101
```

Produces `creation_os_v101` that runs the pure-C σ-math self-test.
Always green on any host with a C11 compiler and libm.  This is the
mode that `make merge-gate` exercises, so the CI stays green even on
hosts without the 1.1 GB BitNet checkpoint.

### Real mode (requires bitnet.cpp build + BitNet checkpoint)

One-time setup:

```sh
# 1. submodules (pulled in once)
git submodule update --init --recursive

# 2. Python venv + bitnet.cpp deps
python3 -m venv .venv-bitnet
.venv-bitnet/bin/pip install -r third_party/bitnet/requirements.txt

# 3. BitNet-b1.58-2B-4T GGUF + tokenizer (~1.1 GB + a few MB)
.venv-bitnet/bin/hf download microsoft/BitNet-b1.58-2B-4T-gguf \
    --local-dir models/BitNet-b1.58-2B-4T
.venv-bitnet/bin/hf download microsoft/bitnet-b1.58-2B-4T \
    --include "tokenizer*" --include "*.json" \
    --local-dir models/BitNet-b1.58-2B-4T-tokenizer

# 4. build bitnet.cpp (this produces libllama + libggml shared libs)
cd third_party/bitnet && \
    ../../.venv-bitnet/bin/python setup_env.py \
        -md ../../models/BitNet-b1.58-2B-4T -q i2_s && \
    cd ../..
```

Then build and smoke-test the real bridge:

```sh
make standalone-v101-real
make bench-v101-smoke
```

Expected smoke output (Apple M4, ~4 s wall clock):

```json
{"text":" Paris. Paris is a city that is known for",
 "sigma_profile":[0.173,0.637,0.186,0.077,0.252,0.500,0.000,0.431],
 "abstained":false}
```

## CLI reference

```
creation_os_v101 --self-test              # σ-math self-test (stub or real)
creation_os_v101 --banner                  # identifies stub vs real build

creation_os_v101 --ll  --gguf PATH --ctx CTX --cont CONT [--n-ctx N]
    → {"loglikelihood":F,"is_greedy":B,
       "n_ctx_tokens":N,"n_cont_tokens":N,"sigma_mean":F}

creation_os_v101 --gen --gguf PATH --ctx CTX
                 [--until "s1,s2,..."] [--max-tokens N]
                 [--sigma-threshold F] [--n-ctx N]
    → {"text":"...","sigma_profile":[8 floats],"abstained":B}
```

## The eight σ-channels

Each is in [0, 1] with the convention `0 = confident`, `1 = uncertain`.
σ itself is the mean of the eight channels.

| Channel        | Formula                                              | Intuition                                              |
|----------------|------------------------------------------------------|--------------------------------------------------------|
| `entropy_norm` | `H(softmax) / log(n_vocab)`                          | Shannon entropy, normalised                            |
| `margin`       | `1 − (p_top1 − p_top2)`                              | top-1 vs top-2 probability gap                         |
| `top_k_mass`   | `1 − Σ_{top-5} p_i`                                  | mass outside the top-5                                 |
| `tail_mass`    | `1 − Σ_{top-20} p_i`                                 | long-tail mass                                         |
| `logit_spread` | `1 − tanh((max_l − mean_l) / std_l / 8)`             | z-score of the argmax logit                            |
| `p_max`        | `1 − p_top1`                                         | straightforward complement                             |
| `n_effective`  | `exp(H) / n_vocab`                                   | effective support size normalised                      |
| `logit_std`    | `1 − tanh(std(l) / 4)`                               | logit concentration                                    |

`σ = mean(channels)` is the scalar the caller thresholds against
`--sigma-threshold` for abstain decisions.

## What v101 is *not*

- **Not a re-implementation of BitNet.**  The forward pass is Microsoft's
  bitnet.cpp.  Creation OS adds only the σ-channel layer plus a stable C
  API for loglikelihood + generate.
- **Not a fine-tuned model.**  Weights are the stock microsoft/BitNet
  1.58 b-2B-4T checkpoint.  Any numbers in `docs/v102/RESULTS.md` are
  measurements on the unmodified public weights.
- **Not AGI.**  Real inference at 44 tok/s on a laptop.  It is measurably
  useful for the tasks run in `benchmarks/v102/`, nothing more.

## Files

```
src/v101/bridge.h                 public API
src/v101/sigma_channels.c         pure-C σ math (both modes)
src/v101/self_test.c              19 σ-math assertions (both modes)
src/v101/bridge_stub.c            stub backend (no weights)
src/v101/bridge_real.c            llama.cpp wrapper (real weights)
src/v101/creation_os_v101.c       CLI (--ll / --gen / --self-test)
```

## Next: v102

The σ-channels are only meaningful if they correlate with correctness on
a real benchmark.  [v102 σ-Eval-Harness](../v102/THE_EVAL_HARNESS.md)
wraps this bridge into a registered lm-evaluation-harness backend, runs
ArcEasy + TruthfulQA-MC2 + GSM8K, and reports the honest delta between
σ-gated and vanilla BitNet in `docs/v102/RESULTS.md`.
