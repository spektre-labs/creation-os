/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v98 — σ-Topology (persistent homology / Vietoris–Rips
 * filtration / Betti-0 + Betti-1 over an integer point cloud).
 *
 * ------------------------------------------------------------------
 *  What v98 is
 * ------------------------------------------------------------------
 *
 * A fully integer, branchless-on-the-hot-path implementation of a
 * small topological-data-analysis primitive set.  No floating point,
 * no malloc, no external dependencies — Q16.16 coordinates in, a
 * persistence barcode out.
 *
 *   1. Vietoris–Rips graph at scale ε:  edge (i,j) iff d²(p_i, p_j) ≤ ε²
 *   2. Betti-0(ε)  =  #connected components of VR_ε (union–find)
 *   3. Betti-1(ε)  =  #independent 1-cycles of VR_ε   (E − V + C)
 *   4. Persistence: for each point we record the radius at which its
 *      component merges with component 0.  Births and deaths align
 *      with the Vietoris–Rips barcode definition.
 *
 * Invariants checked at runtime:
 *   • d²(p,p) = 0, d²(p,q) = d²(q,p) (symmetric)
 *   • ε = 0   ⇒ Betti-0 = N (every point is its own component)
 *   • ε = ∞   ⇒ Betti-0 = 1 (everything connected)
 *   • Betti-0(ε)  monotonically non-increasing in ε
 *   • Betti-1(ε)  ≥ 0 everywhere
 *   • Betti-1(ε) = (#edges at scale ε) − N + Betti-0(ε)
 *   • sentinel intact
 *
 * ------------------------------------------------------------------
 *  Composed 38-bit branchless decision (extends v97)
 * ------------------------------------------------------------------
 *
 *     cos_v98_compose_decision(v97_composed_ok, v98_ok)
 *         = v97_composed_ok & v98_ok
 */

#ifndef COS_V98_TOPOLOGY_H
#define COS_V98_TOPOLOGY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V98_SENTINEL    0x98107007u
#define COS_V98_N           12u             /* #points */
#define COS_V98_D           3u              /* ambient dimension */
#define COS_V98_E_MAX       ((COS_V98_N * (COS_V98_N - 1u)) / 2u)   /* K_N */

typedef int32_t cos_v98_q16_t;

typedef struct {
    cos_v98_q16_t p   [COS_V98_N][COS_V98_D];   /* point cloud, Q16.16 */
    /* Pair distances squared, integer (sum of Q16.16 squared so Q32.32).
     * Stored in the upper-triangular row-major layout; [i][j] valid for
     * i < j and derived for i > j via symmetry. */
    int64_t       d2  [COS_V98_N][COS_V98_N];
    uint32_t      betti_0_last;
    uint32_t      betti_1_last;
    uint32_t      monotone_violations;
    uint32_t      sentinel;
} cos_v98_space_t;

void cos_v98_init(cos_v98_space_t *s);

/* Deterministic point-cloud generator: places points on a noisy
 * circle of radius 1.0 so the filtration has a known Betti-1 = 1 for
 * a range of ε values — a textbook TDA sanity check.                 */
void cos_v98_load_circle(cos_v98_space_t *s, uint64_t seed);

/* Recompute all pair distances squared after `p` has been written. */
void cos_v98_recompute_d2(cos_v98_space_t *s);

/* Betti-0 at scale ε (threshold given squared to stay integer). */
uint32_t cos_v98_betti0_at(const cos_v98_space_t *s, int64_t eps_sq);

/* Betti-1 at scale ε.  Uses E − V + C on the 1-skeleton of the
 * Vietoris–Rips complex.                                             */
uint32_t cos_v98_betti1_at(const cos_v98_space_t *s, int64_t eps_sq);

/* Number of edges present in the 1-skeleton at scale ε. */
uint32_t cos_v98_edges_at(const cos_v98_space_t *s, int64_t eps_sq);

/* Walks a monotone sweep of ε² thresholds and tallies betti0/betti1
 * monotonicity + cross-identity violations.                          */
void cos_v98_filtration_check(cos_v98_space_t *s,
                              const int64_t *eps_sq_schedule,
                              uint32_t schedule_len);

uint32_t cos_v98_ok(const cos_v98_space_t *s);
uint32_t cos_v98_compose_decision(uint32_t v97_composed_ok,
                                  uint32_t v98_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V98_TOPOLOGY_H */
