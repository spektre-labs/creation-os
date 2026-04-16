/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v48 lab: defense-in-depth orchestration (fail-closed aggregate).
 */
#ifndef CREATION_OS_V48_DEFENSE_LAYERS_H
#define CREATION_OS_V48_DEFENSE_LAYERS_H

#include "../sigma/decompose.h"
#include "sandbox.h"

typedef enum {
    DEFENSE_INPUT_FILTER = 0,
    DEFENSE_SIGMA_GATE = 1,
    DEFENSE_ANOMALY_DETECT = 2,
    DEFENSE_SANDBOX = 3,
    DEFENSE_OUTPUT_FILTER = 4,
    DEFENSE_RATE_LIMIT = 5,
    DEFENSE_AUDIT_LOG = 6,
    N_DEFENSE_LAYERS = 7
} defense_layer_t;

typedef struct {
    /** Non-zero iff layer @p i passed (v48 sketch name: `layer_active`). */
    int layer_active[N_DEFENSE_LAYERS];
    int layers_passed;
    int layers_failed;
    int final_decision;
    char *block_reason;
} defense_result_t;

defense_result_t run_all_defenses(
    const char *input,
    const char *output,
    const sigma_decomposed_t *sigmas,
    int n_tokens,
    const sandbox_config_t *sandbox);

void defense_result_free(defense_result_t *r);

#endif /* CREATION_OS_V48_DEFENSE_LAYERS_H */
