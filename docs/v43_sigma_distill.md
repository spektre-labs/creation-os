# v43 — σ-guided knowledge distillation (lab)

**Evidence class:** portable C policy + stable KL helpers (`make check-v43`). **Not** an in-repo teacher/student training loop, API-backed multi-teacher runner, or TruthfulQA row archive.

## What ships

| Module | Role |
|--------|------|
| `src/v43/sigma_distill.c` | `v43_sigma_weight`, forward KL `p_teacher \|\| q_student`, reverse KL for negative-weight “anti-transfer”, `v43_sigma_distillation_loss` |
| `src/v43/progressive_distill.c` | Four-stage curriculum keyed on **teacher epistemic** σ; stage 4 LR `0` (handoff to v42 self-play conceptually) |
| `src/v43/multi_teacher.c` | σ-inverse-weighted mean of teacher logits (same `vocab_size`) |
| `src/v43/calibrated_student.c` | L2 calibration on `(σ_epistemic, σ_aleatoric)` + `L_total = L_distill + λ L_calibrate` |

## Honest limits

- **No external verifier replacement claim** until archived runs exist; see `docs/WHAT_IS_REAL.md` (N-tier rows for un-wired harnesses).
- **Negative KL weight** is an experimental repulsion term; optimization stability depends on the outer trainer — this repo only supplies the scalar contract.

## Verify

```bash
make check-v43
make bench-v43-distill   # stub script; exits 0
```

## Working paper title (only when a harness exists)

**“σ-Guided Knowledge Distillation: Uncertainty-Weighted Targets Reduce Hallucination Transfer in Small Students”**

Thesis sentence: weight each token’s KD term by decomposed teacher σ; treat confident–inconsistent bands as **negative transfer** (reverse KL), and pair distillation with **σ calibration** so the student inherits meta-cognitive structure, not only logits.
