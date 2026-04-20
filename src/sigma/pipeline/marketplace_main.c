/* σ-Marketplace self-test + canonical demo.
 *
 * 4 providers: cheap (0.002€/q, σ=0.25), mid (0.008€/q, σ=0.12),
 * pro (0.020€/q, σ=0.08) all SCSL-accepted; rogue (0.001€/q,
 * σ=0.05) refuses SCSL.
 *
 * Scenarios:
 *   A. loose budget                → picks "cheap"  (lowest price)
 *   B. σ_budget 0.10               → picks "pro"    (only one under σ)
 *   C. σ_budget 0.10 + price ≤ .01 → NULL (mid/pro priced out,
 *                                          cheap over σ)
 *   D. rogue never wins, even with free price
 *   E. "mid" goes bad: 5× σ_result=0.95 → auto-banned
 */

#include "marketplace.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_mkt_self_test();

    cos_mkt_t mkt;
    cos_mkt_provider_t slots[6];
    cos_sigma_mkt_init(&mkt, slots, 6, 0.60f, 0.80f);

    cos_sigma_mkt_register(&mkt, "cheap", "m-small",  0.002, 0.25f, 120.0f, 1);
    cos_sigma_mkt_register(&mkt, "mid",   "m-medium", 0.008, 0.12f,  80.0f, 1);
    cos_sigma_mkt_register(&mkt, "pro",   "m-large",  0.020, 0.08f,  60.0f, 1);
    cos_sigma_mkt_register(&mkt, "rogue", "m-large",  0.001, 0.05f,  40.0f, 0);

    const cos_mkt_provider_t *pickA =
        cos_sigma_mkt_select(&mkt, 0.050, 0.30f, 200.0f);
    const cos_mkt_provider_t *pickB =
        cos_sigma_mkt_select(&mkt, 0.050, 0.10f, 200.0f);
    const cos_mkt_provider_t *pickC =
        cos_sigma_mkt_select(&mkt, 0.010, 0.10f, 200.0f);
    const cos_mkt_provider_t *pickD =
        cos_sigma_mkt_select(&mkt, 0.002, 0.10f, 200.0f);

    /* "mid" goes bad. */
    for (int i = 0; i < 5; ++i)
        cos_sigma_mkt_report(&mkt, "mid", 0.95f, 150.0f, 0.008, 0);

    int idx_mid = -1;  cos_sigma_mkt_find(&mkt, "mid", &idx_mid);

    /* After mid auto-bans, loose budget must now route to pro
     * (cheap still over σ). */
    const cos_mkt_provider_t *pickE =
        cos_sigma_mkt_select(&mkt, 0.050, 0.10f, 200.0f);

    printf("{\"kernel\":\"sigma_marketplace\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"providers\":%d,"
             "\"ban_floor\":%.2f,"
             "\"pick_loose\":\"%s\","
             "\"pick_tight_sigma\":\"%s\","
             "\"pick_tight_both\":\"%s\","
             "\"pick_rogue_filtered\":%s,"
             "\"mid_after_badrun\":{\"sigma\":%.4f,\"banned\":%s,"
                                    "\"failures\":%d,\"successes\":%d},"
             "\"pick_after_ban\":\"%s\","
             "\"total_queries\":%.0f,"
             "\"total_spend_eur\":%.4f"
             "},"
           "\"pass\":%s}\n",
           rc,
           mkt.count,
           (double)mkt.sigma_ban_floor,
           pickA ? pickA->provider_id : "null",
           pickB ? pickB->provider_id : "null",
           pickC ? pickC->provider_id : "null",
           pickD ? "false" : "true",
           (double)slots[idx_mid].mean_sigma,
           slots[idx_mid].banned ? "true" : "false",
           slots[idx_mid].n_failures,
           slots[idx_mid].n_successes,
           pickE ? pickE->provider_id : "null",
           mkt.total_queries,
           mkt.total_spend_eur,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Marketplace demo: loose→%s tight→%s after-ban→%s\n",
                pickA ? pickA->provider_id : "(null)",
                pickB ? pickB->provider_id : "(null)",
                pickE ? pickE->provider_id : "(null)");
    }
    return rc;
}
