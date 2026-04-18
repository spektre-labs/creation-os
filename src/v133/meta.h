/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v133 σ-Meta — system-level metacognition.
 *
 * Creation OS aggregates σ across every layer in the stack.  v133
 * answers "how are we doing as a whole?" and raises the alarm when
 * something degrades.
 *
 * Four primitives, all pure C / deterministic:
 *
 *   1. Weekly snapshot history.  Per-snapshot: overall σ_product
 *      mean, per-channel σ means (8 channels for v126 parity),
 *      and interaction count.  History is appendable and bounded;
 *      when full it drops the oldest entry (ring behaviour).
 *
 *   2. Trend + regression detection.  OLS slope across snapshots.
 *      If abs(slope_per_week) > τ_reg on σ_product (or any
 *      individual channel) → COS_V133_REGRESSION_DETECTED.  This
 *      is the signal that lifts v124 continual-adapter rollback.
 *
 *   3. Auto-diagnose.  Given the latest snapshot, the highest
 *      per-channel σ is mapped to a concrete remediation hint
 *      (adapter-corrupted / KV-cache degenerate / memory-polluted
 *      / red-team failure / unknown).  Kept pure-policy here;
 *      the hook that runs remedies is v133.1.
 *
 *   4. Meta-σ.  "σ of σ itself."  stddev(σ_product) / mean(σ_product)
 *      over the history — a coefficient-of-variation interpretation.
 *      If meta-σ > τ_meta → the σ measurements themselves are
 *      drifting, i.e. calibration is breaking.  This is K(K)=K, the
 *      holographic principle applied in practice: σ must remain σ.
 *
 * Self-benchmark harness: weekly smoke-set runner.  Takes a caller-
 * provided answer_fn that produces a response + σ per question; the
 * harness computes accuracy and mean σ.  Deterministic and
 * framework-free (v133.0) — v133.1 wires it to v106 /v1/reason.
 */
#ifndef COS_V133_META_H
#define COS_V133_META_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V133_CHANNELS                   8
#define COS_V133_MAX_SNAPSHOTS              64
#define COS_V133_REGRESSION_TAU_DEFAULT   0.010f /* σ units / week */
#define COS_V133_META_TAU_DEFAULT         0.40f  /* cv of σ */

typedef struct cos_v133_snapshot {
    uint64_t ts_unix;
    float    avg_sigma_product;
    float    avg_sigma_per_channel[COS_V133_CHANNELS];
    int      n_interactions;
} cos_v133_snapshot_t;

typedef struct cos_v133_history {
    cos_v133_snapshot_t snapshots[COS_V133_MAX_SNAPSHOTS];
    int                 n;
} cos_v133_history_t;

typedef enum {
    COS_V133_OK                   = 0,
    COS_V133_REGRESSION_DETECTED  = 1,
    COS_V133_CALIBRATION_DRIFT    = 2,
} cos_v133_verdict_t;

typedef enum {
    COS_V133_CAUSE_NONE                = 0,
    COS_V133_CAUSE_ADAPTER_CORRUPTED   = 1,  /* ch 0 */
    COS_V133_CAUSE_KV_CACHE_DEGENERATE = 2,  /* ch 1 */
    COS_V133_CAUSE_MEMORY_POLLUTED     = 3,  /* ch 2 */
    COS_V133_CAUSE_TOOL_SIGMA_SPIKE    = 4,  /* ch 3 */
    COS_V133_CAUSE_PLAN_SIGMA_SPIKE    = 5,  /* ch 4 */
    COS_V133_CAUSE_VISION_SIGMA_SPIKE  = 6,  /* ch 5 */
    COS_V133_CAUSE_RED_TEAM_FAIL       = 7,  /* ch 6 */
    COS_V133_CAUSE_FORMAL_VIOLATION    = 8,  /* ch 7 */
    COS_V133_CAUSE_UNKNOWN             = 99,
} cos_v133_cause_t;

typedef struct cos_v133_health {
    cos_v133_snapshot_t latest;
    float               slope_per_week;    /* σ_product */
    cos_v133_verdict_t  verdict;
    cos_v133_cause_t    cause;
    float               meta_sigma;        /* cv of σ_product */
    int                 n_snapshots;
} cos_v133_health_t;

typedef struct cos_v133_bench_result {
    int   n_questions;
    int   n_correct;
    float accuracy;
    float mean_sigma_product;
} cos_v133_bench_result_t;

/* Caller-supplied benchmark adapter. */
typedef int (*cos_v133_answer_fn)(const char *question,
                                  char *out, size_t cap,
                                  float *out_sigma,
                                  void  *ctx);

/* --- History management ------------------------------------------ */

void cos_v133_history_init(cos_v133_history_t *h);

int  cos_v133_history_append(cos_v133_history_t *h,
                             const cos_v133_snapshot_t *s);

/* --- Trend + regression ------------------------------------------ */

/* OLS slope of σ_product across snapshots, expressed as σ per week
 * (604 800 seconds).  Returns 0 if fewer than 2 snapshots. */
float cos_v133_slope_per_week(const cos_v133_history_t *h);

float cos_v133_channel_slope_per_week(const cos_v133_history_t *h, int channel);

/* Regression verdict: positive slope on σ_product above τ is the
 * alarm.  A calibration drift (high meta-σ) promotes to
 * CALIBRATION_DRIFT. */
cos_v133_verdict_t cos_v133_regression_verdict(
    const cos_v133_history_t *h,
    float tau_regression,
    float tau_meta);

/* --- Auto-diagnose ----------------------------------------------- */

cos_v133_cause_t cos_v133_diagnose(const cos_v133_snapshot_t *latest,
                                   float spike_tau);

const char *cos_v133_cause_label(cos_v133_cause_t);

/* --- Meta-σ ------------------------------------------------------- */

/* Coefficient of variation of σ_product across history; returns 0
 * when history has fewer than 2 snapshots or mean is 0. */
float cos_v133_meta_sigma(const cos_v133_history_t *h);

/* --- Self-benchmark runner --------------------------------------- */

int   cos_v133_run_benchmark(cos_v133_answer_fn fn, void *ctx,
                             const char **questions,
                             const char **expected,
                             int n,
                             cos_v133_bench_result_t *out);

/* --- Composite health snapshot ----------------------------------- */

int   cos_v133_health_compose(const cos_v133_history_t *h,
                              float tau_reg, float tau_meta,
                              float spike_tau,
                              cos_v133_health_t *out);

int   cos_v133_health_to_json(const cos_v133_health_t *h,
                              char *out, size_t cap);

int   cos_v133_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
