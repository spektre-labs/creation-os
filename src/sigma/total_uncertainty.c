/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "total_uncertainty.h"

#include "semantic_sigma.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cos_total_uncertainty_t cos_measure_total(int port, const char *prompt,
                                          const char **models, int n_models)
{
    cos_total_uncertainty_t u;
    int                     i, nm;
    float                   w_au, w_eu;

    memset(&u, 0, sizeof u);
    u.models_used[0] = '\0';
    if (prompt == NULL || prompt[0] == '\0' || models == NULL || n_models < 1)
        return u;
    nm = n_models;
    if (nm > COS_TU_MAX_MODELS)
        nm = COS_TU_MAX_MODELS;

    w_au = 0.6f;
    w_eu = 0.4f;
    {
        const char *ea = getenv("COS_TOTAL_UNCERTAINTY_W_AU");
        const char *ee = getenv("COS_TOTAL_UNCERTAINTY_W_EU");
        if (ea != NULL && ea[0] != '\0') {
            float x = (float)strtod(ea, NULL);
            if (x >= 0.f && x <= 1.f)
                w_au = x;
        }
        if (ee != NULL && ee[0] != '\0') {
            float x = (float)strtod(ee, NULL);
            if (x >= 0.f && x <= 1.f)
                w_eu = x;
        }
    }

    u.n_models = nm;
    for (i = 0; i < nm; ++i) {
        const char *m =
            (models[i] != NULL && models[i][0] != '\0') ? models[i] : "gemma3:4b";
        snprintf(u.model_name[i], sizeof u.model_name[i], "%s", m);
        u.sigma_per_model[i] =
            cos_semantic_sigma_ex(prompt, NULL, port, m, 3, NULL, NULL);
    }

    {
        float sum = 0.f;
        for (i = 0; i < nm; ++i)
            sum += u.sigma_per_model[i];
        u.au = sum / (float)nm;
    }

    u.eu = 0.f;
    if (nm >= 2) {
        float acc = 0.f;
        for (i = 0; i < nm; ++i) {
            float d = u.sigma_per_model[i] - u.au;
            acc += d * d;
        }
        u.eu = acc / (float)nm;
    }

    u.total       = u.au + u.eu;
    u.sigma_final = w_au * u.au + w_eu * u.eu;
    if (u.sigma_final > 1.0f)
        u.sigma_final = 1.0f;

    {
        size_t w = 0;
        for (i = 0; i < nm && w + 2 < sizeof u.models_used; ++i) {
            if (i > 0)
                u.models_used[w++] = ',';
            const char *p = u.model_name[i];
            while (*p && w + 1 < sizeof u.models_used)
                u.models_used[w++] = *p++;
        }
        u.models_used[w] = '\0';
    }

    return u;
}
