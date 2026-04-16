/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v63 — σ-Cipher: end-to-end encrypted fabric for local
 * AI agents, bolted to the attestation chain of v61 and the capability
 * gate of v60.
 *
 * ## What this is
 *
 *   v63 distils the 2026 encryption frontier into one self-contained
 *   C layer with zero mandatory dependencies (libc only):
 *
 *     1. BLAKE2b-256              — RFC 7693 hash (same family as v61)
 *     2. HKDF-BLAKE2b             — RFC 5869 Extract + Expand
 *     3. ChaCha20                 — RFC 8439 stream cipher
 *     4. Poly1305                 — RFC 8439 one-time MAC
 *     5. ChaCha20-Poly1305 AEAD   — RFC 8439 Section 2.8
 *     6. X25519                   — RFC 7748 scalar multiplication
 *     7. Constant-time utilities  — ct_eq, secure_zero, masked select
 *     8. Sealed envelope          — attestation-bound symmetric seal:
 *                                    key = HKDF(attest_quote256 ∥ ctx)
 *     9. Symmetric ratchet        — forward-secret KDF chain
 *    10. Session handshake        — X25519(+optional PQ-hybrid)-Noise-
 *                                    style IK with BLAKE2b chaining key
 *    11. Optional libsodium slot  — COS_V63_LIBSODIUM=1 swaps (1)(2)(3)
 *                                    (4)(5)(6) for libsodium primitives
 *    12. Optional PQ-hybrid slot  — COS_V63_LIBOQS=1 augments (10) with
 *                                    ML-KEM-768 encapsulation whose
 *                                    shared secret is mixed into the
 *                                    BLAKE2b chaining key (reishi /
 *                                    Signal SPQR pattern, 2026)
 *
 * ## What this is not
 *
 *   - A replacement for libsodium / OpenSSL in production cloud
 *     workloads.  It is an auditable, dependency-free reference
 *     implementation sized for a local agent runtime, with a
 *     libsodium opt-in for anyone who wants FIPS-adjacent artefacts.
 *   - A full TLS / Noise implementation.  v63 ships the primitives
 *     plus a fixed IK-like handshake pattern, not a policy engine.
 *   - A confidential-compute enclave.  v63 binds keys to the v61
 *     *attestation quote* so that if the quote does not match the
 *     committed runtime, the sealed data will not decrypt — but it
 *     does not by itself provide memory encryption; pair with
 *     Secure Enclave / TDX / SEV-SNP in deployment.
 *
 * ## Composition with v60 + v61 + v62
 *
 *   A request to transmit a reasoning trace is ALLOWed iff:
 *
 *     v60 σ-Shield  (capability + intent σ)       authorises action
 *     v61 Σ-Citadel (BLP + Biba + compartments)   authorises flow
 *     v62 EBT       (energy ≤ budget)             authorises emission
 *     v63 σ-Cipher  (quote bind + AEAD tag ok)    authorises receipt
 *
 *   `cos_v63_compose_decision` takes the three upstream bits plus the
 *   v63 seal-verification bit and folds them into a 4-bit branchless
 *   AND.  No "OR" path.  No scalar fallback.  One sealed envelope per
 *   trace, bound to the attestation of the runtime that produced it.
 *
 * ## Tier semantics (v57 dialect)
 *
 *   M — BLAKE2b-256 core (RFC 7693 vectors)
 *   M — HKDF-BLAKE2b (RFC-5869-style extract+expand vectors)
 *   M — ChaCha20 block (RFC 8439 Appendix A.1 vectors)
 *   M — ChaCha20 stream (RFC 8439 Appendix A.2 vectors)
 *   M — Poly1305 (RFC 8439 Appendix A.3 vectors)
 *   M — ChaCha20-Poly1305 AEAD (RFC 8439 Section 2.8.2 vectors)
 *   M — X25519 (RFC 7748 Section 5.2 vectors)
 *   M — Constant-time equality (no early exit)
 *   M — Sealed-envelope round-trip + tamper detection
 *   M — Symmetric ratchet forward-secrecy (chain keys differ)
 *   I — libsodium opt-in parity (COS_V63_LIBSODIUM=1)
 *   P — liboqs ML-KEM-768 hybrid (COS_V63_LIBOQS=1; pending toolchain)
 *   P — Secure Enclave / TEE memory encryption (deployment-level)
 *
 * ## Hardware discipline (.cursorrules)
 *
 *   1. No dynamic allocation on the AEAD hot path.
 *   2. Branchless tag verification (ct_equal, full scan).
 *   3. Secure_zero on every intermediate key buffer before return.
 *   4. 64-byte aligned cipher state where it exists (ChaCha20 block,
 *      Poly1305 accumulator, HKDF PRK).
 *   5. Per-message one-time Poly1305 key derived from ChaCha20 block 0,
 *      never re-used across messages; counter+nonce are authenticated
 *      via the AEAD construction.
 *   6. X25519 Montgomery ladder: 255 loop iterations, fixed-time,
 *      constant-swap, every scalar bit processed identically.
 */

#ifndef CREATION_OS_V63_CIPHER_H
#define CREATION_OS_V63_CIPHER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* 0. Constants                                                        */
/* ------------------------------------------------------------------ */

#define COS_V63_CHACHA20_KEY_BYTES    32
#define COS_V63_CHACHA20_NONCE_BYTES  12
#define COS_V63_POLY1305_KEY_BYTES    32
#define COS_V63_POLY1305_TAG_BYTES    16
#define COS_V63_AEAD_KEY_BYTES        32
#define COS_V63_AEAD_NONCE_BYTES      12
#define COS_V63_AEAD_TAG_BYTES        16
#define COS_V63_BLAKE2B_256_BYTES     32
#define COS_V63_X25519_SCALAR_BYTES   32
#define COS_V63_X25519_POINT_BYTES    32
#define COS_V63_HKDF_PRK_BYTES        32
#define COS_V63_QUOTE256_BYTES        32    /* mirrors cos_v61_quote256 */

/* ------------------------------------------------------------------ */
/* 1. BLAKE2b-256                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    uint64_t h[8];
    uint64_t t[2];
    uint64_t f[2];
    uint8_t  buf[128];
    uint32_t buflen;
    uint32_t outlen;  /* in bytes; fixed to 32 for v63 */
} cos_v63_blake2b_t;

void cos_v63_blake2b_init_256(cos_v63_blake2b_t *s);
void cos_v63_blake2b_init_keyed_256(cos_v63_blake2b_t *s,
                                    const uint8_t     *key,
                                    size_t             keylen);
void cos_v63_blake2b_update(cos_v63_blake2b_t *s,
                            const uint8_t     *in,
                            size_t             inlen);
void cos_v63_blake2b_final(cos_v63_blake2b_t *s, uint8_t out[32]);

/* One-shot BLAKE2b-256 convenience. */
void cos_v63_blake2b_256(const uint8_t *in,
                         size_t         inlen,
                         uint8_t        out[32]);

/* ------------------------------------------------------------------ */
/* 2. HKDF-BLAKE2b (RFC 5869 structure)                                */
/* ------------------------------------------------------------------ */

/* Extract: PRK = HMAC-BLAKE2b(salt, IKM). */
void cos_v63_hkdf_extract(const uint8_t *salt, size_t saltlen,
                          const uint8_t *ikm,  size_t ikmlen,
                          uint8_t        prk[32]);

/* Expand: OKM = HKDF-Expand(PRK, info, L) with L ≤ 255 * 32 bytes. */
int  cos_v63_hkdf_expand(const uint8_t *prk,
                         const uint8_t *info, size_t infolen,
                         uint8_t       *out,  size_t outlen);

/* Convenience Extract+Expand in one call. */
int  cos_v63_hkdf(const uint8_t *salt, size_t saltlen,
                  const uint8_t *ikm,  size_t ikmlen,
                  const uint8_t *info, size_t infolen,
                  uint8_t       *out,  size_t outlen);

/* ------------------------------------------------------------------ */
/* 3. ChaCha20 stream cipher (RFC 8439)                                */
/* ------------------------------------------------------------------ */

/* Raw ChaCha20 block: key(32) + counter(u32 LE) + nonce(12) → 64 bytes
 * of keystream.  Constant-time; no secret-dependent branches. */
void cos_v63_chacha20_block(const uint8_t key[32],
                            uint32_t      counter,
                            const uint8_t nonce[12],
                            uint8_t       out[64]);

/* In-place / out-of-place ChaCha20 stream.  `counter` is the initial
 * counter (use 1 when used inside the RFC 8439 AEAD; 0 for Poly1305
 * key derivation). */
void cos_v63_chacha20_xor(const uint8_t  key[32],
                          uint32_t       counter,
                          const uint8_t  nonce[12],
                          const uint8_t *in,
                          uint8_t       *out,
                          size_t         len);

/* ------------------------------------------------------------------ */
/* 4. Poly1305 (RFC 8439)                                              */
/* ------------------------------------------------------------------ */

/* One-shot Poly1305: MAC over `in` with 32-byte key → 16-byte tag. */
void cos_v63_poly1305(const uint8_t  key[32],
                      const uint8_t *in,
                      size_t         inlen,
                      uint8_t        tag[16]);

/* ------------------------------------------------------------------ */
/* 5. ChaCha20-Poly1305 AEAD (RFC 8439 Section 2.8)                    */
/* ------------------------------------------------------------------ */

/* Encrypt with associated-data.  ct must have room for `ptlen` bytes,
 * tag receives 16 bytes.  Nonces MUST be unique per key; 96 bits is
 * deliberately not large enough for random nonces at scale — use a
 * counter or derive per-message nonces from HKDF. */
void cos_v63_aead_encrypt(const uint8_t  key[32],
                          const uint8_t  nonce[12],
                          const uint8_t *aad, size_t aadlen,
                          const uint8_t *pt,  size_t ptlen,
                          uint8_t       *ct,
                          uint8_t        tag[16]);

/* Decrypt with associated-data; constant-time tag compare.
 * Returns 1 on authentic, 0 on tamper (in which case `pt` MAY contain
 * partial decrypted bytes and MUST be discarded by the caller). */
int  cos_v63_aead_decrypt(const uint8_t  key[32],
                          const uint8_t  nonce[12],
                          const uint8_t *aad, size_t aadlen,
                          const uint8_t *ct,  size_t ctlen,
                          const uint8_t  tag[16],
                          uint8_t       *pt);

/* ------------------------------------------------------------------ */
/* 6. X25519 (RFC 7748)                                                */
/* ------------------------------------------------------------------ */

/* Scalar multiplication: out = scalar * u.  Constant-time Montgomery
 * ladder, 255 iterations, conditional-swap via mask.  Returns 1 on
 * success, 0 if the resulting shared secret is all-zero (which RFC
 * 7748 recommends rejecting for contributory behaviour). */
int  cos_v63_x25519(uint8_t        out[32],
                    const uint8_t  scalar[32],
                    const uint8_t  u_point[32]);

/* Derive the public key from a scalar: scalar * 9 (the curve base
 * point). */
void cos_v63_x25519_base(uint8_t       pk[32],
                         const uint8_t sk[32]);

/* ------------------------------------------------------------------ */
/* 7. Constant-time utilities                                          */
/* ------------------------------------------------------------------ */

/* Constant-time equality over arbitrary-length buffers.  Returns 1 if
 * equal, 0 otherwise.  No early exit; always walks `n` bytes. */
int  cos_v63_ct_eq(const void *a, const void *b, size_t n);

/* Optimiser-resistant zeroise.  After return, the region is zero and
 * the compiler may not elide the write. */
void cos_v63_secure_zero(void *p, size_t n);

/* ------------------------------------------------------------------ */
/* 8. Attestation-bound sealed envelope                                */
/* ------------------------------------------------------------------ */

/* The 2026 frontier pattern (Chutes, Tinfoil, QSC):
 *
 *    key = HKDF(salt = attest_quote256,
 *               ikm  = ephemeral nonce,
 *               info = "cos/v63/seal" ∥ context)
 *
 * so the ciphertext will only decrypt on a host whose current v61
 * attestation quote matches the one the sender sealed against.  This
 * is *deterministic key binding* — no TEE required — and composes
 * with an actual TEE (Secure Enclave, TDX, SEV-SNP) when present. */
typedef struct {
    uint8_t  quote256 [COS_V63_QUOTE256_BYTES];   /* v61 attestation quote */
    uint8_t  nonce12  [COS_V63_AEAD_NONCE_BYTES]; /* per-envelope nonce    */
    uint32_t ptlen;
    uint32_t aadlen;
    uint8_t  tag16   [COS_V63_AEAD_TAG_BYTES];
    /* ciphertext and aad follow in caller-supplied buffers */
} cos_v63_envelope_hdr_t;

/* Seal plaintext under the attestation quote + context label.
 *
 *   envelope = [ hdr(quote+nonce+lens+tag) | aad | ciphertext ]
 *
 * Returns the total envelope size on success (hdr + aad + ptlen).
 * Caller supplies `out` with at least (sizeof(hdr) + aadlen + ptlen)
 * bytes.  The per-envelope nonce is caller-supplied to stay
 * deterministic under test; production callers should pass fresh
 * random 12-byte nonces.  `context` is a short label (e.g.
 * "trace/reasoning") mixed into HKDF to domain-separate keys.        */
int  cos_v63_seal(const uint8_t  quote256[32],
                  const uint8_t  nonce12 [12],
                  const uint8_t *context,  size_t ctxlen,
                  const uint8_t *aad,      size_t aadlen,
                  const uint8_t *pt,       size_t ptlen,
                  uint8_t       *out,      size_t outcap);

/* Verify-and-open a sealed envelope.  Returns plaintext length on
 * success, -1 on header inconsistency, -2 on AAD mismatch, -3 on
 * tag mismatch.  `out_pt` must have room for at least `hdr.ptlen`
 * bytes. */
int  cos_v63_open(const uint8_t *envelope, size_t envlen,
                  const uint8_t *context,  size_t ctxlen,
                  uint8_t       *out_pt,   size_t out_cap);

/* ------------------------------------------------------------------ */
/* 9. Symmetric ratchet (forward secrecy, one-way chain)               */
/* ------------------------------------------------------------------ */

/* A minimal KDF-based ratchet: each call advances the chain key with
 * HKDF(prev_chain, "cos/v63/ratchet") and emits a fresh message key.
 * Compromise of `chain_out` does not reveal prior chain values.      */
typedef struct {
    uint8_t  chain_key[32];
    uint64_t counter;
} cos_v63_ratchet_t;

void cos_v63_ratchet_init(cos_v63_ratchet_t *r,
                          const uint8_t      root_key[32]);
void cos_v63_ratchet_step(cos_v63_ratchet_t *r,
                          uint8_t            msg_key_out[32]);

/* ------------------------------------------------------------------ */
/* 10. Session handshake (IK-like, optional PQ-hybrid)                 */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t local_sk [32];     /* X25519 static secret                  */
    uint8_t local_pk [32];     /* X25519 static public                  */
    uint8_t remote_pk[32];     /* peer's static public                  */
    uint8_t eph_sk   [32];     /* X25519 ephemeral secret               */
    uint8_t eph_pk   [32];     /* X25519 ephemeral public               */
    uint8_t ck       [32];     /* chaining key (BLAKE2b-256)            */
    uint8_t k        [32];     /* current symmetric key                 */
    uint64_t n;                /* nonce counter                         */
    uint8_t  role;             /* 0 = initiator, 1 = responder          */
    uint8_t  stage;            /* 0 = start, 1 = post-es, 2 = done      */
    uint8_t  _pad[6];
} cos_v63_session_t;

void cos_v63_session_init(cos_v63_session_t *s,
                          uint8_t            role,
                          const uint8_t      local_sk[32],
                          const uint8_t      remote_pk[32]);

/* Single-shot sealed handshake + first message.
 *   Initiator → Responder:  eph_pk(32) || AEAD(k, n=0, pt=local_pk) ||
 *                           AEAD(k', n=1, pt=first_msg)
 *   Responder opens eph_pk, derives shared secrets, recovers local_pk
 *   from AEAD, authenticates first_msg.
 * Returns bytes written on success or a negative error on failure.   */
int  cos_v63_session_seal_first(cos_v63_session_t *s,
                                const uint8_t *msg,   size_t msglen,
                                uint8_t       *out,   size_t outcap);

int  cos_v63_session_open_first(cos_v63_session_t *s,
                                const uint8_t *in,    size_t inlen,
                                uint8_t       *out,   size_t outcap);

/* ------------------------------------------------------------------ */
/* 11. Composition bit with v60 / v61 / v62                            */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t v60_ok;  /* σ-Shield allow                        */
    uint8_t v61_ok;  /* Σ-Citadel lattice allow               */
    uint8_t v62_ok;  /* EBT energy ≤ budget                   */
    uint8_t v63_ok;  /* σ-Cipher seal authentic                */
    uint8_t allow;   /* v60_ok & v61_ok & v62_ok & v63_ok     */
    uint8_t _pad[3];
} cos_v63_decision_t;

cos_v63_decision_t cos_v63_compose_decision(uint8_t v60_ok,
                                            uint8_t v61_ok,
                                            uint8_t v62_ok,
                                            uint8_t v63_ok);

/* ------------------------------------------------------------------ */
/* 12. Version                                                         */
/* ------------------------------------------------------------------ */

typedef struct { int32_t major, minor, patch; } cos_v63_version_t;
cos_v63_version_t cos_v63_version(void);
const char       *cos_v63_build_info(void);

#ifdef __cplusplus
}
#endif
#endif /* CREATION_OS_V63_CIPHER_H */
