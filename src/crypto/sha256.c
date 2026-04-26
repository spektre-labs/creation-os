/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "crypto/sha256.h"

#include <string.h>

static const uint32_t K256[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

static inline uint32_t rotr32(uint32_t x, unsigned n)
{
    return (x >> n) | (x << (32u - n));
}

static void sha256_compress(uint32_t state[8], const uint8_t block[64])
{
    uint32_t W[64];
    int      i;
    for (i = 0; i < 16; ++i) {
        W[i] = ((uint32_t)block[i * 4 + 0] << 24) | ((uint32_t)block[i * 4 + 1] << 16)
             | ((uint32_t)block[i * 4 + 2] << 8) | ((uint32_t)block[i * 4 + 3]);
    }
    for (i = 16; i < 64; ++i) {
        uint32_t s0 = rotr32(W[i - 15], 7) ^ rotr32(W[i - 15], 18) ^ (W[i - 15] >> 3);
        uint32_t s1 = rotr32(W[i - 2], 17) ^ rotr32(W[i - 2], 19) ^ (W[i - 2] >> 10);
        W[i]        = W[i - 16] + s0 + W[i - 7] + s1;
    }
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    for (i = 0; i < 64; ++i) {
        uint32_t S1  = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch  = (e & f) ^ ((~e) & g);
        uint32_t T1  = h + S1 + ch + K256[i] + W[i];
        uint32_t S0  = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t mj  = (a & b) ^ (a & c) ^ (b & c);
        uint32_t T2  = S0 + mj;
        h              = g;
        g              = f;
        f              = e;
        e              = d + T1;
        d              = c;
        c              = b;
        b              = a;
        a              = T1 + T2;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void cos_sha256_init(cos_sha256_ctx_t *ctx)
{
    ctx->h[0]         = 0x6a09e667u;
    ctx->h[1]         = 0xbb67ae85u;
    ctx->h[2]         = 0x3c6ef372u;
    ctx->h[3]         = 0xa54ff53au;
    ctx->h[4]         = 0x510e527fu;
    ctx->h[5]         = 0x9b05688cu;
    ctx->h[6]         = 0x1f83d9abu;
    ctx->h[7]         = 0x5be0cd19u;
    ctx->total_bits   = 0;
    ctx->buf_len      = 0;
}

void cos_sha256_update(cos_sha256_ctx_t *ctx, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    ctx->total_bits += (uint64_t)len * 8u;
    if (ctx->buf_len) {
        size_t need = 64u - ctx->buf_len;
        if (len < need) {
            memcpy(ctx->buf + ctx->buf_len, p, len);
            ctx->buf_len += len;
            return;
        }
        memcpy(ctx->buf + ctx->buf_len, p, need);
        sha256_compress(ctx->h, ctx->buf);
        p += need;
        len -= need;
        ctx->buf_len = 0;
    }
    while (len >= 64u) {
        sha256_compress(ctx->h, p);
        p += 64;
        len -= 64u;
    }
    if (len > 0u) {
        memcpy(ctx->buf, p, len);
        ctx->buf_len = len;
    }
}

void cos_sha256_final(cos_sha256_ctx_t *ctx, uint8_t out[32])
{
    uint64_t bits = ctx->total_bits;
    uint8_t  pad[64];
    size_t   pad_n;
    memset(pad, 0, sizeof pad);
    pad[0] = 0x80;
    pad_n  = (ctx->buf_len < 56u) ? (56u - ctx->buf_len) : (120u - ctx->buf_len);
    cos_sha256_update(ctx, pad, pad_n);
    /* Length field counts only the original message bits. */
    ctx->total_bits = bits;
    {
        uint8_t lenbe[8];
        int     i;
        for (i = 0; i < 8; ++i)
            lenbe[i] = (uint8_t)(bits >> (56 - 8 * i));
        cos_sha256_update(ctx, lenbe, 8u);
    }
    for (int i = 0; i < 8; ++i) {
        out[i * 4 + 0] = (uint8_t)(ctx->h[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(ctx->h[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(ctx->h[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(ctx->h[i]);
    }
}

void cos_sha256(const uint8_t *data, size_t len, uint8_t out[32])
{
    cos_sha256_ctx_t ctx;
    cos_sha256_init(&ctx);
    cos_sha256_update(&ctx, data, len);
    cos_sha256_final(&ctx, out);
}

void cos_sha256_hex(const uint8_t *data, size_t len, char out[65])
{
    uint8_t                 bin[32];
    static const char       H[16] = "0123456789abcdef";
    cos_sha256(data, len, bin);
    for (int i = 0; i < 32; ++i) {
        out[i * 2 + 0] = H[(bin[i] >> 4) & 0xF];
        out[i * 2 + 1] = H[bin[i] & 0xF];
    }
    out[64] = '\0';
}
