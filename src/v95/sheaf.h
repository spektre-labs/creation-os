/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v95 — σ-Sheaf (cellular-sheaf neural network with
 * sheaf-Laplacian diffusion — "Local-to-Global" / Copresheaf-TNN
 * 2026).
 *
 * ------------------------------------------------------------------
 *  What v95 is
 * ------------------------------------------------------------------
 *
 * A cellular sheaf F over a graph G = (V, E) attaches to each vertex
 * v and each edge e a vector-space stalk F(v), F(e), together with
 * *restriction maps* F_{v ◁ e} : F(v) → F(e).
 *
 * v95 realizes this on a ring graph with N=8 vertices and D=4 stalks
 * per vertex/edge (Q16.16 integer throughout).  The sheaf Laplacian
 *
 *     (Δ_F x)_v = Σ_{e = uv} F_{v ◁ e}^⊤ ( F_{v ◁ e} x_v − F_{u ◁ e} x_u )
 *
 * drives the diffusion
 *
 *     x_{t+1} = x_t − λ · Δ_F x_t            (λ encoded as rshift)
 *
 * whose fixed point is the sheaf-harmonic extension H^0(G, F).
 *
 * Seven primitives:
 *
 *   1. cos_v95_sheaf_init        ring graph + per-edge restriction maps
 *   2. cos_v95_set_signal        load x ∈ (R^D)^N
 *   3. cos_v95_laplacian_apply   y ← Δ_F x
 *   4. cos_v95_diffuse_step      one heat-equation step
 *   5. cos_v95_energy            Σ_v ||Δ_F x||_1 (monotone-non-increasing)
 *   6. cos_v95_harmonic_residual energy at t=T vs t=0  → ratio < 1
 *   7. cos_v95_ok / compose      34→35-bit verdict
 *
 * When restriction maps are the identity map, Δ_F collapses to the
 * classical graph Laplacian and diffusion reproduces the usual heat
 * equation.  This lets us cross-check correctness against a textbook
 * reference.
 *
 * ------------------------------------------------------------------
 *  Composed 35-bit branchless decision (extends v94)
 * ------------------------------------------------------------------
 *
 *     cos_v95_compose_decision(v94_composed_ok, v95_ok)
 *         = v94_composed_ok & v95_ok
 *
 * `v95_ok = 1` iff sentinel is intact, the restriction maps are
 * unitary (±1 per entry), and the diffusion energy is non-increasing
 * across steps.
 */

#ifndef COS_V95_SHEAF_H
#define COS_V95_SHEAF_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V95_SENTINEL    0x95C0FAA5u
#define COS_V95_NODES       8u      /* ring graph |V| */
#define COS_V95_EDGES       8u      /* |E| = |V| on the ring */
#define COS_V95_DIM         4u      /* stalk dim */

typedef int32_t cos_v95_q16_t;

/* A cellular sheaf over a ring graph.  Restriction maps F_{v ◁ e} are
 * D×D diagonal matrices with entries in {−1, +1} (i.e. signed-identity
 * restrictions) so that F_{v ◁ e}^⊤ F_{v ◁ e} = I.  This keeps all
 * computations in integer arithmetic without losing the local-to-
 * global structure of the sheaf. */
typedef struct {
    /* For each edge e, edge endpoints. */
    uint8_t       e_u[COS_V95_EDGES];
    uint8_t       e_v[COS_V95_EDGES];
    /* Restriction diagonals F_{u ◁ e}, F_{v ◁ e}, each ∈ {−1, +1}^D. */
    int8_t        r_u [COS_V95_EDGES][COS_V95_DIM];
    int8_t        r_v [COS_V95_EDGES][COS_V95_DIM];
    /* Signal. */
    cos_v95_q16_t x   [COS_V95_NODES][COS_V95_DIM];
    /* Last energy reading + step count. */
    int64_t       last_energy;
    uint32_t      energy_violations;     /* times energy grew */
    uint32_t      steps;
    uint32_t      sentinel;
} cos_v95_sheaf_t;

void cos_v95_sheaf_init(cos_v95_sheaf_t *s, uint64_t seed);
void cos_v95_set_signal(cos_v95_sheaf_t *s,
                        const cos_v95_q16_t x[COS_V95_NODES][COS_V95_DIM]);

/* y = Δ_F x. */
void cos_v95_laplacian_apply(const cos_v95_sheaf_t *s,
                             cos_v95_q16_t y[COS_V95_NODES][COS_V95_DIM]);

/* One diffusion step x ← x − (Δ_F x >> lambda_rshift).  Using >>3
 * corresponds to λ = 1/8 which is stable on the ring graph. */
void cos_v95_diffuse_step(cos_v95_sheaf_t *s);

int64_t cos_v95_energy(const cos_v95_sheaf_t *s);

uint32_t cos_v95_ok(const cos_v95_sheaf_t *s);
uint32_t cos_v95_compose_decision(uint32_t v94_composed_ok,
                                  uint32_t v95_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V95_SHEAF_H */
