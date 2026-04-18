/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v101 σ-BitNet-Bridge — stub mode.
 *
 * Compiled when COS_V101_BITNET_REAL is **not** defined.  Every call that
 * would require a loaded GGUF returns COS_V101_ERR_NO_MODEL (except for
 * `cos_v101_bridge_open`, which returns NULL and prints a one-line note).
 * The σ-channel math is still exercised by `cos_v101_self_test()` — same
 * deterministic numerical answer in both modes, so the CI merge-gate is
 * guaranteed to stay green on hosts without the 1.1 GB BitNet checkpoint.
 */
#include "bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct cos_v101_bridge {
    int placeholder;
};

cos_v101_bridge_t *cos_v101_bridge_open(const char *gguf_path, uint32_t n_ctx)
{
    (void)gguf_path;
    (void)n_ctx;
    fprintf(stderr,
            "cos_v101: stub mode (rebuild with COS_V101_BITNET_REAL=1 and "
            "-Lthird_party/bitnet/build/3rdparty/llama.cpp/src -lllama to "
            "link the real backend)\n");
    return NULL;
}

void cos_v101_bridge_close(cos_v101_bridge_t *b)
{
    (void)b;
}

int cos_v101_bridge_n_vocab(const cos_v101_bridge_t *b)
{
    (void)b;
    return 0;
}

int cos_v101_bridge_tokenize(cos_v101_bridge_t *b, const char *text,
                             int add_special,
                             int32_t *tokens, int max_tokens)
{
    (void)b; (void)text; (void)add_special; (void)tokens; (void)max_tokens;
    return COS_V101_ERR_NO_MODEL;
}

int cos_v101_bridge_detokenize(cos_v101_bridge_t *b,
                               const int32_t *tokens, int n_tokens,
                               char *buf, int max_buf)
{
    (void)b; (void)tokens; (void)n_tokens; (void)buf; (void)max_buf;
    return COS_V101_ERR_NO_MODEL;
}

int cos_v101_bridge_loglikelihood(cos_v101_bridge_t *b,
                                  const char *ctx, const char *cont,
                                  double *ll, int *is_greedy,
                                  int *n_ctx_tokens, int *n_cont_tokens,
                                  float *out_sigma_mean)
{
    (void)b; (void)ctx; (void)cont;
    (void)ll; (void)is_greedy; (void)n_ctx_tokens; (void)n_cont_tokens;
    (void)out_sigma_mean;
    return COS_V101_ERR_NO_MODEL;
}

int cos_v101_bridge_loglikelihood_ex(cos_v101_bridge_t *b,
                                     const char *ctx, const char *cont,
                                     double *ll, int *is_greedy,
                                     int *n_ctx_tokens, int *n_cont_tokens,
                                     float *out_sigma_mean,
                                     float *out_sigma_profile,
                                     float *out_sigma_max_token)
{
    (void)b; (void)ctx; (void)cont;
    (void)ll; (void)is_greedy; (void)n_ctx_tokens; (void)n_cont_tokens;
    (void)out_sigma_mean; (void)out_sigma_profile; (void)out_sigma_max_token;
    return COS_V101_ERR_NO_MODEL;
}

int cos_v101_bridge_loglikelihood_v105(cos_v101_bridge_t *b,
                                       const char *ctx, const char *cont,
                                       double *ll, int *is_greedy,
                                       int *n_ctx_tokens, int *n_cont_tokens,
                                       float *out_sigma_mean,
                                       float *out_sigma_profile,
                                       float *out_sigma_max_token,
                                       float *out_sigma_product)
{
    (void)b; (void)ctx; (void)cont;
    (void)ll; (void)is_greedy; (void)n_ctx_tokens; (void)n_cont_tokens;
    (void)out_sigma_mean; (void)out_sigma_profile; (void)out_sigma_max_token;
    (void)out_sigma_product;
    return COS_V101_ERR_NO_MODEL;
}

int cos_v101_bridge_generate(cos_v101_bridge_t *b,
                             const char *ctx,
                             const char *const *until, int n_until,
                             int max_tokens,
                             float sigma_threshold,
                             char *buf, int max_buf,
                             float *out_sigma_profile,
                             int *out_abstained)
{
    (void)b; (void)ctx; (void)until; (void)n_until;
    (void)max_tokens; (void)sigma_threshold;
    (void)buf; (void)max_buf; (void)out_sigma_profile; (void)out_abstained;
    return COS_V101_ERR_NO_MODEL;
}
