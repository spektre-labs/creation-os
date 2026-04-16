/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "clap_sigma.h"

#include <math.h>
#include <string.h>

static float v45_mean(const float *x, int n)
{
    if (!x || n <= 0) {
        return 0.0f;
    }
    float s = 0.0f;
    for (int i = 0; i < n; i++) {
        s += x[i];
    }
    return s / (float)n;
}

static float v45_var(const float *x, int n)
{
    if (!x || n <= 0) {
        return 0.0f;
    }
    float m = v45_mean(x, n);
    float v = 0.0f;
    for (int i = 0; i < n; i++) {
        float d = x[i] - m;
        v += d * d;
    }
    return v / (float)n;
}

void v45_probe_internals_lab(uint32_t seed, int n_layers, v45_internal_probe_t *out)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof *out);
    if (n_layers < 1) {
        n_layers = 1;
    }
    if (n_layers > V45_MAX_LAYERS) {
        n_layers = V45_MAX_LAYERS;
    }
    for (int i = 0; i < n_layers; i++) {
        uint32_t x = seed ^ (uint32_t)(i * 0x9E3779B9u);
        float e = (float)(x & 0xffu) / 255.0f;
        float nrm = 1.0f + (float)((x >> 8) & 0xffu) / 255.0f;
        out->attention_entropy_per_layer[i] = e;
        out->hidden_state_norm_per_layer[i] = nrm;
    }
    float hv = v45_var(out->hidden_state_norm_per_layer, n_layers);
    out->sigma_internal = v45_var(out->attention_entropy_per_layer, n_layers);
    out->layer_agreement = 1.0f / (1.0f + hv);
}
