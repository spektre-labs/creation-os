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
 *                             (Qwen3 GGUF needs a recent upstream llama-server;
 *                              BitNet fork cannot load architecture qwen3.)
 *    COS_BITNET_SERVER_MODEL  GGUF path — if set, overrides all routing below
 *    COS_MODEL_PATH           preferred medium (9B) weights:
 *                             default ./models/qwen3.5-9b-Q4_K_M.gguf
 *    COS_MODEL_SMALL_PATH     when COS_MODEL_SIZE=small:
 *                             default ./models/qwen3.5-2b-Q4_K_M.gguf
 *    COS_MODEL_SIZE           "small" (2B, fast) or "medium" (9B, default)
 *                             Used only when COS_BITNET_SERVER_MODEL is unset.
 *                             If the Qwen3.5 file is missing, falls back to
 *                             ./models/qwen3-8b-Q4_K_M.gguf when present.
 *    COS_BITNET_SERVER_HOST   bind host         default: 127.0.0.1
 *    COS_BITNET_SERVER_PORT   bind port         default: 8088
 *    COS_BITNET_CHAT_CTX      preferred --ctx-size hint (default 2048 if unset)
 *    COS_BITNET_SERVER_CTX    fallback --ctx-size if COS_BITNET_CHAT_CTX unset
 *    COS_BITNET_SERVER_NPROBS --n-probs value   default: 5
 *    COS_BITNET_SERVER_NPRED  default n_predict default: 64
 *    COS_BITNET_SERVER_EXTERNAL  if "1": skip spawn, assume external
 *                                server already running on host:port
 *    COS_BITNET_CHAT_MODEL    JSON "model" id for /v1/chat/completions
 *                                (default: "bitnet"); when
 *                                COS_INFERENCE_BACKEND=ollama, also the
 *                                default for COS_OLLAMA_MODEL if unset.
 *    COS_BITNET_IO_TIMEOUT_S  recv/send timeout seconds for inference HTTP
 *                                (default 60; clamped 30..600).
 *    COS_BITNET_CHAT_CACHE_PROMPT  set to "1" to append `"cache_prompt":true`
 *                                to /v1/chat/completions and streaming
 *                                /completion bodies (off by default so
 *                                Ollama and other peers are not tripped).
 *
 *    COS_INFERENCE_BACKEND    "llama-server" (default) or "ollama" — when
 *                             ollama: POST http://host:port/api/chat (local
 *                             Metal/MLX via Ollama on Apple Silicon).
 *                             Requires a running `ollama serve`; this module
 *                             does not spawn Ollama.
 *    COS_OLLAMA_HOST          bind host when backend is ollama (optional)
 *    COS_OLLAMA_PORT          TCP port when backend is ollama (default 11434)
 *    COS_OLLAMA_MODEL         chat model id (default "qwen3:8b")
 *    COS_OLLAMA_DEFAULT_SIGMA fallback σ when logprobs are unavailable
 *                             (default "0.5", neutral unknown)
 *    COS_OLLAMA_ENABLE_THINKING  set to "1" to pass enable_thinking:true
 *                             in /api/chat options (default off for Qwen3).
 *    COS_OLLAMA_APPEND_NO_THINK  "0" disables appending " /no_think" to the
 *                             user message on /api/chat (default: append for
 *                             Qwen3 unless set to "0").
 *    COS_BITNET_SIGMA_ADAPTIVE  set to "1" for extra HTTP round-trips when
 *                             per-token logprobs are missing: a verbal
 *                             0–100 confidence follow-up, optionally blended
 *                             with a two-temperature consistency probe.
 *    COS_BITNET_SIGMA_CONSISTENCY  set to "1" with adaptive: same prompt at
 *                             temp 0.3 vs 0.9, σ_consistency = 1 − Jaccard
 *                             token overlap (two extra inferences).
 *    COS_BITNET_SIGMA_FULL_BLEND  set to "1" with adaptive: when logprobs
 *                             exist, combine 0.6·σ_logprob + 0.2·verbal +
 *                             0.2·consistency (consistency still optional).
 *    COS_TEMPERATURE          when set, default sampling for
 *                             /v1/chat/completions if params->temperature≤0;
 *                             Ollama /api/chat uses 0.7 when unset.
 *    COS_TOP_P / COS_TOP_K    optional OpenAI-compat top_p / top_k; Ollama
 *                             /api/chat options default top_p=0.8, top_k=20.
 *
 *    COS_LLAMA_CTX            fourth fallback for --ctx-size after
 *                             COS_BITNET_CHAT_CTX / COS_BITNET_SERVER_CTX
 *    COS_LLAMA_BATCH          llama-server -b / -ub when spawned (512)
 *    COS_LLAMA_THREADS        llama-server -t when spawned (4)
 *    COS_LLAMA_NO_FLASH_ATTN  set "1" to omit --flash-attn on spawn
 *    COS_LLAMA_NO_MLOCK       set "1" to omit --mlock on spawn
 *
 *  System prompt file: see σ pipeline codex loader — COS_CODEX_PATH overrides
 *  the default path to the Atlantean Codex text (same variable as chat).
 *
 *  cos think decomposition (optional second server — small/fast model):
 *    COS_THINK_SMALL_PORT    e.g. 8089 when llama-server loads qwen3.5-2b;
 *                            only affects `cos think` goal breakdown, not subtasks.
 */
#ifndef COS_BITNET_SERVER_H
#define COS_BITNET_SERVER_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

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

/** Last /completion parse: per-token σ (1−p(selected)).  Cleared each
 *  parse; copy before the next `cos_bitnet_server_complete` call.
 *  Returns count written (≤ cap). */
int cos_bitnet_server_copy_last_token_sigmas(float *dst, int cap,
                                             int *n_out);

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

/* TTFT wall clock from the stream path: first non-empty streamed token.
 * Undefined until after a streaming complete() finishes; negative when
 * not streamed or unavailable. */
double cos_bitnet_server_last_ttft_ms(void);
void   cos_bitnet_server_clear_ttft(void);

/** Stream completion to FILE* (stdout/stderr); fills full_response for
 *  downstream σ work.  Returns same codes as cos_bitnet_server_complete_stream.
 *  max_len includes the terminating NUL for full_response. */
int cos_bitnet_stream_response(const char *prompt, const char *system_prompt,
                               FILE *output, char *full_response, int max_len,
                               float *ttft_ms);

/* SIGTERM + reap.  Idempotent; safe to call from atexit. */
void cos_bitnet_server_shutdown(void);

/* Drop cached env snapshot so the next ensure/complete reloads COS_* .
 * Used when switching COS_BITNET_SERVER_PORT for multi-server setups. */
void cos_bitnet_server_invalidate_config(void);

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
