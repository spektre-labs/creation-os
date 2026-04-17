/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v82 — σ-Stream implementation.
 *
 * Uses v81 σ-Lattice SHAKE-256 as the hash primitive (same Keccak
 * state we already trust — no second hash function, no second
 * attestation surface).
 */

#include "stream.h"
#include "../v81/lattice.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline uint32_t all21_mask(void) { return 0x1FFFFFu; }  /* 21 bits */

/* Chain step: h_out = SHAKE256(h_in || bits || payload). */
static void chain_step(uint8_t out[COS_V82_DIGEST_BYTES],
                       const uint8_t in[COS_V82_DIGEST_BYTES],
                       uint32_t kernel_bits,
                       const cos_v82_payload_t *payload)
{
    cos_v82_payload_t zero_payload;
    const cos_v82_payload_t *p = payload;
    if (p == NULL) {
        memset(&zero_payload, 0, sizeof(zero_payload));
        p = &zero_payload;
    }
    uint8_t buf[COS_V82_DIGEST_BYTES + 4 + COS_V82_DIGEST_BYTES];
    memcpy(buf, in, COS_V82_DIGEST_BYTES);
    buf[COS_V82_DIGEST_BYTES + 0] = (uint8_t)(kernel_bits      & 0xFFu);
    buf[COS_V82_DIGEST_BYTES + 1] = (uint8_t)((kernel_bits >> 8)  & 0xFFu);
    buf[COS_V82_DIGEST_BYTES + 2] = (uint8_t)((kernel_bits >> 16) & 0xFFu);
    buf[COS_V82_DIGEST_BYTES + 3] = (uint8_t)((kernel_bits >> 24) & 0xFFu);
    memcpy(buf + COS_V82_DIGEST_BYTES + 4, p->h, COS_V82_DIGEST_BYTES);

    cos_v81_xof_t x;
    cos_v81_shake256_init(&x);
    cos_v81_shake_absorb(&x, buf, sizeof(buf));
    cos_v81_shake_finalize(&x);
    cos_v81_shake_squeeze(&x, out, COS_V82_DIGEST_BYTES);
}

void cos_v82_stream_init(cos_v82_stream_t *s,
                         const uint8_t salt[COS_V82_SALT_BYTES])
{
    memset(s, 0, sizeof(*s));
    /* h_0 = SHAKE256(SALT). */
    cos_v81_xof_t x;
    cos_v81_shake256_init(&x);
    cos_v81_shake_absorb(&x, salt, COS_V82_SALT_BYTES);
    cos_v81_shake_finalize(&x);
    cos_v81_shake_squeeze(&x, s->head, COS_V82_DIGEST_BYTES);
    s->halt_idx     = UINT32_MAX;
    s->running_and  = 1u;
    s->sentinel     = COS_V82_SENTINEL;
}

uint32_t cos_v82_chunk_admit(cos_v82_stream_t *s,
                             uint32_t kernel_bits,
                             const cos_v82_payload_t *payload)
{
    if (s->sentinel != COS_V82_SENTINEL) {
        ++s->invariant_violations;
        return 0u;
    }

    /* all_bits: 1 iff every one of the 21 bits is 1. */
    uint32_t masked = kernel_bits & all21_mask();
    uint32_t all_bits = (masked == all21_mask()) ? 1u : 0u;

    /* already_halted: sticky flag. */
    uint32_t already_halted = (s->halt_idx != UINT32_MAX) ? 1u : 0u;

    /* New running AND. */
    uint32_t new_and = s->running_and & all_bits & (1u - already_halted);

    /* Bump chunk count unconditionally.  We ALWAYS extend the chain
     * — even post-halt — so that the hash-chain receipt lists every
     * chunk the stream ever saw.  This lets an external verifier
     * replay the whole sequence without needing to know where the
     * halt occurred. */
    ++s->chunk_count;
    uint32_t this_index = s->chunk_count - 1u;

    uint8_t new_head[COS_V82_DIGEST_BYTES];
    chain_step(new_head, s->head, masked, payload);
    memcpy(s->head, new_head, COS_V82_DIGEST_BYTES);

    s->running_and = new_and;

    if (new_and == 0u && s->halt_idx == UINT32_MAX) {
        s->halt_idx = this_index;
    }

    (void)already_halted;
    return new_and;
}

void cos_v82_stream_digest(const cos_v82_stream_t *s,
                           uint8_t out[COS_V82_DIGEST_BYTES])
{
    memcpy(out, s->head, COS_V82_DIGEST_BYTES);
}

uint32_t cos_v82_stream_verify(const uint8_t salt[COS_V82_SALT_BYTES],
                               const uint32_t *kernel_bits,
                               const cos_v82_payload_t *payloads,
                               size_t count,
                               const uint8_t expected_head[COS_V82_DIGEST_BYTES])
{
    uint8_t head[COS_V82_DIGEST_BYTES];

    /* Replay h_0 = SHAKE256(SALT). */
    cos_v81_xof_t x;
    cos_v81_shake256_init(&x);
    cos_v81_shake_absorb(&x, salt, COS_V82_SALT_BYTES);
    cos_v81_shake_finalize(&x);
    cos_v81_shake_squeeze(&x, head, COS_V82_DIGEST_BYTES);

    for (size_t i = 0; i < count; ++i) {
        uint8_t next[COS_V82_DIGEST_BYTES];
        chain_step(next, head, kernel_bits[i] & all21_mask(), &payloads[i]);
        memcpy(head, next, COS_V82_DIGEST_BYTES);
    }

    return (memcmp(head, expected_head, COS_V82_DIGEST_BYTES) == 0) ? 1u : 0u;
}

uint32_t cos_v82_ok(const cos_v82_stream_t *s)
{
    uint32_t sentinel_ok = (s->sentinel == COS_V82_SENTINEL) ? 1u : 0u;
    uint32_t no_violations = (s->invariant_violations == 0u) ? 1u : 0u;
    return sentinel_ok & no_violations;
}

uint32_t cos_v82_compose_decision(uint32_t v81_composed_ok, uint32_t v82_ok)
{
    return v81_composed_ok & v82_ok;
}
