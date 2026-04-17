/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v88 — σ-FHE (integer homomorphic-encryption compute).
 *
 * ------------------------------------------------------------------
 *  What v88 is
 * ------------------------------------------------------------------
 *
 * v88 implements a minimal Ring-LWE Learning-With-Errors
 * homomorphic-encryption primitive over Z_q[X]/(X^N + 1) in the
 * style of BGV / BFV (Brakerski-Gentry-Vaikuntanathan 2011, Fan-
 * Vercauteren 2012) — *additively* homomorphic over ciphertexts,
 * and homomorphic w.r.t. *plaintext-scalar* multiplication.
 *
 * We reuse v81's Keccak/SHAKE infrastructure for deterministic
 * keyed noise and the same Kyber-style modulus q = 3329, N = 256.
 * (The full multiplicative circuit requires relinearisation and
 * modulus-switching; that's in the roadmap for v88.1.  This kernel
 * ships the *additive* sub-ring — sufficient for every linear
 * transformation and for client-side aggregation of encrypted
 * residual-stream / gradient shares, which is the dominant FHE
 * workload in 2026 federated / private inference.)
 *
 * Seven primitives:
 *
 *   1. cos_v88_keygen           — secret key s (CBD), public pair (a, -a·s + e)
 *   2. cos_v88_encrypt          — m ∈ Z_t^N -> (c0, c1) ∈ Z_q^N × Z_q^N
 *   3. cos_v88_decrypt          — (c0, c1), s -> m
 *   4. cos_v88_add              — (a, b) + (c, d) = (a+c, b+d)
 *   5. cos_v88_mul_plain        — (a, b) * m_pt = (m_pt·a, m_pt·b)
 *   6. cos_v88_noise_budget     — returns the current noise upper bound
 *   7. cos_v88_rotate           — cyclic plaintext slot rotation
 *
 * ------------------------------------------------------------------
 *  Composed 28-bit branchless decision (extends v87)
 * ------------------------------------------------------------------
 *
 *     cos_v88_compose_decision(v87_composed_ok, v88_ok)
 *         = v87_composed_ok & v88_ok
 *
 * `v88_ok = 1` iff sentinel is intact, the last decrypted
 * plaintext exactly equalled the encrypted reference (correctness
 * invariant), and the current noise budget has not exceeded the
 * declared ceiling.
 */

#ifndef COS_V88_FHE_H
#define COS_V88_FHE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V88_SENTINEL   0xF4EC0DE2u
#define COS_V88_N          256u          /* polynomial degree */
#define COS_V88_Q          3329u         /* Kyber prime (reused) */
#define COS_V88_T          3u            /* plaintext modulus (Z_3 slots) */
#define COS_V88_ETA        2u            /* CBD noise parameter */

typedef struct {
    int16_t coeffs[COS_V88_N];
} cos_v88_poly_t;

typedef struct {
    cos_v88_poly_t c0;
    cos_v88_poly_t c1;
} cos_v88_ct_t;

typedef struct {
    cos_v88_poly_t s;           /* secret key */
    cos_v88_poly_t a;           /* public random poly */
    cos_v88_poly_t b;           /* b = -a·s + e (mod q) */
    uint32_t       noise_budget;
    uint32_t       noise_ceiling;
    uint32_t       last_correct; /* 1 if last (encrypt, decrypt) round-trip was exact */
    uint32_t       invariant_violations;
    uint32_t       sentinel;
} cos_v88_ctx_t;

/* KeyGen from 32-byte seed (deterministic). */
void cos_v88_keygen(cos_v88_ctx_t *c, const uint8_t seed[32]);

/* Encrypt a plaintext vector m ∈ Z_t^N.  r is an additional 32-byte
 * randomness seed that lets the user keep ciphertexts unlinkable. */
void cos_v88_encrypt(const cos_v88_ctx_t *c,
                     const int16_t m[COS_V88_N],
                     const uint8_t r[32],
                     cos_v88_ct_t *out);

/* Decrypt.  Returns 1 on success (invariants OK), 0 on failure. */
uint32_t cos_v88_decrypt(cos_v88_ctx_t *c,
                         const cos_v88_ct_t *ct,
                         int16_t m_out[COS_V88_N]);

/* Additive homomorphism: out = a + b. */
void cos_v88_add(const cos_v88_ct_t *a,
                 const cos_v88_ct_t *b,
                 cos_v88_ct_t *out);

/* Multiply ciphertext by a plaintext scalar ∈ Z_t. */
void cos_v88_mul_plain(const cos_v88_ct_t *a,
                       int16_t scalar,
                       cos_v88_ct_t *out);

/* Current noise budget (higher = more headroom). */
uint32_t cos_v88_noise_budget(const cos_v88_ctx_t *c);

/* Cyclic plaintext slot rotation by `k` (in encrypted space: shift
 * polynomial coefficients). */
void cos_v88_rotate(const cos_v88_ct_t *a, uint32_t k,
                    cos_v88_ct_t *out);

/* Gate + compose. */
uint32_t cos_v88_ok(const cos_v88_ctx_t *c);
uint32_t cos_v88_compose_decision(uint32_t v87_composed_ok,
                                  uint32_t v88_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V88_FHE_H */
