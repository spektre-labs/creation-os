/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * KV cache + quantize micro-checks (v129 lab).
 */
#include "../src/inference/cos_kv_cache_limits.h"
#include "../src/inference/sigma_kv_cache.h"
#include "../src/inference/sigma_kv_quantize.h"
#include "../src/inference/sigma_sliding_window.h"
#include <stdio.h>
#include <string.h>

static int self_kv(void)
{
    sigma_kv_cache_t c;
    int32_t keys[COS_KV_SELFTEST_LAYERS * COS_KV_SELFTEST_MAX_LEN
                * COS_KV_SELFTEST_HEAD_DIM];
    int32_t vals[COS_KV_SELFTEST_LAYERS * COS_KV_SELFTEST_MAX_LEN
                * COS_KV_SELFTEST_HEAD_DIM];
    int32_t sg[COS_KV_SELFTEST_MAX_LEN];
    int32_t k0[COS_KV_SELFTEST_HEAD_DIM];
    int32_t pos;
    int t;
    int L;
    int i;

    sigma_kv_cache_init(&c, keys, vals, sg, 8, COS_KV_SELFTEST_LAYERS,
                       COS_KV_SELFTEST_HEAD_DIM);
    for (t = 0; t < 10; t++) {
        pos = sigma_kv_token_begin(&c);
        if (pos < 0)
            return 0;
        for (i = 0; i < COS_KV_SELFTEST_HEAD_DIM; i++)
            k0[i] = i;
        for (L = 0; L < COS_KV_SELFTEST_LAYERS; L++)
            sigma_kv_set_layer(&c, L, pos, k0, k0);
        sigma_kv_token_commit(&c, pos, (int32_t)(t * 7000));
    }
    if (c.cache_len != 8 || c.n_evicted <= 0)
        return 0;
    return 1;
}

static int self_quant(void)
{
    int32_t src[16];
    int32_t dst[16];
    uint8_t p4[16];
    uint8_t p2[16];
    int i;
    for (i = 0; i < 16; i++)
        src[i] = i * 1000;
    cos_kv_pack_row_q4(src, 16, p4, sizeof p4);
    cos_kv_unpack_row_q4(p4, 16, dst);
    cos_kv_pack_row_q2(src, 16, p2, sizeof p2);
    (void)dst;
    if ((size_t)cos_kv_q4_packed_bytes(16) + (size_t)cos_kv_q2_packed_bytes(16) < 8u)
        return 0;
    return 1;
}

static int self_window(void)
{
    cos_sigma_sliding_window_t sw;
    int32_t wlo;
    int32_t whi;
    sw.base_window = 4096;
    sw.min_window  = 512;
    sw.max_window  = 8192;
    wlo = cos_sigma_sliding_window_size(&sw, 60000);
    whi = cos_sigma_sliding_window_size(&sw, 5000);
    if (wlo >= whi)
        return 0;
    return 1;
}

int main(int argc, char **argv)
{
    (void)argv;
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        if (!self_kv())
            return 1;
        if (!self_quant())
            return 2;
        if (!self_window())
            return 3;
        fprintf(stderr, "cache_bench: self-test OK\n");
        return 0;
    }
    fprintf(stderr, "usage: cache_bench --self-test\n");
    return 2;
}
