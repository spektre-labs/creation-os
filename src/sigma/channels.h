/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v29: eight scalar sigma signals (normalized where noted) + abstention gate.
 */
#ifndef SIGMA_CHANNELS_H
#define SIGMA_CHANNELS_H

#include <stdint.h>

typedef struct {
    float logit_entropy;
    float top_margin;
    float prediction_stability;
    float attention_entropy;
    float kv_coherence;
    float vsa_binding_error;
    float repetition;
    float grammar;
} sigma_state_t;

typedef struct {
    float logit_entropy;
    float top_margin;
    float prediction_stability;
    float attention_entropy;
    float kv_coherence;
    float vsa_binding_error;
    float repetition;
    float grammar;
} sigma_thresholds_t;

float sigma_logit_entropy(const float *logits, int n);
float sigma_top_margin(const float *logits, int n);
float sigma_prediction_stability(float **logit_samples, int n_samples, int n);
float sigma_attention_entropy(const float *attn, int n_heads, int n_tokens);
float sigma_kv_coherence(const float *kv_prev, const float *kv_cur, int dim);
float sigma_vsa_binding_error(const uint64_t *bound, const uint64_t *expected, int n_words);
float sigma_repetition(const int *tokens, int n, int window);
float sigma_grammar(const char *text);
/** Returns 0 = continue, 1..8 = abstain code. */
int sigma_abstain_gate(const sigma_state_t *s, const sigma_thresholds_t *t);

#endif /* SIGMA_CHANNELS_H */
