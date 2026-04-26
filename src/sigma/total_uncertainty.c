/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "total_uncertainty.h"

#include "semantic_sigma.h"

#include <stdio.h>
#include <string.h>

cos_total_uncertainty_t cos_measure_total(int port, const char *prompt,
                                          const char **models, int n_models)
{
    cos_total_uncertainty_t u;
    memset(&u, 0, sizeof u);
    u.models_used[0] = '\0';
    if (prompt == NULL || prompt[0] == '\0' || models == NULL || n_models < 1) {
        u.sigma_final = 0.5f;
        return u;
    }
    if (n_models > 8)
        n_models = 8;

    float sigs[8];
    int   i;
    for (i = 0; i < n_models; ++i) {
        const char *mdl =
            (models[i] != NULL && models[i][0] != '\0') ? models[i] : "gemma3:4b";
        cos_semantic_sigma_result det;
        memset(&det, 0, sizeof det);
        (void)cos_semantic_sigma_ex(prompt, NULL, port, mdl, 3, NULL, &det);
        sigs[i]          = det.sigma;
        u.sigma_per_model[i] = det.sigma;
        if (i == 0) {
            snprintf(u.models_used, sizeof u.models_used, "%s", mdl);
        } else {
            size_t L = strlen(u.models_used);
            if (L + 2 < sizeof u.models_used)
                snprintf(u.models_used + L, sizeof u.models_used - L, ",%s",
                         mdl);
        }
    }
    u.n_models = n_models;

    float sum = 0.0f;
    for (i = 0; i < n_models; ++i)
        sum += sigs[i];
    u.au = sum / (float)n_models;

    u.eu = 0.0f;
    if (n_models >= 2) {
        float acc = 0.0f;
        for (i = 0; i < n_models; ++i) {
            float d = sigs[i] - u.au;
            acc += d * d;
        }
        u.eu = acc / (float)n_models;
    }

    u.total = u.au + u.eu;
    if (u.total > 1.0f)
        u.total = 1.0f;
    u.sigma_final = 0.6f * u.au + 0.4f * u.eu;
    if (u.sigma_final > 1.0f)
        u.sigma_final = 1.0f;
    return u;
}
