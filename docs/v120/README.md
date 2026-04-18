# v120 — σ-Distill: σ-Targeted Knowledge Transfer Big → Small

Classical knowledge distillation ([Hinton 2015][1]) copies **every**
teacher prediction to the student. That is wasteful: the student is
already fine on most of them. v120 uses σ to pick the *opportunity
rows* — records where the teacher is confident and the student is
uncertain. Only those go into the SFT set.

[1]: https://arxiv.org/abs/1503.02531

## The problem v120 solves

v114's swarm routes between a small model (BitNet b1.58 2B) and
larger specialists (Qwen2.5 7B, Llama-3.1 8B). Whenever the large
model emits a low-σ response where the small model emits a high-σ
one, we have a teachable moment. v120 mines those moments and
produces a tight SFT dataset for the small model — so v114 routing
collapses onto the small model more often, saving energy and cost.

## The σ-targeting rule

Row is kept iff

```
σ_big  <  τ_big_low      (default 0.30)   AND
σ_small >  τ_small_high  (default 0.50)
```

Everything else is dropped (and counted, by reason):
student-already-confident, teacher-too-uncertain, both-wrong,
malformed.

## What ships in v120.0

- Pure-C JSONL → JSONL selector that reads
  `{id, prompt, big_response, small_response, sigma_big, sigma_small}`
  and writes the SFT form
  `{id, prompt, response:<big>, sigma_big, sigma_small,
   source:"v120-sigma-distill"}`.
- One-line JSON **manifest** with all drop reasons + mean σ for kept
  rows + the τ thresholds used, so downstream trainers have a
  tamper-evident provenance stub.
- Self-test with an 8-row synthetic corpus: three kept, one
  student-confident reject, one both-wrong reject, one
  teacher-uncertain reject, one boundary non-keep, one malformed.

## What lands in v120.1

- MLX SFT training recipe in `training/v120/` that consumes the
  v120.0 SFT JSONL, runs LoRA on TinyLlama 1.1B or BitNet 2B on
  Apple M3 MLX, and emits
  `models/v120/distill_8b_to_2b.safetensors` loadable by the v101
  bridge.
- Pre/post σ-calibration harness: measure σ drift on the kept rows
  **only**, since general-corpus regression is not the claim.
- Optional streaming mode so the selector can run against a live
  v114 session log instead of a static corpus.

## Source

- [`src/v120/distill.h`](../../src/v120/distill.h)
- [`src/v120/distill.c`](../../src/v120/distill.c) — JSON line
  parser, selector, SFT writer, manifest JSON
- [`src/v120/main.c`](../../src/v120/main.c) — CLI (`--self-test`,
  `--select IN OUT [τ_big_low] [τ_small_high]`)
- [`benchmarks/v120/check_v120_distill_smoke.sh`](../../benchmarks/v120/check_v120_distill_smoke.sh)

## Self-test

```bash
make check-v120
```

Runs the pure-C self-test plus a 5-row smoke that drives the CLI end
to end, checks the manifest counts, and confirms the teacher
response is present in the SFT JSONL (and the student response
isn't).

## Claim discipline

- v120.0 is the **selector and manifest**. The kept rows and their
  provenance are measured by tests; nothing about training is
  claimed by v120.0.
- The "small catches up to big" narrative is **only** valid after
  v120.1's LoRA lands and is evaluated on the exact kept-row
  distribution. Whatever gain shows up there is a v120.1 claim, not a
  v120.0 claim.
- The selector deliberately refuses rows where **both** models are
  wrong (student-σ high AND teacher-σ high): distilling those
  propagates teacher error into the student.

## σ-stack layer

v120 sits at **Training + Persistence**. It transforms a v114 swarm
log plus per-response σ (from v106) into a narrowly-scoped SFT set
for the small model, feeding back into the **Generation** layer.

---

_Copyright (c) 2026 Spektre Labs · AGPL-3.0-or-later · Creation OS_
