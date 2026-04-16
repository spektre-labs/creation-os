/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "v44_ttc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void v44_resp_agg(const v44_proxy_response_t *r, sigma_decomposed_t *agg)
{
    if (!r || !r->sigmas || r->n_tokens <= 0) {
        memset(agg, 0, sizeof *agg);
        return;
    }
    v44_aggregate_sigmas(r->sigmas, r->n_tokens, agg);
}

int v44_sigma_proxy_with_ttc(const v44_engine_config_t *eng, const char *prompt, const v44_sigma_config_t *cfg,
    v44_proxy_response_t *out)
{
    if (!out || !cfg) {
        return -1;
    }
    v44_proxy_response_t r0;
    memset(&r0, 0, sizeof r0);
    if (v44_sigma_proxy_generate(eng, prompt, cfg, 0, &r0) != 0) {
        return -1;
    }
    if (r0.action != ACTION_CITE) {
        *out = r0;
        memset(&r0, 0, sizeof r0);
        return 0;
    }

    size_t plen = prompt ? strlen(prompt) : 0u;
    char *p2 = (char *)malloc(plen + 16u);
    if (!p2) {
        v44_proxy_response_free(&r0);
        return -1;
    }
    (void)snprintf(p2, plen + 16u, "[TTC]%s", prompt ? prompt : "");

    v44_proxy_response_t r1;
    memset(&r1, 0, sizeof r1);
    if (v44_sigma_proxy_generate(eng, p2, cfg, 0, &r1) != 0) {
        free(p2);
        v44_proxy_response_free(&r0);
        return -1;
    }
    free(p2);

    sigma_decomposed_t a0, a1;
    v44_resp_agg(&r0, &a0);
    v44_resp_agg(&r1, &a1);

    if (a1.epistemic + 1e-6f < a0.epistemic) {
        v44_proxy_response_free(&r0);
        *out = r1;
        return 0;
    }
    v44_proxy_response_free(&r1);
    *out = r0;
    return 0;
}
