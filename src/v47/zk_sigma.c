/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v47 ZK-σ stub — **not** a succinct argument system.
 *
 * What exists today:
 *   - `prove_sigma` computes σ with the verified float kernel and fills a
 *     deterministic 32-byte commitment digest over the public `(n, logits[])`
 *     bytes (privacy is **not** offered; this is an integrity / API scaffold).
 *   - `verify_sigma_proof` checks invariants + digest match **when the same
 *     logits vector is supplied inside the proof blob** (see layout below).
 *
 * What is still P-tier:
 *   - No SNARK/STARK circuit, no hiding property, no extractability proof.
 */
#include "zk_sigma.h"
#include "sigma_kernel_verified.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Packed witness fits in `proof[256]` (magic + u32 n + float[n]). */
#define ZK_V47_MAX_PACKED_FLOATS (((256 - 8) / (int)sizeof(float)) > 0 ? ((256 - 8) / (int)sizeof(float)) : 0)

/* FNV-1a 64 → 32-byte commitment (repeat pattern; not a security claim). */
static void v47_fnv_commit(const float *x, int n, uint8_t out32[32])
{
    const unsigned char *b = (const unsigned char *)x;
    size_t nbytes = (size_t)n * sizeof(float);
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < nbytes; i++) {
        h ^= (uint64_t)b[i];
        h *= 1099511628211ULL;
    }
    for (int k = 0; k < 4; k++) {
        uint64_t w = h ^ ((uint64_t)(unsigned)n << (k * 13));
        for (int j = 0; j < 8; j++) {
            out32[k * 8 + (size_t)j] = (uint8_t)(w >> (8 * j));
        }
    }
}

/*
 * Proof blob layout (stub):
 *   [0..3]   magic "COS\x47"
 *   [4..7]   int32 n (LE)
 *   [8..]    float logits[n] (packed; same machine endianness as prover)
 *   remainder zero-padded to 256 bytes (n bounded by lab)
 */
#define ZK_V47_MAGIC0 'C'
#define ZK_V47_MAGIC1 'O'
#define ZK_V47_MAGIC2 'S'
#define ZK_V47_MAGIC3 0x47

static int v47_proof_pack(const float *logits, int n, uint8_t proof[256])
{
    if (!proof || !logits || n <= 0 || n > V47_VERIFIED_MAX_N || n > ZK_V47_MAX_PACKED_FLOATS) {
        return 0;
    }
    memset(proof, 0, 256);
    proof[0] = (uint8_t)ZK_V47_MAGIC0;
    proof[1] = (uint8_t)ZK_V47_MAGIC1;
    proof[2] = (uint8_t)ZK_V47_MAGIC2;
    proof[3] = (uint8_t)ZK_V47_MAGIC3;
    uint32_t un = (uint32_t)n;
    proof[4] = (uint8_t)(un & 0xFFu);
    proof[5] = (uint8_t)((un >> 8) & 0xFFu);
    proof[6] = (uint8_t)((un >> 16) & 0xFFu);
    proof[7] = (uint8_t)((un >> 24) & 0xFFu);
    size_t need = 8 + (size_t)n * sizeof(float);
    if (need > 256) {
        return 0;
    }
    memcpy(proof + 8, logits, (size_t)n * sizeof(float));
    return 1;
}

static int v47_proof_unpack(const uint8_t proof[256], float *logits_out, int logits_cap, int *n_out)
{
    if (!proof || !logits_out || !n_out || logits_cap <= 0) {
        return 0;
    }
    if (!(proof[0] == ZK_V47_MAGIC0 && proof[1] == ZK_V47_MAGIC1 && proof[2] == ZK_V47_MAGIC2 && proof[3] == ZK_V47_MAGIC3)) {
        return 0;
    }
    uint32_t un = (uint32_t)proof[4] | ((uint32_t)proof[5] << 8) | ((uint32_t)proof[6] << 16) | ((uint32_t)proof[7] << 24);
    int n = (int)un;
    if (n <= 0 || n > logits_cap || n > V47_VERIFIED_MAX_N || n > ZK_V47_MAX_PACKED_FLOATS) {
        return 0;
    }
    if (8 + (size_t)n * sizeof(float) > 256) {
        return 0;
    }
    memcpy(logits_out, proof + 8, (size_t)n * sizeof(float));
    *n_out = n;
    return 1;
}

zk_sigma_proof_t prove_sigma(const float *logprobs, int n)
{
    zk_sigma_proof_t z = {0};
    if (!logprobs || n <= 0 || n > V47_VERIFIED_MAX_N) {
        return z;
    }
    v47_verified_dirichlet_decompose(logprobs, n, &z.sigma);
    v47_fnv_commit(logprobs, n, z.commitment);
    if (!v47_proof_pack(logprobs, n, z.proof)) {
        memset(&z, 0, sizeof(z));
        return z;
    }
    z.verified = 1;
    return z;
}

int verify_sigma_proof(zk_sigma_proof_t *proof)
{
    if (!proof || !proof->verified) {
        return 0;
    }
    float logits[ZK_V47_MAX_PACKED_FLOATS];
    int n = 0;
    if (!v47_proof_unpack(proof->proof, logits, ZK_V47_MAX_PACKED_FLOATS, &n)) {
        return 0;
    }
    uint8_t digest[32];
    v47_fnv_commit(logits, n, digest);
    if (memcmp(digest, proof->commitment, sizeof digest) != 0) {
        return 0;
    }
    sigma_decomposed_t chk = {0};
    v47_verified_dirichlet_decompose(logits, n, &chk);
    if (!isfinite(chk.total) || !isfinite(chk.aleatoric) || !isfinite(chk.epistemic)) {
        return 0;
    }
    if (fabsf(chk.total - (chk.aleatoric + chk.epistemic)) > 1e-3f) {
        return 0;
    }
    if (fabsf(chk.total - proof->sigma.total) > 1e-3f || fabsf(chk.aleatoric - proof->sigma.aleatoric) > 1e-3f
        || fabsf(chk.epistemic - proof->sigma.epistemic) > 1e-3f) {
        return 0;
    }
    return 1;
}
