/* Entry point for the σ-speculative self-test binary. */

#include "speculative.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_speculative_self_test();
    float s = cos_sigma_speculative_cost_savings(100, 10, 0.0005f, 0.02f);
    printf("{\"kernel\":\"sigma_speculative\","
           "\"self_test_rc\":%d,"
           "\"canonical_routes\":[\"LOCAL\",\"ESCALATE\"],"
           "\"cost_savings_demo\":{"
             "\"n_total\":100,\"n_escalated\":10,"
             "\"eur_local\":0.0005,\"eur_api\":0.02,"
             "\"savings\":%.4f},"
           "\"pass\":%s}\n",
           rc, (double)s, (rc == 0) ? "true" : "false");
    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        const float grid[] = {0.10f, 0.40f, 0.69f, 0.70f, 0.99f};
        for (size_t i = 0; i < sizeof grid / sizeof grid[0]; ++i) {
            cos_sigma_route_t r =
                cos_sigma_speculative_route(grid[i], 0.70f);
            fprintf(stderr, "  σ_peak=%.2f  τ=0.70  →  %s\n",
                    (double)grid[i], cos_sigma_route_label(r));
        }
    }
    return rc;
}
