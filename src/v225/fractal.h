/*
 * v225 σ-Fractal — self-similar, hierarchical σ with
 * scale-invariance and a K(K)=K holographic check.
 *
 *   σ is the same function at every scale:
 *       L0  tokens     (leaves)
 *       L1  sentences  (aggregate of tokens)
 *       L2  responses  (aggregate of sentences)
 *       L3  sessions   (aggregate of responses)
 *       L4  system     (aggregate of sessions)
 *
 *   The aggregator is the SAME function A at every
 *   level — v225 operationalises "fractal σ":
 *       σ_(L+1)[i] = A( { σ_L[j]  :  parent(j)=i } )
 *   with A := mean in v0 (any monotone, scale-invariant,
 *   [0,1]→[0,1] reducer is admissible; v225.1 can swap
 *   in a tensor contraction from v224 or an entropy
 *   reducer from v227).
 *
 *   Cross-scale coherence:
 *       A fraction of leaves are LOW σ but their parents
 *       are HIGH σ, and vice versa.  σ_cross_scale =
 *       fraction of parent/child pairs where
 *       |σ_parent − A_of_children| > δ_cross.
 *   A well-behaved fractal has σ_cross_scale = 0 by
 *   construction (the aggregator is *exactly* A).  v225
 *   also reports *cross-scale incoherence* = fraction of
 *   nodes where a child-mean disagreement with a
 *   reference σ_parent_ref (a separately supplied
 *   "declared" σ, injected in the fixture on purpose) is
 *   ≥ δ_cross.  That is where "every sentence is right
 *   but the response doesn't answer the question" shows
 *   up as a positive number.
 *
 *   K(K)=K — the holographic fixed point:
 *       K := 1 − σ_root.  Apply the aggregator to a
 *       constant-K vector of length == fan-out; the
 *       result is K itself (mean is idempotent on
 *       constants).  v225 enforces:
 *          |A({K, K, ..., K}) − K| ≤ ε_kk
 *       at every level.  This is the K(K)=K invariant
 *       as an *identity*, not a slogan.
 *
 *   Contracts (v0):
 *     1. 5 levels, fan-out 2 per level → 16 leaves, 31
 *        nodes total.
 *     2. Every σ ∈ [0, 1].
 *     3. σ_(L+1) is EXACTLY the mean of its children
 *        (to 1e-6) — scale-invariance test.
 *     4. K(K)=K invariant: |A({K}_n) − K| ≤ ε_kk at
 *        every node with ≥ 2 children.
 *     5. σ_cross_scale of TRUE-aggregate pairs is 0.
 *     6. σ_cross_scale of DECLARED-reference pairs is
 *        > 0 (the fixture plants ≥ 1 reference
 *        mismatch so the detector has something to
 *        find).
 *     7. FNV-1a chain over all nodes replays byte-
 *        identically.
 *
 *   v225.1 (named): pluggable aggregator A (v224 tensor
 *     contraction, v227 entropy, v193 K_eff),
 *     continuous-scale fractal (Haar / Haar-wavelet
 *     decomposition over live σ-traces), cross-scale
 *     causal attribution.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V225_FRACTAL_H
#define COS_V225_FRACTAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V225_N_LEVELS       5     /* L0..L4 */
#define COS_V225_FANOUT         2     /* binary tree */
#define COS_V225_N_NODES       31     /* Σ 2^L, L=0..4 */

typedef struct {
    int      id;               /* 0..30 in BFS order */
    int      level;            /* 0..4  (0 = leaves) */
    int      parent;           /* -1 for root         */
    int      n_children;
    int      children[COS_V225_FANOUT];

    float    sigma;            /* actual σ at this node */
    float    sigma_declared;   /* 'claimed' σ (fixture) */
    bool     is_reference_mismatch;

    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v225_node_t;

typedef struct {
    cos_v225_node_t  nodes[COS_V225_N_NODES];

    float            delta_cross;        /* 0.10 */
    float            eps_kk;             /* 1e-5 */

    int              n_scale_invariant_ok;
    int              n_cross_true_diff;
    int              n_cross_declared_diff;
    int              n_kk_ok;

    float            k_root;             /* 1 − σ_root */
    float            sigma_root;

    bool             chain_valid;
    uint64_t         terminal_hash;
    uint64_t         seed;
} cos_v225_state_t;

void   cos_v225_init(cos_v225_state_t *s, uint64_t seed);
void   cos_v225_build(cos_v225_state_t *s);
void   cos_v225_run(cos_v225_state_t *s);

size_t cos_v225_to_json(const cos_v225_state_t *s,
                         char *buf, size_t cap);

int    cos_v225_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V225_FRACTAL_H */
