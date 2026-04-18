/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v131 σ-Temporal — time-aware reasoning.
 *
 * Creation OS treats every prompt independently until now.  v131
 * adds four σ-governed time primitives:
 *
 *   1. Session timeline.  Every interaction carries a Unix timestamp
 *      and a topic tag.  "What did we discuss last week?" → window
 *      retrieval.  "What changed since yesterday?" → diff by topic.
 *
 *   2. σ-trend per topic.  Linear regression of σ_product vs time
 *      gives the *learning signal*: negative slope = v124 continual
 *      is working, flat = stable, positive = regression alarm (→ v133
 *      meta-benchmark can lift rollback).
 *
 *   3. σ-weighted decay.  exp(-age/half_life · (1 + α·σ)) — high-σ
 *      old memories decay faster ("uncertain + old = probably
 *      obsolete"), confident old memories stick around.
 *
 *   4. Spike + deadline σ.  Detects sudden σ-jumps within a session
 *      (→ causal prompt for v10 causal-surgery to analyse which
 *      token caused the spike) and predicts σ under time pressure
 *      (v121 planning consumes this: tight deadline → higher
 *      expected σ → more replan headroom).
 *
 * v131.0 (this kernel) is a pure-C timeline + analytics layer.
 * The timeline is in-memory; v131.1 hooks v115 SQLite memory so
 * timelines persist across sessions.
 */
#ifndef COS_V131_TEMPORAL_H
#define COS_V131_TEMPORAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V131_MAX_TOPIC       64
#define COS_V131_MAX_SUMMARY    256
#define COS_V131_HALF_LIFE_DEF   (7 * 24 * 3600UL)  /* 7 days */
#define COS_V131_DECAY_ALPHA     1.00f

typedef struct cos_v131_event {
    uint64_t ts_unix;
    char     topic[COS_V131_MAX_TOPIC];
    float    sigma_product;              /* in [0,1] */
    char     summary[COS_V131_MAX_SUMMARY];
} cos_v131_event_t;

typedef struct cos_v131_timeline {
    cos_v131_event_t *events;
    int               n;
    int               cap;
} cos_v131_timeline_t;

int  cos_v131_timeline_init   (cos_v131_timeline_t *t, int cap);
void cos_v131_timeline_free   (cos_v131_timeline_t *t);
int  cos_v131_timeline_append (cos_v131_timeline_t *t,
                               const cos_v131_event_t *e);

/* Window recall — returns pointers to events e where
 *   since ≤ e->ts_unix ≤ until, optionally filtered by topic.
 * Writes up to `max` pointers to out.  Returns count written. */
int  cos_v131_recall_window(const cos_v131_timeline_t *t,
                            uint64_t since, uint64_t until,
                            const char *topic_or_null,
                            const cos_v131_event_t **out, int max);

/* σ-trend slope per topic by ordinary least squares on (t, σ).
 * Returns slope in units σ / second.  `*n_used_out` gets the
 * sample count used.  Returns 0 if fewer than 2 samples. */
float cos_v131_sigma_trend(const cos_v131_timeline_t *t, const char *topic,
                           uint64_t since, uint64_t until,
                           int *n_used_out);

/* σ-weighted decay weight w(age, σ) = exp(-age/half_life·(1+α·σ)).
 * age_seconds is clamped ≥ 0. */
float cos_v131_decay_weight(uint64_t age_seconds, float sigma_product,
                            uint64_t half_life_seconds, float alpha);

/* Spike detection: flags indices i where σ[i] - σ[i-1] ≥ threshold
 * (same topic, in order).  Returns count written. */
int   cos_v131_detect_spikes(const cos_v131_timeline_t *t,
                             const char *topic_or_null,
                             float threshold,
                             int *out_idx, int max);

/* Deadline σ prediction: models σ̂ = baseline + slope · time_used_frac
 * where time_used_frac ∈ [0,1] is proportion of deadline consumed. */
typedef struct cos_v131_deadline_sigma {
    float baseline_sigma;
    float pressure_slope;
} cos_v131_deadline_sigma_t;

float cos_v131_predict_sigma_under_pressure(
    const cos_v131_deadline_sigma_t *ds, float time_fraction_used);

int   cos_v131_trend_to_json(const char *topic, float slope,
                             int n_used, char *out, size_t cap);

int   cos_v131_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
