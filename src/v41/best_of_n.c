/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "best_of_n.h"

int v41_compute_n_samples(float epistemic)
{
    if (epistemic < 0.2f) {
        return 1;
    }
    if (epistemic < 0.4f) {
        return 3;
    }
    if (epistemic < 0.6f) {
        return 8;
    }
    if (epistemic < 0.8f) {
        return 16;
    }
    return 0;
}

int v41_argmin_float(const float *a, int n)
{
    if (!a || n <= 0) {
        return 0;
    }
    int best = 0;
    for (int i = 1; i < n; i++) {
        if (a[i] < a[best]) {
            best = i;
        }
    }
    return best;
}
