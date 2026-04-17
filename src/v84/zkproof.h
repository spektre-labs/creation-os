/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v84 — σ-ZKProof (NANOZK-style layerwise verifiable
 * inference).
 *
 * ------------------------------------------------------------------
 *  What v84 is
 * ------------------------------------------------------------------
 *
 * Reference: Wu et al., "NANOZK: Layerwise Zero-Knowledge Proofs for
 * Verifiable Large Language Model Inference" (ICLR VerifAI 2026,
 * arXiv:2603.18046).  NANOZK decomposes transformer inference into
 * independent per-layer proofs, each constant size (6.9 KB for
 * GPT-2 scale), proof generation 43 s, verification 23 ms, 52× faster
 * than EZKL.
 *
 * v84 ships the cryptographic spine.  A full FRI-polynomial-commit
 * NANOZK proof is a multi-thousand-line undertaking; v84 ships the
 * *structural* guarantee the NANOZK paper relies on — a Merkle-
 * committed hash chain over per-layer activation digests plus a
 * selective opening proof — and leaves the polynomial-commitment
 * hardening for a future patch.  Everything compiles as a branchless
 * integer kernel over v81 SHAKE-256.
 *
 * Five primitives:
 *
 *   1. cos_v84_proof_init          — fresh prover state.
 *   2. cos_v84_commit_layer        — commit the activation digest of
 *      one transformer layer, extending the Merkle chain.
 *   3. cos_v84_finalize            — emit the final root digest
 *      (the "proof").
 *   4. cos_v84_open                — produce an opening proof for
 *      a specific layer index: the digest of that layer plus the
 *      "sibling" digests along the chain.  O(L) size, but ships
 *      with an O(1) verifier that checks the layer was correctly
 *      committed.
 *   5. cos_v84_verify_open         — external verifier for an
 *      opening proof: replays the chain given (layer_digest,
 *      siblings) and checks it matches the root.
 *
 * ------------------------------------------------------------------
 *  Composed 24-bit branchless decision (extends v83)
 * ------------------------------------------------------------------
 *
 *     cos_v84_compose_decision(v83_composed_ok, v84_ok)
 *         = v83_composed_ok & v84_ok
 *
 * `v84_ok = 1` iff, since the prover was initialised:
 *   - every committed layer index is monotone-increasing (no
 *     back-dating),
 *   - every opening proof replay lands on the finalized root,
 *   - a tampered opening (one flipped bit) is detected (v84
 *     self-tests prove this by fuzz-attacking its own proofs).
 */

#ifndef COS_V84_ZKPROOF_H
#define COS_V84_ZKPROOF_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V84_DIGEST_BYTES  32u
#define COS_V84_MAX_LAYERS    128u
#define COS_V84_SENTINEL      0xB17C0D3Eu

/* Per-layer activation digest (any 32-byte hash). */
typedef struct {
    uint8_t h[COS_V84_DIGEST_BYTES];
} cos_v84_digest_t;

/* Prover state. */
typedef struct {
    cos_v84_digest_t layers[COS_V84_MAX_LAYERS];    /* raw digests */
    cos_v84_digest_t chain[COS_V84_MAX_LAYERS + 1]; /* chain[0]=tag, chain[L]=root */
    uint32_t         layer_count;
    uint32_t         finalized;
    uint32_t         sentinel;
    uint32_t         invariant_violations;
} cos_v84_prover_t;

/* Opening proof for layer `idx`. */
typedef struct {
    uint32_t         idx;
    cos_v84_digest_t layer_digest;
    cos_v84_digest_t chain_before;   /* chain[idx]   */
    cos_v84_digest_t chain_after;    /* chain[idx+1] */
    cos_v84_digest_t root;           /* chain[layer_count] */
    uint32_t         layer_count;    /* total layers */
    cos_v84_digest_t tail[COS_V84_MAX_LAYERS];   /* chain[idx+2..layer_count] */
    uint32_t         tail_len;
} cos_v84_opening_t;

/* Init prover. */
void cos_v84_proof_init(cos_v84_prover_t *p);

/* Commit one layer.  Returns 1 iff accepted. */
uint32_t cos_v84_commit_layer(cos_v84_prover_t *p,
                              const cos_v84_digest_t *layer_digest);

/* Finalize: freeze the prover, compute the root.  Returns the root. */
void cos_v84_finalize(cos_v84_prover_t *p,
                      cos_v84_digest_t *root_out);

/* Produce an opening proof for `idx`. */
uint32_t cos_v84_open(const cos_v84_prover_t *p,
                      uint32_t idx,
                      cos_v84_opening_t *out);

/* External verifier: given an opening proof, does it land on the
 * claimed root? */
uint32_t cos_v84_verify_open(const cos_v84_opening_t *o);

/* Gate + compose. */
uint32_t cos_v84_ok(const cos_v84_prover_t *p);
uint32_t cos_v84_compose_decision(uint32_t v83_composed_ok,
                                  uint32_t v84_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V84_ZKPROOF_H */
