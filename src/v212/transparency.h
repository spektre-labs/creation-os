/*
 * v212 σ-Transparency — glass-box activity + intention
 * declarations + σ_opacity.
 *
 *   For every agent action the model declares what it
 *   *intends* to do before it does it; the realised
 *   action is then compared back to the declaration.
 *   A mismatch raises σ_transparency (per-event), and
 *   the aggregate σ_opacity reports how glass-boxed the
 *   system is overall.
 *
 *   v0 fixture: 30 real-time activity events
 *     - declared_op == realised_op             → match
 *     - declared_target == realised_target     → match
 *     - σ_transparency_event = 0.02                  when matched
 *     - σ_transparency_event ∈ [0.45, 0.75]          when mismatched
 *     - 3 events are engineered mismatches (10 %) so the
 *       kernel proves it catches them, not that they
 *       don't exist.
 *
 *   σ_opacity = mean(σ_transparency_event) — the lower
 *   the better.  Target: σ_opacity < 0.05 with zero
 *   mismatches; the fixture sits slightly above that
 *   by construction to validate the gate on realistic
 *   (non-perfect) traces.
 *
 *   Third-party audit API (v0): the output JSON is the
 *   `/v1/audit/stream` payload contract — every event
 *   carries (ts, op, declared, realised, σ, hash).
 *
 *   Contracts (v0):
 *     1. 30 events; ≥ 1 mismatch detected; every mismatch
 *        has σ_transparency_event ≥ τ_event (0.30).
 *     2. No matched event has σ_transparency_event > 0.05.
 *     3. σ_opacity ∈ [0, 1] and strictly less than 0.15
 *        (the fixture is mostly transparent by design).
 *     4. Every event carries a pre_ts (declare) and
 *        post_ts (realise) with `post_ts >= pre_ts`
 *        — declarations precede realisations.
 *     5. Public audit stream is byte-deterministic
 *        (FNV-1a chain).
 *
 *   v212.1 (named): live /v1/audit/stream SSE endpoint,
 *     v179 interpret per-decision explanation, ed25519
 *     signature per event.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V212_TRANSPARENCY_H
#define COS_V212_TRANSPARENCY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V212_N_EVENTS 30
#define COS_V212_STR_MAX  32

typedef struct {
    int      id;
    uint64_t pre_ts;                /* µs since epoch start */
    uint64_t post_ts;
    int      declared_op;           /* cos_v209_op_t */
    int      realised_op;
    char     declared_target[COS_V212_STR_MAX];
    char     realised_target[COS_V212_STR_MAX];
    bool     match;
    float    sigma_transparency;    /* per-event */
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v212_event_t;

typedef struct {
    int                 n;
    cos_v212_event_t    events[COS_V212_N_EVENTS];

    float               tau_event;    /* 0.30 */
    float               tau_opacity;  /* 0.15 */

    int                 n_matched;
    int                 n_mismatched;
    float               sigma_opacity;

    bool                chain_valid;
    uint64_t            terminal_hash;
    uint64_t            seed;
} cos_v212_state_t;

void   cos_v212_init(cos_v212_state_t *s, uint64_t seed);
void   cos_v212_build(cos_v212_state_t *s);
void   cos_v212_run(cos_v212_state_t *s);

size_t cos_v212_to_json(const cos_v212_state_t *s,
                         char *buf, size_t cap);

int    cos_v212_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V212_TRANSPARENCY_H */
