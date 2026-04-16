/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v43 lab: progressive σ-staged distillation curriculum (teacher epistemic bands).
 */
#ifndef CREATION_OS_V43_PROGRESSIVE_DISTILL_H
#define CREATION_OS_V43_PROGRESSIVE_DISTILL_H

typedef struct {
    int stage;
    float sigma_min;
    float sigma_max;
    float learning_rate;
    int n_tokens;
} v43_distill_stage_t;

/** Four stages: easy → medium → edge → beyond-teacher (hand off to v42 self-play). */
extern const v43_distill_stage_t v43_distill_stages[4];

/** Map teacher epistemic σ to stage index 0..3 (inclusive). */
int v43_stage_index_for_teacher_epistemic(float epistemic);

const v43_distill_stage_t *v43_stage_for_teacher_epistemic(float epistemic);

#endif /* CREATION_OS_V43_PROGRESSIVE_DISTILL_H */
