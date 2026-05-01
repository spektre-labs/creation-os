/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Omega — recursive self-improvement loop.
 *
 * v306 wrote Ω = argmin ∫σ dt s.t. K ≥ K_crit as a thesis-level
 * statement.  σ-Omega is that equation as code: one function that
 * composes the S-series primitives into a single iterate step and
 * records σ-integrals, best-so-far snapshots, and K_eff safety.
 *
 * Each iteration does, in order:
 *
 *   1. Run a caller-supplied benchmark callback → σ_integral_t.
 *   2. If σ_integral improved, snapshot the config as `best`.
 *   3. (Optional) caller-driven: self-play batch + TTT + evolve
 *      + meta assessment — these are all exposed as individual
 *      hook callbacks so σ-Omega stays the conductor, not the
 *      soloist.
 *   4. Evaluate K_eff.  If K_eff < K_crit, ROLLBACK to best
 *      snapshot and stamp `rolled_back_at = i`.
 *
 * The ∫σ dt record is the running sum of per-step σ_integral
 * values; we also track a min (the best ever) and mean so callers
 * can see trend vs. one-shot performance.
 *
 * Deterministic smoke: a caller plugs in a benchmark callback
 * that models σ_integral decreasing monotonically with iteration
 * count (fake "learning"), and a K_eff callback that drops below
 * K_crit on iteration 5.  The loop must roll back, recover, and
 * finish at a σ_integral ≤ the best snapshot's value.
 *
 * Contracts (v0):
 *   1. init(n_iterations) rejects n < 1; allocates no memory.
 *   2. Every iterate(): best_σ is monotonically non-increasing.
 *   3. rolled_back_at is set exactly when K_eff crosses K_crit.
 *   4. run() writes a report with:
 *        iterations, min_sigma, mean_sigma, last_sigma,
 *        best_iter, n_rollbacks, k_eff_min.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_OMEGA_H
#define COS_SIGMA_OMEGA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque config pointer — σ-Omega never inspects its fields, only
 * passes it to the caller's callbacks.  Treat this as the genome
 * of the running system. */
typedef void cos_omega_config_t;

/* Step hooks.  All may be NULL if the caller chooses not to run
 * that stage in a given build.  Return 0 on success. */
typedef struct {
    /* Benchmark ∫σ dt on the current config. */
    float (*benchmark_sigma_integral)(cos_omega_config_t *cfg, void *ctx);

    /* K_eff safety metric; lower than K_crit → rollback. */
    float (*compute_k_eff)(cos_omega_config_t *cfg, void *ctx);

    /* Optional sub-loops (all permitted to mutate cfg). */
    int   (*selfplay_batch)(cos_omega_config_t *cfg, void *ctx, int n_rounds);
    int   (*ttt_batch)     (cos_omega_config_t *cfg, void *ctx);
    int   (*evolve_step)   (cos_omega_config_t *cfg, void *ctx);
    int   (*meta_assess)   (cos_omega_config_t *cfg, void *ctx,
                            char *out_focus, size_t cap_focus);

    /* Snapshot / restore hooks — σ-Omega calls these to save
     * best-so-far and restore on rollback.  save() writes cfg
     * into an opaque buffer; restore() writes buffer back into
     * cfg.  NULL hooks disable snapshot/rollback. */
    int   (*save_snapshot)   (const cos_omega_config_t *cfg, void *ctx,
                              void *out_buffer, size_t cap);
    int   (*restore_snapshot)(cos_omega_config_t *cfg, void *ctx,
                              const void *buffer, size_t n);
} cos_omega_hooks_t;

typedef struct {
    int    n_iterations;     /* max iterate steps                  */
    int    selfplay_rounds;  /* per-iteration self-play batch size */
    float  k_crit;           /* rollback threshold on K_eff        */
    float  improvement_eps;  /* σ-integral must drop by at least   */
} cos_omega_policy_t;

typedef struct {
    int    iteration;
    float  sigma_integral;
    float  k_eff;
    int    improved;
    int    rolled_back;
    char   focus[32];
} cos_omega_iter_log_t;

typedef struct {
    int    iterations;          /* total steps executed            */
    int    best_iter;
    float  best_sigma;          /* min σ_integral ever             */
    float  last_sigma;
    float  mean_sigma;
    float  k_eff_min;
    int    n_improvements;
    int    n_rollbacks;
    cos_omega_iter_log_t *trace; /* caller-owned, cap = n_iterations */
    int    trace_cap;
} cos_omega_report_t;

void cos_sigma_omega_policy_default(cos_omega_policy_t *p);

/* Run the Ω loop.  snapshot_buffer must have room for one save()
 * of cfg (cap bytes).  trace/cap may be NULL/0 for no trace. */
int  cos_sigma_omega_run(cos_omega_config_t *cfg,
                         const cos_omega_hooks_t *hooks,
                         void *hooks_ctx,
                         const cos_omega_policy_t *policy,
                         void *snapshot_buffer, size_t snapshot_cap,
                         cos_omega_iter_log_t *trace, int trace_cap,
                         cos_omega_report_t *out_report);

int  cos_sigma_omega_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_OMEGA_H */
