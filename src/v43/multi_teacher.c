/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "multi_teacher.h"

#include <math.h>
#include <stddef.h>

int v43_ensemble_teacher_logits(const v43_teacher_output_t *teachers, int n_teachers, int vocab_size, float *out)
{
    if (!teachers || !out || n_teachers < 1 || vocab_size < 1) {
        return 1;
    }
    for (int v = 0; v < vocab_size; v++) {
        out[v] = 0.0f;
    }
    float total_weight = 0.0f;
    for (int t = 0; t < n_teachers; t++) {
        if (!teachers[t].logits) {
            return 1;
        }
        float w = 1.0f / (teachers[t].sigma.epistemic + 0.1f);
        if (!isfinite(w) || w <= 0.0f) {
            return 1;
        }
        for (int v = 0; v < vocab_size; v++) {
            out[v] += w * teachers[t].logits[v];
        }
        total_weight += w;
    }
    if (total_weight <= 1e-12f) {
        return 1;
    }
    float inv = 1.0f / total_weight;
    for (int v = 0; v < vocab_size; v++) {
        out[v] *= inv;
    }
    return 0;
}
