/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Stream — per-token σ streaming inference.
 *
 * `cos chat` today emits the whole response at once.  In production
 * we stream: one token at a time, with the σ for that token
 * reported alongside, and a RETHINK event surfaced in-band when σ
 * crosses τ_rethink during generation.  σ-Stream is the substrate-
 * agnostic driver for that behaviour and has two jobs:
 *
 *   1. Drive a caller-supplied generator one token at a time.
 *      The generator returns (token, σ, done); σ-Stream pushes each
 *      tuple through the callback, maintains a running trace, and
 *      decides whether a rethink is needed.
 *
 *   2. Emit RETHINK events *in-band*.  When σ_token ≥ τ_rethink
 *      the module fires the callback with token = NULL, σ = σ_raw,
 *      is_rethinking = 1.  The caller's generator may then revise
 *      its internal state; σ-Stream simply continues pulling
 *      tokens after the marker.
 *
 * This module has no opinion about the underlying model.  It is
 * the contract that sits between `cos chat --stream` and whatever
 * decode path happens to be live (BitNet, Titans, a plain llama
 * server).  The contract includes:
 *
 *   - Deterministic replay: given the same generator output, the
 *     callback sequence is byte-identical.
 *   - Bounded trace: at most COS_STREAM_MAX_TOKENS tokens and
 *     COS_STREAM_MAX_TEXT bytes per stream; older tokens are
 *     dropped into a "truncated" flag, never silently forgotten.
 *   - No allocation in the hot path after the first token: the
 *     trace buffers grow geometrically and are reused across
 *     streams via cos_sigma_stream_reset.
 *
 * Downstream σ-pipeline uses the emitted per-token σ directly:
 *   - τ-calibrator aggregates σ_token over the stream
 *   - σ-engram records the top-σ token for retrieval targeting
 *   - CLI (`cos chat --stream`) renders [σ=...] next to each token
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_STREAM_H
#define COS_SIGMA_STREAM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef COS_STREAM_MAX_TOKENS
#define COS_STREAM_MAX_TOKENS 4096
#endif

#ifndef COS_STREAM_MAX_TEXT
#define COS_STREAM_MAX_TEXT   (64 * 1024)
#endif

#ifndef COS_STREAM_TAU_RETHINK_DEFAULT
#define COS_STREAM_TAU_RETHINK_DEFAULT 0.50f
#endif

enum cos_stream_status {
    COS_STREAM_OK          =  0,
    COS_STREAM_ERR_ARG     = -1,
    COS_STREAM_ERR_GEN     = -2,
    COS_STREAM_ERR_OOM     = -3,
};

/* Callback fired for every token *and* every rethink marker.
 *   token == NULL, is_rethinking == 1   → rethink event
 *   token != NULL, is_rethinking == 0   → normal token emit
 *   sigma is always the current σ reading (clip01'd) */
typedef void (*cos_sigma_stream_callback_t)(const char *token,
                                            float sigma,
                                            int is_rethinking,
                                            void *user);

/* Generator contract.  Called repeatedly by the driver.
 *   - On success: populate *out_token (must be NUL-terminated,
 *     aliased storage that stays valid until the next call),
 *     *out_sigma, and set *done=0.  Return 0.
 *   - When the stream is finished: set *done=1 and return 0.
 *     *out_token may be empty.
 *   - On error: return negative; the driver bubbles the code up. */
typedef int (*cos_sigma_stream_gen_t)(void *gen_state,
                                      const char **out_token,
                                      float *out_sigma,
                                      int   *done);

typedef struct {
    float    tau_rethink;        /* σ ≥ τ → rethink marker fired   */
    int      max_tokens;         /* 0 → COS_STREAM_MAX_TOKENS      */
    size_t   max_text;           /* 0 → COS_STREAM_MAX_TEXT        */
    int      suppress_zero_len;  /* drop empty tokens silently     */
} cos_sigma_stream_config_t;

typedef struct {
    int      n_tokens;           /* tokens actually emitted        */
    int      n_rethink;          /* rethink markers fired          */
    float    sigma_sum;
    float    sigma_sum_sq;       /* Welford-free M2 proxy          */
    float    sigma_max;
    float    sigma_mean;
    float    sigma_std;
    int      truncated_tokens;   /* hit COS_STREAM_MAX_TOKENS       */
    int      truncated_text;     /* hit COS_STREAM_MAX_TEXT         */
    int      gen_error;          /* 1 if generator returned < 0    */
    size_t   text_len;
    char    *text;               /* concatenated tokens, owned     */
    float   *sigma_per_token;    /* length n_tokens, owned         */
} cos_sigma_stream_trace_t;

/* -------- API ----------------------------------------------------- */

/* Initialise a blank trace; cheap, does not allocate. */
void cos_sigma_stream_trace_init(cos_sigma_stream_trace_t *tr);

/* Free the trace's buffers; `tr` reusable after reset. */
void cos_sigma_stream_trace_free(cos_sigma_stream_trace_t *tr);

/* Drive the generator, emit callbacks, and record the trace.
 * Every field of the config and every callback pointer may be
 * NULL; only the generator is mandatory. */
int cos_sigma_stream_run(const cos_sigma_stream_config_t *cfg,
                         cos_sigma_stream_gen_t           gen,
                         void                            *gen_state,
                         cos_sigma_stream_callback_t      cb,
                         void                            *user,
                         cos_sigma_stream_trace_t        *trace);

/* -------- Self-test ----------------------------------------------- */

int cos_sigma_stream_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_STREAM_H */
