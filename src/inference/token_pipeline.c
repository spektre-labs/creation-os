/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * Per-token forward: embedding → layered attention + ternary FFN → logits → σ → argmax.
 */
#include "cos_inference_limits.h"
#include "integer_attention.h"
#include "token_pipeline.h"
#include <stdint.h>

static void quantize_hidden_i8(const int32_t *hidden, int32_t hidden_dim,
                              int8_t *out_x)
{
    int32_t i;
    for (i = 0; i < hidden_dim; i++) {
        int32_t v = hidden[i] >> 8;
        if (v > 127)
            v = 127;
        if (v < -128)
            v = -128;
        out_x[i] = (int8_t)v;
    }
}

void inference_state_init(inference_state_t *st, ternary_model_t *model,
                         int32_t *kv_k, int32_t *kv_v, int32_t max_cache)
{
    if (!st)
        return;
    st->model       = *model;
    st->kv_cache_k  = kv_k;
    st->kv_cache_v  = kv_v;
    st->cache_len   = 0;
    st->max_cache   = max_cache;
    cos_sigma_q16_init(&st->gate);
}

int32_t generate_token(inference_state_t *state, int32_t input_token)
{
    int8_t x8[COS_INF_MAX_HIDDEN];
    int32_t hidden[COS_INF_MAX_HIDDEN];
    int32_t attn_out[COS_INF_MAX_HIDDEN];
    int32_t ffn_out[COS_INF_MAX_HIDDEN];
    int32_t logits[COS_INF_MAX_VOCAB];
    const int8_t *embedding;
    int32_t max_logit;
    int64_t sum_abs;
    int32_t entropy_sigma_q;
    int32_t best_token;
    int32_t l;
    int32_t i;
    int32_t pos_stride;
    int32_t hd;
    int32_t seq_here;

    if (!state || !state->model.embed || !state->model.layers
        || state->model.vocab_size <= 0 || state->model.hidden_dim <= 0
        || state->model.head_dim <= 0
        || state->model.hidden_dim > COS_INF_MAX_HIDDEN
        || state->model.head_dim > COS_INF_MAX_HEAD_DIM
        || state->model.vocab_size > COS_INF_MAX_VOCAB
        || input_token < 0 || input_token >= state->model.vocab_size)
        return 0;

    hd = state->model.head_dim;
    pos_stride = state->max_cache * hd;

    embedding = state->model.embed
        + (int32_t)input_token * state->model.hidden_dim;

    for (i = 0; i < state->model.hidden_dim; i++)
        hidden[i] = (int32_t)embedding[i] << 8;

    for (l = 0; l < state->model.n_layer_blocks; l++) {
        int32_t write_pos;
        int32_t *row_k;
        int32_t *row_v;

        write_pos = state->cache_len;
        if (write_pos >= state->max_cache)
            write_pos = state->max_cache - 1;

        row_k = state->kv_cache_k + l * pos_stride + write_pos * hd;
        row_v = state->kv_cache_v + l * pos_stride + write_pos * hd;

        for (i = 0; i < hd; i++) {
            row_k[i] = hidden[i];
            row_v[i] = hidden[i];
        }

        seq_here = state->cache_len + 1;
        if (seq_here > state->max_cache)
            seq_here = state->max_cache;

        integer_attention(hidden,
                         state->kv_cache_k + l * pos_stride,
                         state->kv_cache_v + l * pos_stride, attn_out,
                         seq_here, hd);

        for (i = 0; i < hd; i++)
            hidden[i] += attn_out[i];

        quantize_hidden_i8(hidden, state->model.hidden_dim, x8);
        ternary_matmul(state->model.layers[l].weights, x8, ffn_out,
                      state->model.layers[l].rows,
                      state->model.layers[l].cols);
        if (state->model.layers[l].biases) {
            for (i = 0; i < state->model.layers[l].rows; i++)
                ffn_out[i] += state->model.layers[l].biases[i];
        }
        for (i = 0; i < state->model.hidden_dim; i++)
            hidden[i] += ffn_out[i];
    }

    quantize_hidden_i8(hidden, state->model.hidden_dim, x8);
    ternary_matmul(state->model.lm_head.weights, x8, logits,
                  state->model.lm_head.rows, state->model.lm_head.cols);
    if (state->model.lm_head.biases) {
        for (i = 0; i < state->model.lm_head.rows; i++)
            logits[i] += state->model.lm_head.biases[i];
    }

    max_logit = logits[0];
    sum_abs   = 0;
    for (i = 0; i < state->model.vocab_size; i++) {
        int32_t li = logits[i];
        if (li > max_logit)
            max_logit = li;
        sum_abs += (li > 0) ? (int64_t)li : (int64_t)(-li);
    }
    if (sum_abs < 1)
        sum_abs = 1;
    entropy_sigma_q = (int32_t)(COS_SIGMA_Q16_ONE
                               - (((int64_t)max_logit * COS_SIGMA_Q16_ONE)
                                  / sum_abs));
    if (entropy_sigma_q < 0)
        entropy_sigma_q = 0;
    if (entropy_sigma_q > COS_SIGMA_Q16_ONE - 1)
        entropy_sigma_q = COS_SIGMA_Q16_ONE - 1;
    cos_sigma_q16_update(&state->gate, entropy_sigma_q,
                        (int32_t)((int64_t)9 * COS_SIGMA_Q16_ONE / 10));

    best_token = 0;
    for (i = 1; i < state->model.vocab_size; i++) {
        if (logits[i] > logits[best_token])
            best_token = i;
    }

    if (state->cache_len < state->max_cache)
        state->cache_len++;

    return best_token;
}
