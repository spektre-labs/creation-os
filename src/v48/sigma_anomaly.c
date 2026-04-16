/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "sigma_anomaly.h"

#include <math.h>
#include <stddef.h>

static float mean_epistemic_slice(const sigma_decomposed_t *s, int n, int start, int end)
{
    if (!s || n <= 0 || start < 0 || end <= start || start >= n) {
        return 0.0f;
    }
    if (end > n) {
        end = n;
    }
    float acc = 0.0f;
    int k = 0;
    for (int i = start; i < end; i++) {
        acc += s[i].epistemic;
        k++;
    }
    return k > 0 ? acc / (float)k : 0.0f;
}

sigma_anomaly_t detect_anomaly(
    const sigma_decomposed_t *per_token_sigmas,
    int n_tokens,
    const float *baseline_sigma_profile,
    int baseline_len)
{
    sigma_anomaly_t a = {0};
    if (!per_token_sigmas || n_tokens <= 0) {
        return a;
    }

    for (int i = 0; i < n_tokens; i++) {
        a.sigma_mean += per_token_sigmas[i].epistemic;
    }
    a.sigma_mean /= (float)n_tokens;

    for (int i = 0; i < n_tokens; i++) {
        float diff = per_token_sigmas[i].epistemic - a.sigma_mean;
        a.sigma_variance += diff * diff;
    }
    a.sigma_variance /= (float)n_tokens;

    if (n_tokens >= 2) {
        a.sigma_trend = (per_token_sigmas[n_tokens - 1].epistemic - per_token_sigmas[0].epistemic) / (float)(n_tokens - 1);
    } else {
        a.sigma_trend = 0.0f;
    }

    float thr = 2.0f * a.sigma_mean;
    if (thr < 1e-6f) {
        thr = 1e-6f;
    }
    for (int i = 1; i < n_tokens; i++) {
        float delta = fabsf(per_token_sigmas[i].epistemic - per_token_sigmas[i - 1].epistemic);
        if (delta > thr) {
            a.sigma_spike_count += 1.0f;
        }
    }

    int third = n_tokens / 3;
    if (third < 1) {
        third = 1;
    }
    a.instruction_sigma = mean_epistemic_slice(per_token_sigmas, n_tokens, 0, third);
    a.response_sigma = mean_epistemic_slice(per_token_sigmas, n_tokens, n_tokens - third, n_tokens);
    a.gap = fabsf(a.instruction_sigma - a.response_sigma);

    float profile_distance = 0.0f;
    if (baseline_sigma_profile && baseline_len > 0) {
        int compare_len = n_tokens < baseline_len ? n_tokens : baseline_len;
        if (compare_len > 0) {
            float acc = 0.0f;
            for (int i = 0; i < compare_len; i++) {
                float diff = per_token_sigmas[i].epistemic - baseline_sigma_profile[i];
                acc += diff * diff;
            }
            profile_distance = sqrtf(acc / (float)compare_len);
        }
    }

    int too_many_spikes = a.sigma_spike_count > (float)n_tokens / 10.0f;
    int diverges = profile_distance > 0.3f;
    int confident_volatile = (a.sigma_mean < 0.15f) && (a.sigma_variance > 0.1f);

    a.anomaly_detected = (diverges || too_many_spikes || confident_volatile) ? 1 : 0;
    return a;
}
