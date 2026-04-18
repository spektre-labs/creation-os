/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v136 σ-Evolve — gradient-free architecture search for the
 * σ-aggregator itself.
 *
 * v29 aggregates eight σ channels into a single σ_product.  v104
 * selected the geometric mean *manually*.  v136 asks: given a
 * fixed sidecar dataset of (channels, should_abstain) pairs,
 * which per-channel weight vector and τ threshold yield the best
 * AURCC-shaped balanced accuracy?
 *
 * The search is a deterministic (μ/μ, λ) Evolution Strategy with
 * diagonal covariance adaptation — a member of the CMA-ES family,
 * consciously simplified so the whole optimiser fits in ~250 lines
 * of pure C and runs in seconds on an M3.  No gradients, no GPU.
 *
 * Genotype (9 reals):
 *     w[0..7]   channel weights            in [0, 1]
 *     tau       abstain threshold          in [0, 1]
 *
 * Phenotype (decision rule):
 *     sigma_aggr(x) = weighted_geom_mean(x, w)
 *     verdict(x)    = sigma_aggr(x) > tau ? ABSTAIN : EMIT
 *
 * Fitness: balanced accuracy on the sidecar:
 *     fit = 0.5 * TPR + 0.5 * TNR      (higher is better)
 *
 * v136.0 ships the ES + synthetic sidecar generator.  v136.1 plugs
 * the ES into real archived σ-logs and writes the evolved config
 * to models/v136/evolved_sigma_config.toml for v29 to pick up at
 * runtime.
 */
#ifndef COS_V136_EVOLVE_H
#define COS_V136_EVOLVE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V136_CHANNELS    8
#define COS_V136_GENOME_DIM  (COS_V136_CHANNELS + 1)   /* + tau */
#define COS_V136_POP_MAX     64

typedef struct cos_v136_sample {
    float channels[COS_V136_CHANNELS];
    int   should_abstain;       /* 1 = high σ truth, 0 = emit       */
} cos_v136_sample_t;

typedef struct cos_v136_genome {
    float w[COS_V136_CHANNELS];
    float tau;
    float fitness;              /* filled by evaluate                */
} cos_v136_genome_t;

typedef struct cos_v136_cfg {
    int      pop_size;          /* λ (e.g. 20)                       */
    int      mu;                /* parents kept (e.g. 5)             */
    int      generations;       /* e.g. 50                           */
    float    sigma_mut;         /* mutation step size (0.10 default) */
    uint64_t seed;
} cos_v136_cfg_t;

void  cos_v136_cfg_defaults (cos_v136_cfg_t *cfg);

/* Phenotype: aggregate σ via weighted geometric mean. */
float cos_v136_aggregate    (const cos_v136_genome_t *g,
                             const float channels[COS_V136_CHANNELS]);

/* Fitness: balanced accuracy on a sidecar of N samples. */
float cos_v136_evaluate     (cos_v136_genome_t *g,
                             const cos_v136_sample_t *data, int n);

/* Deterministic synthetic sidecar.  N samples; half are low-σ
 * "should-emit" and half are high-σ "should-abstain", with
 * controlled noise across channels 5..7. */
void  cos_v136_synthetic    (cos_v136_sample_t *out, int n,
                             uint64_t seed);

/* Run the ES.  On success `out_best` receives the best genome
 * found; returns 0 on success, <0 on error. */
int   cos_v136_run          (const cos_v136_cfg_t *cfg,
                             const cos_v136_sample_t *data, int n,
                             cos_v136_genome_t *out_best,
                             float *trajectory /* nullable, [generations] */);

/* Write evolved config in TOML for v29 pickup. */
int   cos_v136_write_toml   (const cos_v136_genome_t *g,
                             const char *path);

int   cos_v136_self_test    (void);

#ifdef __cplusplus
}
#endif
#endif
