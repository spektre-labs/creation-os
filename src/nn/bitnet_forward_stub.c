/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "bitnet_forward.h"

#include <stdlib.h>
#include <string.h>

struct bitnet_model {
    int n_layers;
    int hidden_dim;
    int intermediate_dim;
    int n_heads;
    int vocab_size;
};

bitnet_model_t *bitnet_model_stub(int n_layers, int hidden_dim, int intermediate_dim, int n_heads, int vocab_size)
{
    if (n_layers <= 0 || hidden_dim <= 0 || intermediate_dim <= 0 || n_heads <= 0 || vocab_size <= 0)
        return NULL;
    if ((hidden_dim % n_heads) != 0)
        return NULL;
    bitnet_model_t *m = (bitnet_model_t *)calloc(1, sizeof(bitnet_model_t));
    if (!m)
        return NULL;
    m->n_layers = n_layers;
    m->hidden_dim = hidden_dim;
    m->intermediate_dim = intermediate_dim;
    m->n_heads = n_heads;
    m->vocab_size = vocab_size;
    return m;
}

void bitnet_free(bitnet_model_t *m)
{
    free(m);
}

void bitnet_forward(bitnet_model_t *m, const int *tokens, int n_tokens, float *logits)
{
    if (!m || !logits || n_tokens <= 0)
        return;
    (void)tokens;
    /* Deterministic toy logits: peaked on token id 7 % vocab for σ plumbing tests. */
    const int peak = 7 % m->vocab_size;
    for (int i = 0; i < m->vocab_size; i++)
        logits[i] = (i == peak) ? 4.f : -4.f;
}
