# v103 σ-τ-Sweep

**Status.** Measured (tier M) for the three-task sweep on Apple M3 /
bitnet.cpp i2_s, 2026-04-17.  See [RESULTS.md](RESULTS.md) for the
current numbers; see [SELECTIVE_PREDICTION.md](SELECTIVE_PREDICTION.md)
for the literature context and the formal definitions of AURCC / AUACC
/ Coverage@target.

## What it is

v103 is **not a new C kernel**.  It is a pure post-hoc analysis layer
on top of v101 σ-BitNet-Bridge + v102 σ-Eval-Harness:

1. A second lm-eval backend (`creation_os_v103`) that is identical to
   v102's `creation_os` backend **except** it serialises, per
   loglikelihood call, the full eight-channel σ-profile plus the
   max-per-token σ encountered during teacher-forced decoding of the
   continuation.  That sidecar JSONL becomes the primitive from which
   every τ-point in the sweep is constructed.
2. A Python analyser (`benchmarks/v103/compute_rc_metrics.py`) that
   reads the lm-eval sample files and the σ sidecar, joins them by
   `(doc_index, cand_index)`, and produces risk-coverage curves for
   six gating signals, then computes AURCC, AUACC, Coverage@0.90,
   Coverage@0.95, Coverage@0.99 for each.

## Why post-hoc, not 10 forward passes per τ

The directive that asked for this sweep proposed re-running the full
eval **once per τ value** (10 values × 3 tasks × 500 samples ≈
15 000 ll-calls × 10 = 150 000 ll-calls ≈ 20 hours).  That is
wasteful: the abstain threshold τ does not affect the forward pass; it
only determines whether a document is *kept* or *dropped* when the
final accuracy is aggregated.  We therefore do **one** run that logs
σ, then sweep τ offline.  Total compute for the 2026-04-17 pass:
~2 h wall on Apple M3 / Metal / bitnet.cpp i2_s, identical to the
v102 pass.

The post-hoc approach is not just cheaper — it is also **more honest**
because the analyser can swap the gating signal (σ-mean vs entropy vs
margin vs random) without re-running anything, so the head-to-head
comparison in RESULTS.md's AURCC column is computed on exactly the
same set of logits.  That eliminates run-to-run noise as a confounder.

## Gating signals compared

| signal           | definition                                                      |
|------------------|------------------------------------------------------------------|
| `sigma_mean`     | mean of the eight σ-channels on the **top-1** (argmax-ll) candidate; this is the v103 primary signal |
| `sigma_max_token` | worst single-token σ seen during the top-1 continuation — captures one-step spikes that the mean would smooth out |
| `entropy`        | σ_profile[0] alone (entropy_norm) — the "entropy baseline" of arXiv:2603.21172 and the classic selective-prediction control |
| `margin_inv`     | σ_profile[1] alone (1 − (p_top1 − p_top2)) — the "top-margin" baseline |
| `p_max`          | σ_profile[5] alone (1 − p_top1) — a simple max-prob control |
| `random`         | uniform [0,1] noise seeded with 1234 — the statistical floor |

Each signal is sorted ascending (most-confident first) to build the
coverage axis.  The risk-coverage curve then tells the reader:
*"If the model is allowed to refuse the α least-confident questions,
what is its conditional accuracy on the remaining (1-α)?"*

## Task-level caveat: MC2 vs single-answer MC

lm-eval's three tasks have different underlying scoring:

- **arc_easy / arc_challenge** — per-doc `acc` ∈ {0, 1} based on
  argmax over four candidate continuations.  Classical selective
  prediction: drop a doc, and the remaining subset's mean acc moves.
- **truthfulqa_mc2** — per-doc `acc` ∈ [0, 1] (continuous): the
  MC2 score is the *normalised probability mass on correct labels*.
  We treat conditional_acc as mean(acc) over kept docs; the AURCC
  geometry remains identical but the "0 or 1" interpretation does
  not apply.

For both styles the signal used to decide abstention is σ of the
**top-1 candidate** (arc: the argmax-ll candidate; MC2: the same —
even though MC2 does not itself take argmax, the model's most-likely
continuation is the natural place to score its own confidence).

## How a τ-sweep is reconstructed from one run

Suppose we want "coverage at τ = 0.4".

1. For every document in every task, look up σ_mean of the top-1
   candidate in the sidecar.
2. Keep docs where σ_mean ≤ 0.4, drop the rest.
3. Coverage = |kept| / |total|; conditional_acc = mean(acc) over kept.

No new forward pass is required; no model state is needed.  The full
RC curve is obtained by sweeping τ ∈ [0, 1] (equivalently, sweeping
coverage ∈ (0, 1] after sorting by σ).  AURCC / AUACC /
Coverage@target are then trivial integrations over that curve.

## Limits of the v103 claim

v103 cannot claim that σ-gating *improves* BitNet's accuracy — the
accuracy at τ = 0 is fixed by the model and was measured in v102 on
2026-04-17.  v103 **can** claim, if the numbers come out that way,
that σ is a *better ranking signal* than entropy alone for abstention
— i.e. AURCC(σ) < AURCC(entropy) on all three tasks outside combined
stderr.  If that inequality does not hold, v103 reports a null result
("σ ≅ entropy") or a negative result ("σ > entropy").  All three
outcomes are pre-registered in
[RESULTS.md](RESULTS.md#honest-interpretation-guide).
