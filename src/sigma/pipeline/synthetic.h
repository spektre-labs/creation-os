/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Synthetic — quality-filtered synthetic training data with a
 * model-collapse guard.
 *
 * Shumailov et al. (2024) showed that "AI models collapse when
 * trained on recursively generated data".  The collapse signature
 * is obvious in hindsight: as the model trains on its own output,
 * its σ falls (false confidence) while the diversity of the
 * outputs falls faster (variance collapse) — you end up with a
 * trainer that looks happy and a trainee that only says one thing.
 *
 * σ-Synthetic runs rejection sampling on (q, a) pairs the caller
 * generates, accepts only pairs with σ_a < τ_quality, and tracks
 * the collapse signal explicitly:
 *
 *     σ_diversity = unique_prefixes / total_samples   ∈ [0, 1]
 *
 * If σ_diversity drops below `tau_diversity` the generator stamps
 * `collapsed = 1` and refuses to accept any more pairs until the
 * store is reset.  No silent degradation, no implicit trust.
 *
 * The sampling loop also carries an ANCHOR mix knob: the caller
 * supplies a bank of real pairs, and the accepted set enforces at
 * most `(1 − anchor_ratio)` fraction synthetic.  Default 80/20
 * follows the canonical anchor recipe (20% real).
 *
 * Contracts (v0):
 *   1. init rejects invalid thresholds / NULL generator.
 *   2. run() stops when `n_target` accepted OR collapse detected
 *      OR `max_attempts` hit; reports acceptance_rate + diversity.
 *   3. Every accepted pair has `sigma < tau_quality`.  If no pair
 *      passes, the store is empty and `accepted == 0`.
 *   4. Real-anchor enforcement keeps the accepted mix within 1% of
 *      the requested ratio (rounding-safe).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SYNTHETIC_H
#define COS_SIGMA_SYNTHETIC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *input;       /* q                                    */
    const char *output;      /* a                                    */
    float       sigma;       /* σ_a                                  */
    int         is_real;     /* 1 = anchor, 0 = generated            */
    int         accepted;    /* passed quality gate                  */
} cos_synth_pair_t;

/* Generator callback — produces ONE candidate pair per call.  The
 * caller owns the storage behind `input`/`output` (they must live
 * for the duration of the accepted set).  Returns 0 on success. */
typedef int (*cos_synth_generate_fn)(const char *domain, int seed,
                                     void *ctx,
                                     cos_synth_pair_t *out);

typedef struct {
    float tau_quality;       /* accept iff σ < this                  */
    float tau_diversity;     /* collapse iff diversity < this        */
    float anchor_ratio;      /* ≥ this fraction of accepted = real   */
    int   max_attempts;      /* safety cap on the generator loop     */
} cos_synth_config_t;

typedef struct {
    int     n_target;        /* requested accepts                    */
    int     attempts;
    int     accepted;
    int     n_real;
    int     n_synthetic;
    int     rejected_quality;
    int     rejected_anchor;
    float   diversity;       /* σ_diversity                          */
    float   acceptance_rate;
    int     collapsed;       /* stopped early due to collapse        */
    int     hit_attempt_cap; /* stopped early due to max_attempts    */
} cos_synth_report_t;

int  cos_sigma_synth_init(cos_synth_config_t *cfg,
                          float tau_quality,
                          float tau_diversity,
                          float anchor_ratio,
                          int max_attempts);

int  cos_sigma_synth_generate(const cos_synth_config_t *cfg,
                              const char *domain,
                              int n_target,
                              cos_synth_generate_fn generator,
                              void *generator_ctx,
                              cos_synth_pair_t *out_pairs, int cap,
                              cos_synth_report_t *out_report);

/* Compute σ_diversity over a flat pair array (unique 8-byte prefix
 * of `output` / total).  Exposed for callers that want to audit
 * diversity independently of the main loop. */
float cos_sigma_synth_diversity(const cos_synth_pair_t *pairs, int n);

int  cos_sigma_synth_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_SYNTHETIC_H */
