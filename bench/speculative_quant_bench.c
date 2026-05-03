/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#include "../src/inference/cos_inference_gguf.h"
#include "../src/inference/cos_inference_limits.h"
#include "../src/inference/sigma_quantize.h"
#include "../src/inference/sigma_speculative.h"
#include "../src/inference/token_pipeline.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    cos_inference_model_storage_t storage;
    inference_state_t draft;
    inference_state_t target;
    int32_t kv_d[COS_INF_MAX_LAYERS * COS_INF_MAX_SEQ * COS_INF_MAX_HIDDEN];
    int32_t kv_D[COS_INF_MAX_LAYERS * COS_INF_MAX_SEQ * COS_INF_MAX_HIDDEN];
    int32_t kv_t[COS_INF_MAX_LAYERS * COS_INF_MAX_SEQ * COS_INF_MAX_HIDDEN];
    int32_t kv_T[COS_INF_MAX_LAYERS * COS_INF_MAX_SEQ * COS_INF_MAX_HIDDEN];
    sigma_speculative_t sp;
    int32_t draft_tok[8];
    int32_t acc[8];
    int32_t n;
    int32_t qplane[64];
    const int32_t *pp[1];
    int32_t sz[1];
    sigma_quant_config_t qcfg;
    int i;

    if (argc < 2 || strcmp(argv[1], "--self-test") != 0) {
        fprintf(stderr, "usage: speculative_quant_bench --self-test\n");
        return 2;
    }

    cos_inference_init_toy_model(&storage, 64, 32, 32, 2);
    inference_state_init(&draft, &storage.model, kv_d, kv_D, COS_INF_MAX_SEQ);
    inference_state_init(&target, &storage.model, kv_t, kv_T, COS_INF_MAX_SEQ);
    sigma_speculative_init(&sp, &draft, &target, 3, 50000);
    n = sigma_speculative_step(&sp, 3, draft_tok, 8, acc, 8);
    if (n < 1)
        return 1;
    if (sigma_adaptive_draft_len_q16(&sp, 4) < 2)
        return 2;

    for (i = 0; i < 64; i++)
        qplane[i] = (i - 32) * 1000;
    pp[0] = qplane;
    sz[0] = 64;
    sigma_quant_auto_layers(&qcfg, pp, sz, 1, 12000);
    if (qcfg.layer_bits[0] < 2)
        return 3;

    fprintf(stderr, "speculative_quant_bench: OK\n");
    return 0;
}
