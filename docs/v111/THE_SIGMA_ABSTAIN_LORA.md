# v111.3 — σ-Abstention LoRA

_A fine-tune that turns σ into a first-class label: supervised LoRA on
TinyLlama / Phi-2 / BitNet using automatically-extracted `(prompt, "I
don't know.")` pairs from the v104 n=5000 sidecars._

## The σ-self-distillation idea

1. Run lm-evaluation-harness on the frozen base model with the v103/v104
   σ-logging backend; the sidecar records eight σ-channels per candidate
   per doc.
2. Compute `sigma_product` from each sidecar row.
3. Label rule (symmetric):
   - `sigma_product > 0.12` → **"I don't know."**  (the model is already
     uncertain; teach it to say so explicitly).
   - `sigma_product < 0.05` **and** `acc == 1` → **the model's own confident
     and correct answer**  (self-distillation of successful behaviour).
   - everything in between → dropped (ambiguous).
4. Fine-tune a small LoRA adapter on the resulting JSONL with vanilla
   causal-LM loss.  No custom loss, no KL term, no external grader.

The σ-supervision is entirely _in the data_.  The training loop is
standard MLX-LM LoRA.

## Thresholds

Tuned to the v105 `sigma_product` distribution on BitNet b1.58 2B-4T.
From the n=5000 sidecars:

| sidecar                     | min  | P25  | P50  | P75  | P90  | P99  | max  |
|-----------------------------|-----:|-----:|-----:|-----:|-----:|-----:|-----:|
| `arc_challenge_sigma.jsonl` | 0.007| 0.050| 0.068| 0.092| 0.119| 0.179| 0.270|
| `truthfulqa_mc2_sigma.jsonl`| 0.005| 0.040| 0.054| 0.071| 0.092| 0.130| 0.168|

Defaults:

| flag              | default | rationale                                      |
|-------------------|--------:|------------------------------------------------|
| `--high-tau`      | 0.12    | top ≈ 10% → IDK                                |
| `--low-tau`       | 0.05    | bottom ≈ 25% → answer-kept                     |
| `--require-correct` | ON    | only self-distil CORRECT answers (always)      |

## Pipeline

```bash
# 1. extract supervised pairs (fast; uses existing sidecars)
.venv-bitnet/bin/python training/v111/extract_sigma_training_data.py \
    --sidecars benchmarks/v104/n5000_results \
    --out      training/v111/data/sigma_abstain.jsonl

# 2. inspect split quickly
head -1 training/v111/data/sigma_abstain.jsonl | jq .

# 3. smoke the trainer (no MLX download, no GPU)
.venv-bitnet/bin/python training/v111/train_sigma_abstain.py --dry-run

# 4. install MLX if you haven't (≈ 220 MB)
.venv-bitnet/bin/pip install mlx mlx-lm

# 5. short live run (≈ 5 min on M3, 10 iters, for sanity)
.venv-bitnet/bin/python training/v111/train_sigma_abstain.py --iters 10

# 6. full run (≈ 6–12 h on an 8 GB M3, 2 epochs)
.venv-bitnet/bin/python training/v111/train_sigma_abstain.py --iters 600 \
    --steps-per-eval 100 --save-every 100
```

## Output

```
models/v111/sigma_abstain_lora/
├── adapter.safetensors       # LoRA adapter weights
├── adapter_config.json       # rank, alpha, target modules
└── v111_manifest.json        # base model + data + hyperparams
```

## Base model choice

| base                                           | size 4-bit | RAM need | notes                    |
|------------------------------------------------|-----------:|---------:|--------------------------|
| `mlx-community/TinyLlama-1.1B-Chat-v1.0-4bit`  | ~630 MB    |  ≈ 2 GB  | default — M3 8 GB fit    |
| `mlx-community/phi-2-4bit`                     | ~1.5 GB    |  ≈ 4 GB  | higher quality           |
| `mlx-community/BitNet-b1.58-2B-4T-mlx`         | ~1.1 GB    |  ≈ 4 GB  | matches inference backend; experimental |

Pass with `--base-model <repo>`.

## How the adapter is consumed

Short-term (v111.3 ship):

- the adapter lives as standard MLX-LM safetensors
- `mlx_lm.generate --adapter-path models/v111/sigma_abstain_lora` runs
  it directly (no Creation OS code needed)
- the adapter is **not** yet wired into the v101 bridge — the v101
  bridge consumes GGUF, not safetensors

Medium-term (v109 + v111.3 convergence):

- v109 extends the v101 bridge to accept _any_ GGUF, including a
  GGUF-converted base plus LoRA merge
- the σ-abstain LoRA can be merged into the base GGUF with
  `llama.cpp/examples/lora-merge` and served by v106 transparently
- at that point `POST /v1/reason` fires the fine-tuned behaviour
  automatically

The MLX-Python path is the primary consumer for v111.3; the
GGUF-conversion path is tracked as a v109.x follow-up in
`docs/ROADMAP_TO_AGI.md`.

## What this training does NOT do

- It is **not** RLHF.  No preference dataset, no reward model.
- It is **not** constitutional fine-tuning.  The IDK label comes from
  the model's own σ, not a written constitution.
- It does **not** train a new base model.  It only adds a LoRA adapter
  of a few million parameters on top of a pre-trained checkpoint.
- It does **not** beat GPT-4 on anything.  The measurement targeted
  here is "does the σ-supervised adapter reduce calibrated risk at a
  given coverage versus the untuned base?"  Evaluating that needs a
  post-training pass of the v111.1 frontier-matrix and is tracked as a
  v111.4 follow-up (`docs/ROADMAP_TO_AGI.md` §v111.4).

## CI coverage

- `make check-v111-sft-smoke` — runs the extractor on existing sidecars,
  asserts both IDK and answer classes appear, and dry-runs the trainer
  to verify the mlx_lm.lora invocation.  No network, no GPU, no MLX
  required.
- `COS_V111_SFT_LIVE=1 make check-v111-sft-smoke` — also runs a
  single-iter live LoRA (opt-in; requires MLX + TinyLlama cache).

## Relation to the σ-stack

Layer 5 ("Training") of `docs/AGI_ARCHITECTURE.md`.  This is the feedback
loop that turns measured uncertainty (from Layer 3) into next-generation
inference calibration (Layer 2).
