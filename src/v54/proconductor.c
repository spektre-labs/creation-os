/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v54 σ-Proconductor — registry + defaults.
 */
#include "proconductor.h"

#include <string.h>

static float v54_clip01(float x)
{
    if (!(x >= 0.0f)) return 0.0f;
    if (x > 1.0f)    return 1.0f;
    return x;
}

static void v54_copy_bounded(char *dst, size_t cap, const char *src)
{
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

const char *v54_domain_name(v54_domain_t d)
{
    switch (d) {
    case V54_DOMAIN_LOGIC:    return "logic";
    case V54_DOMAIN_FACTUAL:  return "factual";
    case V54_DOMAIN_CREATIVE: return "creative";
    case V54_DOMAIN_CODE:     return "code";
    case V54_DOMAIN_META:     return "meta";
    default:                  return "?";
    }
}

void v54_proconductor_init(v54_proconductor_t *p)
{
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->convergence_threshold = 0.30f;
    p->disagreement_abstain  = 0.50f;
    p->max_parallel_queries  = 4;
}

int v54_proconductor_register(v54_proconductor_t *p,
                              const char *name,
                              const char *endpoint,
                              float cost_per_1k_tokens,
                              float latency_p50_ms,
                              const float sigma_profile[V54_N_DOMAINS])
{
    if (!p || !name) return -1;
    if (p->n_agents >= V54_MAX_AGENTS) return -1;

    v54_subagent_t *s = &p->agents[p->n_agents];
    memset(s, 0, sizeof(*s));
    v54_copy_bounded(s->name, V54_MAX_NAME, name);
    v54_copy_bounded(s->endpoint, V54_MAX_NAME,
                     endpoint ? endpoint : "unspecified");
    s->cost_per_1k_tokens = cost_per_1k_tokens >= 0.0f ? cost_per_1k_tokens : 0.0f;
    s->latency_p50_ms     = latency_p50_ms     >= 0.0f ? latency_p50_ms     : 0.0f;
    for (int i = 0; i < V54_N_DOMAINS; i++) {
        s->sigma_profile[i]     = sigma_profile
                                  ? v54_clip01(sigma_profile[i])
                                  : 0.5f;
        s->observed_accuracy[i] = 0.0f;
    }
    return p->n_agents++;
}

int v54_proconductor_register_defaults(v54_proconductor_t *p)
{
    if (!p) return -1;

    /* Hand-tuned σ-profiles from the v54 design note (illustrative;
     * NOT measured). Order: {logic, factual, creative, code, meta}. */
    const float claude[V54_N_DOMAINS]   = {0.15f, 0.30f, 0.25f, 0.20f, 0.22f};
    const float gpt[V54_N_DOMAINS]      = {0.25f, 0.18f, 0.22f, 0.15f, 0.24f};
    const float gemini[V54_N_DOMAINS]   = {0.30f, 0.20f, 0.18f, 0.28f, 0.26f};
    const float deepseek[V54_N_DOMAINS] = {0.20f, 0.28f, 0.35f, 0.15f, 0.25f};
    const float bitnet[V54_N_DOMAINS]   = {0.45f, 0.40f, 0.50f, 0.45f, 0.50f};

    int rc = 0;
    rc |= (v54_proconductor_register(p, "claude",       "external/claude",   15.0f, 900.0f,  claude)   < 0);
    rc |= (v54_proconductor_register(p, "gpt",          "external/gpt",      10.0f, 600.0f,  gpt)      < 0);
    rc |= (v54_proconductor_register(p, "gemini",       "external/gemini",    7.0f, 700.0f,  gemini)   < 0);
    rc |= (v54_proconductor_register(p, "deepseek",     "external/deepseek",  2.0f, 500.0f,  deepseek) < 0);
    rc |= (v54_proconductor_register(p, "local_bitnet", "local/bitnet-2b4t",  0.0f,  40.0f,  bitnet)   < 0);
    return rc ? -1 : 0;
}
