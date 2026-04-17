/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v92 — σ-Titans (neural long-term memory, test-time
 * learning, surprise-gated writes — Behrouz/Zhong/Mirrokni NeurIPS
 * 2025).
 *
 * ------------------------------------------------------------------
 *  What v92 is
 * ------------------------------------------------------------------
 *
 * v92 implements the "neural long-term memory" module from Titans as
 * a deterministic Q16.16 integer memory bank:
 *
 *      M = {(k_i, v_i)}, i = 0..63
 *
 * Each incoming (key, target) event:
 *
 *   1. retrieval: pick the slot with largest integer dot-product
 *      similarity sim_i = <k, k_i>; return v_i as the prediction ŷ
 *   2. surprise:  S = L1(target - ŷ)
 *   3. write gate:
 *        - if S > threshold_lo, the slot with smallest usage is
 *          overwritten with (k, v_new) where
 *             v_new = (1 - β) * target + β * v_i_best    (momentum)
 *        - else the retrieved slot's usage is incremented
 *   4. forget:  all slot usages are multiplied by γ ≈ 31/32 every
 *               FORGET_PERIOD events (adaptive decay).
 *
 * This is a faithful integer-only realization of the Titans
 * "surprise-driven write + adaptive forgetting" policy.  It gives the
 * rest of the stack a persistent, test-time-learnable, bounded-capacity
 * memory without any backprop on the hot path.
 *
 * ------------------------------------------------------------------
 *  Composed 32-bit branchless decision (extends v91)
 * ------------------------------------------------------------------
 *
 *     cos_v92_compose_decision(v91_composed_ok, v92_ok)
 *         = v91_composed_ok & v92_ok
 *
 * `v92_ok = 1` iff sentinel is intact, usage counters are bounded,
 * and every slot's value magnitude stays within the declared
 * value-clamp range.
 */

#ifndef COS_V92_TITANS_H
#define COS_V92_TITANS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V92_SENTINEL       0x9217AA5Eu
#define COS_V92_KEY_DIM        16u
#define COS_V92_VAL_DIM        8u
#define COS_V92_NUM_SLOTS      64u
#define COS_V92_FORGET_PERIOD  64u
#define COS_V92_USAGE_MAX      65535u
#define COS_V92_VAL_CLAMP      (8 << 16)       /* |v_i[d]| ≤ 8.0  */
#define COS_V92_MOMENTUM_RSHIFT 2u              /* β = 1/4 */
#define COS_V92_SURPRISE_LO    (1 << 11)       /* ≈ 0.031 Q16.16 */

typedef int32_t cos_v92_q16_t;

typedef struct {
    cos_v92_q16_t keys [COS_V92_NUM_SLOTS][COS_V92_KEY_DIM];
    cos_v92_q16_t vals [COS_V92_NUM_SLOTS][COS_V92_VAL_DIM];
    uint32_t      usage[COS_V92_NUM_SLOTS];

    uint32_t      events;
    uint32_t      writes;
    uint32_t      sentinel;
} cos_v92_mem_t;

void cos_v92_mem_init(cos_v92_mem_t *m, uint64_t seed);

/* Retrieve: pick slot with highest <k, k_i>; return its value in yhat
 * and the chosen slot index in *best. */
void cos_v92_retrieve(const cos_v92_mem_t *m,
                      const cos_v92_q16_t  key   [COS_V92_KEY_DIM],
                      cos_v92_q16_t        yhat  [COS_V92_VAL_DIM],
                      uint32_t            *best);

/* One test-time learning event. Returns the surprise S (Q16.16, L1). */
cos_v92_q16_t cos_v92_event(cos_v92_mem_t       *m,
                            const cos_v92_q16_t  key   [COS_V92_KEY_DIM],
                            const cos_v92_q16_t  target[COS_V92_VAL_DIM]);

/* Compress old slots by scaling usage by 31/32. */
void cos_v92_forget(cos_v92_mem_t *m);

uint32_t cos_v92_ok(const cos_v92_mem_t *m);
uint32_t cos_v92_compose_decision(uint32_t v91_composed_ok,
                                  uint32_t v92_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V92_TITANS_H */
