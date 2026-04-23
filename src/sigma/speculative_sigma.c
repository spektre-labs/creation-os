/*
 * Speculative σ — combines domain running-mean prior with semantic-cache peek.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "speculative_sigma.h"

#include "inference_cache.h"

#include <stddef.h>

enum { COS_SPEC_DOM = 256 };

typedef struct {
    double   sum_sigma;
    uint32_t n;
} cos_spec_domain_t;

static cos_spec_domain_t g_dom[COS_SPEC_DOM];

void cos_speculative_sigma_observe(uint64_t domain_hash, float measured_sigma)
{
    unsigned ix      = (unsigned)(domain_hash % (uint64_t)COS_SPEC_DOM);
    float    sigma_c = measured_sigma;
    if (sigma_c < 0.f)
        sigma_c = 0.f;
    if (sigma_c > 1.f)
        sigma_c = 1.f;
    g_dom[ix].sum_sigma += (double)sigma_c;
    g_dom[ix].n++;
}

struct cos_speculative_sigma cos_predict_sigma(const uint64_t *bsc_prompt,
                                               uint64_t domain_hash)
{
    struct cos_speculative_sigma out;
    out.predicted_sigma    = 0.5f;
    out.confidence         = 0.15f;
    out.skip_verification  = 0;
    out.early_abstain      = 0;

    if (!bsc_prompt)
        return out;

    unsigned ix = (unsigned)(domain_hash % (uint64_t)COS_SPEC_DOM);
    float    domain_prior = 0.5f;
    if (g_dom[ix].n > 0)
        domain_prior =
            (float)(g_dom[ix].sum_sigma / (double)g_dom[ix].n);

    float cache_sigma = 0.f, dist = 1.f;
    int   have_cache =
        cos_inference_cache_peek_nearest(bsc_prompt, COS_INF_W,
                                         &cache_sigma, &dist);

    float combined = domain_prior;
    float conf     = 0.22f;

    if (have_cache && dist < 0.45f) {
        float w_dom =
            0.30f + 0.55f * (1.f - dist / 0.45f); /* far → trust cache more */
        if (w_dom < 0.12f)
            w_dom = 0.12f;
        if (w_dom > 0.88f)
            w_dom = 0.88f;
        combined = w_dom * domain_prior + (1.f - w_dom) * cache_sigma;
        conf     = 0.38f + 0.52f * (1.f - dist / 0.45f);
        if (g_dom[ix].n >= 3u)
            conf += 0.06f;
    } else {
        combined = domain_prior;
        conf     = 0.20f
            + 0.06f * (float)(g_dom[ix].n > 12u ? 12u : g_dom[ix].n);
    }

    if (conf > 1.f)
        conf = 1.f;

    out.predicted_sigma = combined;
    out.confidence      = conf;

    if (combined < 0.05f && conf >= 0.38f)
        out.skip_verification = 1;
    if (combined > 0.89f && conf >= 0.30f)
        out.early_abstain = 1;

    return out;
}

int cos_speculative_sigma_self_test(void)
{
    cos_inference_cache_clear(1);
    if (cos_inference_cache_init(16) != 0)
        return 1;

    cos_speculative_sigma_observe(999ULL, 0.96f);
    cos_speculative_sigma_observe(999ULL, 0.94f);

    uint64_t b[COS_INF_W];
    cos_inference_bsc_encode_prompt("speculative sigma regression", b);

    struct cos_speculative_sigma hi =
        cos_predict_sigma(b, 999ULL);
    if (!hi.early_abstain)
        return 2;

    for (int i = 0; i < 12; i++)
        cos_speculative_sigma_observe(42ULL, 0.02f);

    cos_inference_bsc_encode_prompt("easy arithmetic drill two plus two", b);
    struct cos_speculative_sigma lo = cos_predict_sigma(b, 42ULL);
    if (!lo.skip_verification)
        return 3;

    uint64_t z[COS_INF_W];
    cos_inference_bsc_encode_prompt("cached anchor phrase alpha beta", z);
    if (cos_inference_cache_store(z, "unit-test-anchor", 0.15f) != 0)
        return 4;

    float cs = 0.f, hd = 0.f;
    if (!cos_inference_cache_peek_nearest(z, COS_INF_W, &cs, &hd))
        return 5;
    if (hd > 1e-5f)
        return 6;

    cos_inference_bsc_encode_prompt(
        "orthogonal prompt mix domain seven seven seven tokens", b);
    struct cos_speculative_sigma mix = cos_predict_sigma(b, 777ULL);
    if (mix.predicted_sigma <= 0.01f || mix.predicted_sigma >= 0.99f)
        return 7;

    struct cos_speculative_sigma null =
        cos_predict_sigma(NULL, 1ULL);
    if (null.predicted_sigma != 0.5f || null.early_abstain || null.skip_verification)
        return 8;

    cos_inference_cache_clear(1);
    return 0;
}
