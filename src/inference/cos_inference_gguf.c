/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * GGUF header probe + deterministic toy weights for fixed-cap inference.
 */
#include "cos_inference_gguf.h"
#include "cos_inference_limits.h"
#include <stdint.h>
#include <stdio.h>

#define COS_GGUF_MAGIC_LE 0x46554747u /* "GGUF" little-endian u32 */

void cos_inference_init_toy_model(cos_inference_model_storage_t *buf,
                                  int32_t vocab_size, int32_t hidden_dim,
                                  int32_t head_dim, int32_t n_blocks)
{
    int32_t L;
    int32_t i;
    int32_t j;

    if (!buf)
        return;
    if (vocab_size < 8)
        vocab_size = 8;
    if (vocab_size > COS_INF_MAX_VOCAB)
        vocab_size = COS_INF_MAX_VOCAB;
    if (hidden_dim < 8)
        hidden_dim = 8;
    if (hidden_dim > COS_INF_MAX_HIDDEN)
        hidden_dim = COS_INF_MAX_HIDDEN;
    if (head_dim < 8)
        head_dim = 8;
    if (head_dim > COS_INF_MAX_HEAD_DIM)
        head_dim = COS_INF_MAX_HEAD_DIM;
    if (head_dim > hidden_dim)
        head_dim = hidden_dim;
    if (head_dim != hidden_dim) {
        /* Toy single-head path: attend over full width. */
        head_dim = hidden_dim;
    }
    if (n_blocks < 1)
        n_blocks = 1;
    if (n_blocks > COS_INF_MAX_LAYERS)
        n_blocks = COS_INF_MAX_LAYERS;

    for (i = 0; i < vocab_size * hidden_dim; i++) {
        int32_t v = (int32_t)(((i * 17) % 253) - 127);
        buf->embed[i] = (int8_t)v;
    }

    for (L = 0; L < n_blocks; L++) {
        int32_t rows = hidden_dim;
        int32_t cols = hidden_dim;
        for (i = 0; i < rows * cols; i++) {
            int32_t s = (i + L * 131) % 3;
            buf->ffn_w[L][i] = (int8_t)(s - 1);
        }
        for (i = 0; i < rows; i++)
            buf->ffn_bias[L][i] = 0;
        buf->blocks[L].weights = buf->ffn_w[L];
        buf->blocks[L].biases = buf->ffn_bias[L];
        buf->blocks[L].rows   = rows;
        buf->blocks[L].cols   = cols;
    }

    for (j = 0; j < vocab_size * hidden_dim; j++) {
        int32_t S = (j * 19 + 1) % 3;
        buf->lm_w[j] = (int8_t)(S - 1);
    }
    for (i = 0; i < vocab_size; i++)
        buf->lm_bias[i] = 0;

    buf->lm_head.weights = buf->lm_w;
    buf->lm_head.biases  = buf->lm_bias;
    buf->lm_head.rows    = vocab_size;
    buf->lm_head.cols    = hidden_dim;

    buf->model.embed           = buf->embed;
    buf->model.layers          = buf->blocks;
    buf->model.n_layer_blocks  = n_blocks;
    buf->model.lm_head         = buf->lm_head;
    buf->model.vocab_size      = vocab_size;
    buf->model.hidden_dim      = hidden_dim;
    buf->model.n_heads         = 1;
    buf->model.head_dim        = head_dim;
}

int cos_gguf_load_model(const char *path, ternary_model_t *model)
{
    FILE *f;
    uint32_t magic;
    size_t nrd;

    (void)model;
    if (!path)
        return -1;
    f = fopen(path, "rb");
    if (!f)
        return -2;
    nrd = fread(&magic, sizeof magic, 1, f);
    fclose(f);
    if (nrd != 1)
        return -3;
    if (magic != COS_GGUF_MAGIC_LE) {
        if (magic != 0x47475546u)
            return -4;
    }
    return -10;
}
