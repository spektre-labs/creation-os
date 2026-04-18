/*
 * v204 σ-Hypothesis — automated hypothesis generation,
 * literature grounding, novelty scoring, impact-ranked
 * queue.
 *
 *   Input: a single observation (topic id + severity).
 *   v0 pipeline:
 *     1. Generate N = 10 candidate hypotheses via a
 *        deterministic closed-form expansion of the
 *        observation.
 *     2. σ_hypothesis per candidate — how confident we are
 *        in the raw generation.
 *     3. σ_grounding per candidate — does it contradict
 *        known corpus facts?  v0 models this with a small
 *        in-source "literature" table: 5 canonical facts,
 *        and a deterministic compatibility score.
 *     4. σ_novelty per candidate — embedding-distance from
 *        the nearest known hypothesis.  v0 uses a 16-dim
 *        deterministic hash embedding and L2 distance.
 *     5. impact = σ_novelty × (1 − σ_hypothesis), bounded
 *        by grounding: if a hypothesis is logically
 *        inconsistent (σ_grounding > τ_ground) its impact
 *        is hard-zeroed.
 *     6. Rank by impact; top-3 go to the TEST_QUEUE, the
 *        rest go to MEMORY (v115).
 *
 *   Contracts (v0):
 *     1. N = 10 hypotheses generated.
 *     2. Every (σ_hypothesis, σ_grounding, σ_novelty) ∈
 *        [0, 1]; impact ∈ [0, 1].
 *     3. Exactly 3 hypotheses promoted to TEST_QUEUE.
 *     4. The top-3 impact scores are strictly higher than
 *        the remaining 7.
 *     5. At least one hypothesis is flagged as
 *        SPECULATIVE (σ_hypothesis > τ_spec) and not
 *        silently pruned — σ-honesty.
 *     6. At least one hypothesis is hard-zeroed by
 *        grounding.
 *     7. FNV-1a hash chain over the (hypothesis, σ,
 *        impact, queue) tuples replays byte-identically.
 *
 * v204.1 (named): live v111.2 reason generator, v152
 *   corpus grounding, v169 ontology, v135 Prolog logical
 *   consistency, v126 σ-embed real embeddings.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V204_HYPOTHESIS_H
#define COS_V204_HYPOTHESIS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V204_N_HYPOTHESES    10
#define COS_V204_TEST_QUEUE_SIZE  3
#define COS_V204_EMBED_DIM       16
#define COS_V204_N_KNOWN         5
#define COS_V204_STR_MAX         48

typedef enum {
    COS_V204_STATUS_CONFIDENT   = 0,
    COS_V204_STATUS_SPECULATIVE = 1,
    COS_V204_STATUS_REJECTED    = 2
} cos_v204_status_t;

typedef enum {
    COS_V204_DEST_TEST_QUEUE = 0,
    COS_V204_DEST_MEMORY     = 1
} cos_v204_dest_t;

typedef struct {
    int      id;
    char     statement[COS_V204_STR_MAX];
    float    embedding[COS_V204_EMBED_DIM];
    float    sigma_hypothesis;   /* raw generator confidence */
    float    sigma_grounding;    /* conflict with corpus */
    float    sigma_novelty;      /* distance to nearest known */
    float    impact;             /* novelty · (1 - sigma_h), zeroed if grounded-out */
    int      status;
    int      dest;
    int      rank;               /* 1 = highest impact */
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v204_hypothesis_t;

typedef struct {
    int                      observation_topic;
    float                    observation_severity;

    int                      n;
    cos_v204_hypothesis_t    hypotheses[COS_V204_N_HYPOTHESES];

    float                    tau_spec;            /* 0.60 */
    float                    tau_ground;          /* 0.55 */

    int                      n_speculative;
    int                      n_rejected;
    int                      n_test_queue;
    int                      n_to_memory;

    bool                     chain_valid;
    uint64_t                 terminal_hash;
    uint64_t                 seed;
} cos_v204_state_t;

void   cos_v204_init(cos_v204_state_t *s, uint64_t seed);
void   cos_v204_build(cos_v204_state_t *s);
void   cos_v204_run(cos_v204_state_t *s);

size_t cos_v204_to_json(const cos_v204_state_t *s,
                         char *buf, size_t cap);

int    cos_v204_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V204_HYPOTHESIS_H */
