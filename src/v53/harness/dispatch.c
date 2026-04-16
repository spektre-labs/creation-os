/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v53 σ-triggered sub-agent dispatch — scaffold.
 */
#include "dispatch.h"

#include <string.h>

static void v53_copy_name(char *dst, size_t cap, const char *src)
{
    if (!dst || cap == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t n = strlen(src);
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

int v53_default_specialists(v53_specialist_cfg_t *out, int cap)
{
    if (!out || cap <= 0) {
        return 0;
    }
    const struct { const char *n; float trig; float scap; } seeds[] = {
        { "security",    0.55f, 0.40f },
        { "performance", 0.60f, 0.55f },
        { "correctness", 0.50f, 0.35f },
    };
    const int N = (int)(sizeof(seeds) / sizeof(seeds[0]));
    int n = (cap < N) ? cap : N;
    for (int i = 0; i < n; i++) {
        v53_copy_name(out[i].name, V53_DOMAIN_NAME_MAX, seeds[i].n);
        out[i].spawn_trigger          = seeds[i].trig;
        out[i].specialist_sigma_cap   = seeds[i].scap;
    }
    return n;
}

static const v53_specialist_cfg_t *v53_find_cfg(
    const v53_specialist_cfg_t *cfgs, int n_cfgs, const char *domain)
{
    if (!cfgs || n_cfgs <= 0 || !domain || !*domain) {
        return NULL;
    }
    for (int i = 0; i < n_cfgs; i++) {
        if (strncmp(cfgs[i].name, domain, V53_DOMAIN_NAME_MAX) == 0) {
            return &cfgs[i];
        }
    }
    return NULL;
}

v53_dispatch_result_t v53_dispatch_if_needed(
    const v53_specialist_cfg_t *cfgs, int n_cfgs,
    const char *domain,
    const sigma_decomposed_t *main_domain_sigma)
{
    v53_dispatch_result_t r;
    memset(&r, 0, sizeof(r));
    if (domain) {
        v53_copy_name(r.domain, V53_DOMAIN_NAME_MAX, domain);
    }

    const v53_specialist_cfg_t *cfg = v53_find_cfg(cfgs, n_cfgs, domain);
    if (!cfg) {
        r.summary = "no specialist registered for domain";
        return r;
    }

    const float msig = main_domain_sigma ? main_domain_sigma->total : 0.0f;
    if (msig < cfg->spawn_trigger) {
        r.summary = "main σ below spawn trigger";
        return r;
    }

    /* Scaffold "fresh context" proxy: specialist σ = 0.5 × main σ. In a
     * real runtime this would come from a sub-agent process that gets a
     * narrow, uncorrelated prompt and its own logits stream. */
    r.spawned                    = 1;
    r.specialist_sigma_observed  = 0.5f * msig;
    r.specialist_abstained       =
        (r.specialist_sigma_observed > cfg->specialist_sigma_cap) ? 1 : 0;
    r.summary = r.specialist_abstained
                ? "specialist spawned but abstained"
                : "specialist spawned and returned a confident summary";
    return r;
}
