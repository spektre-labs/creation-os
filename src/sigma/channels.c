/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "channels.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define COS_POPCOUNTLL(x) __builtin_popcountll((x))
#else
static int cos_popcountll(uint64_t x)
{
    int c = 0;
    while (x) {
        x &= x - 1ull;
        c++;
    }
    return c;
}
#define COS_POPCOUNTLL(x) cos_popcountll((x))
#endif

float sigma_logit_entropy(const float *logits, int n)
{
    if (!logits || n <= 0)
        return 0.f;
    float max_l = logits[0];
    for (int i = 1; i < n; i++)
        if (logits[i] > max_l)
            max_l = logits[i];
    double sum = 0.0;
    for (int i = 0; i < n; i++)
        sum += exp((double)(logits[i] - max_l));
    if (sum <= 0.0)
        return 0.f;
    double h = 0.0;
    for (int i = 0; i < n; i++) {
        double p = exp((double)(logits[i] - max_l)) / sum;
        if (p > 1e-10)
            h -= p * log(p);
    }
    return (float)(h / log((double)n > 1 ? (double)n : 2.0));
}

float sigma_top_margin(const float *logits, int n)
{
    if (!logits || n <= 0)
        return 0.f;
    float m1 = logits[0], m2 = -INFINITY;
    for (int i = 1; i < n; i++) {
        if (logits[i] > m1) {
            m2 = m1;
            m1 = logits[i];
        } else if (logits[i] > m2) {
            m2 = logits[i];
        }
    }
    if (!isfinite(m2))
        m2 = m1;
    return 1.0f - (m1 - m2) / (fabsf(m1) + 1.0f);
}

float sigma_prediction_stability(float **logit_samples, int n_samples, int n)
{
    if (!logit_samples || n_samples <= 0 || n <= 0)
        return 0.f;
    int *tops = (int *)malloc(sizeof(int) * (size_t)n_samples);
    if (!tops)
        return 0.f;
    double mean_top = 0.0;
    for (int s = 0; s < n_samples; s++) {
        const float *row = logit_samples[s];
        int top = 0;
        for (int i = 1; i < n; i++)
            if (row[i] > row[top])
                top = i;
        tops[s] = top;
        mean_top += (double)top;
    }
    mean_top /= (double)n_samples;
    double var = 0.0;
    for (int s = 0; s < n_samples; s++) {
        double d = (double)tops[s] - mean_top;
        var += d * d;
    }
    free(tops);
    return (float)(sqrt(var / (double)n_samples) / (double)n);
}

float sigma_attention_entropy(const float *attn, int n_heads, int n_tokens)
{
    if (!attn || n_heads <= 0 || n_tokens <= 0)
        return 0.f;
    double total = 0.0;
    const int stride = n_tokens * n_tokens;
    for (int h = 0; h < n_heads; h++) {
        for (int t = 0; t < n_tokens; t++) {
            const float *row = attn + h * stride + t * n_tokens;
            double ent = 0.0;
            for (int i = 0; i < n_tokens; i++) {
                double p = (double)row[i];
                if (p > 1e-10)
                    ent -= p * log(p);
            }
            total += ent / log((double)n_tokens > 1 ? (double)n_tokens : 2.0);
        }
    }
    return (float)(total / (double)(n_heads * n_tokens));
}

float sigma_kv_coherence(const float *kv_prev, const float *kv_cur, int dim)
{
    if (!kv_prev || !kv_cur || dim <= 0)
        return 0.f;
    double dot = 0.0, np = 0.0, nc = 0.0;
    for (int i = 0; i < dim; i++) {
        double a = (double)kv_prev[i];
        double b = (double)kv_cur[i];
        dot += a * b;
        np += a * a;
        nc += b * b;
    }
    double cosv = dot / (sqrt(np) * sqrt(nc) + 1e-10);
    return (float)(1.0 - fabs(cosv));
}

float sigma_vsa_binding_error(const uint64_t *bound, const uint64_t *expected, int n_words)
{
    if (!bound || !expected || n_words <= 0)
        return 0.f;
    int errors = 0;
    for (int i = 0; i < n_words; i++)
        errors += COS_POPCOUNTLL(bound[i] ^ expected[i]);
    return (float)errors / (float)(n_words * 64);
}

float sigma_repetition(const int *tokens, int n, int window)
{
    if (!tokens || n < window * 2 || window <= 0)
        return 0.f;
    int matches = 0;
    for (int i = n - window; i < n; i++) {
        for (int j = i - window; j >= 0 && j >= i - window * 4; j--) {
            if (tokens[i] == tokens[j]) {
                matches++;
                break;
            }
        }
    }
    return (float)matches / (float)window;
}

float sigma_grammar(const char *text)
{
    if (!text)
        return 1.0f;
    int len = (int)strlen(text);
    if (len == 0)
        return 1.0f;
    int balanced = 0;
    for (int i = 0; i < len; i++) {
        if (text[i] == '(' || text[i] == '[' || text[i] == '{')
            balanced++;
        if (text[i] == ')' || text[i] == ']' || text[i] == '}')
            balanced--;
    }
    return fabsf((float)balanced) / ((float)len + 1.0f);
}

int sigma_abstain_gate(const sigma_state_t *s, const sigma_thresholds_t *t)
{
    if (!s || !t)
        return 0;
    if (s->logit_entropy > t->logit_entropy)
        return 1;
    if (s->top_margin > t->top_margin)
        return 2;
    if (s->prediction_stability > t->prediction_stability)
        return 3;
    if (s->attention_entropy > t->attention_entropy)
        return 4;
    if (s->kv_coherence > t->kv_coherence)
        return 5;
    if (s->vsa_binding_error > t->vsa_binding_error)
        return 6;
    if (s->repetition > t->repetition)
        return 7;
    if (s->grammar > t->grammar)
        return 8;
    return 0;
}
