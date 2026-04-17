/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v91 — σ-Quantum (classical integer simulation of a small
 * quantum register with Grover-style amplitude amplification).
 *
 * ------------------------------------------------------------------
 *  What v91 is
 * ------------------------------------------------------------------
 *
 * v91 is a deterministic, integer-only (Q16.16 fixed-point),
 * branchless-on-the-hot-path simulator of a 4-qubit quantum register
 * (16 amplitudes) that supports:
 *
 *     X_i,  Z_i,  H_i,  CNOT_{c,t},  oracle-flip(marked),  diffusion
 *
 * with exact Q16.16 arithmetic.  Hadamard uses a pre-computed integer
 * approximation of 1/√2 (see `COS_V91_INV_SQRT2`).
 *
 * The amplitudes stay real for the gate set {X, Z, H, CNOT} when the
 * input is the standard basis state |0>, which is exactly the regime
 * of Grover search.  This is the "quantum leap" of 2026-style
 * tensor-network / stabilizer-adjacent classical simulation: a tiny
 * amount of silicon can faithfully emulate a small quantum circuit
 * without any floating-point whatsoever.
 *
 * Seven primitives (all integer, all deterministic):
 *
 *   1. cos_v91_reg_init       — |0000> in Q16.16 amplitudes
 *   2. cos_v91_x              — Pauli-X on qubit i
 *   3. cos_v91_z              — Pauli-Z on qubit i
 *   4. cos_v91_h              — Hadamard on qubit i (1/√2 in Q16.16)
 *   5. cos_v91_cnot           — controlled-NOT
 *   6. cos_v91_oracle         — sign-flip the marked basis state
 *   7. cos_v91_diffusion      — Grover diffusion 2|s><s| − I
 *
 * Plus:
 *   - cos_v91_prob_mass(reg)  — Σ |a_x|^2 in Q16.16 (should stay ≈ 1)
 *   - cos_v91_argmax(reg)     — marked state after amplification
 *   - cos_v91_grover(reg, m)  — canonical 3-iteration Grover on 4q
 *
 * ------------------------------------------------------------------
 *  Composed 31-bit branchless decision (extends v90)
 * ------------------------------------------------------------------
 *
 *     cos_v91_compose_decision(v90_composed_ok, v91_ok)
 *         = v90_composed_ok & v91_ok
 *
 * `v91_ok = 1` iff sentinel is intact, the total probability mass is
 * within ±1/256 of unity (quantization bound), and argmax after
 * Grover matches the oracle's marked index.
 */

#ifndef COS_V91_QUANTUM_H
#define COS_V91_QUANTUM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V91_SENTINEL      0x91C0FFEEu
#define COS_V91_NQUBITS       4u
#define COS_V91_DIM           (1u << COS_V91_NQUBITS)    /* 16 basis states */
#define COS_V91_INV_SQRT2     46341                       /* ⌊2^16 / √2⌋ */
#define COS_V91_Q1            65536                       /* 1.0 in Q16.16 */

typedef int32_t cos_v91_q16_t;

typedef struct {
    cos_v91_q16_t a[COS_V91_DIM];        /* real amplitudes, Q16.16 */
    uint32_t      ops;                    /* ops applied so far */
    uint32_t      invariant_violations;
    uint32_t      sentinel;
} cos_v91_reg_t;

/* Initialization: |0000>. */
void cos_v91_reg_init(cos_v91_reg_t *r);

/* Gates. `qubit` is 0..3; `c != t`. */
void cos_v91_x   (cos_v91_reg_t *r, uint32_t qubit);
void cos_v91_z   (cos_v91_reg_t *r, uint32_t qubit);
void cos_v91_h   (cos_v91_reg_t *r, uint32_t qubit);
void cos_v91_cnot(cos_v91_reg_t *r, uint32_t c, uint32_t t);

/* Grover oracle: flip sign of amplitude a[marked]. */
void cos_v91_oracle    (cos_v91_reg_t *r, uint32_t marked);
/* Grover diffusion about the uniform superposition |s>. */
void cos_v91_diffusion (cos_v91_reg_t *r);

/* Canonical 3-iteration Grover on 4-qubit register (optimum for N=16). */
void cos_v91_grover(cos_v91_reg_t *r, uint32_t marked);

/* Readouts. */
uint32_t cos_v91_argmax(const cos_v91_reg_t *r);
int64_t  cos_v91_prob_mass_q32(const cos_v91_reg_t *r);  /* Q32.32 */

/* Gate + compose. */
uint32_t cos_v91_ok(const cos_v91_reg_t *r, uint32_t marked);
uint32_t cos_v91_compose_decision(uint32_t v90_composed_ok,
                                  uint32_t v91_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V91_QUANTUM_H */
