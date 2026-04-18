/*
 * v192 σ-Emergent — superlinear behaviour detection.
 *
 *   Complex systems routinely produce behaviour their parts
 *   cannot explain on their own.  v192 quantifies "how
 *   superlinear is this combination" on a σ-grounded metric:
 *
 *       σ_emergent = 1 - (score_sum_of_parts / score_combined)
 *
 *   where score_sum_of_parts is the max per-part score used as
 *   a linear-upper-bound surrogate and score_combined is the
 *   measured joint score.  σ_emergent > 0 signals genuine
 *   superlinearity (emergence); σ_emergent ≤ 0 signals that the
 *   combination is at best a re-weighting of its parts.
 *
 *   Risk classification:
 *
 *       σ_emergent > τ_risk  AND  combined score drops on a
 *                                 safety metric  ⇒ risky
 *       σ_emergent > τ_risk  AND  combined score rises on a
 *                                 safety metric  ⇒ beneficial
 *
 *   Each superlinear event is appended to an emergence journal
 *   (`~/.creation-os/emergence.jsonl` at runtime; in v0 we
 *   return the journal in memory only and enforce that each
 *   entry is uniquely hashed).
 *
 *   Merge-gate contracts:
 *     * ≥ 2 superlinear pairs detected on the fixture
 *     * ≥ 1 risky pair classified as risky
 *     * ≥ 1 beneficial pair classified as beneficial
 *     * 0 false-positive pairs (linear combinations flagged)
 *     * journal hash chain replays byte-identically
 *
 * v192.0 (this file) uses a deterministic fixture of N=12
 * kernel pairs drawn from {v150, v135, v139, v146, v147, v138,
 * v140, v183, v144}.  For each pair we simulate part-scores
 * and a combined-score that is either strictly additive
 * (linear) or superlinear with controlled Δ.
 *
 * v192.1 (named, not shipped):
 *   - live v143 benchmark grid sweep over real kernel pairs;
 *   - v140 causal attribution to decompose risky emergence.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V192_EMERGENT_H
#define COS_V192_EMERGENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V192_N_PAIRS    12
#define COS_V192_STR_MAX    32

typedef enum {
    COS_V192_CLASS_LINEAR      = 0,
    COS_V192_CLASS_BENEFICIAL  = 1,
    COS_V192_CLASS_RISKY       = 2
} cos_v192_class_t;

typedef struct {
    int    id;
    char   kernel_a[COS_V192_STR_MAX];
    char   kernel_b[COS_V192_STR_MAX];

    /* Per-kernel scores on two metrics: quality and safety. */
    float  quality_a, quality_b;
    float  safety_a,  safety_b;

    /* Combined score when both run together. */
    float  quality_combined;
    float  safety_combined;

    /* Linear surrogate (upper bound assuming each part's best): */
    float  quality_sum_of_parts;   /* max(qa, qb) */
    float  safety_sum_of_parts;    /* max(sa, sb) */

    /* Metrics. */
    float  sigma_emergent_quality; /* 1 - sum/combined */
    float  sigma_emergent_safety;  /* 1 - sum/combined */
    int    class_;                 /* cos_v192_class_t */
    bool   superlinear_quality;
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v192_pair_t;

typedef struct {
    int               n_pairs;
    cos_v192_pair_t   pairs[COS_V192_N_PAIRS];

    float             tau_risk;           /* σ_emergent > this */

    /* Aggregates. */
    int               n_superlinear;
    int               n_beneficial;
    int               n_risky;
    int               n_linear_false_pos;   /* MUST be 0 */

    bool              journal_valid;
    uint64_t          journal_hash;

    uint64_t          seed;
} cos_v192_state_t;

void   cos_v192_init(cos_v192_state_t *s, uint64_t seed);
void   cos_v192_build(cos_v192_state_t *s);
void   cos_v192_run(cos_v192_state_t *s);
bool   cos_v192_verify_journal(const cos_v192_state_t *s);

size_t cos_v192_to_json(const cos_v192_state_t *s,
                         char *buf, size_t cap);

int    cos_v192_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V192_EMERGENT_H */
