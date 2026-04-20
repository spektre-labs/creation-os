/* σ-Sovereign self-test binary + canonical zero-cloud demo. */

#include "sovereign.h"

#include <stdio.h>
#include <string.h>

static const char *verdict_name(cos_sovereign_verdict_t v) {
    switch (v) {
        case COS_SOVEREIGN_FULL:      return "FULL";
        case COS_SOVEREIGN_HYBRID:    return "HYBRID";
        case COS_SOVEREIGN_DEPENDENT: return "DEPENDENT";
        default:                      return "?";
    }
}

int main(int argc, char **argv) {
    int rc = cos_sigma_sovereign_self_test();

    /* Canonical demo: 85 local calls (BitNet on M3 Air, ~€0.0001
     * each amortised electricity) + 15 cloud calls (Claude-Haiku
     * tier, ~€0.012 each).  fraction = 0.85, verdict = HYBRID,
     * eur_per_call ≈ €0.001885, monthly @ 100 calls/day ≈ €5.66. */
    cos_sigma_sovereign_t s;
    cos_sigma_sovereign_init(&s, 0.85f);
    for (int i = 0; i < 85; ++i)
        cos_sigma_sovereign_record_local(&s, 0.0001);
    for (int i = 0; i < 15; ++i)
        cos_sigma_sovereign_record_cloud(&s, 0.012, 1024, 4096);

    cos_sovereign_verdict_t v = cos_sigma_sovereign_verdict(&s);
    float frac = cos_sigma_sovereign_fraction(&s);
    double per_call = cos_sigma_sovereign_eur_per_call(&s);
    double monthly  = cos_sigma_sovereign_monthly_eur_estimate(&s, 100);

    /* Counterfactual: same workload, NO local — what would the
     * monthly estimate be?  100 calls × €0.012 × 30 = €36.0. */
    double cloud_only_monthly = 100 * 0.012 * 30.0;
    double saved_pct = (cloud_only_monthly > 0.0)
        ? 100.0 * (1.0 - monthly / cloud_only_monthly)
        : 0.0;

    printf("{\"kernel\":\"sigma_sovereign\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"min_sovereign_fraction\":%.4f,"
             "\"n_local\":%u,\"n_cloud\":%u,\"n_abstain\":%u,"
             "\"eur_local_total\":%.6f,"
             "\"eur_cloud_total\":%.6f,"
             "\"bytes_sent_cloud\":%llu,"
             "\"bytes_recv_cloud\":%llu,"
             "\"sovereign_fraction\":%.4f,"
             "\"eur_per_call\":%.6f,"
             "\"monthly_eur_at_100\":%.4f,"
             "\"cloud_only_monthly_eur\":%.4f,"
             "\"saved_pct\":%.2f,"
             "\"verdict\":\"%s\"},"
           "\"pass\":%s}\n",
           rc,
           (double)s.min_sovereign_fraction,
           s.n_local, s.n_cloud, s.n_abstain,
           s.eur_local_total, s.eur_cloud_total,
           (unsigned long long)s.bytes_sent_cloud,
           (unsigned long long)s.bytes_recv_cloud,
           (double)frac, per_call, monthly,
           cloud_only_monthly, saved_pct,
           verdict_name(v),
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr,
            "σ-Sovereign demo:\n"
            "  %u local, %u cloud, %u abstain → %.0f%% local\n"
            "  €%.4f / call  →  €%.2f / month at 100 calls/day\n"
            "  cloud-only equivalent: €%.2f / month  (saved %.1f%%)\n"
            "  verdict: %s\n",
            s.n_local, s.n_cloud, s.n_abstain, (double)(frac * 100.0f),
            per_call, monthly, cloud_only_monthly, saved_pct,
            verdict_name(v));
    }
    return rc;
}
