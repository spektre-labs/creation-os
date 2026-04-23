/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * σ-gates for MCP tool/call requests (internal safety path).
 */
#ifndef COS_SIGMA_MCP_GATE_H
#define COS_SIGMA_MCP_GATE_H

#include "error_attribution.h"
#include "sigma_tools.h"

#include <stddef.h>

typedef struct {
    int                     risk_level; /* 0..4 same as COS_TOOL_* order */
    cos_tool_risk_class_t   risk_class;
    float                   sigma_request;
    float                   sigma_result;
    enum cos_error_source   attribution;
    char                    reject_reason[256];
    int                     gated; /* 1 if gate ran */
    int                     rejected;
} cos_sigma_mcp_gate_result_t;

float cos_sigma_mcp_content_sigma(const char *text);

void cos_sigma_mcp_tau_tool_bucket(float tau_out[5]);

float cos_sigma_mcp_tau_injection_default(void);

/* Inspect tool name + raw params JSON substring; fills gate result (rejected + reason). */
int cos_sigma_mcp_precheck_tool_call(const char *tool_name,
                                     const char *params_json_fragment,
                                     cos_sigma_mcp_gate_result_t *out);

void cos_sigma_mcp_attribution_label(enum cos_error_source src,
                                     char *buf, size_t cap);

/* Log injection attempt into episodic engram (SQLite; optional offline). */
int cos_sigma_mcp_store_injection_episode(const char *snippet,
                                          float injection_score);

int cos_sigma_mcp_gate_self_test(void);

#endif /* COS_SIGMA_MCP_GATE_H */
