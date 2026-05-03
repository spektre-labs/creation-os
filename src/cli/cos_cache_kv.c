/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#include "../inference/semantic_cache_bsc.h"
#include "../inference/sigma_kv_cache.h"
#include "cos_cache_kv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COS_KV_DEMO_LAYERS   8
#define COS_KV_DEMO_HEAD     64
#define COS_KV_DEMO_MAX_LEN  128

static int32_t threshold_q16_from_string(const char *s)
{
    char *end = NULL;
    double x;
    if (!s || !s[0])
        return 0;
    x = strtod(s, &end);
    (void)end;
    if (x < 0.0)
        x = 0.0;
    if (x > 1.0)
        x = 1.0;
    return (int32_t)(x * 65536.0 + 0.5);
}

static int cmd_kv_stats(void)
{
    sigma_kv_cache_t c;
    int32_t keys[COS_KV_DEMO_LAYERS * COS_KV_DEMO_MAX_LEN * COS_KV_DEMO_HEAD];
    int32_t vals[COS_KV_DEMO_LAYERS * COS_KV_DEMO_MAX_LEN * COS_KV_DEMO_HEAD];
    int32_t sig[COS_KV_DEMO_MAX_LEN];
    int32_t i;
    int32_t L;
    int32_t pos;
    int32_t row[COS_KV_DEMO_HEAD];

    sigma_kv_cache_init(&c, keys, vals, sig, COS_KV_DEMO_MAX_LEN,
                       COS_KV_DEMO_LAYERS, COS_KV_DEMO_HEAD);
    for (i = 0; i < COS_KV_DEMO_HEAD; i++)
        row[i] = i - COS_KV_DEMO_HEAD / 2;
    for (i = 0; i < 130; i++) {
        int32_t sigma_q = (i * 500) % 62000;
        pos = sigma_kv_token_begin(&c);
        if (pos < 0)
            break;
        for (L = 0; L < COS_KV_DEMO_LAYERS; L++)
            sigma_kv_set_layer(&c, L, pos, row, row);
        sigma_kv_token_commit(&c, pos, sigma_q);
    }
    printf("sigma_kv (demo, synthetic load)\n"
           "  cache_len : %d (max %d)\n"
           "  evicted   : %d\n"
           "  retention : %d / 65536 slot fill vs max_len\n",
           (int)c.cache_len, (int)c.max_len, (int)c.n_evicted,
           (int)sigma_kv_slot_retention_q16(&c));
    return 0;
}

static int cmd_kv_prune(const char *thresh_str)
{
    sigma_kv_cache_t c;
    int32_t keys[COS_KV_DEMO_LAYERS * COS_KV_DEMO_MAX_LEN * COS_KV_DEMO_HEAD];
    int32_t vals[COS_KV_DEMO_LAYERS * COS_KV_DEMO_MAX_LEN * COS_KV_DEMO_HEAD];
    int32_t sg[COS_KV_DEMO_MAX_LEN];
    int32_t thr;
    int32_t n;
    int32_t i;
    int32_t pos;
    int32_t L;
    int32_t row[COS_KV_DEMO_HEAD];

    thr = threshold_q16_from_string(thresh_str);
    sigma_kv_cache_init(&c, keys, vals, sg, COS_KV_DEMO_MAX_LEN,
                       COS_KV_DEMO_LAYERS, COS_KV_DEMO_HEAD);
    for (i = 0; i < COS_KV_DEMO_HEAD; i++)
        row[i] = 1;
    for (i = 0; i < 96; i++) {
        pos = sigma_kv_token_begin(&c);
        if (pos < 0)
            break;
        for (L = 0; L < COS_KV_DEMO_LAYERS; L++)
            sigma_kv_set_layer(&c, L, pos, row, row);
        sigma_kv_token_commit(&c, pos, (i * 901) % 65535);
    }
    n = sigma_kv_prune_above(&c, thr);
    printf("sigma_kv prune demo: threshold_q16=%d pruned=%d remaining=%d\n",
           (int)thr, (int)n, (int)c.cache_len);
    return 0;
}

static int cmd_semantic_bsc_stats(void)
{
    cos_semantic_cache_t sc;
    cos_semantic_cache_entry_t e;
    uint64_t h1[COS_SEM_BSC_WORDS];
    uint64_t h2[COS_SEM_BSC_WORDS];
    int idx;
    int i;
    cos_semantic_cache_init(&sc);
    for (i = 0; i < COS_SEM_BSC_WORDS; i++) {
        h1[i] = (uint64_t)(i * 0x9e3779b97f4a7c15ULL);
        h2[i] = h1[i] ^ (1ULL << (i & 63));
    }
    memset(&e, 0, sizeof e);
    memcpy(e.prompt_hash, h1, sizeof h1);
    e.sigma_q16      = 40000;
    e.verdict        = 0;
    e.response_token = 42;
    cos_semantic_cache_insert(&sc, &e);
    (void)cos_semantic_cache_lookup(&sc, h1, 60000, &idx);
    (void)cos_semantic_cache_lookup(&sc, h2, 65000, &idx);
    printf("semantic_bsc cache (in-process demo)\n"
           "  entries : %d\n"
           "  hits    : %d\n"
           "  misses  : %d\n",
           (int)sc.count, (int)sc.hits, (int)sc.misses);
    if (sc.hits + sc.misses > 0)
        printf("  hit_rate: %.2f%% (demo only)\n",
               (double)sc.hits * 100.0 / (double)(sc.hits + sc.misses));
    return 0;
}

static int cmd_kv_bench(const char *ctxlist)
{
    const char *p;
    int32_t ctx;
    int32_t layers = 32;
    int32_t heads = 32;
    int32_t head_dim = 128;
    int64_t bytes_std;
    int64_t bytes_pack_est;
    printf("sigma_kv synthetic byte budget (NOT measured silicon; lab model)\n"
           "  assumes %d layers, %d heads, head_dim %d, fp16 KV baseline ref\n",
           (int)layers, (int)heads, (int)head_dim);
    printf("  context_tokens | KV_bytes_fp16_model | σ-quant_policy_bytes_est\n");
    p = ctxlist;
    while (p && p[0]) {
        ctx = (int32_t)strtol(p, NULL, 10);
        if (ctx <= 0)
            break;
        bytes_std = (int64_t)layers * (int64_t)ctx * (int64_t)heads
            * (int64_t)head_dim * 2LL * 2LL;
        bytes_pack_est = (bytes_std * 7) / 10 / 4 + (bytes_std * 3) / 10 / 16;
        printf("  %15d | %18lld | %27lld\n", (int)ctx, (long long)bytes_std,
               (long long)bytes_pack_est);
        p = strchr(p, ',');
        if (p)
            p++;
        else
            break;
    }
    printf("  See docs for claim discipline: do not merge microbench rows with harness scores.\n");
    return 0;
}

int cos_cache_kv_dispatch(int argc, char **argv)
{
    int i;
    const char *thresh = NULL;
    const char *ctx    = NULL;
    int want_stats = 0;
    int want_sem   = 0;
    int want_prune = 0;
    int want_bench = 0;

    if (argc <= 0 || argv == NULL)
        return 1;
    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--stats") == 0)
            want_stats = 1;
        else if (strcmp(argv[i], "--semantic") == 0)
            want_sem = 1;
        else if (strcmp(argv[i], "--prune") == 0)
            want_prune = 1;
        else if (strcmp(argv[i], "--bench") == 0)
            want_bench = 1;
        else if (i + 1 < argc && strcmp(argv[i], "--threshold") == 0)
            thresh = argv[++i];
        else if (i + 1 < argc && strcmp(argv[i], "--context") == 0)
            ctx = argv[++i];
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("cos cache v129 flags (see also default cos-cache behaviour)\n"
                   "  --stats              σ-KV demo statistics\n"
                   "  --prune --threshold 0..1   demo bulk interior prune\n"
                   "  --semantic --stats   BSC hash cache hit/miss demo\n"
                   "  --bench --context N[,N...] synthetic KV byte model\n");
            return 0;
        }
    }
    if (want_sem)
        return cmd_semantic_bsc_stats() == 0 ? 0 : 2;
    if (want_stats && !want_sem && !want_prune && !want_bench)
        return cmd_kv_stats();
    if (want_prune) {
        if (!thresh)
            thresh = "0.5";
        return cmd_kv_prune(thresh);
    }
    if (want_bench) {
        if (!ctx || !ctx[0])
            ctx = "4096,16384,65536,131072";
        return cmd_kv_bench(ctx);
    }
    return 1;
}
