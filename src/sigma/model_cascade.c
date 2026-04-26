/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "model_cascade.h"

#include "bitnet_server.h"
#include "semantic_sigma.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *model;
    int         max_tokens;
    float       tau_proceed;
} cascade_tier_t;

static const char *cascade_model_env(int tier)
{
    const char *e = NULL;
    if (tier == 0)
        e = getenv("COS_CASCADE_MODEL_0");
    else if (tier == 1)
        e = getenv("COS_CASCADE_MODEL_1");
    else
        e = getenv("COS_CASCADE_MODEL_2");
    return (e != NULL && e[0] != '\0') ? e : NULL;
}

static float cascade_tau_env(int tier, float def)
{
    const char *e = NULL;
    if (tier == 0)
        e = getenv("COS_CASCADE_TAU_0");
    else if (tier == 1)
        e = getenv("COS_CASCADE_TAU_1");
    else
        e = getenv("COS_CASCADE_TAU_2");
    if (e == NULL || e[0] == '\0')
        return def;
    float x = (float)strtod(e, NULL);
    if (x < 0.f || x > 1.f)
        return def;
    return x;
}

int cos_cascade_query(int port, const char *prompt, const char *system_prompt,
                      char *response_out, size_t response_cap, float *sigma_out,
                      int *tier_used)
{
    if (response_out == NULL || response_cap < 8u || sigma_out == NULL
        || tier_used == NULL)
        return -1;
    response_out[0] = '\0';

    static const cascade_tier_t def[] = {
        { "qwen3.5:0.6b", 80, 0.25f },
        { "gemma3:4b", 120, 0.40f },
        { "llama3.2:3b", 120, 0.50f },
    };

    static const char k_follow[] =
        "Give only the direct answer. Maximum one sentence. No explanation.";

    for (int t = 0; t < 3; t++) {
        const char *mid = cascade_model_env(t);
        if (mid == NULL)
            mid = def[t].model;
        float tau = cascade_tau_env(t, def[t].tau_proceed);
        int   mt  = def[t].max_tokens;
        {
            const char *em = NULL;
            if (t == 0)
                em = getenv("COS_CASCADE_MAX_TOKENS_0");
            else if (t == 1)
                em = getenv("COS_CASCADE_MAX_TOKENS_1");
            else
                em = getenv("COS_CASCADE_MAX_TOKENS_2");
            if (em != NULL && em[0] != '\0') {
                int v = atoi(em);
                if (v >= 8 && v <= 512)
                    mt = v;
            }
        }

        char *resp = cos_bitnet_query_model(port, prompt, system_prompt, mid,
                                            0.7f, mt);
        if (resp == NULL || resp[0] == '\0') {
            free(resp);
            fprintf(stderr,
                    "[cascade] tier %d (%s) empty response → escalate\n", t,
                    mid);
            continue;
        }

        cos_semantic_sigma_result det;
        memset(&det, 0, sizeof(det));
        float sigma =
            cos_semantic_sigma_ex(prompt, k_follow, port, mid, 3, resp, &det);

        fprintf(stderr,
                "[cascade] tier %d (%s) σ=%.3f τ=%.3f (intra-model samples)\n",
                t, mid, (double)sigma, (double)tau);

        if (sigma < tau) {
            snprintf(response_out, response_cap, "%s", resp);
            free(resp);
            *sigma_out  = sigma;
            *tier_used  = t;
            return 0;
        }

        fprintf(stderr,
                "[cascade] tier %d (%s) σ=%.3f ≥ τ=%.3f → escalate\n", t, mid,
                (double)sigma, (double)tau);
        free(resp);
    }

    snprintf(response_out, response_cap, "%s",
             "[ABSTAIN: all tiers uncertain]");
    *sigma_out = 1.0f;
    *tier_used = -1;
    return 0;
}
