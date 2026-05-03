/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * cos inference — ternary toy path + bench flags (no llama.cpp).
 */
#include "cos_inference_limits.h"
#include "cos_inference_gguf.h"
#include "sigma_speculative.h"
#include "token_pipeline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int path_executable(const char *p)
{
    return p && p[0] && access(p, X_OK) == 0;
}

static int try_llama_bench_line(double *tok_s_out)
{
    const char *candidates[] = {
        "llama-bench", "llama.cpp/build/bin/llama-bench", "./llama-bench", NULL
    };
    int i;
    (void)tok_s_out;
    for (i = 0; candidates[i]; i++) {
        if (path_executable(candidates[i]))
            return 1;
    }
    return 0;
}

int cos_inference_main(int argc, char **argv)
{
    int bench      = 0;
    int compare    = 0;
    int speculate  = 0;
    int adaptive   = 0;
    int draft_len  = 3;
    const char *draft_path  = NULL;
    const char *target_path = NULL;
    const char *model_path  = NULL;
    const char *prompt      = NULL;
    int n_tok               = 16;
    cos_inference_model_storage_t storage;
    inference_state_t ist;
    int32_t kv_k[COS_INF_MAX_LAYERS * COS_INF_MAX_SEQ * COS_INF_MAX_HIDDEN];
    int32_t kv_v[COS_INF_MAX_LAYERS * COS_INF_MAX_SEQ * COS_INF_MAX_HIDDEN];
    int i;
    struct timespec t0, t1;
    double elapsed;
    int tok0;

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--bench") == 0)
            bench = 1;
        else if (strcmp(argv[i], "--compare") == 0)
            compare = 1;
        else if (strcmp(argv[i], "--speculative") == 0)
            speculate = 1;
        else if (strcmp(argv[i], "--adaptive") == 0)
            adaptive = 1;
        else if (i + 1 < argc && strcmp(argv[i], "--draft-len") == 0)
            draft_len = atoi(argv[++i]);
        else if (i + 1 < argc && strcmp(argv[i], "--draft") == 0)
            draft_path = argv[++i];
        else if (i + 1 < argc && strcmp(argv[i], "--target") == 0)
            target_path = argv[++i];
        else if (i + 1 < argc && strcmp(argv[i], "--model") == 0)
            model_path = argv[++i];
        else if (i + 1 < argc && strcmp(argv[i], "--prompt") == 0)
            prompt = argv[++i];
        else if (i + 1 < argc
                 && (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--n") == 0))
            n_tok = atoi(argv[++i]);
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("cos inference [--bench] [--speculative] [--adaptive] "
                   "[--draft-len N] [--draft PATH] [--target PATH] "
                   "[--model PATH] [--prompt STR] [-n N]\n");
            return 0;
        }
    }

    if (model_path && cos_gguf_load_model(model_path, &storage.model) == 0) {
        fprintf(stderr,
                "cos inference: GGUF path loaded (unexpected; stub returns "
                "error).\n");
    } else if (model_path) {
        fprintf(stderr,
                "cos inference: GGUF full loader not implemented for '%s' — "
                "toy weights.\n",
                model_path);
    }

    if (draft_path)
        fprintf(stderr,
                "cos inference: --draft '%s' (paths not loaded; toy shared "
                "weights for KV policy demo).\n",
                draft_path);
    if (target_path)
        fprintf(stderr,
                "cos inference: --target '%s' (paths not loaded; toy shared "
                "weights for KV policy demo).\n",
                target_path);

    cos_inference_init_toy_model(&storage, 128, 48, 48, 2);
    inference_state_init(&ist, &storage.model, kv_k, kv_v, COS_INF_MAX_SEQ);

    if (speculate) {
        inference_state_t d_st;
        inference_state_t t_st;
        int32_t          kv_dk[COS_INF_MAX_LAYERS * COS_INF_MAX_SEQ
                        * COS_INF_MAX_HIDDEN];
        int32_t          kv_dv[COS_INF_MAX_LAYERS * COS_INF_MAX_SEQ
                        * COS_INF_MAX_HIDDEN];
        int32_t          kv_tk[COS_INF_MAX_LAYERS * COS_INF_MAX_SEQ
                        * COS_INF_MAX_HIDDEN];
        int32_t          kv_tv[COS_INF_MAX_LAYERS * COS_INF_MAX_SEQ
                        * COS_INF_MAX_HIDDEN];
        sigma_speculative_t sp;
        int32_t draft_buf[8];
        int32_t acc[8];
        int32_t n;
        int32_t dl;
        int32_t produced;
        int32_t skip_tau;

        inference_state_init(&d_st, &storage.model, kv_dk, kv_dv,
                            COS_INF_MAX_SEQ);
        inference_state_init(&t_st, &storage.model, kv_tk, kv_tv,
                            COS_INF_MAX_SEQ);
        dl = draft_len;
        if (dl < 1)
            dl = 1;
        if (dl > 8)
            dl = 8;
        skip_tau = (int32_t)((6 * COS_SIGMA_Q16_ONE) / 10);
        sigma_speculative_init(&sp, &d_st, &t_st, dl, skip_tau);
        tok0 = 7;
        if (prompt && prompt[0])
            tok0 = (int)(unsigned char)prompt[0] % storage.model.vocab_size;
        produced = 0;
        while (produced < n_tok) {
            int j;
            n = sigma_speculative_step(&sp, tok0, draft_buf, 8, acc, 8);
            if (n <= 0)
                break;
            for (j = 0; j < n; j++) {
                printf("spec accepted[%d] id=%d\n", produced + j, (int)acc[j]);
            }
            tok0 = acc[n - 1];
            produced += n;
            if (adaptive) {
                dl = sigma_adaptive_draft_len_q16(&sp, draft_len);
                sp.draft_len = dl;
            }
        }
        printf("speculative: target_skipped=%u target_called=%u "
               "(lab counters; not a throughput claim)\n",
               (unsigned)sp.target_skipped, (unsigned)sp.target_called);
        return 0;
    }

    if (compare) {
        double dummy = 0.0;
        if (!try_llama_bench_line(&dummy))
            fprintf(stderr,
                    "cos inference --compare: SKIP (no llama-bench on PATH).\n");
        else
            fprintf(stderr,
                    "cos inference --compare: found llama-bench — "
                    "wire JSON parse for tok/s when needed.\n");
    }

    if (bench) {
        if (n_tok < 1)
            n_tok = 1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (i = 0; i < n_tok; i++)
            (void)generate_token(&ist, i % storage.model.vocab_size);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        elapsed = (double)(t1.tv_sec - t0.tv_sec)
                + (double)(t1.tv_nsec - t0.tv_nsec) / 1.0e9;
        if (elapsed <= 0.0)
            elapsed = 1e-9;
        printf("inference bench  tokens=%d  tok/s=%.1f  ms/tok=%.3f\n", n_tok,
               (double)n_tok / elapsed, (elapsed / (double)n_tok) * 1000.0);
        printf("sigma (Q16.16)   last_sigma=%d  k_eff=%d\n",
               (int)ist.gate.sigma, (int)ist.gate.k_eff);
        printf("Est mJ/tok (5.8W guess): %.4f\n",
               (5.8 * elapsed / (double)n_tok) * 1000.0);
        return 0;
    }

    tok0 = 7;
    if (prompt && prompt[0])
        tok0 = (int)(unsigned char)prompt[0] % storage.model.vocab_size;
    for (i = 0; i < n_tok; i++) {
        int32_t t = generate_token(&ist, tok0);
        printf("token[%d] id=%d  sigma_q16=%d  k_eff=%d\n", i, (int)t,
               (int)ist.gate.sigma, (int)ist.gate.k_eff);
        tok0 = (int)t;
    }
    return 0;
}
