/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "independence_test.h"

#include <math.h>
#include <string.h>

static float mean(const float *x, int n)
{
    if (n <= 0) {
        return 0.0f;
    }
    double acc = 0.0;
    for (int i = 0; i < n; i++) {
        acc += (double)x[i];
    }
    return (float)(acc / (double)n);
}

static float var_pop(const float *x, int n, float mx)
{
    if (n <= 0) {
        return 0.0f;
    }
    double acc = 0.0;
    for (int i = 0; i < n; i++) {
        double d = (double)x[i] - (double)mx;
        acc += d * d;
    }
    return (float)(acc / (double)n);
}

static float pearson_r(const float *a, const float *b, int n)
{
    if (n < 2) {
        return 0.0f;
    }
    float ma = mean(a, n);
    float mb = mean(b, n);
    float va = var_pop(a, n, ma);
    float vb = var_pop(b, n, mb);
    if (!(va > 1e-12f) || !(vb > 1e-12f)) {
        return 0.0f;
    }
    double cov = 0.0;
    for (int i = 0; i < n; i++) {
        cov += ((double)a[i] - (double)ma) * ((double)b[i] - (double)mb);
    }
    cov /= (double)n;
    double den = sqrt((double)va * (double)vb);
    if (!(den > 1e-18)) {
        return 0.0f;
    }
    return (float)(cov / den);
}

sigma_independence_t measure_channel_independence(const float *channel_samples, int n_channels, int n_samples)
{
    sigma_independence_t out;
    memset(&out, 0, sizeof out);
    if (!channel_samples || n_channels <= 0 || n_samples < 2) {
        return out;
    }
    if (n_channels > SIGMA_INDEP_MAX_CHANNELS) {
        n_channels = SIGMA_INDEP_MAX_CHANNELS;
    }

    float acc_mean_abs = 0.0f;
    int pairs = 0;
    for (int i = 0; i < n_channels; i++) {
        const float *xi = channel_samples + (i * n_samples);
        out.correlation_matrix[i][i] = 1.0f;
        for (int j = i + 1; j < n_channels; j++) {
            const float *xj = channel_samples + (j * n_samples);
            float r = pearson_r(xi, xj, n_samples);
            out.correlation_matrix[i][j] = r;
            out.correlation_matrix[j][i] = r;
            acc_mean_abs += fabsf(r);
            pairs++;
        }
    }
    if (pairs > 0) {
        out.mean_correlation = acc_mean_abs / (float)pairs;
    } else {
        out.mean_correlation = 0.0f;
    }

    for (int i = 0; i < n_channels; i++) {
        const float *xi = channel_samples + (i * n_samples);
        float acc = 0.0f;
        int pj = 0;
        for (int j = 0; j < n_channels; j++) {
            if (j == i) {
                continue;
            }
            const float *xj = channel_samples + (j * n_samples);
            float r = pearson_r(xi, xj, n_samples);
            acc += fabsf(r);
            pj++;
        }
        float m = (pj > 0) ? (acc / (float)pj) : 0.0f;
        if (m < 0.5f) {
            out.n_independent++;
        }
    }

    out.threshold_regime = (out.n_independent >= 4) ? 1 : 0;
    return out;
}
