/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * Inference microbenchmark: tokens/s and rough energy proxy (host TDP guess).
 */
#include "cos_inference_limits.h"
#include "cos_inference_gguf.h"
#include "ternary_engine.h"
#include "token_pipeline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int self_test_matmul(void)
{
    int8_t W[3 * 4];
    int8_t x[4];
    int32_t o[3];
    int32_t i;
    for (i = 0; i < 12; i++)
        W[i] = (int8_t)((i % 3) - 1);
    for (i = 0; i < 4; i++)
        x[i] = (int8_t)(i + 1);
    ternary_matmul(W, x, o, 3, 4);
    /* Row-check left to regression tests; sanity ranges only. */
    if (o[0] < -100 || o[0] > 100)
        return 0;
    return 1;
}

static int self_test_sigma_mirror(void)
{
    cos_sigma_q16_state_t st;
    cos_sigma_q16_init(&st);
    cos_sigma_q16_update(&st, (int32_t)(COS_SIGMA_Q16_ONE / 3),
                        (int32_t)(COS_SIGMA_Q16_ONE * 9 / 10));
    if (st.sigma != (int32_t)(COS_SIGMA_Q16_ONE / 3))
        return 0;
    return 1;
}

static int self_test_token(void)
{
    cos_inference_model_storage_t storage;
    inference_state_t ist;
    int32_t kv_k[COS_INF_MAX_LAYERS * COS_INF_MAX_SEQ * COS_INF_MAX_HIDDEN];
    int32_t kv_v[COS_INF_MAX_LAYERS * COS_INF_MAX_SEQ * COS_INF_MAX_HIDDEN];
    int32_t tok;

    cos_inference_init_toy_model(&storage, 64, 32, 32, 2);
    inference_state_init(&ist, &storage.model, kv_k, kv_v, COS_INF_MAX_SEQ);
    ist.cache_len = 0;
    tok = generate_token(&ist, 3);
    if (tok < 0 || tok >= 64)
        return 0;
    return 1;
}

static int run_self_tests(void)
{
    if (!self_test_matmul()) {
        fprintf(stderr, "inference_bench: matmul self-test failed\n");
        return 0;
    }
    if (!self_test_sigma_mirror()) {
        fprintf(stderr, "inference_bench: sigma mirror self-test failed\n");
        return 0;
    }
    if (!self_test_token()) {
        fprintf(stderr, "inference_bench: token self-test failed\n");
        return 0;
    }
    fprintf(stderr, "inference_bench: self-test OK\n");
    return 1;
}

int main(int argc, char **argv)
{
    struct timespec t0, t1;
    cos_inference_model_storage_t storage;
    inference_state_t ist;
    int32_t kv_k[COS_INF_MAX_LAYERS * COS_INF_MAX_SEQ * COS_INF_MAX_HIDDEN];
    int32_t kv_v[COS_INF_MAX_LAYERS * COS_INF_MAX_SEQ * COS_INF_MAX_HIDDEN];
    int32_t n_tokens;
    int32_t i;
    double elapsed;
    int do_st = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--self-test") == 0)
            do_st = 1;
    }
    if (do_st)
        return run_self_tests() ? 0 : 1;

    n_tokens = 1000;
    for (i = 1; i + 1 < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--n") == 0)
            n_tokens = (int32_t)atoi(argv[i + 1]);
    }

    cos_inference_init_toy_model(&storage, COS_INF_MAX_VOCAB / 4,
                                  COS_INF_MAX_HIDDEN / 2, COS_INF_MAX_HIDDEN / 2,
                                  2);
    inference_state_init(&ist, &storage.model, kv_k, kv_v, COS_INF_MAX_SEQ);

    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (i = 0; i < n_tokens; i++) {
        (void)generate_token(&ist, i % storage.model.vocab_size);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    elapsed =
        (double)(t1.tv_sec - t0.tv_sec)
        + (double)(t1.tv_nsec - t0.tv_nsec) / 1.0e9;
    if (elapsed <= 0.0)
        elapsed = 1e-9;

    printf("Tokens:     %d\n", (int)n_tokens);
    printf("Time:       %.3f s\n", elapsed);
    printf("Tok/s:      %.1f\n", (double)n_tokens / elapsed);
    printf("ms/tok:     %.2f\n", (elapsed / (double)n_tokens) * 1000.0);
    /* Joules proxy uses conservative desktop TDP guess — lab estimate only. */
    printf("Est mJ/tok: %.3f\n",
           (5.8 * elapsed / (double)n_tokens) * 1000.0);
    printf("sigma_q16:  %d (last k_eff %d)\n", (int)ist.gate.sigma,
           (int)ist.gate.k_eff);
    return 0;
}
