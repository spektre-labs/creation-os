# v143 — σ-Benchmark

> Don't use someone else's benchmark. Create your own.

## Problem

External LLM benchmarks (MMLU, ARC, HellaSwag, GSM8K) measure what
their authors cared about — usually accuracy on tokenwise
next-prediction. Creation OS is a **σ-gated** system, so the
interesting questions are different:

* Does σ *correlate with error*?
* Does the model abstain when it cannot know?
* Does the σ-router pick the right specialist?
* Does continual learning improve σ *without forgetting*?
* Does σ rise under adversarial input?

v143 ships the first five-category suite that measures exactly those,
and writes a single canonical JSON so v133 meta-dashboard, external
consumers, and (in v143.1) Hugging Face leaderboards can all consume
the same shape.

## Five categories

| Category              | What it measures                                          | Primary metric                    |
| --------------------- | --------------------------------------------------------- | --------------------------------- |
| σ-Calibration         | Does σ correlate with wrongness?                          | ECE across 10 bins                |
| σ-Abstention          | Does the model abstain on oracle-unknowable questions?    | Coverage @ 95% accuracy           |
| σ-Swarm routing       | Does argmin σ pick the right specialist?                  | Routing accuracy + σ-spread       |
| σ-Learning            | Does continual learning improve σ without forgetting?     | ΔAccuracy + hold-out drift        |
| σ-Adversarial         | Does σ rise on prompt-injection / jailbreak attempts?     | Detection rate @ FPR ≤ 5%         |

## Output shape

A single canonical JSON object at
`benchmarks/v143/creation_os_benchmark.json`:

```json
{
  "version": 1,
  "seed": 12605371,
  "tier": "v143.0-synthetic",
  "calibration":  { "ece": 0.0662,  "n": 1000 },
  "abstention":   { "coverage_at_95": 0.618, "accuracy": 0.9806, "n": 500 },
  "swarm":        { "routing_accuracy": 1.000, "sigma_spread": 0.740, "n": 200 },
  "learning":     { "accuracy_before": 0.14, "accuracy_after": 0.66,
                    "sigma_before": 0.55, "sigma_after": 0.30,
                    "holdout_drop": -0.07, "n": 100 },
  "adversarial":  { "detection_at_fpr05": 0.985, "fpr": 0.040, "n": 200 }
}
```

The `"tier"` field is the key honesty flag:

* `"v143.0-synthetic"` — every number came from a deterministic
  PRNG-driven synthetic dataset. The numbers verify the *benchmark
  infrastructure*, not a live Creation OS run.
* `"v143.1-archived"` (future) — numbers come from σ-traces
  captured by v104/v106 during real usage.

## Merge-gate measurements

All deterministic under a fixed seed (SplitMix64 + Box-Muller).

| Stage                                              | Gate                                    |
| -------------------------------------------------- | --------------------------------------- |
| self-test: 5 categories green on synthetic         | ECE < 0.15, coverage > 0.30 @ 95% acc,  |
|                                                    | routing > 0.80, spread > 0.30, Δacc > 0,|
|                                                    | holdout |Δ| < 0.10, detection > 0.70    |
| `--run --seed` emits complete JSON                 | 5 category keys + tier label present    |
| `--run --seed --out path` writes JSON to disk      | file non-empty + key fields present     |
| byte-identical JSON across repeated runs           | `diff -q` on two runs with same seed    |

## v143.0 / v143.1 split

**v143.0 (shipped)**

* Five categories on deterministic synthetic data.
* SplitMix64 + Box-Muller PRNG (deterministic across platforms).
* Canonical JSON output with `"tier":"v143.0-synthetic"` flag.
* CLI with `--self-test`, `--run --seed N`, `--out path`.

**v143.1 (follow-up)**

* Replace synthetic calibration set with **1000 archived
  (prompt, response, σ, is_correct) tuples** from v104/v106.
* Replace synthetic abstention set with **500 RAG unknowables** with
  oracle labels.
* Replace synthetic swarm traces with real v114 multi-specialist
  routes.
* Replace synthetic learning set with v124 before/after captures.
* Replace synthetic adversarial set with v122 red-team corpus.
* Publish the entire bundle to
  `spektre-labs/cos-benchmark` on Hugging Face so external models
  can run the same benchmark and report scores.
* New tier label: `"v143.1-archived"`.

## Honest claims

* v143.0's numbers do **not** represent any real Creation OS run.
  They confirm that the benchmark pipeline (metric math, JSON
  writer, determinism) works correctly. Every honest consumer must
  look at the `"tier"` field before quoting numbers.
* ECE is computed *only* when a bin is non-empty, following Naeini
  et al. (2015). Very small synthetic samples can give unstable ECE;
  the self-test uses n=1000 to keep variance contained.
* Coverage @ 95% accuracy uses a **discrete** τ-sweep (20 steps)
  because a continuous optimisation would blur the accuracy cliff.
  v143.1's archived data uses a finer sweep.
* Adversarial detection assumes the attacker does **not** specifically
  target the σ-layer. A calibrated adversary who co-optimises prompt
  complexity to mimic σ noise can defeat any single-threshold
  detector; v122 red-team + v138 proof + v143 benchmark together are
  the answer, not v143 alone.
