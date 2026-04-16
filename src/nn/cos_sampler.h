/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef COS_SAMPLER_H
#define COS_SAMPLER_H

#include <stdint.h>

/** Categorical sample from logits after temperature / top-k / top-p (nucleus). Uses `rand_r` if `rnd!=NULL`, else rand(). */
int cos_sample_logits(const float *logits, int vocab, float temperature, int topk, float topp, unsigned int *rnd_state,
                      uint32_t *out_id);

/** Shannon entropy of softmax(logits) (natural log). */
float cos_logits_entropy(const float *logits, int n);

#endif /* COS_SAMPLER_H */
