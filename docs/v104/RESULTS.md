# v104 — σ-Operator Search Results

**Host.** Apple M3 / macOS 14.6 / bitnet.cpp i2_s / llama.cpp backend.
**Model.** BitNet b1.58 2B-4T (`models/BitNet-b1.58-2B-4T/ggml-model-i2_s.gguf`,
published GGUF from Microsoft).
**Harness.** `third_party/lm-eval` (EleutherAI lm-evaluation-harness,
vendored editable install), v0.4.12.dev0.
**σ profile.** v103 backend persists per-(doc, candidate) full 8-channel
σ profile + σ_max_token into a sidecar JSONL; v104 operators are
closed-form reductions over that sidecar.

**Pre-registration.** [OPERATOR_SEARCH.md](OPERATOR_SEARCH.md) fixes the
ten σ-operators, the reference (entropy = channel 0), the Bonferroni
thresholds, and the three hypotheses H1/H2/H3 **before** any n=5000
number was read.

**Date.** 2026-04-17 evening → 2026-04-18 local time.
**Wall-time.** σ-log pass ≈ 2 h 37 min total for 3 tasks.
Post-hoc analysis ≈ 10 min.

---

## 1. Dataset sizes (full splits)

lm-eval caps `--limit` at dataset size.  We passed `--limit 5000`, which
for these MC tasks resolves to the full validation/test split:

| task             | docs  | ll-calls logged |
|------------------|------:|----------------:|
| `truthfulqa_mc2` |   817 |           5 882 |
| `arc_challenge`  | 1 172 |           4 687 |
| `arc_easy`       | 2 376 |           9 501 |
| **total**        | 4 365 |      **20 070** |

Overall top-1 accuracy (lm-eval `acc`) on each:
- truthfulqa_mc2: **0.4638**
- arc_challenge: **0.4676**
- arc_easy: **0.7551**

These match Microsoft's published BitNet b1.58 2B-4T numbers within
sampling noise (their full-split arc_easy/arc_challenge/truthfulqa_mc2
are 77.4 / 43.9 / 45.7).  Accuracy is **not** the deliverable here; the
deliverable is risk-coverage behaviour.

## 2. Operator comparison — paired bootstrap ΔAURCC vs. entropy

2 000 paired bootstrap resamples.  Bonferroni α = 0.05 / 10 = **0.005**
over the ten σ-candidates.  `sig?` flags Bonferroni significance (`yes`
= Δ < 0 and p < 0.005; `HURTS` = Δ > 0 and p < 0.005).

### truthfulqa_mc2 (n = 817, acc = 0.4638)

| operator | AURCC ↓ | ΔAURCC vs entropy | CI95 | p | sig? |
|---|---:|---:|---|---:|:---:|
| `sigma_mean` | 0.5617 | +0.0089 | [+0.0014, +0.0160] | 0.020 |  |
| **`sigma_max_token`** | **0.5080** | **−0.0447** | **[−0.0708, −0.0189]** | **0.0005** | **yes** |
| `sigma_max_channel` | 0.5489 | −0.0038 | [−0.0344, +0.0288] | 0.809 |  |
| `sigma_topk_mean_k3` | 0.5664 | +0.0140 | [+0.0033, +0.0251] | 0.010 |  |
| **`sigma_p95_token`** | **0.5126** | **−0.0401** | **[−0.0633, −0.0176]** | **0.0005** | **yes** |
| **`sigma_product`** | **0.5411** | **−0.0114** | **[−0.0191, −0.0040]** | **0.0030** | **yes** |
| `sigma_l2` | 0.5642 | +0.0114 | [−0.0008, +0.0241] | 0.074 |  |
| `sigma_max_of_max` | 0.5365 | −0.0161 | [−0.0470, +0.0154] | 0.308 |  |
| **`sigma_entropy_hybrid`** | **0.5151** | **−0.0376** | **[−0.0573, −0.0183]** | **0.0005** | **yes** |
| `sigma_per_channel_best` | 0.5528 | 0 | — | — |  |
| `entropy` *(ref)* | 0.5528 | 0 | — | — |  |
| `margin` | 0.5725 | +0.0198 | [+0.0078, +0.0318] | 0.001 | **HURTS** |
| `p_max` | 0.5625 | +0.0098 | [+0.0012, +0.0184] | 0.022 |  |
| `random` | 0.5503 | −0.0029 | [−0.0466, +0.0387] | 0.887 |  |

**Four of the ten σ candidates beat entropy Bonferroni-significantly.**
The winner `sigma_max_token` achieves AURCC 0.5080 vs entropy 0.5528
(−8 % relative AURCC reduction).  `sigma_p95_token` is a close second.
The `margin` **single channel** hurts selective prediction
Bonferroni-significantly — i.e. the ranking the argmin of the
top1-vs-top2 margin produces is *worse* than random ranking inside the
σ profile.

### arc_challenge (n = 1 172, acc = 0.4676)

| operator | AURCC ↓ | ΔAURCC vs entropy | CI95 | p | sig? |
|---|---:|---:|---|---:|:---:|
| `sigma_mean` | 0.4712 | +0.0042 | [−0.0026, +0.0110] | 0.217 |  |
| `sigma_max_token` | 0.4572 | −0.0101 | [−0.0319, +0.0109] | 0.351 |  |
| `sigma_max_channel` | 0.4955 | +0.0287 | [+0.0075, +0.0486] | 0.006 |  |
| `sigma_topk_mean_k3` | 0.4825 | +0.0155 | [+0.0042, +0.0264] | 0.010 |  |
| `sigma_p95_token` | 0.4578 | −0.0094 | [−0.0280, +0.0084] | 0.321 |  |
| **`sigma_product`** | **0.4585** | **−0.0087** | **[−0.0143, −0.0028]** | **0.0040** | **yes** |
| `sigma_l2` | 0.4795 | +0.0125 | [+0.0015, +0.0235] | 0.034 |  |
| `sigma_max_of_max` | 0.4908 | +0.0242 | [+0.0037, +0.0437] | 0.018 |  |
| `sigma_entropy_hybrid` | 0.4562 | −0.0109 | [−0.0263, +0.0038] | 0.157 |  |
| `sigma_per_channel_best` | 0.4672 | 0 | — | — |  |
| `entropy` *(ref)* | 0.4672 | 0 | — | — |  |
| `margin` | 0.4843 | +0.0173 | [+0.0041, +0.0303] | 0.010 |  |
| `p_max` | 0.4746 | +0.0075 | [−0.0019, +0.0168] | 0.104 |  |
| `random` | 0.5199 | +0.0526 | [+0.0131, +0.0935] | 0.008 |  |

**Only `sigma_product` passes Bonferroni here** (p = 0.004; paired-
bootstrap CI strictly below 0).  The other candidates trend negative
but the CIs are too wide at n = 1 172.  `sigma_max_token` is
Δ = −0.0101 (directionally consistent with truthfulqa) but p = 0.35.

### arc_easy (n = 2 376, acc = 0.7551)

| operator | AURCC ↓ | ΔAURCC vs entropy | CI95 | p | sig? |
|---|---:|---:|---|---:|:---:|
| `sigma_mean` | 0.1727 | +0.0017 | [−0.0016, +0.0049] | 0.325 |  |
| `sigma_max_token` | 0.1691 | −0.0018 | [−0.0124, +0.0091] | 0.755 |  |
| `sigma_max_channel` | 0.1930 | +0.0219 | [+0.0112, +0.0326] | 0.0005 | **HURTS** |
| `sigma_topk_mean_k3` | 0.1780 | +0.0071 | [+0.0014, +0.0125] | 0.017 |  |
| `sigma_p95_token` | 0.1679 | −0.0030 | [−0.0121, +0.0061] | 0.527 |  |
| `sigma_product` | 0.1712 | +0.0003 | [−0.0025, +0.0031] | 0.864 |  |
| `sigma_l2` | 0.1769 | +0.0060 | [+0.0010, +0.0111] | 0.023 |  |
| `sigma_max_of_max` | 0.1927 | +0.0217 | [+0.0112, +0.0318] | 0.0005 | **HURTS** |
| `sigma_entropy_hybrid` | 0.1661 | −0.0048 | [−0.0119, +0.0023] | 0.194 |  |
| `sigma_per_channel_best` | 0.1710 | 0 | — | — |  |
| `entropy` *(ref)* | 0.1710 | 0 | — | — |  |
| `margin` | 0.1795 | +0.0085 | [+0.0015, +0.0152] | 0.014 |  |
| `p_max` | 0.1742 | +0.0032 | [−0.0013, +0.0076] | 0.174 |  |
| `random` | 0.2487 | +0.0778 | [+0.0556, +0.0997] | 0.0005 | **HURTS** |

**No σ candidate beats entropy Bonferroni-significantly on arc_easy.**
This is expected: base accuracy is 0.755, leaving thin selective-
prediction headroom.  The operators that aggregate via max
(`sigma_max_channel`, `sigma_max_of_max`) *hurt* Bonferroni-sig here —
the max is noise-dominated at high baseline accuracy.

### Cross-task robustness (unregistered, secondary)

| operator | truthfulqa p | arc_challenge p | arc_easy p | 2-of-3 sig? |
|---|---:|---:|---:|:---:|
| `sigma_max_token` | **0.0005** yes | 0.35 | 0.76 | 1 of 3 |
| `sigma_p95_token` | **0.0005** yes | 0.32 | 0.53 | 1 of 3 |
| `sigma_entropy_hybrid` | **0.0005** yes | 0.16 | 0.19 | 1 of 3 |
| **`sigma_product`** | **0.003** yes | **0.004** yes | 0.86 (neutral) | **2 of 3** |

**Only `sigma_product` beats entropy Bonferroni-significantly on both
difficult tasks simultaneously and is Δ-neutral on arc_easy.**  This
is a stronger cross-task finding than H1 alone.

## 3. Per-channel diagnostic

2 000 paired bootstrap resamples.  Bonferroni α = 0.05 / 7 = **0.00714**
over channels 1..7 vs. channel 0 (entropy_norm).

### truthfulqa_mc2

| ch | name           | AURCC ↓ | Δ vs entropy | CI95 | p | sig? |
|---:|----------------|---:|---:|---|---:|:---:|
| 0 | `entropy_norm` | 0.5528 | 0 | — | — |  |
| 1 | `margin`       | 0.5725 | +0.0200 | [+0.0083, +0.0326] | **0.002** | **HURTS** |
| 2 | `top_k_mass`   | 0.5366 | −0.0162 | [−0.0300, −0.0022] | 0.023 |  |
| 3 | **`tail_mass`**| 0.5267 | −0.0255 | [−0.0432, −0.0086] | **0.004** | **yes** |
| 4 | `logit_spread` | 0.5566 | +0.0043 | [−0.0162, +0.0258] | 0.682 |  |
| 5 | `p_max`        | 0.5625 | +0.0100 | [+0.0016, +0.0184] | 0.024 |  |
| 6 | **`n_effective`** | 0.5316 | −0.0207 | [−0.0311, −0.0100] | **0.0005** | **yes** |
| 7 | `logit_std`    | 0.5537 | +0.0013 | [−0.0392, +0.0435] | 0.962 |  |

### arc_challenge

| ch | name           | AURCC ↓ | Δ vs entropy | CI95 | p | sig? |
|---:|----------------|---:|---:|---|---:|:---:|
| 0 | `entropy_norm` | 0.4672 | 0 | — | — |  |
| 1 | `margin`       | 0.4843 | +0.0171 | [+0.0042, +0.0309] | 0.013 |  |
| 2 | `top_k_mass`   | 0.4561 | −0.0114 | [−0.0228, +0.0005] | 0.064 |  |
| 3 | `tail_mass`    | 0.4521 | −0.0152 | [−0.0310, +0.0003] | 0.055 |  |
| 4 | `logit_spread` | 0.4754 | +0.0077 | [−0.0149, +0.0317] | 0.526 |  |
| 5 | `p_max`        | 0.4746 | +0.0074 | [−0.0017, +0.0165] | 0.110 |  |
| 6 | `n_effective`  | 0.4591 | −0.0088 | [−0.0175, −0.0007] | 0.036 |  |
| 7 | `logit_std`    | 0.5302 | +0.0626 | [+0.0282, +0.0986] | **0.0005** | **HURTS** |

### arc_easy

| ch | name           | AURCC ↓ | Δ vs entropy | CI95 | p | sig? |
|---:|----------------|---:|---:|---|---:|:---:|
| 0 | `entropy_norm` | 0.1710 | 0 | — | — |  |
| 1 | `margin`       | 0.1795 | +0.0086 | [+0.0020, +0.0152] | 0.013 |  |
| 2 | `top_k_mass`   | 0.1719 | +0.0008 | [−0.0045, +0.0060] | 0.764 |  |
| 3 | `tail_mass`    | 0.1807 | +0.0096 | [+0.0023, +0.0170] | 0.010 |  |
| 4 | `logit_spread` | 0.1837 | +0.0126 | [+0.0012, +0.0243] | 0.036 |  |
| 5 | `p_max`        | 0.1742 | +0.0031 | [−0.0014, +0.0080] | 0.178 |  |
| 6 | `n_effective`  | 0.1721 | +0.0010 | [−0.0032, +0.0054] | 0.668 |  |
| 7 | `logit_std`    | 0.2438 | +0.0727 | [+0.0529, +0.0928] | **0.0005** | **HURTS** |

**Headline from the channel diagnostic.** `logit_std` (channel 7) is a
Bonferroni-significant **anti-signal** on both arc_easy and
arc_challenge (ΔAURCC > 0, p = 0.0005) and neutral on truthfulqa.  It
is *the* channel that drags σ_mean down to a null result — the mean
dilutes the informative channels (tail_mass, n_effective on
truthfulqa) with a destructive one.  `sigma_product`'s cross-task win
has a very likely mechanistic explanation: the geometric mean of a
value near-zero with a value near-one is near-zero, so a low logit_std
does **not** drag the product up the way it drags the mean up.  The
mean is the wrong aggregator; the product is roughly the right one.

## 4. α-sweep hybrid (hybrid_α = α · sigma_second + (1−α) · entropy)

Bonferroni α = 0.05 / 11 = **0.00455** (11 α-points 0.0..1.0).

### second = `sigma_max_token`

| task | best α* | AURCC at α* | Δ at α* | p at α* | interior optimum? |
|---|---:|---:|---:|---:|:---:|
| truthfulqa_mc2 | 1.0 | 0.5080 | −0.0446 | 0.0010 | **no** (boundary) |
| arc_challenge  | 0.6 | 0.4561 | −0.0112 | 0.169  | **yes** (but p > Bonf) |
| arc_easy       | 0.5 | 0.1661 | −0.0050 | 0.197  | **yes** (but p > Bonf) |

On truthfulqa, the α-sweep is strictly monotone: α = 1.0 (pure
σ_max_token) is best.  Pure σ_max_token dominates the entropy-σ
hybrid — entropy adds **no independent information** given σ_max_token
on this task.  On the other two tasks the optimum is interior
(α* ∈ {0.5, 0.6}) but the paired-bootstrap p exceeds the
α-sweep Bonferroni bar.

### second = `sigma_product`

| task | best α* | AURCC at α* | Δ at α* | p at α* | significant? |
|---|---:|---:|---:|---:|:---:|
| truthfulqa_mc2 | 1.0 | 0.5411 | −0.0115 | 0.003  | **yes** |
| arc_challenge  | 1.0 | 0.4585 | −0.0088 | 0.004  | **yes** |
| arc_easy       | 0.6 | 0.1709 | −0.0001 | 0.868  |  |

Same story at α = 1.0: pure σ_product wins on both hard tasks.

**Implication for H3.** H3 is **not** confirmed.  The observed α-
sweep shapes are monotone on the one task where H1 is confirmed, and
the two tasks where the optimum is interior have CIs too wide to
distinguish from zero at Bonferroni.  σ and entropy are **not
carrying independent selective-prediction information** in a way an
affine combination can exploit at n ≤ 2 376.  Either they are highly
correlated (so the linear combination buys nothing) or independence
exists but is weak and needs > 10 k docs per task to detect.

## 5. Pre-registered hypotheses — verdict

| H | description | outcome |
|---|---|---|
| **H1** | σ_max_token beats entropy on truthfulqa_mc2 at n = 5 000 (paired-bootstrap 95 % CI upper bound < 0) | **CONFIRMED** — Δ = −0.0447, CI = [−0.071, −0.019], p = 0.0005 |
| **H2** | some single σ channel (1..7) beats entropy Bonferroni-significantly | **CONFIRMED** — on truthfulqa: `n_effective` (p = 0.0005) and `tail_mass` (p = 0.004), both CI-clear of zero.  Also `logit_std` is Bonferroni-HURTS on arc_easy and arc_challenge. |
| **H3** | best α-sweep hybrid is interior and Bonferroni-significant | **NOT CONFIRMED** — monotone on truthfulqa (α* = 1.0, boundary); interior on the other two but CIs too wide. |

## 6. Unregistered, secondary findings

1. **`sigma_product` is the cross-task-robust winner** — only operator
   Bonferroni-significant on *both* arc_challenge and truthfulqa_mc2,
   and Δ-neutral on arc_easy.  This is not a pre-registered hypothesis,
   so we do not overclaim; it is the most natural next pre-registered
   hypothesis for a follow-up experiment at a second scale
   (7 B, 70 B) or on open-ended generation tasks.
2. **σ is over-aggregated by the mean.** `margin` and `logit_std` are
   Bonferroni-hurt in several places, and yet they currently enter
   `sigma_mean` with equal weight as the informative channels.  This
   suggests a rethink of the v29 architecture — either drop those
   channels, or make the per-channel weights learned.
3. **`sigma_per_channel_best`** (the "pick one channel" oracle) is
   identical to `entropy` in every task because no per-channel `p` was
   Bonferroni-significant on any task except truthfulqa_mc2, and on
   truthfulqa the winner is `n_effective` which we did not lock in as
   the operator-search candidate.  The lock-in would be a v104.5
   experiment with `n_effective` as the single-channel gate; we have
   not run it.

## 7. Reproducibility

- Repo: `creation-os-kernel` @ branch `feature/operator-search-v104`.
- σ-logging run:
  ```sh
  make standalone-v101-real
  bash benchmarks/v104/run_n5000_sigma_log.sh --limit 5000
  ```
- Post-hoc analysis:
  ```sh
  PY=.venv-bitnet/bin/python
  export PYTHONPATH="$PWD/third_party/lm-eval:$PWD/benchmarks/v104:$PWD/benchmarks/v103"
  $PY benchmarks/v104/compute_operator_rcc.py --results benchmarks/v104/n5000_results
  $PY benchmarks/v104/channel_analysis.py     --results benchmarks/v104/n5000_results
  $PY benchmarks/v104/hybrid_sweep.py         --results benchmarks/v104/n5000_results --second sigma_max_token
  $PY benchmarks/v104/hybrid_sweep.py         --results benchmarks/v104/n5000_results --second sigma_product
  ```
- Archived deliverables:
  - `benchmarks/v104/results/operator_comparison.{json,md}`
  - `benchmarks/v104/results/channel_ranking.{json,md}`
  - `benchmarks/v104/results/hybrid_sweep_sigma_max_token.{json,md}`
  - `benchmarks/v104/results/hybrid_sweep_sigma_product.{json,md}`
  - Sidecar JSONL + lm-eval samples: **not archived** to git (too
    large); regenerate via the shell command above.

## 8. Conclusion

v103 asked: is σ a selective-prediction signal?  It answered null for
σ_mean and suggestive for σ_max_token.  v104 answers the sharper
question: **which aggregator?**

At n = 4 365 across three MC tasks, the honest verdict is:

- **σ-stack has real selective-prediction signal, but the mean throws
  it away.**  Mean aggregation dilutes informative channels
  (`tail_mass`, `n_effective`) with a destructive channel (`logit_std`),
  and dilutes per-token peaks with per-token averages.
- **The two aggregators that extract this signal** are the **per-token
  maximum** (`sigma_max_token`, winner on truthfulqa_mc2 alone) and the
  **per-channel geometric mean** (`sigma_product`, cross-task winner on
  both hard tasks).  The former punishes bad *moments*; the latter
  punishes bad *channels*.
- **σ and entropy are not independent** in a way the affine hybrid can
  exploit at this scale.  H3 is unsupported.

Implication for v105: replace the `σ_mean > τ` gate with either
`σ_product > τ` (cross-task-robust default) or `σ_max_token > τ`
(per-task tuned, known-best on hard open-ended tasks like truthfulqa).
The σ-stack is kept; the aggregator is swapped.  See
[INTERPRETATION.md](INTERPRETATION.md) for which roadmap branch this
implies and [../ROADMAP_TO_AGI.md](../ROADMAP_TO_AGI.md) §0d / §0e for
the current status.
