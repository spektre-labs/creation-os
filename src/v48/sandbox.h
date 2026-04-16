/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v48 lab: σ-gated privilege sandbox (policy outside the model weights).
 */
#ifndef CREATION_OS_V48_SANDBOX_H
#define CREATION_OS_V48_SANDBOX_H

#include "../sigma/decompose.h"

typedef struct {
    int can_read_files;
    int can_write_files;
    int can_execute_code;
    int can_make_api_calls;
    int can_access_network;
    int max_output_tokens;
    float sigma_threshold_for_action;
    int require_human_approval_above;
} sandbox_config_t;

typedef struct {
    char *proposed_action;
    float action_sigma;
    int allowed;
    char *reason;
} sandbox_decision_t;

/** Caller must free strings with sandbox_decision_free. */
sandbox_decision_t sandbox_check(const sandbox_config_t *cfg, const char *proposed_action, sigma_decomposed_t sigma);

void sandbox_decision_free(sandbox_decision_t *d);

#endif /* CREATION_OS_V48_SANDBOX_H */
