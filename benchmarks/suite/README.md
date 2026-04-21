# Multi-dataset σ-gate suite (SCI-6)

This directory anchors the **multi-dataset** evaluation surface for the σ-gate.  It is deliberately populated in *layers*: TruthfulQA ships as real BitNet-b1.58-2B measurements; ARC / GSM8K / HellaSwag are wired into the aggregator but their per-row detail JSONLs are **not** committed until the maintainer actually produces them with `cos-chat`.  The aggregator reports `"measured": false` for any dataset whose detail file is absent — no numbers are fabricated.

## Files in this directory

| File | Status | How to (re)produce |
|------|--------|---------------------|
| [`full_results.json`](full_results.json) | Emitted by `cos-bench-suite-sci`; schema `cos.suite_sci.v1`. | `./cos-bench-suite-sci --alpha 0.80 --delta 0.10 --out benchmarks/suite/full_results.json` |
| `arc_challenge_detail.jsonl` | **NOT present.** | See *Producing a detail JSONL* below. |
| `arc_easy_detail.jsonl` | **NOT present.** | See below. |
| `gsm8k_detail.jsonl` | **NOT present.** | See below. |
| `hellaswag_detail.jsonl` | **NOT present.** | See below. |

TruthfulQA's detail file lives one directory up at [`../pipeline/truthfulqa_817_detail.jsonl`](../pipeline/truthfulqa_817_detail.jsonl) and is the authoritative **measured** dataset today.

## Producing a detail JSONL

Every detail JSONL must contain **one JSON object per line** with at least these fields:

```json
{"id": "...", "prompt": "...", "generated": "...",
 "sigma": 0.37, "correct": true,
 "mode": "pipeline"}
```

Required: `sigma` (float), `correct` (bool or `null`), `mode` (string; mirrors the "pipeline" filter in the TruthfulQA detail file).  Optional: `category` (used by SCI-2 per-domain calibration when `--classify-prompts` is off).

### Recipe

1. **Fetch the dataset.** For ARC/HellaSwag/GSM8K the canonical source is the Hugging Face `allenai/ai2_arc`, `Rowan/hellaswag`, `gsm8k` splits.

2. **Drive cos-chat once per prompt:**

   ```bash
   ./cos chat --once --prompt "$PROMPT" --max-tokens 48 \
              --keep-server --json-line
   ```

   where `--json-line` prints the machine-readable per-row summary that the aggregator expects.  This flag is on the roadmap; the current `cos chat` prints a banner + the generated text, which the operator scripts into the JSONL.

3. **Score with the dataset's native scorer.** For ARC/HellaSwag, multiple-choice substring match against the labeled answer; for GSM8K, exact-number match. Drop rows that cannot be scored into `correct: null` so the conformal calibration excludes them from the denominator.

4. **Concatenate** the per-row JSONs into one `*_detail.jsonl` file under this directory.

Re-running `./cos-bench-suite-sci` will then pick it up; no code change needed.

## Evidence hygiene

Per [`docs/CLAIM_DISCIPLINE.md`](../../docs/CLAIM_DISCIPLINE.md) §1, every cell in `full_results.json` falls into the **Harness** evidence class: each comes from a labelled, repeat-runnable eval, not a microbenchmark or a simulation.  The `"measured": false` flag is non-negotiable — any dataset without a real detail JSONL stays in that state until its JSONL is produced and committed.

## Current state

Only TruthfulQA is measured on-repo right now.  The others are **wired** (they will appear with `measured: 1` the moment the respective detail file exists) but **not filled in**.  A PR filling one of them in is welcome and specifically labelled "good first benchmark" in [`docs/GOOD_FIRST_ISSUES.md`](../../docs/GOOD_FIRST_ISSUES.md) (when that row is added).

---

*Spektre Labs · Creation OS · 2026*
