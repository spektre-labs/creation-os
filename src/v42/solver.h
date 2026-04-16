/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef CREATION_OS_V42_SOLVER_H
#define CREATION_OS_V42_SOLVER_H

#include "../sigma/decompose.h"
#include "challenger.h"

typedef struct {
    char *solution;
    sigma_decomposed_t sigma;
    int n_attempts;
    float reward;
} solve_result_t;

void v42_solve_result_free(solve_result_t *r);

/** Deterministic multi-sample “agreement” proxy in [0,1] (lab; not semantic embedding). */
float v42_measure_consistency_lab(const char *prompt, int n);

/**
 * σ-decomposed reward table (lab) — uses epistemic σ + consistency proxy.
 * No external verifier; host must not treat this as a harness row without calibration.
 */
int v42_solve_with_sigma_reward_lab(const challenge_t *challenge, solve_result_t *out);

#endif /* CREATION_OS_V42_SOLVER_H */
