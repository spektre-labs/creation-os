/*
 * σ-watchdog — continuous quality monitoring.
 *
 * Feeds every production turn a (timestamp, σ, escalation_flag,
 * cost_eur) sample.  The watchdog keeps a ring buffer of recent
 * samples, derives 1-hour / 24-hour / 7-day rolling means, fits a
 * simple per-day linear trend via least squares, and emits an
 * alert + suggested remedy whenever σ drifts up or escalation rate
 * rises over the configured thresholds.
 *
 * The kernel is pure in-memory and deterministic: given a fixed
 * sample sequence, the same state + alerts come out on every host,
 * so CI can pin the behaviour.
 *
 * Auto-heal returns a structured cos_watchdog_remedy_t rather than
 * touching the runtime — the agent daemon wires the remedy into
 * `cos omega` or `cos lora train` calls and reports back.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_WATCHDOG_H
#define COS_SIGMA_WATCHDOG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_WATCHDOG_CAP              4096

#define COS_WATCHDOG_ONE_HOUR_S       3600u
#define COS_WATCHDOG_ONE_DAY_S        86400u
#define COS_WATCHDOG_ONE_WEEK_S      (7u * 86400u)

/* Defaults (caller may override). */
#define COS_WATCHDOG_TAU_SIGMA_UP       0.01f  /* σ/day                       */
#define COS_WATCHDOG_TAU_ESCAL_UP       0.005f /* escalation_rate /day        */

typedef enum {
    COS_WATCHDOG_HEALTHY   = 0,
    COS_WATCHDOG_DEGRADING = 1,
    COS_WATCHDOG_ALERT     = 2,
} cos_watchdog_health_t;

typedef struct {
    uint64_t timestamp_s;   /* seconds since epoch (monotonic up)     */
    float    sigma;         /* σ of that turn (0..1)                  */
    uint8_t  escalated;     /* 1 = the turn escalated to human / moe  */
    float    cost_eur;      /* optional — 0.0f if not tracked         */
} cos_watchdog_sample_t;

typedef struct {
    cos_watchdog_sample_t ring[COS_WATCHDOG_CAP];
    int      head;                 /* next write slot           */
    int      count;                /* # of live samples (≤ CAP) */

    float    tau_sigma_up;
    float    tau_escal_up;

    /* Derived after cos_watchdog_check. */
    float    sigma_mean_1h;
    float    sigma_mean_24h;
    float    sigma_mean_7d;
    float    sigma_trend_per_day;  /* LS slope over last 7d          */
    float    escal_rate_24h;
    float    escal_trend_per_day;
    float    cost_today_eur;

    cos_watchdog_health_t health;
    int      alert_sigma;
    int      alert_escal;
} cos_watchdog_t;

typedef enum {
    COS_WATCHDOG_REMEDY_NONE        = 0,
    COS_WATCHDOG_REMEDY_RUN_OMEGA   = 1,  /* cos omega --iterations N     */
    COS_WATCHDOG_REMEDY_TRAIN_LORA  = 2,  /* cos lora train               */
    COS_WATCHDOG_REMEDY_NOTIFY_HUMAN= 3,  /* both remedies exhausted      */
} cos_watchdog_remedy_t;

typedef struct {
    cos_watchdog_remedy_t remedy;
    int     iterations;          /* for RUN_OMEGA                  */
    char    reason[128];
} cos_watchdog_plan_t;

/* ==================================================================
 * API
 * ================================================================== */

void cos_watchdog_init(cos_watchdog_t *w);
void cos_watchdog_set_thresholds(cos_watchdog_t *w,
                                 float tau_sigma_up,
                                 float tau_escal_up);

/* Push one sample.  Samples must be in non-decreasing timestamp
 * order; older samples are overwritten when the ring is full. */
void cos_watchdog_observe(cos_watchdog_t *w,
                          uint64_t timestamp_s, float sigma,
                          int escalated, float cost_eur);

/* Refresh all derived fields against `now_s`.  `now_s` is taken as
 * the reference "now" so tests can replay synthetic histories. */
void cos_watchdog_check(cos_watchdog_t *w, uint64_t now_s);

/* Given the current (post-check) state, propose an auto-heal
 * action.  Returns REMEDY_NONE when the system is healthy. */
void cos_watchdog_plan(const cos_watchdog_t *w, cos_watchdog_plan_t *plan);

/* Human-readable label for dashboards. */
const char *cos_watchdog_health_str(cos_watchdog_health_t h);
const char *cos_watchdog_remedy_str(cos_watchdog_remedy_t r);

int cos_watchdog_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_WATCHDOG_H */
