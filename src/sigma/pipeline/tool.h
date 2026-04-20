/*
 * σ-Tool — σ-gated tool calling with risk-class dispatch.
 *
 * Frontier agent stacks in 2026 (GPT-5.4, Claude Opus 4.6, GLM-4.7)
 * all converge on the same shape: a model proposes a tool call, a
 * runtime decides whether to execute it, and a policy decides how
 * much human approval is in the loop. Creation OS wires this to σ
 * and runs it locally:
 *
 *   sigma_select_tool(intent, tools[]) → tool_call_t{ tool, args, σ_sel }
 *   sigma_gate_tool(tool, σ_sel)        → ALLOW | CONFIRM | BLOCK
 *                                        (via the σ-Agent P15 gate)
 *   sigma_call_tool(call, executor)     → exec + σ_result
 *
 * Risk classes carry rising autonomy requirements:
 *
 *   SAFE         (calculator, pure compute)      → COS_ACT_READ
 *   READ         (file_read, list_dir)           → COS_ACT_READ
 *   WRITE        (file_write, db_update)         → COS_ACT_WRITE
 *   NETWORK      (web_fetch, api_call)           → COS_ACT_NETWORK
 *   IRREVERSIBLE (rm -rf, DROP TABLE, etc.)      → COS_ACT_IRREVERSIBLE
 *
 * The gate REUSES the σ-Agent primitive — this module adds the tool
 * registry and the selection / post-hoc σ measurement around it, it
 * does not re-implement the autonomy math.
 *
 * Post-hoc σ:
 *   Every tool executor returns σ_result ∈ [0, 1] — how noisy the
 *   tool's output looked.  A web page that 404'd has σ≈1.  A cached
 *   calculator answer has σ≈0.  The pipeline uses σ_result to decide
 *   whether to *trust* what the tool returned before splicing it into
 *   the response.
 *
 * Contracts (v0):
 *   1. init_registry rejects NULL or zero-capacity storage.
 *   2. register_tool copies by value; arguments are owned by the
 *      caller and stored as shallow pointers (tools are static config).
 *   3. select_tool returns a tool_call_t with tool == NULL on no-match.
 *   4. gate_tool on NULL call or unknown risk returns BLOCK.
 *   5. call_tool propagates the executor's return code and stamps
 *      σ_result into the call record.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_TOOL_H
#define COS_SIGMA_TOOL_H

#include <stddef.h>
#include <stdint.h>

#include "agent.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COS_TOOL_SAFE         = 0,  /* pure compute, no I/O    */
    COS_TOOL_READ         = 1,  /* read-only I/O           */
    COS_TOOL_WRITE        = 2,  /* writes to local state   */
    COS_TOOL_NETWORK      = 3,  /* talks to the network    */
    COS_TOOL_IRREVERSIBLE = 4,  /* delete / rm / drop      */
    COS_TOOL__N           = 5,
} cos_tool_risk_t;

typedef struct cos_tool_s {
    const char     *name;         /* "calculator", "file_read", ... */
    const char     *description;  /* natural-language hint          */
    cos_tool_risk_t risk;
    /* Optional canonical argument schema — free-form for now. */
    const char     *args_schema;
    /* Cost per call in €; used by the σ-plan budget tracker.      */
    double          cost_eur;
} cos_tool_t;

typedef struct {
    cos_tool_t *slots;
    int         cap;
    int         count;
} cos_tool_registry_t;

/* One invocation record. `tool` points into the registry; `arguments`
 * is caller-owned (the pipeline usually produces it from the model
 * output and frees it after the call returns). */
typedef struct {
    const cos_tool_t *tool;
    const char       *arguments;
    /* σ_selection — how confident the selector is.                 */
    float             sigma_select;
    /* σ_result — how noisy/untrusted the executor's output looked. */
    float             sigma_result;
    /* Executor return code (0 on success, <0 on error).            */
    int               rc;
    /* Gate decision applied at dispatch time.                      */
    cos_agent_decision_t decision;
    /* Realized cost for this call.                                 */
    double            cost_eur;
} cos_tool_call_t;

/* Executor signature: executes `call->tool` with `call->arguments`
 * and writes the result text (caller-owned, static-lifetime or
 * executor-managed) plus σ_result into the output fields.  Returns
 * 0 on success. */
typedef int (*cos_tool_executor_fn)(const cos_tool_call_t *call,
                                    void *ctx,
                                    const char **out_text,
                                    float *out_sigma_result);

int  cos_sigma_tool_registry_init(cos_tool_registry_t *reg,
                                  cos_tool_t *storage, int cap);

int  cos_sigma_tool_register(cos_tool_registry_t *reg,
                             const cos_tool_t *tool);

/* Selector: pick the best-matching tool for `intent`.  v0 policy is
 * "first name whose substring appears in intent"; the selector stores
 * its confidence in σ_select.  If no tool matches, returns a zero'd
 * call_t and sigma_select = 1.0. */
cos_tool_call_t
     cos_sigma_tool_select(const cos_tool_registry_t *reg,
                           const char *intent,
                           const char *arguments,
                           float      *out_sigma_select);

/* Map a tool risk to an σ-Agent action class. */
cos_action_class_t cos_sigma_tool_risk_to_action(cos_tool_risk_t r);

/* Gate: consult the σ-Agent autonomy rule.  Returns ALLOW, CONFIRM
 * or BLOCK.  IRREVERSIBLE always requires CONFIRM minimum (the
 * canonical "ask every time" rule), regardless of σ_select. */
cos_agent_decision_t
     cos_sigma_tool_gate(const cos_sigma_agent_t *agent,
                         const cos_tool_call_t   *call);

/* Execute: if the gate allows, run the executor and stamp σ_result
 * and rc onto the call.  Returns 0 on ALLOW+success, -1 on BLOCK,
 * -2 if CONFIRM is required but no confirmation was supplied
 * (caller passes confirmed=1 after prompting the user).  */
int  cos_sigma_tool_call(cos_tool_call_t *call,
                         const cos_sigma_agent_t *agent,
                         cos_tool_executor_fn executor,
                         void *executor_ctx,
                         int confirmed,
                         const char **out_text);

int  cos_sigma_tool_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_TOOL_H */
