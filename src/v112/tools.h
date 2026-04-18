/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v112 σ-Agent — OpenAI-compatible function calling with σ-gated
 * tool selection.  Sits on top of the v101 σ-bridge (generation +
 * σ-channels), the v105 aggregator (σ_product default), and the
 * v106 HTTP surface (/v1/chat/completions).
 *
 * What v112 adds on top of v106:
 *
 *   - `tools`       array parsing (OpenAI shape: type=function,
 *                   function={name, description, parameters})
 *   - `tool_choice` policy parsing ("auto"|"none"|{...})
 *   - a σ-gated tool-selection driver that:
 *       1. prompts the model to emit either natural-language reply
 *          or a single JSON `tool_call` block
 *       2. parses the block out of the model output
 *       3. measures σ_product on the tokens that produced the
 *          tool_call (proxy: the σ-profile snapshot from v101
 *          after generation), aggregates via v105
 *       4. if σ_product > τ_tool  →  abstains with diagnostic:
 *            {abstained: true,
 *             reason:    "low_confidence_tool_selection",
 *             channel:   "n_effective" | "max_token" | ...,
 *             sigma:     0.73,
 *             tool_call_parsed: {name, arguments}}
 *       5. otherwise returns the normal OpenAI response with
 *          message.tool_calls = [{id, type, function:{name, args}}]
 *
 * The σ-gate is the novel part.  OpenAI Swarm / LangGraph / CrewAI
 * route by rule or by heuristic; Creation OS routes by measurement
 * and refuses to route when the measurement says "I'm not sure
 * which tool to call, on what arguments."
 *
 * This file is a self-contained C library: no HTTP deps, no JSON
 * library, no threads.  It composes the v101 bridge (passed in by
 * the caller) — the server.c dispatcher owns the socket.
 */
#ifndef COS_V112_TOOLS_H
#define COS_V112_TOOLS_H

#include <stddef.h>
#include <stdint.h>

#include "../v101/bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- limits ---------------------------------------------------- */

#define COS_V112_TOOLS_MAX         16
#define COS_V112_TOOL_NAME_MAX     64
#define COS_V112_TOOL_DESC_MAX     512
#define COS_V112_TOOL_PARAMS_MAX   4096
#define COS_V112_TOOL_ARGS_MAX     4096
#define COS_V112_TOOL_CALL_ID_MAX  48

/* --- request-side ---------------------------------------------- */

typedef struct cos_v112_tool_def {
    char name[COS_V112_TOOL_NAME_MAX];
    char description[COS_V112_TOOL_DESC_MAX];
    /* JSON-schema blob as-received.  We don't validate the schema
     * here; the σ-gate doesn't need to.  Stored verbatim so we can
     * echo it back in diagnostics / re-render for the prompt. */
    char parameters_json[COS_V112_TOOL_PARAMS_MAX];
} cos_v112_tool_def_t;

typedef enum cos_v112_tool_choice_mode {
    COS_V112_TC_AUTO = 0,
    COS_V112_TC_NONE,
    COS_V112_TC_FORCED    /* choice.name specifies the tool */
} cos_v112_tool_choice_mode_t;

typedef struct cos_v112_tool_choice {
    cos_v112_tool_choice_mode_t mode;
    char forced_name[COS_V112_TOOL_NAME_MAX];
} cos_v112_tool_choice_t;

/* Parse the OpenAI `tools` array out of a chat-completions request
 * body.  Writes up to `cap` entries into `out`, returns the number
 * written (0 if the key is absent or malformed), or -1 on overflow.
 *
 * The parser is deliberately forgiving: it walks the body for
 * top-level `"tools":[` and then scans `{"type":"function",
 * "function":{"name":"...","description":"...","parameters":{…}}}`
 * objects.  Parameters are extracted as the literal JSON object
 * contents (between matching braces), preserving escapes.
 */
int cos_v112_parse_tools(const char *body,
                         cos_v112_tool_def_t *out,
                         size_t cap);

/* Parse `tool_choice` — OpenAI accepts both the string form
 *   "auto"|"none"|"required"
 * and the object form
 *   {"type":"function","function":{"name":"weather"}}
 * (we treat "required" the same as "forced" with no name, which
 * becomes AUTO with a high tau_tool — i.e. "you MUST call a tool,
 * pick the most confident one").  Returns 0 on success; default
 * (on missing key) is AUTO. */
int cos_v112_parse_tool_choice(const char *body,
                               cos_v112_tool_choice_t *out);

/* --- σ-gate ----------------------------------------------------- */

/* Diagnostic: which σ-channel collapsed the most.  The index maps
 * into the v101 8-channel profile. */
typedef enum cos_v112_collapsed_channel {
    COS_V112_CHAN_NONE          = -1,
    COS_V112_CHAN_ENTROPY       = 0,
    COS_V112_CHAN_SIGMA_MEAN    = 1,
    COS_V112_CHAN_MAX_TOKEN     = 2,
    COS_V112_CHAN_PRODUCT       = 3,
    COS_V112_CHAN_TAIL_MASS     = 4,
    COS_V112_CHAN_N_EFFECTIVE   = 5,
    COS_V112_CHAN_MARGIN        = 6,
    COS_V112_CHAN_STABILITY     = 7
} cos_v112_collapsed_channel_t;

/* Outcome of running the σ-gated tool selection pipeline. */
typedef struct cos_v112_tool_call_report {
    /* Raw model output (for debugging / the assistant message). */
    char   raw_output[2048];

    /* Parsed tool call (when the model emitted a <tool_call> JSON). */
    int    tool_called;                        /* 1 if a tool_call was
                                                * parsed from raw_output */
    char   tool_name[COS_V112_TOOL_NAME_MAX];
    char   tool_arguments_json[COS_V112_TOOL_ARGS_MAX];
    char   tool_call_id[COS_V112_TOOL_CALL_ID_MAX];

    /* σ telemetry after the generation. */
    float  sigma_profile[8];
    float  sigma_mean;
    float  sigma_product;
    float  sigma_max_token;

    /* Gate decision. */
    int    abstained;                          /* 1 iff σ > τ_tool */
    float  tau_tool;                           /* threshold in force */
    cos_v112_collapsed_channel_t collapsed;    /* most-collapsed
                                                  channel (lowest
                                                  per-channel value) */
    char   abstain_reason[96];
} cos_v112_tool_call_report_t;

typedef struct cos_v112_opts {
    /* σ-product threshold for abstaining from a tool call.  Default
     * 0.65 when unset.  Lower = stricter. */
    float  tau_tool;
    /* Max tokens to generate for the tool-call slot. */
    int    max_tokens;
    /* If 1, always parse a tool_call even when σ is high — useful
     * for tests that want to see the parsed tool name regardless.
     * Default 0 (respect the gate). */
    int    always_parse;
} cos_v112_opts_t;

/* Defaults helper. */
void cos_v112_opts_defaults(cos_v112_opts_t *opts);

/* Extract a `<tool_call>{…}</tool_call>` block (or `<function_call>`
 * alias) from `text` and populate the parsed name + arguments into
 * `report`.  Returns 1 on parse, 0 on no-tool-call (plain text),
 * -1 on malformed JSON inside the tags.  This is a pure string
 * operation; the σ-gate runs after.
 *
 * Also accepts the raw JSON form {"tool_call":{...}} and
 * {"function":"name","arguments":"{...}"} for looser models.
 */
int cos_v112_parse_tool_call_text(const char *text,
                                  cos_v112_tool_call_report_t *report);

/* Compose the "system"-style tool manifest prompt that BitNet /
 * Llama / Qwen / Mistral can all digest.  Writes into `out`,
 * returns bytes written.  The manifest format:
 *
 *   You have access to the following tools.  If one applies, emit
 *   exactly one <tool_call>{"name":"...","arguments":{…}}</tool_call>
 *   block and nothing else.  Otherwise answer in natural language.
 *
 *   Tools:
 *   - weather(location: string): current conditions for a place
 *   - add(a: number, b: number): integer sum
 *   ...
 */
int cos_v112_render_tool_manifest(const cos_v112_tool_def_t *tools,
                                  size_t n_tools,
                                  const cos_v112_tool_choice_t *choice,
                                  char *out, size_t cap);

/* Main driver.  Given an already-opened v101 bridge, a user prompt,
 * the tool catalogue, and an options block — generate the tool-call
 * candidate, measure σ, decide on abstention, and populate `report`.
 *
 * The bridge parameter may be NULL in which case the function
 * returns a stub report with tool_called=0 and abstained=1 (used by
 * the v112 self-test + CI smoke check).
 *
 * Return value: 0 on success (including σ-abstention — that's still
 * a valid report), non-zero on fatal error.
 */
int cos_v112_run_tool_selection(cos_v101_bridge_t *bridge,
                                const char *user_prompt,
                                const cos_v112_tool_def_t *tools,
                                size_t n_tools,
                                const cos_v112_tool_choice_t *choice,
                                const cos_v112_opts_t *opts,
                                cos_v112_tool_call_report_t *report);

/* Serialize the tool-call report into an OpenAI-shaped
 * `choices[0].message` JSON fragment.  If `abstained`, emits:
 *
 *   {"role":"assistant","content":null,"tool_calls":[…parsed…],
 *    "cos_abstained":true,"cos_abstain_reason":"…","cos_sigma":0.73,
 *    "cos_collapsed_channel":"n_effective"}
 *
 * If not abstained and a tool_call was parsed, emits the standard
 *   {"role":"assistant","content":null,"tool_calls":[{id,type,function:{name,arguments}}]}
 *
 * If not abstained and no tool_call was parsed, emits plain:
 *   {"role":"assistant","content":"…raw_output…"}
 *
 * Caller ensures `out` is big enough (4 KiB is the budget
 * of v106 chat completions).  Returns bytes written, -1 on overflow.
 */
int cos_v112_report_to_message_json(const cos_v112_tool_call_report_t *r,
                                    char *out, size_t cap);

/* Pure-C smoke: runs ~20 string / JSON parsing checks that don't
 * require a loaded bridge.  Returns 0 on all-pass, non-zero on any
 * failure.  Called by `creation_os_v112_tools --self-test`. */
int cos_v112_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_V112_TOOLS_H */
