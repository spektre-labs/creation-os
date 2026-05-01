/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v166 σ-Stream — real-time streaming inference with per-token σ.
 *
 * v106 σ-HTTP is request-response.  v166 turns it into a
 * continuous bidirectional stream.  The wire protocol emits a
 * framed event per generated token:
 *
 *     { "kind":"token",
 *       "seq":N,
 *       "token":"...",
 *       "sigma_product":0.xx,
 *       "channels":[σ0..σ7] }
 *
 * An **interrupt-on-sigma** rule: if sigma_product >
 * tau_interrupt *during* generation, the model stops itself
 * and emits:
 *
 *     { "kind":"interrupted",
 *       "reason":"sigma_burst",
 *       "seq":N,
 *       "sigma_product":0.xx }
 *
 * This is what makes the model *ethical under time pressure*:
 * it doesn't finish the sentence just because the user is
 * waiting — it stops as soon as its own uncertainty crosses
 * a declared budget.
 *
 * v166.0 (this file) ships:
 *   - a tokenizer that splits a prompt into up to N tokens by
 *     whitespace + punctuation (deterministic)
 *   - a per-token σ generator: 8 channels each driven by a
 *     SplitMix64 stream keyed by (seed, token_index, channel),
 *     with an *injected burst* at a caller-specified token
 *     index to test the interrupt path
 *   - sigma_product = geomean(channels) per token
 *   - a streaming loop that emits one framed event per token
 *     and stops on interrupt
 *   - a voice-pipeline model: each token has an `audible_delay_ms`
 *     that scales with sigma_product → high σ slows the voice
 *     (v127 extension hook)
 *
 * v166.1 (named, not shipped):
 *   - actual WebSocket server (ws:// or wss://)
 *   - real tokenizer from the GGUF vocabulary
 *   - integration into v127 voice pipeline (mic → Whisper →
 *     stream → TTS → speaker)
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V166_STREAM_H
#define COS_V166_STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V166_MAX_TOKENS    256
#define COS_V166_MAX_TOKEN     32
#define COS_V166_N_CHANNELS     8
#define COS_V166_MAX_MSG       160

typedef enum {
    COS_V166_EVENT_START        = 0,
    COS_V166_EVENT_TOKEN        = 1,
    COS_V166_EVENT_INTERRUPTED  = 2,
    COS_V166_EVENT_COMPLETE     = 3,
} cos_v166_event_kind_t;

typedef struct {
    cos_v166_event_kind_t kind;
    int      seq;
    char     token[COS_V166_MAX_TOKEN];
    float    sigma_product;
    float    channels[COS_V166_N_CHANNELS];
    int      audible_delay_ms;     /* v127 voice hint */
    bool     interrupted;
    char     reason[COS_V166_MAX_MSG];
} cos_v166_frame_t;

typedef struct {
    uint64_t seed;
    float    tau_interrupt;         /* abstain threshold during stream */
    int      n_tokens;
    cos_v166_frame_t frames[COS_V166_MAX_TOKENS];
    int      n_emitted;
    bool     was_interrupted;
    int      interrupt_seq;         /* token index of the interrupt */
    float    sigma_final;           /* σ at stop */
} cos_v166_stream_t;

/* Deterministic tokenization of `prompt` into `out->frames[i].token`.
 * Returns the number of tokens produced (≤ COS_V166_MAX_TOKENS). */
int  cos_v166_tokenize(const char *prompt,
                       cos_v166_stream_t *out);

/* Run the stream.  `inject_burst_seq` ≥ 0 forces a σ-burst at that
 * token (used to test interrupt-on-sigma deterministically);
 * set -1 to disable. */
void cos_v166_run(cos_v166_stream_t *s,
                  const char *prompt,
                  uint64_t seed,
                  float tau_interrupt,
                  int inject_burst_seq);

/* Serialize a single frame. */
size_t cos_v166_frame_to_json(const cos_v166_frame_t *f,
                              char *buf, size_t cap);

/* Serialize the entire stream as newline-delimited JSON (NDJSON). */
size_t cos_v166_stream_to_ndjson(const cos_v166_stream_t *s,
                                 char *buf, size_t cap);

int cos_v166_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V166_STREAM_H */
