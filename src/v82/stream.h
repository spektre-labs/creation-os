/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v82 — σ-Stream (streaming per-chunk composed decision).
 *
 * ------------------------------------------------------------------
 *  What v82 is
 * ------------------------------------------------------------------
 *
 * The composed-decision AND up through v81 is a *final* gate: the
 * whole batch either passes or fails.  For a chat-class interactive
 * UX the user wants tokens streamed as they are decided, and wants
 * the stream to *halt cleanly* the instant any kernel flips to 0.
 *
 * v82 is that streaming gate.  It accepts a stream of chunks, each
 * carrying the 21 per-kernel verdict bits (v60..v81), and emits for
 * every chunk a BLAKE2b-style Merkle-chained receipt:
 *
 *     h_0 = H(SALT)
 *     h_k = H(h_{k-1} || chunk_k_bits || chunk_k_payload_digest)
 *
 * The decision for chunk k is the AND of its 21 bits AND the decision
 * of chunk k-1.  The first chunk that introduces a zero halts the
 * stream: every subsequent chunk is rejected before it is even
 * processed, and the hash chain is frozen.  This turns the composed
 * AND from a batch property into a *monotone* streaming property:
 *     once false, always false.
 *
 * Six primitives:
 *
 *   1. cos_v82_stream_init        — fresh stream with salt.
 *   2. cos_v82_chunk_admit        — try to admit a chunk of 21 bits
 *      plus an arbitrary payload digest; returns (admitted, halted).
 *   3. cos_v82_stream_digest      — read the current chain head.
 *   4. cos_v82_stream_step_count  — total chunks seen.
 *   5. cos_v82_stream_halt_index  — index of the chunk that first
 *      turned the AND false, or UINT32_MAX if never halted.
 *   6. cos_v82_ok / cos_v82_compose_decision — the 22-bit gate.
 *
 * ------------------------------------------------------------------
 *  Composed 22-bit branchless decision (extends v81)
 * ------------------------------------------------------------------
 *
 *     cos_v82_compose_decision(v81_composed_ok, v82_ok)
 *         = v81_composed_ok & v82_ok
 *
 * `v82_ok = 1` iff, since the stream was initialised:
 *   - every chunk admitted had its full-bit (the bit-k=21 AND of the
 *     21 per-kernel bits) equal to the running AND,
 *   - the monotone invariant "once false, always false" was never
 *     violated (no attempt to admit a chunk past the halt point
 *     silently succeeded),
 *   - the chain head digest matches a recomputed digest of the full
 *     admitted sequence,
 *   - the internal sentinel is intact.
 *
 * Hardware discipline: integer, branchless, libc-only, no malloc on
 * the hot path.  Hash primitive: SHAKE-256 from v81 σ-Lattice (so
 * v82 depends on v81 — the PQC spine becomes the streaming-receipt
 * spine for free).
 */

#ifndef COS_V82_STREAM_H
#define COS_V82_STREAM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V82_DIGEST_BYTES  32u   /* chain head digest, SHAKE-256 */
#define COS_V82_SALT_BYTES    32u
#define COS_V82_SENTINEL      0xF00DBABEu

/* Payload digest per chunk (any 32-byte hash; typically BLAKE2b of
 * the token bytes the chunk emitted). */
typedef struct {
    uint8_t h[COS_V82_DIGEST_BYTES];
} cos_v82_payload_t;

typedef struct {
    uint8_t   head[COS_V82_DIGEST_BYTES];     /* Merkle chain head  */
    uint32_t  chunk_count;
    uint32_t  halt_idx;                        /* UINT32_MAX if live */
    uint32_t  running_and;                     /* current AND of bits */
    uint32_t  sentinel;
    uint32_t  invariant_violations;
} cos_v82_stream_t;

/* Initialise a fresh stream with the given 32-byte salt. */
void cos_v82_stream_init(cos_v82_stream_t *s,
                         const uint8_t salt[COS_V82_SALT_BYTES]);

/* Admit a chunk.  `kernel_bits` is a 21-bit word carrying
 * (v60_ok, v61_ok, ..., v80_ok, v81_ok) in bits 0..20.
 * Returns 1 if admitted + AND-true, 0 otherwise (halted).
 * After halt, all subsequent calls return 0 and make no state
 * mutation except to bump chunk_count (auditable halt horizon). */
uint32_t cos_v82_chunk_admit(cos_v82_stream_t *s,
                             uint32_t kernel_bits,
                             const cos_v82_payload_t *payload);

/* Read the current chain head digest. */
void cos_v82_stream_digest(const cos_v82_stream_t *s,
                           uint8_t out[COS_V82_DIGEST_BYTES]);

/* Recompute the head digest from first principles given a replay
 * of (kernel_bits, payload) pairs; returns 1 iff the replay lands
 * on the same head as `live`, 0 otherwise.  Used by self-tests
 * and by external verifiers. */
uint32_t cos_v82_stream_verify(const uint8_t salt[COS_V82_SALT_BYTES],
                               const uint32_t *kernel_bits,
                               const cos_v82_payload_t *payloads,
                               size_t count,
                               const uint8_t expected_head[COS_V82_DIGEST_BYTES]);

/* Gate + compose. */
uint32_t cos_v82_ok(const cos_v82_stream_t *s);
uint32_t cos_v82_compose_decision(uint32_t v81_composed_ok,
                                  uint32_t v82_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V82_STREAM_H */
