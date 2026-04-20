/* σ-speculative primitive implementation + exhaustive self-test. */

#include "speculative.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

const char *cos_sigma_route_label(cos_sigma_route_t r) {
    switch (r) {
        case COS_SIGMA_ROUTE_LOCAL:    return "LOCAL";
        case COS_SIGMA_ROUTE_ESCALATE: return "ESCALATE";
        default:                       return "UNKNOWN";
    }
}

float cos_sigma_speculative_cost_savings(int n_total, int n_escalated,
                                         float eur_local_per_req,
                                         float eur_api_per_req) {
    if (n_total <= 0)                         return 0.0f;
    if (n_escalated < 0)                      return 0.0f;
    if (n_escalated > n_total)                return 0.0f;
    if (!(eur_local_per_req >= 0.0f))         return 0.0f;
    if (!(eur_api_per_req   >  0.0f))         return 0.0f;

    int n_local = n_total - n_escalated;
    float cost_api_only = (float)n_total * eur_api_per_req;
    float cost_hybrid = (float)n_local      * eur_local_per_req
                      + (float)n_escalated  * (eur_local_per_req
                                             + eur_api_per_req);
    float savings = (cost_api_only - cost_hybrid) / cost_api_only;
    if (savings < 0.0f) return 0.0f;
    if (savings > 1.0f) return 1.0f;
    return savings;
}

/* --- self-test --------------------------------------------------------- */

struct row_route {
    float sigma_peak;
    float tau_escalate;
    cos_sigma_route_t expected;
};

static const struct row_route canonical[] = {
    { 0.00f, 0.70f, COS_SIGMA_ROUTE_LOCAL    },
    { 0.50f, 0.70f, COS_SIGMA_ROUTE_LOCAL    },
    { 0.69f, 0.70f, COS_SIGMA_ROUTE_LOCAL    },
    { 0.70f, 0.70f, COS_SIGMA_ROUTE_ESCALATE },
    { 0.99f, 0.70f, COS_SIGMA_ROUTE_ESCALATE },
    { 1.00f, 0.70f, COS_SIGMA_ROUTE_ESCALATE },
    /* Safe defaults. */
    { 0.0f / 0.0f, 0.70f, COS_SIGMA_ROUTE_ESCALATE },   /* NaN */
    { -1.0f,       0.70f, COS_SIGMA_ROUTE_ESCALATE },
};

static int check_canonical(void) {
    const size_t n = sizeof canonical / sizeof canonical[0];
    for (size_t i = 0; i < n; ++i) {
        cos_sigma_route_t r = cos_sigma_speculative_route(
            canonical[i].sigma_peak, canonical[i].tau_escalate);
        if (r != canonical[i].expected) return 1 + (int)i;
    }
    return 0;
}

static int check_monotone(void) {
    /* Route is monotone in σ_peak: once ESCALATE, never LOCAL. */
    cos_sigma_route_t prev = cos_sigma_speculative_route(0.0f, 0.5f);
    for (int i = 1; i <= 10000; ++i) {
        float sp = (float)i / 10000.0f;
        cos_sigma_route_t cur = cos_sigma_speculative_route(sp, 0.5f);
        if ((int)cur < (int)prev) return 20;
        prev = cur;
    }
    return 0;
}

static int check_peak_update(void) {
    float peak = 0.0f;
    const float samples[] = {0.1f, 0.3f, 0.2f, 0.8f, 0.5f, 0.6f};
    for (size_t i = 0; i < sizeof samples / sizeof samples[0]; ++i) {
        peak = cos_sigma_speculative_peak_update(peak, samples[i]);
    }
    if (peak != 0.8f) return 30;
    /* NaN leaves peak unchanged. */
    float peak2 = cos_sigma_speculative_peak_update(0.4f, 0.0f / 0.0f);
    if (peak2 != 0.4f) return 31;
    /* Negative leaves peak unchanged. */
    float peak3 = cos_sigma_speculative_peak_update(0.4f, -1.0f);
    if (peak3 != 0.4f) return 32;
    return 0;
}

static int check_segment_boundary(void) {
    if (!cos_sigma_speculative_is_segment_boundary('.'))   return 40;
    if (!cos_sigma_speculative_is_segment_boundary('!'))   return 41;
    if (!cos_sigma_speculative_is_segment_boundary('?'))   return 42;
    if (!cos_sigma_speculative_is_segment_boundary('\n'))  return 43;
    if (!cos_sigma_speculative_is_segment_boundary(';'))   return 44;
    if (cos_sigma_speculative_is_segment_boundary('a'))    return 45;
    if (cos_sigma_speculative_is_segment_boundary(' '))    return 46;
    if (cos_sigma_speculative_is_segment_boundary(','))    return 47;
    return 0;
}

static int check_cost_savings(void) {
    /* API-only baseline: 100 requests @ 0.02€ = 2.00€
     * Hybrid: 90 local @ 0.0005€ + 10 escalate @ (0.0005+0.02)€
     *       = 90·0.0005 + 10·0.0205 = 0.045 + 0.205 = 0.250€
     * Savings = (2.00 - 0.250) / 2.00 = 0.875 */
    float s = cos_sigma_speculative_cost_savings(100, 10, 0.0005f, 0.02f);
    if (!(s > 0.87f && s < 0.88f)) return 50;
    /* Never-escalate ≈ full savings when local is ≈ free. */
    float s0 = cos_sigma_speculative_cost_savings(100, 0, 0.0005f, 0.02f);
    if (!(s0 > 0.97f && s0 < 0.98f)) return 51;
    /* Always-escalate ≈ zero savings (slightly negative → clamped to 0). */
    float s1 = cos_sigma_speculative_cost_savings(100, 100, 0.0005f, 0.02f);
    if (s1 != 0.0f) return 52;
    /* Malformed inputs → 0. */
    if (cos_sigma_speculative_cost_savings( 0, 10, 0.001f, 0.02f) != 0.0f) return 53;
    if (cos_sigma_speculative_cost_savings(100, 200, 0.001f, 0.02f) != 0.0f) return 54;
    if (cos_sigma_speculative_cost_savings(100, 10, -0.1f, 0.02f) != 0.0f) return 55;
    if (cos_sigma_speculative_cost_savings(100, 10, 0.001f, 0.0f) != 0.0f) return 56;
    return 0;
}

static int check_lcg_grid(void) {
    uint32_t st = 0xDEADCAFEu;
    for (int i = 0; i < 100000; ++i) {
        st = st * 1664525u + 1013904223u;
        float sp  = (float)((st >> 8) & 0xFFFF) / 65535.0f;
        st = st * 1664525u + 1013904223u;
        float tau = (float)((st >> 8) & 0xFFFF) / 65535.0f;
        cos_sigma_route_t r = cos_sigma_speculative_route(sp, tau);
        if ((int)r < 0 || (int)r > 1) return 60;
    }
    return 0;
}

int cos_sigma_speculative_self_test(void) {
    int rc;
    if ((rc = check_canonical()))        return rc;
    if ((rc = check_monotone()))         return rc;
    if ((rc = check_peak_update()))      return rc;
    if ((rc = check_segment_boundary())) return rc;
    if ((rc = check_cost_savings()))     return rc;
    if ((rc = check_lcg_grid()))         return rc;
    return 0;
}
