/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "decompose.h"

#include <math.h>
#include <string.h>

void sigma_decompose_dirichlet_evidence(const float *logits, int n, sigma_decomposed_t *out)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!logits || n <= 0) {
        return;
    }
    float maxv = logits[0];
    for (int i = 1; i < n; i++)
        if (logits[i] > maxv)
            maxv = logits[i];
    double suma = 0.0;
    for (int i = 0; i < n; i++)
        suma += exp((double)(logits[i] - maxv));
    if (!(suma > 0.0)) {
        return;
    }
    float K = (float)n;
    float S = (float)suma;
    out->aleatoric = K / S;
    out->epistemic = K * (K - 1.0f) / (S * (S + 1.0f));
    out->total = out->aleatoric + out->epistemic;
}

void sigma_decompose_softmax_mass(const float *logits, int n, float mass, sigma_decomposed_t *out)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!logits || n <= 0 || !(mass > 0.0f)) {
        return;
    }
    float maxv = logits[0];
    for (int i = 1; i < n; i++)
        if (logits[i] > maxv)
            maxv = logits[i];
    double sume = 0.0;
    for (int i = 0; i < n; i++)
        sume += exp((double)(logits[i] - maxv));
    if (!(sume > 0.0)) {
        return;
    }
    float K = (float)n;
    float S = mass;
    (void)sume;
    out->aleatoric = K / S;
    out->epistemic = K * (K - 1.0f) / (S * (S + 1.0f));
    out->total = out->aleatoric + out->epistemic;
}
