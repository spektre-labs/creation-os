/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v55 EASD — Entropy-Aware Speculative Decoding guard
 * (Su, Zhang, He — arXiv:2512.23765, Dec 2025; ICLR 2026 under review).
 *
 * Insight: when BOTH draft and target exhibit high entropy AND their
 * top-N candidates substantially overlap, the draft proposal is
 * likely low-confidence and its errors can propagate through
 * standard rejection sampling.  EASD intercepts this case by
 * forcing a target-model re-sample regardless of the accept/reject
 * outcome of the underlying scheme.  Reported: up to +3.54 % accuracy
 * on reasoning benchmarks, throughput near standard spec-decode.
 *
 * Creation OS framing: EASD is a **quality gate** on top of EARS.
 * EARS loosens acceptance when uncertainty is asymmetric (only the
 * target is unsure).  EASD tightens it when uncertainty is
 * symmetric AND the two distributions agree on "who's plausible" —
 * exactly the regime where "random acceptance of a confident-looking
 * draft" would propagate a bad token.
 *
 * Scaffold tier C: operates on caller-supplied probability vectors.
 */
#ifndef CREATION_OS_V55_EASD_H
#define CREATION_OS_V55_EASD_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int   top_n;             /* number of top indices compared (e.g. 8) */
    float h_thresh;          /* normalized entropy threshold, [0,1] (e.g. 0.75) */
    float overlap_thresh;    /* top-N overlap fraction to trigger reject, [0,1] (e.g. 0.5) */
} v55_easd_params_t;

typedef struct {
    float h_target_norm;     /* H(target) / log2(N), in [0,1] */
    float h_draft_norm;
    float overlap_frac;      /* overlap / top_n, in [0,1] */
    int   reject;            /* 1 ⇒ force target resample, 0 ⇒ pass through */
} v55_easd_decision_t;

/* Take a decision from full probability vectors.
 * Scratch buffers not required; we allocate two small int arrays on
 * the stack (top_n ≤ 32 by contract). */
void v55_easd_decide(const v55_easd_params_t *p,
                     const float *probs_target,
                     const float *probs_draft,
                     int n,
                     v55_easd_decision_t *out);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V55_EASD_H */
