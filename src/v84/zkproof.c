/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v84 — σ-ZKProof implementation.
 */

#include "zkproof.h"
#include "../v81/lattice.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static void hash_pair(cos_v84_digest_t *out,
                      const cos_v84_digest_t *prev,
                      uint32_t layer_idx,
                      const cos_v84_digest_t *layer_digest)
{
    uint8_t buf[COS_V84_DIGEST_BYTES + 4 + COS_V84_DIGEST_BYTES];
    memcpy(buf, prev->h, COS_V84_DIGEST_BYTES);
    buf[COS_V84_DIGEST_BYTES + 0] = (uint8_t)(layer_idx      & 0xFFu);
    buf[COS_V84_DIGEST_BYTES + 1] = (uint8_t)((layer_idx>>8) & 0xFFu);
    buf[COS_V84_DIGEST_BYTES + 2] = (uint8_t)((layer_idx>>16)& 0xFFu);
    buf[COS_V84_DIGEST_BYTES + 3] = (uint8_t)((layer_idx>>24)& 0xFFu);
    memcpy(buf + COS_V84_DIGEST_BYTES + 4, layer_digest->h,
           COS_V84_DIGEST_BYTES);

    cos_v81_xof_t x;
    cos_v81_shake256_init(&x);
    cos_v81_shake_absorb(&x, buf, sizeof(buf));
    cos_v81_shake_finalize(&x);
    cos_v81_shake_squeeze(&x, out->h, COS_V84_DIGEST_BYTES);
}

void cos_v84_proof_init(cos_v84_prover_t *p)
{
    memset(p, 0, sizeof(*p));
    static const uint8_t tag[8] = {'c','o','s','-','v','8','4','z'};
    cos_v81_xof_t x;
    cos_v81_shake256_init(&x);
    cos_v81_shake_absorb(&x, tag, sizeof(tag));
    cos_v81_shake_finalize(&x);
    cos_v81_shake_squeeze(&x, p->chain[0].h, COS_V84_DIGEST_BYTES);
    p->sentinel = COS_V84_SENTINEL;
}

uint32_t cos_v84_commit_layer(cos_v84_prover_t *p,
                              const cos_v84_digest_t *layer_digest)
{
    if (p->sentinel != COS_V84_SENTINEL) {
        ++p->invariant_violations;
        return 0u;
    }
    if (p->finalized) {
        ++p->invariant_violations;
        return 0u;
    }
    if (p->layer_count >= COS_V84_MAX_LAYERS) {
        ++p->invariant_violations;
        return 0u;
    }
    uint32_t idx = p->layer_count;
    p->layers[idx] = *layer_digest;
    hash_pair(&p->chain[idx + 1], &p->chain[idx], idx, layer_digest);
    ++p->layer_count;
    return 1u;
}

void cos_v84_finalize(cos_v84_prover_t *p, cos_v84_digest_t *root_out)
{
    p->finalized = 1u;
    *root_out = p->chain[p->layer_count];
}

uint32_t cos_v84_open(const cos_v84_prover_t *p,
                      uint32_t idx,
                      cos_v84_opening_t *out)
{
    if (p->sentinel != COS_V84_SENTINEL) return 0u;
    if (idx >= p->layer_count) return 0u;

    memset(out, 0, sizeof(*out));
    out->idx          = idx;
    out->layer_digest = p->layers[idx];
    out->chain_before = p->chain[idx];
    out->chain_after  = p->chain[idx + 1];
    out->root         = p->chain[p->layer_count];
    out->layer_count  = p->layer_count;

    /* Tail = chain[idx+2 .. layer_count] plus the layers used to
     * derive each.  To keep the verifier stateless we ship the
     * (layer_digest, chain_result) pairs so the verifier can replay. */
    uint32_t tail_len = 0;
    for (uint32_t k = idx + 1; k < p->layer_count; ++k) {
        /* Each tail entry is the layer digest at k (already committed). */
        out->tail[tail_len++] = p->layers[k];
    }
    out->tail_len = tail_len;
    return 1u;
}

uint32_t cos_v84_verify_open(const cos_v84_opening_t *o)
{
    if (o->idx >= o->layer_count) return 0u;

    /* Step 1: verify that chain_after = H(chain_before, idx, layer_digest). */
    cos_v84_digest_t recomputed;
    hash_pair(&recomputed, &o->chain_before, o->idx, &o->layer_digest);
    if (memcmp(recomputed.h, o->chain_after.h, COS_V84_DIGEST_BYTES) != 0) {
        return 0u;
    }

    /* Step 2: replay the tail to land on root. */
    cos_v84_digest_t running = o->chain_after;
    if (o->tail_len != o->layer_count - o->idx - 1u) return 0u;
    for (uint32_t k = 0; k < o->tail_len; ++k) {
        uint32_t layer_idx = o->idx + 1u + k;
        cos_v84_digest_t next;
        hash_pair(&next, &running, layer_idx, &o->tail[k]);
        running = next;
    }
    if (memcmp(running.h, o->root.h, COS_V84_DIGEST_BYTES) != 0) return 0u;
    return 1u;
}

uint32_t cos_v84_ok(const cos_v84_prover_t *p)
{
    uint32_t sentinel_ok = (p->sentinel == COS_V84_SENTINEL) ? 1u : 0u;
    uint32_t no_violations = (p->invariant_violations == 0u) ? 1u : 0u;
    return sentinel_ok & no_violations;
}

uint32_t cos_v84_compose_decision(uint32_t v83_composed_ok, uint32_t v84_ok)
{
    return v83_composed_ok & v84_ok;
}
