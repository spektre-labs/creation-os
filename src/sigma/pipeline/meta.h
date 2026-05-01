/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Meta — domain-level competence awareness.
 *
 * "I know what I don't know" operationalised: tag every σ sample
 * with a domain string, bucket over N domains, expose mean σ and
 * drift-slope per bucket.  The runtime uses those buckets to
 * route curriculum effort (S2) and self-play topic selection (S1)
 * toward whichever domain has the worst σ_mean.
 *
 * Categorical trend: a rolling mean-σ is tracked per domain, and
 * a slope estimator (first half vs. second half of arrivals)
 * labels the bucket LEARNING (σ falling), STABLE (no change),
 * or DRIFTING (σ rising).  "DRIFTING on a strong domain" is the
 * earliest silent-degradation signal we can afford to emit.
 *
 * Contracts (v0):
 *   1. init(cap) allocates a caller-owned bucket array; cap ≥ 1.
 *   2. observe(domain, σ) upserts the bucket; NULL/empty domain
 *      is rejected (−1).  If cap is exhausted the function
 *      returns −2 and does NOT silently drop the observation.
 *   3. assess() writes competence flags (STRONG/MODERATE/WEAK/
 *      LIMITED) using the canonical thresholds 0.30 / 0.50 /
 *      0.70 over σ_mean; it also stamps `weakest_idx` and
 *      `strongest_idx` for σ-routing.
 *   4. recommend_focus() returns the domain with the worst
 *      σ_mean (or NULL if the store is empty).
 *   5. Self-test uses a deterministic stream: factual_qa strong
 *      (0.15, 0.20, 0.18), math weak (0.65, 0.60, 0.70), code
 *      moderate (0.35, 0.40, 0.30).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_META_H
#define COS_SIGMA_META_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_META_DOMAIN_CAP  32   /* max bytes of domain string    */

typedef enum {
    COS_META_STRONG   = 0,   /* σ_mean < 0.30                      */
    COS_META_MODERATE = 1,   /* 0.30 ≤ σ_mean < 0.50               */
    COS_META_WEAK     = 2,   /* 0.50 ≤ σ_mean < 0.70               */
    COS_META_LIMITED  = 3,   /* σ_mean ≥ 0.70                      */
} cos_meta_competence_t;

typedef enum {
    COS_META_TREND_STABLE   = 0,
    COS_META_TREND_LEARNING = 1,   /* σ falling                    */
    COS_META_TREND_DRIFTING = 2,   /* σ rising                     */
} cos_meta_trend_t;

typedef struct {
    char     domain[COS_META_DOMAIN_CAP];
    float    sigma_sum;           /* Σ σ                            */
    float    sigma_min;
    float    sigma_max;
    int      n_samples;
    float    sigma_sum_firsthalf; /* samples ⌊n/2⌋ counting from 1  */
    int      n_firsthalf;
    float    sigma_sum_secondhalf;
    int      n_secondhalf;
    float    sigma_mean;          /* cached on assess()             */
    float    slope;               /* 2nd-half mean − 1st-half mean  */
    cos_meta_competence_t competence;
    cos_meta_trend_t      trend;
} cos_meta_domain_t;

typedef struct {
    cos_meta_domain_t *slots;
    int                cap;
    int                count;
    int                total_samples;
    int                weakest_idx;   /* index of worst σ_mean     */
    int                strongest_idx; /* index of best  σ_mean     */
    float              overall_mean_sigma;
} cos_meta_store_t;

int  cos_sigma_meta_init(cos_meta_store_t *store,
                         cos_meta_domain_t *storage, int cap);

int  cos_sigma_meta_observe(cos_meta_store_t *store,
                            const char *domain, float sigma);

/* Recompute means, trends, competence flags, and weakest/strongest
 * indices.  Pure-read by the caller after this returns. */
void cos_sigma_meta_assess(cos_meta_store_t *store);

/* Return the domain bucket with the worst σ_mean, or NULL. */
const cos_meta_domain_t *cos_sigma_meta_recommend_focus(
    const cos_meta_store_t *store);

const char *cos_sigma_meta_competence_name(cos_meta_competence_t c);
const char *cos_sigma_meta_trend_name(cos_meta_trend_t t);

int  cos_sigma_meta_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_META_H */
