/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * Rule-based Ω-loop config adaptation (EvoTest-style, auditable).
 */
#ifndef COS_OMEGA_EVOLVER_H
#define COS_OMEGA_EVOLVER_H

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cos_evolver_config {
    float tau_accept;
    float tau_rethink;
    float semantic_temp_low;
    float semantic_temp_high;
    int   semantic_max_tokens;
    int   preferred_tier;
} cos_evolver_config_t;

/** Deterministic step: `sigma_history` / `correct_history` length = n_history.
 *  correct_history[i]=1 means terminal action was ACCEPT for that episode.
 *  Returns `next` with clamped fields. */
cos_evolver_config_t cos_evolver_step(cos_evolver_config_t current,
                                       const float *sigma_history, int n_history,
                                       const int *correct_history, int n_correct);

void cos_evolver_emit_jsonl(FILE *fp, int64_t t_ms, int turn,
                            const cos_evolver_config_t *prev,
                            const cos_evolver_config_t *next);

/** Defaults suitable for cos_think / Ω-loop. */
void cos_evolver_config_default(cos_evolver_config_t *c);

/** Gate [evolver] stderr rule prints (Ω-loop passes --verbose here). */
void cos_evolver_set_stderr_verbose(int on);

int cos_evolver_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_OMEGA_EVOLVER_H */
