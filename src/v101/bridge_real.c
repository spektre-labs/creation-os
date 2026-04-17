/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v101 σ-BitNet-Bridge — real mode.
 *
 * Compiled when COS_V101_BITNET_REAL is defined.  Links against bitnet.cpp's
 * fork of llama.cpp (located at third_party/bitnet/3rdparty/llama.cpp).  The
 * bitnet.cpp fork ships i2_s / TL1 / TL2 kernels that execute the BitNet
 * b1.58 ternary weight format natively at ~44 tok/s on Apple M4 with the
 * Metal backend.  Everything below is a thin wrapper: σ-channels are the
 * only Creation OS-specific logic on the forward path.
 *
 * Build:
 *   clang -DCOS_V101_BITNET_REAL=1 \
 *     -Ithird_party/bitnet/3rdparty/llama.cpp/include \
 *     -Ithird_party/bitnet/3rdparty/llama.cpp/ggml/include \
 *     -Lthird_party/bitnet/build/3rdparty/llama.cpp/src -lllama \
 *     -Lthird_party/bitnet/build/3rdparty/llama.cpp/ggml/src -lggml \
 *     src/v101/bridge_real.c src/v101/sigma_channels.c
 *
 * See Makefile target `check-v101-real` for the canonical invocation.
 */
#include "bridge.h"
#include "llama.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct cos_v101_bridge {
    struct llama_model   *model;
    struct llama_context *ctx;
    int                   n_vocab;
    uint32_t              n_ctx;
};

static int init_backend_once(void)
{
    static int done = 0;
    if (done) return 0;
    llama_backend_init();
    done = 1;
    return 0;
}

cos_v101_bridge_t *cos_v101_bridge_open(const char *gguf_path, uint32_t n_ctx)
{
    if (!gguf_path) return NULL;
    init_backend_once();

    struct llama_model_params mparams = llama_model_default_params();
    /* On Apple Silicon the Metal backend is enabled when we offload at least
     * one layer; bitnet.cpp forces offloading for the BitNet kernels to be
     * used when the weight format is i2_s.
     */
    mparams.n_gpu_layers = 999;
    mparams.use_mmap     = true;

    struct llama_model *model = llama_load_model_from_file(gguf_path, mparams);
    if (!model) {
        fprintf(stderr, "cos_v101: llama_load_model_from_file failed for %s\n", gguf_path);
        return NULL;
    }

    struct llama_context_params cparams = llama_context_default_params();
    if (n_ctx > 0) cparams.n_ctx = n_ctx;
    cparams.n_batch         = 2048;
    cparams.n_threads       = 4;
    cparams.n_threads_batch = 4;
    cparams.logits_all      = false;
    cparams.no_perf         = true;

    struct llama_context *ctx = llama_new_context_with_model(model, cparams);
    if (!ctx) {
        fprintf(stderr, "cos_v101: llama_new_context_with_model failed\n");
        llama_free_model(model);
        return NULL;
    }

    cos_v101_bridge_t *b = (cos_v101_bridge_t *)calloc(1, sizeof(*b));
    if (!b) {
        llama_free(ctx);
        llama_free_model(model);
        return NULL;
    }
    b->model   = model;
    b->ctx     = ctx;
    b->n_vocab = llama_n_vocab(model);
    b->n_ctx   = (uint32_t)llama_n_ctx(ctx);
    return b;
}

void cos_v101_bridge_close(cos_v101_bridge_t *b)
{
    if (!b) return;
    if (b->ctx)   llama_free(b->ctx);
    if (b->model) llama_free_model(b->model);
    free(b);
}

int cos_v101_bridge_n_vocab(const cos_v101_bridge_t *b)
{
    return b ? b->n_vocab : 0;
}

int cos_v101_bridge_tokenize(cos_v101_bridge_t *b, const char *text,
                             int add_special,
                             int32_t *tokens, int max_tokens)
{
    if (!b || !text || !tokens || max_tokens <= 0) return COS_V101_ERR_INVAL;
    int32_t n = llama_tokenize(b->model, text, (int32_t)strlen(text),
                               tokens, max_tokens,
                               add_special ? true : false,
                               /*parse_special=*/true);
    if (n < 0) return COS_V101_ERR_TOKENIZE;
    return (int)n;
}

int cos_v101_bridge_detokenize(cos_v101_bridge_t *b,
                               const int32_t *tokens, int n_tokens,
                               char *buf, int max_buf)
{
    if (!b || !tokens || n_tokens <= 0 || !buf || max_buf <= 0) return COS_V101_ERR_INVAL;
    int32_t w = llama_detokenize(b->model, tokens, n_tokens, buf, max_buf,
                                 /*remove_special=*/false,
                                 /*unparse_special=*/true);
    if (w < 0) return COS_V101_ERR_OVERFLOW;
    return (int)w;
}

/* Fill a fresh llama_batch with a sequence of tokens.  Only the last token's
 * logits are normally requested, but callers can pass logits_mode != 0 to
 * request logits on every token (needed for loglikelihood scoring).
 */
static void batch_fill_seq(struct llama_batch *batch,
                           const int32_t *tokens, int n_tokens,
                           int start_pos, int logits_mode)
{
    batch->n_tokens = 0;
    for (int i = 0; i < n_tokens; i++) {
        int idx = batch->n_tokens;
        batch->token   [idx] = tokens[i];
        batch->pos     [idx] = (int)(start_pos + i);
        batch->n_seq_id[idx] = 1;
        batch->seq_id  [idx][0] = 0;
        batch->logits  [idx] = (int8_t)(logits_mode ? 1 : (i == n_tokens - 1));
        batch->n_tokens++;
    }
}

static int decode_tokens(struct llama_context *ctx,
                         const int32_t *tokens, int n_tokens,
                         int start_pos, int logits_mode,
                         int n_batch)
{
    if (n_tokens <= 0) return 0;
    struct llama_batch batch = llama_batch_init(n_batch, /*embd=*/0, /*n_seq_max=*/1);
    int pos = start_pos;
    int i = 0;
    while (i < n_tokens) {
        int chunk = n_tokens - i;
        if (chunk > n_batch) chunk = n_batch;
        batch_fill_seq(&batch, tokens + i, chunk, pos, logits_mode);
        int rc = llama_decode(ctx, batch);
        if (rc != 0) {
            llama_batch_free(batch);
            return COS_V101_ERR_DECODE;
        }
        pos += chunk;
        i   += chunk;
    }
    llama_batch_free(batch);
    return 0;
}

/* ---------------------------------------------------------------------- */
/* Loglikelihood: score log P(cont_tokens | ctx_tokens)                    */
/* ---------------------------------------------------------------------- */

int cos_v101_bridge_loglikelihood_ex(cos_v101_bridge_t *b,
                                     const char *ctx_text, const char *cont_text,
                                     double *ll_out, int *is_greedy_out,
                                     int *n_ctx_tokens_out, int *n_cont_tokens_out,
                                     float *out_sigma_mean,
                                     float *out_sigma_profile,  /* [8] */
                                     float *out_sigma_max_token)
{
    if (!b || !ctx_text || !cont_text) return COS_V101_ERR_INVAL;

    /* Tokenize ctx with BOS, cont without.  The cont's first token's
     * logits are read from the LAST position of ctx (teacher-forcing).
     */
    enum { MAX_TOKENS = 8192 };
    int32_t *toks = (int32_t *)calloc(MAX_TOKENS, sizeof(int32_t));
    if (!toks) return COS_V101_ERR_OVERFLOW;

    int n_ctx_t = llama_tokenize(b->model, ctx_text, (int32_t)strlen(ctx_text),
                                 toks, MAX_TOKENS,
                                 /*add_special=*/true, /*parse_special=*/true);
    if (n_ctx_t < 1) { free(toks); return COS_V101_ERR_TOKENIZE; }

    int32_t *cont_toks = toks + n_ctx_t;
    int max_cont = MAX_TOKENS - n_ctx_t;
    int n_cont_t = llama_tokenize(b->model, cont_text, (int32_t)strlen(cont_text),
                                  cont_toks, max_cont,
                                  /*add_special=*/false, /*parse_special=*/false);
    if (n_cont_t < 1) { free(toks); return COS_V101_ERR_TOKENIZE; }

    if (n_ctx_tokens_out)  *n_ctx_tokens_out  = n_ctx_t;
    if (n_cont_tokens_out) *n_cont_tokens_out = n_cont_t;

    if ((uint32_t)(n_ctx_t + n_cont_t) > b->n_ctx) {
        /* Truncate from the LEFT of ctx (keep continuation intact) to fit. */
        int drop = (n_ctx_t + n_cont_t) - (int)b->n_ctx;
        if (drop >= n_ctx_t) { free(toks); return COS_V101_ERR_OVERFLOW; }
        memmove(toks, toks + drop, (size_t)(n_ctx_t - drop) * sizeof(int32_t));
        n_ctx_t -= drop;
        /* cont_toks pointer is now invalid; relocate. */
        cont_toks = toks + n_ctx_t;
        memmove(cont_toks, cont_toks + drop, (size_t)n_cont_t * sizeof(int32_t));
    }

    /* Clear KV cache. */
    llama_kv_cache_clear(b->ctx);

    /* Decode ctx with logits_mode = 0 (we only need the last position). */
    int rc = decode_tokens(b->ctx, toks, n_ctx_t - 1, /*start_pos=*/0,
                           /*logits_mode=*/0, /*n_batch=*/512);
    if (rc != 0) { free(toks); return rc; }

    /* Decode the transition token (last of ctx) WITH logits enabled
     * — it produces the distribution over the *first* cont token.
     */
    rc = decode_tokens(b->ctx, &toks[n_ctx_t - 1], 1, /*start_pos=*/n_ctx_t - 1,
                       /*logits_mode=*/1, /*n_batch=*/512);
    if (rc != 0) { free(toks); return rc; }

    double  ll_sum        = 0.0;
    int     all_greedy    = 1;
    double  sigma_sum     = 0.0;
    double  sigma_prof_sum[COS_V101_SIGMA_CHANNELS] = {0};
    float   sigma_max_tok = 0.0f;
    int     n_vocab       = b->n_vocab;

    /* For each cont token, read the logits at the current tail,
     * record log p(cont_tok | prefix), then decode cont_tok with
     * logits_mode=1 so we have the next distribution.  Last token's
     * next-distribution is computed but not scored.
     */
    for (int k = 0; k < n_cont_t; k++) {
        float *logits = llama_get_logits_ith(b->ctx, -1);
        if (!logits) { free(toks); return COS_V101_ERR_DECODE; }

        /* Softmax with log-sum-exp normalization. */
        float max_l = logits[0];
        int   argmax = 0;
        for (int i = 1; i < n_vocab; i++) {
            if (logits[i] > max_l) { max_l = logits[i]; argmax = i; }
        }
        double Z = 0.0;
        for (int i = 0; i < n_vocab; i++) Z += exp((double)(logits[i] - max_l));
        double log_Z = log(Z) + (double)max_l;
        double log_p = (double)logits[cont_toks[k]] - log_Z;
        ll_sum += log_p;
        if (argmax != cont_toks[k]) all_greedy = 0;

        /* σ-channel aggregation. */
        cos_v101_sigma_t s;
        if (cos_v101_sigma_from_logits(logits, n_vocab, &s) == 0) {
            sigma_sum += (double)s.sigma;
            sigma_prof_sum[0] += (double)s.entropy_norm;
            sigma_prof_sum[1] += (double)s.margin;
            sigma_prof_sum[2] += (double)s.top_k_mass;
            sigma_prof_sum[3] += (double)s.tail_mass;
            sigma_prof_sum[4] += (double)s.logit_spread;
            sigma_prof_sum[5] += (double)s.p_max;
            sigma_prof_sum[6] += (double)s.n_effective;
            sigma_prof_sum[7] += (double)s.logit_std;
            if (s.sigma > sigma_max_tok) sigma_max_tok = s.sigma;
        }

        /* Consume this cont token to advance KV cache and produce the
         * distribution for the next step (unless it's the last).
         */
        int32_t t = cont_toks[k];
        int next_pos = (n_ctx_t - 1) + 1 + k;
        int want_logits = (k + 1 < n_cont_t) ? 1 : 0;
        if (want_logits) {
            int rc2 = decode_tokens(b->ctx, &t, 1, next_pos, /*logits_mode=*/1, /*n_batch=*/512);
            if (rc2 != 0) { free(toks); return rc2; }
        }
    }

    double denom = (double)(n_cont_t > 0 ? n_cont_t : 1);
    if (ll_out)             *ll_out = ll_sum;
    if (is_greedy_out)      *is_greedy_out = all_greedy;
    if (out_sigma_mean)     *out_sigma_mean = (float)(sigma_sum / denom);
    if (out_sigma_profile) {
        for (int c = 0; c < COS_V101_SIGMA_CHANNELS; c++) {
            out_sigma_profile[c] = (float)(sigma_prof_sum[c] / denom);
        }
    }
    if (out_sigma_max_token) *out_sigma_max_token = sigma_max_tok;

    free(toks);
    return COS_V101_OK;
}

int cos_v101_bridge_loglikelihood(cos_v101_bridge_t *b,
                                  const char *ctx_text, const char *cont_text,
                                  double *ll_out, int *is_greedy_out,
                                  int *n_ctx_tokens_out, int *n_cont_tokens_out,
                                  float *out_sigma_mean)
{
    return cos_v101_bridge_loglikelihood_ex(b, ctx_text, cont_text,
                                            ll_out, is_greedy_out,
                                            n_ctx_tokens_out, n_cont_tokens_out,
                                            out_sigma_mean,
                                            /*out_sigma_profile=*/NULL,
                                            /*out_sigma_max_token=*/NULL);
}

/* ---------------------------------------------------------------------- */
/* Generate: greedy decode with σ-gated early-stop                          */
/* ---------------------------------------------------------------------- */

static int ends_with_any(const char *buf, int n,
                         const char *const *until, int n_until)
{
    if (n_until <= 0 || n <= 0) return 0;
    for (int u = 0; u < n_until; u++) {
        const char *s = until[u];
        if (!s) continue;
        int ls = (int)strlen(s);
        if (ls <= 0 || ls > n) continue;
        if (memcmp(buf + (n - ls), s, (size_t)ls) == 0) return 1;
    }
    return 0;
}

int cos_v101_bridge_generate(cos_v101_bridge_t *b,
                             const char *ctx_text,
                             const char *const *until, int n_until,
                             int max_tokens,
                             float sigma_threshold,
                             char *buf, int max_buf,
                             float *out_sigma_profile,
                             int *out_abstained)
{
    if (!b || !ctx_text || !buf || max_buf <= 1 || max_tokens <= 0) return COS_V101_ERR_INVAL;

    enum { MAX_TOKENS_IN = 8192 };
    int32_t *ctx_toks = (int32_t *)calloc(MAX_TOKENS_IN, sizeof(int32_t));
    if (!ctx_toks) return COS_V101_ERR_OVERFLOW;

    int n_ctx_t = llama_tokenize(b->model, ctx_text, (int32_t)strlen(ctx_text),
                                 ctx_toks, MAX_TOKENS_IN,
                                 /*add_special=*/true, /*parse_special=*/true);
    if (n_ctx_t < 1) { free(ctx_toks); return COS_V101_ERR_TOKENIZE; }

    if ((uint32_t)n_ctx_t >= b->n_ctx) {
        int drop = n_ctx_t - (int)(b->n_ctx - 1);
        memmove(ctx_toks, ctx_toks + drop, (size_t)(n_ctx_t - drop) * sizeof(int32_t));
        n_ctx_t -= drop;
    }

    llama_kv_cache_clear(b->ctx);

    /* Decode all but last with logits off, last with logits on. */
    if (n_ctx_t > 1) {
        int rc = decode_tokens(b->ctx, ctx_toks, n_ctx_t - 1, 0, /*logits_mode=*/0, /*n_batch=*/512);
        if (rc != 0) { free(ctx_toks); return rc; }
    }
    {
        int rc = decode_tokens(b->ctx, &ctx_toks[n_ctx_t - 1], 1, n_ctx_t - 1, /*logits_mode=*/1, /*n_batch=*/512);
        if (rc != 0) { free(ctx_toks); return rc; }
    }

    int pos = n_ctx_t;
    int written = 0;
    int abstained = 0;
    int n_vocab = b->n_vocab;

    double sigma_sum[COS_V101_SIGMA_CHANNELS] = {0};
    int    sigma_count = 0;

    buf[0] = 0;

    for (int step = 0; step < max_tokens; step++) {
        float *logits = llama_get_logits_ith(b->ctx, -1);
        if (!logits) break;

        cos_v101_sigma_t s;
        if (cos_v101_sigma_from_logits(logits, n_vocab, &s) == 0) {
            sigma_sum[0] += s.entropy_norm;
            sigma_sum[1] += s.margin;
            sigma_sum[2] += s.top_k_mass;
            sigma_sum[3] += s.tail_mass;
            sigma_sum[4] += s.logit_spread;
            sigma_sum[5] += s.p_max;
            sigma_sum[6] += s.n_effective;
            sigma_sum[7] += s.logit_std;
            sigma_count++;
            if (sigma_threshold > 0.0f && s.sigma > sigma_threshold) {
                abstained = 1;
                break;
            }
        }

        /* Greedy argmax sampler on the raw logits. */
        float max_l = logits[0]; int argmax = 0;
        for (int i = 1; i < n_vocab; i++) {
            if (logits[i] > max_l) { max_l = logits[i]; argmax = i; }
        }

        if (llama_token_is_eog(b->model, argmax)) break;

        /* Decode the picked token. */
        int32_t t = (int32_t)argmax;
        int rc = decode_tokens(b->ctx, &t, 1, pos, /*logits_mode=*/1, /*n_batch=*/512);
        if (rc != 0) { free(ctx_toks); return rc; }
        pos++;

        /* Detokenize this single token into a small scratch and append. */
        char piece[256];
        int32_t w = llama_token_to_piece(b->model, t, piece, (int32_t)sizeof(piece), 0, /*special=*/false);
        if (w < 0) w = 0;
        if (w > 0 && written + w + 1 < max_buf) {
            memcpy(buf + written, piece, (size_t)w);
            written += w;
            buf[written] = 0;
            if (ends_with_any(buf, written, until, n_until)) break;
        }
    }

    if (out_sigma_profile) {
        for (int i = 0; i < COS_V101_SIGMA_CHANNELS; i++) {
            out_sigma_profile[i] = (float)((sigma_count > 0) ? sigma_sum[i] / (double)sigma_count : 0.0);
        }
    }
    if (out_abstained) *out_abstained = abstained;
    free(ctx_toks);
    return written;
}
