/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 *
 *  src/cli/escalation.h
 *  --------------------------------------------------------------------
 *  DEV-5: API escalation tier.
 *
 *  When the σ-gate ABSTAINs and the pipeline is not in LOCAL_ONLY
 *  mode, cos-chat forwards the prompt to a cloud teacher LLM and
 *  appends a (student → teacher) distill pair to disk.  The next
 *  time we retrain / quantize BitNet we mine this log to pick up
 *  the prompts where the local model fell off a cliff.
 *
 *  Providers (priority):
 *    COS_ESCALATION_BACKEND — optional explicit tier: deepseek | claude |
 *                openai | gpt (case-insensitive).  When unset,
 *                CREATION_OS_ESCALATION_PROVIDER is consulted for the
 *                same strings (legacy).  When both are unset but
 *                COS_DEEPSEEK_API_KEY or CREATION_OS_DEEPSEEK_API_KEY is
 *                set, DeepSeek V4 (deepseek-chat) is the default cloud
 *                teacher — cheapest OpenAI-compatible frontier tier.
 *
 *    claude    → POST https://api.anthropic.com/v1/messages
 *                CREATION_OS_CLAUDE_API_KEY (required)
 *                default model: claude-3-5-haiku-20241022 (cheap tier)
 *    openai    → POST https://api.openai.com/v1/chat/completions
 *                CREATION_OS_OPENAI_API_KEY (required)
 *                default model: gpt-4o-mini
 *    deepseek  → POST https://api.deepseek.com/v1/chat/completions
 *                (OpenAI-compatible; Bearer COS_DEEPSEEK_API_KEY or
 *                CREATION_OS_DEEPSEEK_API_KEY)
 *                default model: deepseek-chat (V4-class API surface)
 *
 *  Overrides:
 *    CREATION_OS_ESCALATION_MODEL  — override per-provider default
 *    CREATION_OS_DISTILL_LOG       — distill pair jsonl path
 *                                    (default: ~/.cos/distill_pairs.jsonl)
 *    CREATION_OS_ESCALATION_MAX_TOKENS — cap output (default: 1024)
 *
 *  σ on the teacher side:
 *    OpenAI  + logprobs=true → max(1 - exp(top_logprob)) per token
 *    Claude                  → flat 0.10 (API does not expose logprobs)
 *    DeepSeek                → σ recomputed locally on (prompt, answer)
 *                              after the HTTP response (text-shape
 *                              heuristic; no provider logprobs).
 *
 *  Distill pair (one line per escalation):
 *    {"ts":<int>,"prompt":"...","student":"...","student_sigma":<f>,
 *     "teacher":"...","teacher_sigma":<f>,"provider":"...",
 *     "model":"...","cost_eur":<f>,"bytes_sent":<i>,"bytes_recv":<i>}
 */
#ifndef COS_CLI_ESCALATION_H
#define COS_CLI_ESCALATION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Shared between cos_cli_chat_generate and cos_cli_escalate_api.
 * cos-chat is single-threaded so a process-wide singleton is safe;
 * every generate() call writes here, escalate() reads and logs.
 *
 * The singleton + setter live in stub_gen.c (not escalation.c) so
 * that binaries which only link stub_gen (cos-benchmark etc.) do
 * not need to pull in libcurl.  escalation.c references them via
 * `extern` below. */
typedef struct {
    char  student_text[8192];
    float student_sigma;
    int   student_valid;    /* 1 after at least one generate() */
} cos_escalation_ctx_t;

extern cos_escalation_ctx_t g_cos_escalation_ctx;

/* Record last student response for later distill logging.  Called
 * by cos_cli_chat_generate right before it returns. */
void cos_cli_escalation_record_student(const char *text, float sigma);

/* Plug-in escalate callback compatible with
 * cos_pipeline_escalator_fn.  Dispatches on
 * CREATION_OS_ESCALATION_PROVIDER; falls back to the deterministic
 * stub when no provider / key is configured (preserves CI parity).
 *
 * Returns 0 on success (including stub fallback), non-zero only on
 * unrecoverable provider error — in which case the pipeline treats
 * escalation as failed and keeps the ABSTAIN message. */
int cos_cli_escalate_api(const char *prompt, void *ctx,
                         const char **out_text, float *out_sigma,
                         double *out_cost_eur,
                         uint64_t *out_bytes_sent,
                         uint64_t *out_bytes_recv);

/* Diagnostic: resolved provider + model for the current env.
 * Writes into caller buffers; either may be NULL.  Returns 1 if
 * a real API provider is configured (keys present), 0 for stub. */
int cos_cli_escalation_diag(char *provider_out, size_t prov_cap,
                            char *model_out,    size_t model_cap);

/* Path to the distill pair log (resolves ~/.cos/distill_pairs.jsonl
 * or CREATION_OS_DISTILL_LOG).  Returns pointer to a static buffer;
 * never NULL.  Not thread-safe. */
const char *cos_cli_escalation_distill_path(void);

/* Last successful API-escalation route label for receipts, e.g.
 * "CLOUD(DS-V4)" | "CLOUD(Claude)" | "CLOUD(OpenAI)".  NULL when the
 * last escalation was stub-only or failed before a teacher answer. */
const char *cos_cli_escalation_route_receipt(void);

void cos_cli_escalation_route_receipt_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_CLI_ESCALATION_H */
