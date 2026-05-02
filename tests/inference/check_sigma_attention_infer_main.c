/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#include "../../src/inference/sigma_attention.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    cos_infer_sigma_attention_t sa;
    int32_t                      sig[8];
    int32_t                      Q[8 * 4];
    int32_t                      K[8 * 4];
    int32_t                      V[8 * 4];
    int32_t                      out[8 * 4];
    int32_t                      kv[8 * (4 * 2)];
    int32_t                      ks;
    int                          i;
    int64_t                      ops_s;
    int64_t                      ops_d;

    for (i = 0; i < 8; i++) {
        sig[i] = (i < 3) ? (COS_INFER_ATTN_Q16 / 10) : (COS_INFER_ATTN_Q16 * 3 / 4);
    }
    memset(Q, 0, sizeof(Q));
    memset(K, 0, sizeof(K));
    memset(V, 0, sizeof(V));
    for (i = 0; i < 8 * 4; i++) {
        Q[i] = 1;
        K[i] = 1 + (i % 3);
        V[i] = i % 7;
    }

    for (i = 0; i < 8; i++) {
        memcpy(kv + i * 8, K + i * 4, (size_t)4 * sizeof(int32_t));
        memcpy(kv + i * 8 + 4, V + i * 4, (size_t)4 * sizeof(int32_t));
    }

    cos_infer_sigma_attention_init(&sa, 8, 4, 1, sig, COS_INFER_ATTN_Q16 / 2);
    cos_infer_sigma_sparse_attention(&sa, Q, K, V, out, 2);

    if (sa.tokens_full != 3U || sa.tokens_pruned != 5U) {
        fprintf(stderr, "sigma_attention: expected 3 full / 5 pruned, got %u / %u\n",
                (unsigned)sa.tokens_full, (unsigned)sa.tokens_pruned);
        return 1;
    }

    ks = cos_infer_sigma_kv_prune(&sa, kv, sig, 8, 4, 6);
    if (ks != 7) {
        fprintf(stderr, "sigma_attention: kv_prune expected len 7, got %d\n", (int)ks);
        return 2;
    }

    cos_infer_sigma_attention_ops_estimate(&sa, 8, 2, 4, &ops_s, &ops_d);
    if (ops_s <= 0 || ops_d <= 0 || ops_s > ops_d) {
        fprintf(stderr, "sigma_attention: op accounting inconsistent\n");
        return 3;
    }

    puts("check_sigma_attention_infer_main: OK");
    return 0;
}
