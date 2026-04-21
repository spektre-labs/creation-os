/*
 * AGI-2 — σ-guided self-play with curriculum.
 *
 * Wraps the existing σ-selfplay primitive (pipeline/selfplay.c) with
 * an outer loop that moves the difficulty target through the canonical
 * "learning zone" band (σ ≈ 0.3–0.5) using only σ observations — no
 * external labels, no cloud.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_AGI_SELFPLAY_H
#define COS_SIGMA_AGI_SELFPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int    n_rounds;
    int    n_escalations;   /* σ_answer ≥ escalation_threshold      */
    float  sigma_mean;
    float  sigma_target_end;/* curriculum knob after last round       */
    float  difficulty_end;
} cos_agi_selfplay_report_t;

/* Run `n_rounds` self-play rounds on `domain` starting from
 * difficulty 0.35.  The solver σ is synthesised as a smooth function
 * of |difficulty − σ_target| so the curriculum has a stable fixed
 * point near the target band.  Escalation is counted when σ_answer
 * exceeds `escalation_threshold` (teacher fallback in a full build).
 *
 * Deterministic: identical (domain, n_rounds, seed) → identical
 * report fields.  Returns 0 on success. */
int cos_agi_selfplay_run(const char *domain, int n_rounds,
                         float escalation_threshold,
                         cos_agi_selfplay_report_t *out);

int cos_agi_selfplay_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_AGI_SELFPLAY_H */
