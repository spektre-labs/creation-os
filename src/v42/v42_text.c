/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "v42_text.h"

#include <string.h>

void v42_scratch_logits_from_text(const char *t, float *log, int n)
{
    unsigned h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)t; p && *p; p++) {
        h = h * 16777619u ^ (unsigned)*p;
    }
    for (int i = 0; i < n; i++) {
        unsigned x = h ^ (unsigned)(i * 0x9E3779B1u);
        log[i] = (float)(x & 0xffu) * 0.01f;
    }
}

float v42_epistemic_from_text(const char *t)
{
    enum { N = 32 };
    float log[N];
    v42_scratch_logits_from_text(t, log, N);
    sigma_decomposed_t s;
    sigma_decompose_dirichlet_evidence(log, N, &s);
    return s.epistemic;
}
