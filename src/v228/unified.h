/*
 * v228 σ-Unified — field equation, conservation law,
 *   phase transitions.
 *
 *   σ(t) is a scalar σ-field aggregated over the whole
 *   system.  K_eff(t) := 1 − σ(t) is the effective
 *   coherence.  τ(t) is the characteristic decision
 *   timescale.  The field equation (v0, Euler-forward):
 *
 *       dσ/dt = − α · K_eff + β · noise + γ · interaction
 *
 *   with α > 0 (learning pressure pulls σ down), β·noise
 *   is a zero-mean perturbation, and γ·interaction is a
 *   bounded coupling term.  σ is clamped to [0, 1] at
 *   every step.
 *
 *   Noether-style conservation (the 1=1 symmetry):
 *       K_eff(t) · τ(t) = C   for every t.
 *   v228 enforces this by DEFINITION in v0:
 *       τ(t) := C / K_eff(t)
 *   so the conservation is an identity, not an
 *   assumption.  This mirrors v225's K(K)=K: the
 *   invariant is baked into the dynamics; v228.1 will
 *   replace it with a measured conservation that only
 *   has to hold within ε.
 *
 *   Phase transitions:
 *       coherent      phase if K_eff(t) ≥ K_crit
 *       incoherent    phase if K_eff(t) <  K_crit
 *   n_transitions = # sign-flips of (K_eff − K_crit)
 *   along the trajectory.  Contract demands n ≥ 1 —
 *   the trajectory has to cross the critical line at
 *   least once under the v0 fixture.
 *
 *   Deterministic noise generator:
 *       noise(t) = sin(phi(t))
 *       phi(t) = phi(t−1) + 0.37 + (t % 7) * 0.11
 *   Mean is ≈ 0 over 100 steps (empirically small);
 *   β = 0.02 keeps the noise bounded under the
 *   learning pressure so σ trends down.
 *
 *   Interaction term:
 *       interaction(t) = cos(0.13 · t)
 *   with γ = 0.01.  Small compared to α so convergence
 *   dominates.
 *
 *   Contracts (v0):
 *     1. N_STEPS = 100.
 *     2. σ(t), K_eff(t) ∈ [0, 1] for every t.
 *     3. |K_eff(t) · τ(t) − C| ≤ ε_cons (1e-6) for
 *        every t.
 *     4. n_transitions ≥ 1 (trajectory actually
 *        crosses the critical line).
 *     5. σ(N) < σ(0)  (the field equation is
 *        integrating, not diverging).
 *     6. FNV-1a chain over every (t, σ, K, τ,
 *        noise, interaction) tuple replays byte-
 *        identically.
 *
 *   v228.1 (named): measured (not definitional)
 *     conservation with ε-bound, coupling to v224
 *     tensor / v225 fractal / v227 free-energy
 *     channels, wiring into v203 civilization-collapse
 *     detector.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V228_UNIFIED_H
#define COS_V228_UNIFIED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V228_N_STEPS  100

typedef struct {
    int   t;
    float sigma;
    float k_eff;
    float tau;
    float noise;
    float interaction;
    float k_eff_tau;     /* K_eff * τ        (should = C) */
    int   phase;         /* 0 incoherent, 1 coherent */
} cos_v228_sample_t;

typedef struct {
    cos_v228_sample_t  trace[COS_V228_N_STEPS + 1];

    float    alpha;       /* 0.20 */
    float    beta;        /* 0.02 */
    float    gamma_;      /* 0.01 */
    float    k_crit;      /* 0.50 */
    float    eps_cons;    /* 1e-6 */
    float    C;           /* K_eff(0) · τ(0) */

    int      n_transitions;
    float    sigma_start;
    float    sigma_end;
    float    max_cons_error;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v228_state_t;

void   cos_v228_init(cos_v228_state_t *s, uint64_t seed);
void   cos_v228_run(cos_v228_state_t *s);

size_t cos_v228_to_json(const cos_v228_state_t *s,
                         char *buf, size_t cap);

int    cos_v228_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V228_UNIFIED_H */
