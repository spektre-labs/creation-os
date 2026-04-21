/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 *
 *  src/import/bitnet_server.h
 *  --------------------------------------------------------------------
 *  Real per-token σ for the σ-gated chat: spawn bitnet.cpp's
 *  `llama-server` (OpenAI-compatible HTTP, /completion + n_probs),
 *  POST the prompt, parse the returned `completion_probabilities`
 *  array and reduce it into a single (text, σ, token_count) tuple.
 *
 *  Contract:
 *    σ_token    = 1.0 - probs[0].prob   (probs is sorted desc by prob;
 *                                       index 0 is the sampled token)
 *    σ_response = max(σ_token) over all predicted tokens
 *                 ("max per-token uncertainty")
 *
 *  Lifecycle:
 *    1. cos_bitnet_server_ensure() forks + execs the server binary,
 *       polls /health, registers an atexit shutdown.
 *    2. cos_bitnet_server_complete() does one HTTP POST /completion
 *       per call.  Single-threaded, single-slot.
 *    3. cos_bitnet_server_shutdown() sends SIGTERM, waits, and
 *       reaps the child.  Called by atexit; idempotent.
 *
 *  Environment overrides (all optional):
 *    COS_BITNET_SERVER_EXE    path to llama-server
 *                             default: ./third_party/bitnet/build/bin/llama-server
 *    COS_BITNET_SERVER_MODEL  GGUF model path
 *                             default: ./models/BitNet-b1.58-2B-4T/ggml-model-i2_s.gguf
 *    COS_BITNET_SERVER_HOST   bind host         default: 127.0.0.1
 *    COS_BITNET_SERVER_PORT   bind port         default: 8088
 *    COS_BITNET_SERVER_CTX    --ctx-size        default: 2048
 *    COS_BITNET_SERVER_NPROBS --n-probs value   default: 5
 *    COS_BITNET_SERVER_NPRED  default n_predict default: 64
 *    COS_BITNET_SERVER_EXTERNAL  if "1": skip spawn, assume external
 *                                server already running on host:port
 */
#ifndef COS_BITNET_SERVER_H
#define COS_BITNET_SERVER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* Sampling knobs.  round lets the RETHINK loop rotate the seed
     * and/or bump temperature to break plateaus. */
    int     n_predict;    /* max tokens; 0 → use env default */
    int     seed;         /* < 0 → random; >= 0 → deterministic */
    float   temperature;  /* <= 0 → server default (0.8)      */
    int     n_probs;      /* 0 → use env default (5)          */
    const char *stop_word;/* NULL → no extra stop             */

    /* Optional system prompt (DEV-6: Codex).  When non-NULL, wired
     * into the /completion body as the top-level `system_prompt`
     * field.  llama-server prepends it with its own BOS/instruction
     * tokens. */
    const char *system_prompt;
} cos_bitnet_server_params_t;

typedef struct {
    /* NUL-terminated predicted text.  Owned by the server module;
     * valid until the next complete() call.  Caller must NOT free. */
    const char *text;

    /* σ_response = max(1.0 - selected_prob) over all predicted
     * tokens.  1.0 if the server returned no probs. */
    float       sigma;

    /* Token-level detail.  token_count is tokens_predicted from the
     * server; per_token_sigma is the max over the n_probs top array
     * at each step, written into a caller-owned ring buffer of
     * capacity COS_BITNET_SERVER_MAX_TOKENS (defined in .c). */
    int         token_count;
    float       mean_sigma;   /* mean per-token sigma */
    int         stopped_eos;  /* server stopped on EOS */
    int         stopped_limit;/* server hit n_predict */

    /* Local-call cost estimate (BitNet electricity only).  Forwarded
     * to the sovereign ledger. */
    double      cost_eur;

    /* Wall time of the /completion call. */
    double      elapsed_ms;
} cos_bitnet_server_result_t;

/* Explicitly opt into the server backend.  Idempotent.  Returns 0
 * on success, non-zero on fork/exec or health-check failure. */
int  cos_bitnet_server_ensure(void);

/* Send one /completion request.  `params` may be NULL for
 * per-env defaults.  Returns 0 on success (even if the text is
 * empty), non-zero on transport / JSON failure. */
int  cos_bitnet_server_complete(const char                     *prompt,
                                const cos_bitnet_server_params_t *params,
                                cos_bitnet_server_result_t       *out);

/* DEV-4: streaming completion.
 *
 * Opens a POST /completion with "stream": true + "n_probs" and reads
 * the SSE response line-by-line.  For each token chunk, invokes the
 * caller-supplied `token_cb` with:
 *
 *   - tok_text : one token's "content" string (NUL-terminated, valid
 *                until the cb returns).  Empty for the final chunk.
 *   - sigma    : 1.0 - max(top-k prob) for this token.  1.0 if the
 *                server omitted probs on this chunk.
 *   - is_last  : 0 for each streamed token, 1 on the terminal chunk
 *                (which carries stop=true + tokens_predicted summary).
 *
 * After streaming completes, `out` is filled with the usual summary:
 *   - text        : accumulated full response (server-module-owned buf)
 *   - sigma       : max over all per-token σ
 *   - token_count : count of streamed tokens
 *   - mean_sigma  : mean over all per-token σ
 *   - stopped_eos / stopped_limit / elapsed_ms / cost_eur
 *
 * The callback MAY return non-zero to abort the stream early; that is
 * reported as stopped_limit=1 in the summary, and the socket is
 * closed.  Returns 0 on success, non-zero on transport / parse error.
 *
 * Uses the native /completion endpoint (NOT /v1/chat/completions)
 * because the chat endpoint drops probabilities in streaming mode —
 * we need real per-token σ, so we pay the quality cost of skipping
 * the chat template.  For chat-quality streaming, enable Codex
 * (DEV-6) which prepends the system prompt manually. */
typedef int (*cos_bitnet_server_token_cb_t)(
    const char *tok_text,
    float       sigma,
    int         is_last,
    void       *ctx);

int  cos_bitnet_server_complete_stream(
    const char                         *prompt,
    const cos_bitnet_server_params_t   *params,
    cos_bitnet_server_token_cb_t        token_cb,
    void                               *cb_ctx,
    cos_bitnet_server_result_t         *out);

/* SIGTERM + reap.  Idempotent; safe to call from atexit. */
void cos_bitnet_server_shutdown(void);

/* Probe the /health endpoint on the currently-configured host:port.
 * Returns 1 on "ok", 0 otherwise.  Does NOT spawn the server. */
int  cos_bitnet_server_is_healthy(void);

/* Resolve the effective host/port/pid (for diagnostics / cos health).
 * pid is 0 if the server is external or not running. */
void cos_bitnet_server_diag(const char **out_host, int *out_port,
                            int *out_pid);

#ifdef __cplusplus
}
#endif

#endif /* COS_BITNET_SERVER_H */
