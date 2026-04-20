/*
 * σ-Live — feedback-driven τ adaptation (v278 recursive-self-improve).
 *
 * The τ_accept / τ_rethink thresholds used by ``sigma_reinforce``
 * are currently static (set at install time).  That's fine on day
 * one, but a user's domain drift is real: the σ that "90% of the
 * time means correct" today may mean "70% correct" tomorrow.
 *
 * This primitive learns τ from user feedback.  Every call is a
 * (σ, correct?) observation; we keep a sliding ring-buffer window
 * of the last N such observations and periodically recompute
 * τ_accept / τ_rethink from the empirical correctness-vs-σ curve.
 *
 * Algorithm:
 *   1. Sort the window by σ ascending.
 *   2. Walk cumulatively; track (correct_so_far / count_so_far).
 *   3. τ_accept  = largest σ at which cumulative accuracy ≥ target_accept
 *                  (default 0.95).
 *   4. τ_rethink = largest σ at which cumulative accuracy ≥ target_rethink
 *                  (default 0.50).  If τ_rethink < τ_accept → clamp equal.
 *   5. Clamp both to [tau_min, tau_max]; fall back to seed values if
 *      fewer than min_samples observations are in the window.
 *
 * Conformal-inspired but simpler than true conformal prediction —
 * we report the adapted τ + a confidence proxy (the accuracy at
 * the picked σ) so callers can decide to override if they want
 * stricter guarantees.
 *
 * Contracts (v0):
 *   1. Before min_samples → τ_accept / τ_rethink return the seed
 *      values unchanged.
 *   2. τ_accept ≤ τ_rethink always (monotone gate).
 *   3. τ_accept, τ_rethink ∈ [tau_min, tau_max] always.
 *   4. observe() writes into the ring buffer regardless of the
 *      adaptation state; adapt() may be called explicitly or is
 *      re-run on every observe() (cheap: O(N log N) with N ≤ 256).
 *   5. lifetime accuracy = correct_seen / total_seen (both ≥ 0);
 *      never overflows uint64.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_LIVE_H
#define COS_SIGMA_LIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t sigma_q;     /* σ · 65535 */
    uint8_t  correct;     /* 0 or 1     */
    uint8_t  _pad;
} cos_live_obs_t;

_Static_assert(sizeof(cos_live_obs_t) == 4,
               "cos_live_obs_t must be 4 bytes");

typedef struct {
    /* Caller-owned ring buffer. */
    cos_live_obs_t *buf;
    uint32_t capacity;
    uint32_t head;         /* next write slot */
    uint32_t count;        /* samples stored (<= capacity) */

    /* Lifetime counters (never reset). */
    uint64_t total_seen;
    uint64_t correct_seen;

    /* Adapted thresholds (read by callers). */
    float tau_accept;
    float tau_rethink;

    /* Configuration. */
    float    tau_min, tau_max;
    float    seed_tau_accept, seed_tau_rethink;
    float    target_accept;    /* default 0.95 */
    float    target_rethink;   /* default 0.50 */
    uint32_t min_samples;      /* gate on adaptation, default 16 */

    /* Observability: accuracy at the picked τ_accept (for logging). */
    float    accept_accuracy;
    float    rethink_accuracy;
    uint32_t n_adapts;
} cos_sigma_live_t;

/* Configure + zero the ring buffer. */
int cos_sigma_live_init(cos_sigma_live_t *l,
                        cos_live_obs_t *buf, uint32_t capacity,
                        float seed_tau_accept, float seed_tau_rethink,
                        float tau_min, float tau_max,
                        float target_accept, float target_rethink,
                        uint32_t min_samples);

/* Ingest one user-feedback observation and re-adapt. */
void cos_sigma_live_observe(cos_sigma_live_t *l,
                            float sigma, bool correct);

/* Force an adapt pass (called internally by observe). */
void cos_sigma_live_adapt(cos_sigma_live_t *l);

/* Lifetime accuracy (correct_seen / total_seen); returns 0 if no
 * observations have been ingested yet. */
float cos_sigma_live_lifetime_accuracy(const cos_sigma_live_t *l);

int cos_sigma_live_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_LIVE_H */
