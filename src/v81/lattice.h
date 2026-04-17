/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v81 — σ-Lattice (post-quantum lattice cryptography plane).
 *
 * ------------------------------------------------------------------
 *  What v81 is
 * ------------------------------------------------------------------
 *
 * v63 σ-Cipher ships classical primitives (BLAKE2b, HKDF, X25519,
 * ChaCha20-Poly1305).  Under "harvest-now-decrypt-later" (HNDL) they
 * are all on borrowed time — NIST finalised FIPS 203/204/205 in
 * August 2024, CNSA 2.0 mandates PQC for US NSS from Jan 2027, and
 * Chrome 131+ / Firefox 135+ already ship ML-KEM-768 in hybrid TLS.
 *
 * v81 adds the arithmetic spine that any FIPS-203-shaped key encaps
 * needs: Keccak-f[1600] + SHAKE-128/256, the Kyber modulus q = 3329
 * with Barrett + Montgomery reductions, a centered-binomial sampler,
 * and a 256-coefficient number-theoretic transform (NTT / INTT).
 *
 * The eight primitives, each grounded in a real standard or paper:
 *
 *   1. cos_v81_keccak_f1600   — the Keccak permutation (Bertoni et
 *      al. 2011, NIST FIPS 202).  24 rounds of θ/ρ/π/χ/ι on a 25-lane
 *      uint64 state.  Branchless; all data-flow is XOR / AND / NOT /
 *      rotate.
 *
 *   2. cos_v81_shake128       — SHAKE-128 XOF (FIPS 202 §6.3,
 *      rate 168 B).  Absorb arbitrary message, squeeze arbitrary
 *      output.  Used as the seed expander in any Kyber-shaped KEM.
 *
 *   3. cos_v81_shake256       — SHAKE-256 XOF (FIPS 202 §6.3,
 *      rate 136 B).  Higher capacity; used for key derivation and
 *      noise sampling.
 *
 *   4. cos_v81_barrett_reduce — constant-time Barrett reduction mod
 *      q = 3329 on signed 16-bit inputs, mapping to the signed range
 *      {-(q-1)/2, ..., (q-1)/2}.  Reference: pq-code-package/mlkem-
 *      native poly.c `mlk_barrett_reduce`, with magic constant 20159
 *      = round(2^26 / 3329).
 *
 *   5. cos_v81_montgomery_mul — Montgomery multiplication a·b·R^{-1}
 *      mod q with R = 2^16.  QINV = -q^{-1} mod 2^16 = 62209 for
 *      q = 3329.  Used inside every NTT butterfly.
 *
 *   6. cos_v81_ntt / cos_v81_intt — the forward and inverse number-
 *      theoretic transform on a 256-coefficient polynomial in Z_q
 *      using the Kyber ζ = 17 primitive-256-th root of unity.
 *      Radix-2 Cooley-Tukey + Gentleman-Sande, in place, branchless.
 *
 *   7. cos_v81_cbd2           — centered-binomial distribution of
 *      parameter η = 2.  Each coefficient sampled from Σ(a_i - b_i)
 *      where a_i, b_i are four bits each.  FIPS 203 §4.2.2
 *      Algorithm 2 (SamplePolyCBD_η).
 *
 *   8. cos_v81_kem_keygen / cos_v81_kem_encaps / cos_v81_kem_decaps —
 *      a Kyber-shaped KEM that composes primitives 1..7.  We do *not*
 *      claim FIPS 203 bit-exact conformance (the reference uses very
 *      specific zeta tables, rejection sampling for A, and domain
 *      separators we do not reproduce here); we do guarantee a
 *      complete, constant-time, correctness-round-trippable lattice
 *      KEM over Z_{3329}^{256}.  It is the arithmetic spine; a
 *      future patch can lift it to bit-exact ML-KEM-768 without
 *      changing the v81 ABI.
 *
 * ------------------------------------------------------------------
 *  Composed 21-bit branchless decision (extends v80)
 * ------------------------------------------------------------------
 *
 *     cos_v81_compose_decision(v80_composed_ok, v81_ok)
 *         = v80_composed_ok & v81_ok
 *
 * `v81_ok = 1` iff, for the KEM round-trip just executed:
 *   - the Keccak state visited exactly 24 rounds (no under-run),
 *   - every NTT / INTT round-trip recovered the input exactly,
 *   - every Barrett reduction landed in {-(q-1)/2, ..., (q-1)/2},
 *   - CBD samples stayed in {-η, ..., η},
 *   - encaps/decaps round-trip yielded a byte-identical shared
 *     secret (the central KEM correctness predicate),
 *   - no sentinel was clobbered.
 *
 * ------------------------------------------------------------------
 *  Hardware discipline (unchanged)
 * ------------------------------------------------------------------
 *
 *   - All arithmetic integer; polys laid out as int16_t[256].
 *   - Branchless hot path; masks instead of `if`.
 *   - 64-byte aligned state buffers; no malloc on the hot path.
 *   - libc-only (string.h + stdint.h).
 *   - Keccak state is 25 × uint64 = 200 bytes.
 */

#ifndef COS_V81_LATTICE_H
#define COS_V81_LATTICE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 *  0.  Constants & primitive types.
 * ================================================================== */

/* Kyber modulus (Z_q, q = 3329 = 2^8 * 13 + 1 chosen so that
 * 2^16 ≡ -q^{-1} mod R gives a clean Montgomery R = 2^16). */
#define COS_V81_Q              3329
#define COS_V81_QINV           62209u       /* -q^{-1} mod 2^16 */
#define COS_V81_N              256u         /* polynomial degree */
#define COS_V81_K              3u           /* matrix rank (Kyber-768-shaped) */

/* Barrett magic constant: round(2^26 / q). */
#define COS_V81_BARRETT_MAGIC  20159

/* CBD noise parameter (η = 2 for Kyber-768). */
#define COS_V81_ETA            2u

/* Keccak-f[1600] state: 25 lanes of 64 bits = 200 bytes. */
#define COS_V81_KECCAK_LANES   25u

/* SHAKE rates (bytes of each squeezed block). */
#define COS_V81_SHAKE128_RATE  168u
#define COS_V81_SHAKE256_RATE  136u

/* KEM payload sizes (Kyber-768-shaped). */
#define COS_V81_SYMBYTES       32u          /* shared secret length  */
#define COS_V81_POLYBYTES      384u         /* 12 bits * 256 / 8     */
#define COS_V81_POLYVEC_BYTES  (COS_V81_K * COS_V81_POLYBYTES)   /* 1152 */
#define COS_V81_PKBYTES        (COS_V81_POLYVEC_BYTES + COS_V81_SYMBYTES)  /* 1184 */
#define COS_V81_SKBYTES        COS_V81_POLYVEC_BYTES                       /* 1152 */
#define COS_V81_CTBYTES        (COS_V81_POLYVEC_BYTES + COS_V81_POLYBYTES) /* 1536 */

/* Polynomial representation. */
typedef struct { int16_t c[COS_V81_N]; } cos_v81_poly_t;

/* Polynomial vector of length K. */
typedef struct { cos_v81_poly_t v[COS_V81_K]; } cos_v81_polyvec_t;

/* Keccak state. */
typedef struct {
    uint64_t s[COS_V81_KECCAK_LANES];
    uint32_t rate;          /* 168 for SHAKE-128, 136 for SHAKE-256 */
    uint32_t pos;           /* current position within the current block */
    uint32_t squeezing;     /* 0 = absorbing, 1 = squeezing */
    uint32_t rounds_done;   /* total rounds run; invariant: always k*24 */
} cos_v81_xof_t;

/* ==================================================================
 *  API — Keccak / SHAKE
 * ================================================================== */

/* Run the Keccak-f[1600] permutation in place. */
void cos_v81_keccak_f1600(uint64_t s[25]);

/* SHAKE-128 / SHAKE-256 init, absorb, finalise, squeeze. */
void cos_v81_shake128_init(cos_v81_xof_t *x);
void cos_v81_shake256_init(cos_v81_xof_t *x);
void cos_v81_shake_absorb(cos_v81_xof_t *x, const uint8_t *in, size_t len);
void cos_v81_shake_finalize(cos_v81_xof_t *x);
void cos_v81_shake_squeeze(cos_v81_xof_t *x, uint8_t *out, size_t len);

/* ==================================================================
 *  API — Kyber arithmetic
 * ================================================================== */

/* Barrett reduction mod q, in signed-centered range. */
int16_t cos_v81_barrett_reduce(int16_t a);

/* Montgomery multiplication: returns a·b·R^{-1} mod q, R = 2^16. */
int16_t cos_v81_montgomery_mul(int16_t a, int16_t b);

/* In-place NTT / inverse NTT on a 256-coefficient polynomial. */
void cos_v81_ntt(int16_t p[COS_V81_N]);
void cos_v81_intt(int16_t p[COS_V81_N]);

/* Pointwise polynomial multiplication in NTT domain. */
void cos_v81_poly_ntt_mul(cos_v81_poly_t *r,
                          const cos_v81_poly_t *a,
                          const cos_v81_poly_t *b);

/* Polynomial add / reduce. */
void cos_v81_poly_add(cos_v81_poly_t *r,
                      const cos_v81_poly_t *a,
                      const cos_v81_poly_t *b);
void cos_v81_poly_sub(cos_v81_poly_t *r,
                      const cos_v81_poly_t *a,
                      const cos_v81_poly_t *b);
void cos_v81_poly_reduce(cos_v81_poly_t *p);

/* CBD sampler (η = 2). */
void cos_v81_cbd2(cos_v81_poly_t *r, const uint8_t buf[128]);

/* Poly (de)serialisation into 12-bit packed bytes. */
void cos_v81_poly_tobytes(uint8_t out[COS_V81_POLYBYTES],
                          const cos_v81_poly_t *p);
void cos_v81_poly_frombytes(cos_v81_poly_t *p,
                            const uint8_t in[COS_V81_POLYBYTES]);

/* ==================================================================
 *  API — KEM
 * ================================================================== */

/* Key generation.  Returns 0 on success. */
int cos_v81_kem_keygen(uint8_t pk[COS_V81_PKBYTES],
                       uint8_t sk[COS_V81_SKBYTES],
                       const uint8_t seed_d[COS_V81_SYMBYTES],
                       const uint8_t seed_z[COS_V81_SYMBYTES]);

/* Encapsulation. */
int cos_v81_kem_encaps(uint8_t ct[COS_V81_CTBYTES],
                       uint8_t ss[COS_V81_SYMBYTES],
                       const uint8_t pk[COS_V81_PKBYTES],
                       const uint8_t seed_m[COS_V81_SYMBYTES]);

/* Decapsulation. */
int cos_v81_kem_decaps(uint8_t ss[COS_V81_SYMBYTES],
                       const uint8_t ct[COS_V81_CTBYTES],
                       const uint8_t sk[COS_V81_SKBYTES],
                       const uint8_t pk[COS_V81_PKBYTES]);

/* ==================================================================
 *  API — Gate + 21-bit compose
 * ================================================================== */

/* Runtime invariant probe: returns 1 iff every internal postcondition
 * currently holds (Barrett range, NTT round-trip, KEM round-trip on
 * the last call, Keccak rounds == 24, no sentinel break). */
uint32_t cos_v81_ok(void);

/* 21-bit branchless composed decision. */
uint32_t cos_v81_compose_decision(uint32_t v80_composed_ok,
                                  uint32_t v81_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V81_LATTICE_H */
