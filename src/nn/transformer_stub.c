/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Toy dense forward used by v28 self-tests (not BitNet-2B).
 */
#include "transformer_stub.h"

#include <math.h>
#include <string.h>

void cos_nn_toy_linear_f32(const float *w /* nout x nin row-major */, const float *x, int nin, int nout, float *y)
{
    for (int r = 0; r < nout; r++) {
        float acc = 0.f;
        for (int c = 0; c < nin; c++)
            acc += w[r * nin + c] * x[c];
        y[r] = acc;
    }
}

float cos_nn_squared_relu(float x)
{
    float t = x > 0.f ? x : 0.f;
    return t * t;
}
