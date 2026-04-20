# v111.1 — The Frontier Parity Matrix

_A Bonferroni-corrected comparison of six σ-gating signals against the
entropy baseline, across four task families, on a single frozen BitNet
b1.58 2B-4T checkpoint._

## Why this exists

v103 / v104 answered "which σ-aggregator is best for Creation OS on three
multiple-choice tasks at n=5000?"  v111 widens that question to the
four task families the open-LLM evaluation community agrees matter for
frontier parity: ARC-class reasoning, TruthfulQA, HellaSwag, and the
(heavier, partially pending) MMLU / GSM8K / HumanEval triple.

For each family and each signal we publish:

- AURCC (area under the risk-coverage curve; lower is better)
- ΔAURCC vs the classical `entropy` baseline, with paired-bootstrap CI95
- p-value (two-sided, on the paired bootstrap distribution)
- coverage-at-conditional-accuracy ≥ 0.90 and ≥ 0.95
- a boolean "significant after Bonferroni" flag at α = 0.05 / 24

## The six pre-registered signals

Order is fixed before any n=5000 numbers were computed and matches
`benchmarks/v111/frontier_signals.py::SIGNALS`.

| # | name             | formula                                                        | rationale                                |
|---|------------------|----------------------------------------------------------------|------------------------------------------|
| 0 | `entropy`        | channel 0 = H(softmax) / log V                                 | classical selective-prediction baseline  |
| 1 | `sigma_mean`     | arithmetic mean of 8 σ-channels                                | v103 default aggregator                  |
| 2 | `sigma_max_token`| max over tokens of the per-token scalar σ                      | v103 secondary; suggestive at n=500      |
| 3 | `sigma_product`  | geometric mean of 8 σ-channels (ε = 1e-6 floor)                | v104 winner; v105 default                |
| 4 | `sigma_tail_mass`| channel 3: 1 − P(top-20 tokens)                                | information-theoretic worst-case mass    |
| 5 | `sigma_n_effective` | channel 6: 1 − exp(H) / V                                   | normalised effective support-size        |

## The four task families

| family           | vehicle                         | status in v111.1                   |
|------------------|---------------------------------|------------------------------------|
| ARC-class        | `arc_easy`, `arc_challenge`     | n ≈ 2 376 + 1 172 (v104 n=5000 pass) |
| TruthfulQA       | `truthfulqa_mc2`                | n ≈ 817 (v104 n=5000 pass)         |
| HellaSwag        | `hellaswag`                     | n = 746 · acc = 0.4853 (v111 σ-sidecar, lm-eval gold via arrow) |
| MMLU / GSM8K / HumanEval | full tasks               | PENDING — see §4 for exact repro commands |

The merge-gate row in §3 renders the four families whose sidecars exist
at merge time.  Adding a new family is _only_ adding a σ-sidecar to a
sibling directory under `benchmarks/v111/results/` or
`benchmarks/v104/n5000_results/`; `frontier_matrix.py` discovers them
automatically.

## 2. Reproduce

```bash
# from repo root, with .venv-bitnet activated
bash benchmarks/v111/run_matrix.sh                  # analyse whatever sidecars exist
bash benchmarks/v111/run_matrix.sh hellaswag        # generate hellaswag n=1000 sidecar (requires BitNet + .venv-bitnet + bridge)
.venv-bitnet/bin/python benchmarks/v111/frontier_matrix.py --n-boot 2000

# if the σ-sidecar for a task exists but the lm-eval `acc` samples do not:
.venv-bitnet/bin/python benchmarks/v111/synthesize_hellaswag_lmeval.py   # rebuilds lm_eval/hellaswag/ from the existing sidecar + HF arrow cache
```

The `synthesize_*.py` fallback is used when a v103 σ-logging run was
completed but the companion lm-eval samples file was not archived.  It
re-derives per-doc accuracy from the sidecar's stored log-likelihoods and
the public validation gold labels read from the local pyarrow cache; no
BitNet inference is re-executed.

Output artefacts:

- `benchmarks/v111/results/frontier_matrix.md` — human table
- `benchmarks/v111/results/frontier_matrix.json` — full numeric dump
- `benchmarks/v111/results/rc_curves.json` — per-task-per-signal curves

## 3. Measured results (shipped in-tree)

See `benchmarks/v111/results/frontier_matrix.md` for the live version.
At merge time the Bonferroni (α = 0.00208) winners were:

| task              | n    | acc   | winning signal      | ΔAURCC vs entropy | p-value | Bonferroni |
|-------------------|-----:|------:|---------------------|------------------:|--------:|:-----------|
| arc_easy          | 2376 | 0.7551 | —                   | —                 | —       | no winner  |
| arc_challenge     | 1172 | 0.4676 | —                   | —                 | —       | no winner  |
| truthfulqa_mc2    |  817 | 0.4638 | `sigma_max_token`   | −0.0447           | 0.0005  | **yes**    |
| truthfulqa_mc2    |  817 | 0.4638 | `sigma_n_effective` | −0.0206           | 0.0005  | **yes**    |
| hellaswag         |  746 | 0.4853 | —                   | —                 | —       | no winner  |

(numbers from `n_boot=2000` archival pass written to
`benchmarks/v111/results/frontier_matrix.md`.  On HellaSwag the classical
`entropy` baseline is strong; `sigma_max_token` actively hurts AURCC on
this task — Δ = +0.0496, p = 0.003.  This is reported honestly; no task
is silenced for inconvenience.)

## 4. Full-scale repro commands (MMLU / GSM8K / HumanEval)

### 4.1 MMLU 5-shot (all 57 subjects)

MMLU is 14 042 docs across 57 subjects at 5-shot; on the v103 backend at
~0.8 s per loglikelihood that is ≈ 3 hours.  Exact command:

```bash
bash benchmarks/v111/run_matrix.sh mmlu     # currently runs mmlu_high_school_psychology n=500

# for the full 57-subject 5-shot run:
COS_V103_SIGMA_SIDECAR=benchmarks/v111/results/samples_mmlu_sigma.jsonl \
  .venv-bitnet/bin/python benchmarks/v103/run_lm_eval_v103.py \
  --model creation_os_v103 \
  --model_args "bridge=$PWD/creation_os_v101,gguf=$PWD/models/BitNet-b1.58-2B-4T/ggml-model-i2_s.gguf,n_ctx=2048,task_tag=mmlu" \
  --tasks mmlu --num_fewshot 5 \
  --output_path benchmarks/v111/results/lm_eval/mmlu \
  --log_samples
```

### 4.2 GSM8K

GSM8K is `generate_until`-style; the v103 backend needs the
continuation-generation σ-logging path (documented as a v103.1 open TODO
in `docs/v103/RESULTS.md`).  This is the primary blocker to putting
GSM8K into the matrix.

### 4.3 HumanEval

HumanEval requires a live code-execution sandbox; Creation OS does not
yet ship one (tracked under "code-execution sandbox" in
`docs/ROADMAP_TO_AGI.md`).  Until that lands, we publish perplexity-only
rows here or pair with an external harness.

## 5. What this matrix does NOT claim

- It does **not** compare BitNet 2B against GPT-4 or Claude 3.
  It measures _Creation OS's σ-governance layer_ against _Creation OS's
  own entropy baseline_ on a fixed BitNet checkpoint.  The question
  answered is: "does our σ-stack add selective-prediction signal beyond
  the classical entropy reference?"  The answer as of v111 is: "yes,
  Bonferroni-significantly, on TruthfulQA, via `sigma_max_token` and
  `sigma_n_effective`."

- It does **not** certify BitNet 2B as a frontier-grade base model.
  BitNet 2B is a 2-billion-parameter 1.58-bit model; raw accuracy on
  these tasks is the number it is, not a benchmark-topping one.  What
  the σ-stack demonstrates is that _given a fixed base model's calibration_,
  our gating signal can route correct answers through and uncertain
  answers to abstention with measurable advantage over entropy.

- It does **not** substitute for the full doctoral read-path.  See
  `docs/RESEARCH_AND_THESIS_ARCHITECTURE.md` and
  `docs/CLAIM_DISCIPLINE.md`.

## 6. Extension path

- **v111.1.1**: add hellaswag n=2000 to the matrix.
- **v111.1.2**: add mmlu n=2000 across 5 random subjects.
- **v111.1.3**: add GSM8K via a generate_until σ-logging extension to
  the v103 backend.
- **v111.1.4**: add HumanEval via an external harness + σ-sidecar hook.

All of these are additive: landing a new sidecar triggers the matrix to
widen without any code change outside `benchmarks/v111/results/`.
