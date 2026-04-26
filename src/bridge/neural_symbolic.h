/*
 * Explicit neural → symbolic hand-off (audit-oriented).
 *
 * This module names the boundary where probabilistic model output meets
 * constitutional rules and τ-gates. It does not replace the full
 * σ-pipeline; it summarises a completed (text, σ) pair for logging and
 * optional dual-process UI.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_BRIDGE_NEURAL_SYMBOLIC_H
#define COS_BRIDGE_NEURAL_SYMBOLIC_H

#include "../sigma/pipeline/reinforce.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cos_bridge_result {
    float              sigma_combined;
    float              tau_accept;
    float              tau_rethink;
    cos_sigma_action_t gate;
    int                constitution_violations;
    int                constitution_mandatory_hits;
    char               violation_summary[256];
    /** SHA-256 over a canonical digest string (not a full proof receipt). */
    uint8_t            bridge_digest[32];
} cos_bridge_result_t;

/**
 * Summarise symbolic checks for one neural output.
 *
 * @param prompt   user prompt (may be NULL)
 * @param response model output (may be NULL)
 * @param sigma    combined σ already computed by the neural path
 * @param tau_accept / tau_rethink thresholds used for the gate line
 */
int cos_bridge_evaluate(const char *prompt, const char *response, float sigma,
                         float tau_accept, float tau_rethink,
                         cos_bridge_result_t *out);

int cos_neural_symbolic_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_BRIDGE_NEURAL_SYMBOLIC_H */
