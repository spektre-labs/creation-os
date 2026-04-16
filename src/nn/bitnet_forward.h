/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v29: BitNet-shaped model struct + forward entrypoints (stub weights until wired to GGUF kernels).
 */
#ifndef BITNET_FORWARD_H
#define BITNET_FORWARD_H

#include <stdint.h>

typedef struct bitnet_model bitnet_model_t;

void bitnet_forward(bitnet_model_t *m, const int *tokens, int n_tokens, float *logits);
void bitnet_free(bitnet_model_t *m);

/** Construct a zeroed stub model for dimensions / plumbing tests (no real checkpoint). */
bitnet_model_t *bitnet_model_stub(int n_layers, int hidden_dim, int intermediate_dim, int n_heads, int vocab_size);

#endif /* BITNET_FORWARD_H */
