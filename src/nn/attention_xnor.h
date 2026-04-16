/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v29: toy XNOR / Hamming-style attention path (lab harness, not a drop-in transformer).
 */
#ifndef ATTENTION_XNOR_H
#define ATTENTION_XNOR_H

#include <stdint.h>

void quantize_to_bsc(const int8_t *x, int d, uint64_t *out_words, int n_words);
float bsc_similarity(const uint64_t *a, const uint64_t *b, int n_words);
void bsc_bind(const uint64_t *a, const uint64_t *b, uint64_t *out, int n_words);
void attention_xnor(const int8_t *q, const int8_t *k, const int8_t *v, int8_t *out, int n_tokens, int d, int n_heads,
                    int bsc_words);

#endif /* ATTENTION_XNOR_H */
