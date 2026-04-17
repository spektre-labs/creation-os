/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v93 — σ-MoR (Mixture-of-Recursions, NeurIPS 2025,
 * Bae & Kim & al.: adaptive token-level recursion depth).
 *
 * ------------------------------------------------------------------
 *  What v93 is
 * ------------------------------------------------------------------
 *
 * MoR reuses one shared recursion layer R across D recursion steps
 * and adds a *per-token router* that decides, at each step, whether
 * that token should *exit* or *recurse further*.  The result is a
 * Pareto-improved compute/quality trade-off: hard tokens get more
 * thinking, easy tokens exit early.
 *
 * v93 is an integer-only (Q16.16), branchless-on-the-hot-path
 * implementation of exactly that:
 *
 *     for step r = 0 .. MAX_DEPTH-1:
 *         for each still-active token t:
 *             h_t <- R(h_t)                          # shared layer
 *             s_t <- <router_w, h_t>                 # router score
 *             if s_t > threshold[r]:
 *                 mark t exited at depth r
 *
 *     output: {h_t, exit_depth_t} for all t
 *
 * R is a 1-layer shared residual:
 *     R(h) = clamp(h + W·h + b, -CLAMP, +CLAMP)
 *
 * The shared recursion step enables parameter efficiency; the router
 * enables adaptive computation.  MoR therefore gives us an O(avg_depth
 * · T) compute bill instead of O(MAX_DEPTH · T).
 *
 * ------------------------------------------------------------------
 *  Composed 33-bit branchless decision (extends v92)
 * ------------------------------------------------------------------
 *
 *     cos_v93_compose_decision(v92_composed_ok, v93_ok)
 *         = v92_composed_ok & v93_ok
 *
 * `v93_ok = 1` iff sentinel is intact, every token's hidden state is
 * within the declared clamp, exit depths are monotone non-decreasing
 * w.r.t. step-order, and `avg_depth ≤ MAX_DEPTH`.
 */

#ifndef COS_V93_MOR_H
#define COS_V93_MOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V93_SENTINEL    0x93F40017u
#define COS_V93_MAX_TOKENS  32u
#define COS_V93_HDIM        16u
#define COS_V93_MAX_DEPTH   6u
#define COS_V93_HCLAMP      (4 << 16)      /* |h| ≤ 4.0 */

typedef int32_t cos_v93_q16_t;

typedef struct {
    /* Shared recursion layer R: h ← clamp(h + W·h + b). */
    cos_v93_q16_t W      [COS_V93_HDIM][COS_V93_HDIM];
    cos_v93_q16_t b      [COS_V93_HDIM];
    /* Router weight: scalar score s = <rw, h>. */
    cos_v93_q16_t rw     [COS_V93_HDIM];
    /* Router threshold per step (Q16.16). */
    cos_v93_q16_t thresh [COS_V93_MAX_DEPTH];
    /* Working state. */
    cos_v93_q16_t h      [COS_V93_MAX_TOKENS][COS_V93_HDIM];
    uint32_t      exit_d [COS_V93_MAX_TOKENS];
    uint32_t      active [COS_V93_MAX_TOKENS];        /* 1 = still recursing */
    uint32_t      n_tokens;
    uint32_t      steps_run;
    uint32_t      sentinel;
} cos_v93_mor_t;

void cos_v93_mor_init(cos_v93_mor_t *m, uint64_t seed, uint32_t n_tokens);

/* Load a token's initial hidden state. */
void cos_v93_mor_set_input(cos_v93_mor_t *m, uint32_t t,
                           const cos_v93_q16_t h0[COS_V93_HDIM]);

/* Run up to MAX_DEPTH recursion steps with adaptive routing. */
void cos_v93_mor_run(cos_v93_mor_t *m);

/* Average exit depth across active tokens, Q16.16. */
cos_v93_q16_t cos_v93_mor_avg_depth(const cos_v93_mor_t *m);

uint32_t cos_v93_ok(const cos_v93_mor_t *m);
uint32_t cos_v93_compose_decision(uint32_t v92_composed_ok,
                                  uint32_t v93_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V93_MOR_H */
