/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v53 σ-prioritized context compression — scaffold.
 *
 * Claude Code's 4-layer compression is **temporal** (older messages go
 * first). v53's policy is **qualitative**: keep the moments where
 * learning happened (σ fell from high to low), keep messages that
 * reference invariants, drop routine filler.
 *
 * This file is the scoring surface only — it does not touch a real
 * message buffer or tokenizer. Callers supply metadata + a score
 * threshold, and we return a keep/drop decision per message.
 */
#ifndef CREATION_OS_V53_HARNESS_COMPRESS_H
#define CREATION_OS_V53_HARNESS_COMPRESS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int   index;
    float peak_sigma;         /* max σ seen while this message was live */
    int   tool_calls_made;
    int   sigma_resolved;     /* 1 if σ dropped materially after this msg */
    int   contains_invariant; /* 1 if msg references creation.md */
} v53_message_meta_t;

typedef struct {
    float score;    /* in [0, +∞); higher = keep */
    int   keep;     /* 1 = keep, 0 = drop */
} v53_compress_decision_t;

/* Score a single message. Weights are hard-coded so the policy is
 * auditable; tune via v53_compress_score_weighted if needed. */
float v53_compress_score(const v53_message_meta_t *m);

v53_compress_decision_t v53_compress_decide(const v53_message_meta_t *m,
                                            float drop_below);

/* Run the policy across n messages. Writes one decision per input.
 * `target_drop_fraction` ∈ [0,1]: approximate fraction of messages to
 * drop. Scaffold uses a simple percentile cutoff (no quickselect). */
void v53_compress_batch(const v53_message_meta_t *ms, int n,
                        float target_drop_fraction,
                        v53_compress_decision_t *out);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V53_HARNESS_COMPRESS_H */
