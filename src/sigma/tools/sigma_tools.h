/*
 * σ-gated tool use (HORIZON-1).
 *
 * Classifies a shell-style tool invocation before execution: operation
 * family (READ / WRITE / DELETE / EXEC / NETWORK), target-path
 * sensitivity (/tmp vs /home vs /etc), and a scalar σ_tool ∈ [0,1].
 * The gate maps σ_tool to AUTO / CONFIRM / BLOCKED using caller-
 * supplied τ_low and τ_high.
 *
 * This is a static risk heuristic — not a substitute for OS-level
 * sandboxing (see HORIZON-5).  It exists so agents never fire blindly.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_TOOLS_H
#define COS_SIGMA_TOOLS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COS_TOOL_READ = 0,
    COS_TOOL_WRITE,
    COS_TOOL_DELETE,
    COS_TOOL_EXEC,
    COS_TOOL_NETWORK,
} cos_tool_risk_class_t;

typedef enum {
    COS_TOOL_DEC_AUTO = 0,
    COS_TOOL_DEC_CONFIRM,
    COS_TOOL_DEC_BLOCKED,
} cos_tool_decision_t;

typedef struct {
    char                    expanded_cmd[512];
    cos_tool_risk_class_t   risk_class;
    cos_tool_decision_t     decision;
    float                   sigma_tool;
    float                   tau_low;
    float                   tau_high;
    char                    risk_label[16];
    char                    decision_label[16];
    char                    block_reason[256];
} cos_tool_gate_result_t;

/* Defaults: τ_low=0.35 τ_high=0.65 — override with env
 * COS_TOOL_TAU_LOW / COS_TOOL_TAU_HIGH (parsed by gate). */
void cos_sigma_tool_thresholds_default(float *tau_low, float *tau_high);

/* Full pipeline: optional natural-language expansion → classify → σ
 * → decision.  Returns 0 on success, -1 on empty/invalid input.
 * `user_line` is the raw REPL line (may be NL or shell). */
int cos_sigma_tool_gate(const char *user_line,
                        float tau_low, float tau_high,
                        cos_tool_gate_result_t *out);

/* Classify an already-expanded shell command (no NL patterns). */
int cos_sigma_tool_classify(const char *shell_cmd,
                            float tau_low, float tau_high,
                            cos_tool_gate_result_t *out);

/* 20-case deterministic self-test; returns 0 on success. */
int cos_sigma_tools_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_TOOLS_H */
