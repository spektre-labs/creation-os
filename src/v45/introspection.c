/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "introspection.h"

#include "../sigma/decompose.h"
#include "../v42/v42_text.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t v45_fnv1a(const char *s)
{
    uint32_t h = 2166136261u;
    if (!s) {
        return h;
    }
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h = h * 16777619u ^ (uint32_t)*p;
    }
    return h;
}

static float v45_unit_interval_from_hash(uint32_t h)
{
    return (float)(h & 0xffffffu) / (float)0xffffffu;
}

static float v45_variance(const float *x, int n)
{
    if (!x || n <= 0) {
        return 0.0f;
    }
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        sum += x[i];
    }
    float m = sum / (float)n;
    float v = 0.0f;
    for (int i = 0; i < n; i++) {
        float d = x[i] - m;
        v += d * d;
    }
    return v / (float)n;
}

void v45_measure_introspection_lab(const char *prompt, const char *response, float self_report_in,
    v45_introspection_state_t *out)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof *out);
    const char *p = prompt ? prompt : "";
    const char *r = response ? response : "";

    enum { N = 32 };
    float logits[N];
    v42_scratch_logits_from_text(r, logits, N);
    sigma_decomposed_t sig;
    sigma_decompose_dirichlet_evidence(logits, N, &sig);
    out->confidence_actual = 1.0f - sig.epistemic;
    if (isnan(self_report_in)) {
        uint32_t h = v45_fnv1a("SELF_REPORT") ^ v45_fnv1a(p) ^ v45_fnv1a(r);
        out->confidence_self_report = v45_unit_interval_from_hash(h);
    } else {
        out->confidence_self_report = self_report_in;
    }
    out->calibration_gap = fabsf(out->confidence_self_report - out->confidence_actual);
    if (out->calibration_gap > 1.0f) {
        out->calibration_gap = 1.0f;
    }

    float sigma_samples[5];
    for (int i = 0; i < 5; i++) {
        char alt[128];
        (void)snprintf(alt, sizeof alt, "%s|ALT%d|%s", p, i, r);
        v42_scratch_logits_from_text(alt, logits, N);
        sigma_decompose_dirichlet_evidence(logits, N, &sig);
        sigma_samples[i] = sig.epistemic;
    }
    out->meta_sigma = v45_variance(sigma_samples, 5);
    float acc = 1.0f - out->calibration_gap;
    out->introspective_accuracy = acc > 0.0f ? acc : 0.0f;
}
