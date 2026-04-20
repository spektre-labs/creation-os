/*
 * σ-watchdog demo.  Replays a synthetic 7-day production trace and
 * reports:  health, per-window σ means, trend, and the auto-heal
 * plan.  Deterministic: every field is derived from the scripted
 * samples and the fixed `now_s`.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "watchdog.h"

#include <stdio.h>

static void run_scenario(const char *label,
                         int drift,
                         int *first) {
    cos_watchdog_t w;
    cos_watchdog_init(&w);
    uint64_t now = 7ULL * COS_WATCHDOG_ONE_DAY_S;

    for (int d = 0; d < 7; d++) {
        for (int h = 0; h < 24; h++) {
            uint64_t ts = (uint64_t)d * COS_WATCHDOG_ONE_DAY_S
                        + (uint64_t)h * COS_WATCHDOG_ONE_HOUR_S;
            float sigma;
            int   esc;
            if (drift) {
                float frac = (float)(d * 24 + h) / (7.0f * 24.0f);
                sigma = 0.10f + 0.15f * frac;
                esc   = (((d * 24 + h) * 7 + 3) % 100)
                      < (int)(5 + 15 * frac) ? 1 : 0;
            } else {
                sigma = 0.15f;
                esc   = 0;
            }
            cos_watchdog_observe(&w, ts, sigma, esc, 0.0005f);
        }
    }
    cos_watchdog_check(&w, now);

    cos_watchdog_plan_t plan;
    cos_watchdog_plan(&w, &plan);

    if (!*first) printf(",\n");
    *first = 0;
    printf("    { \"label\": \"%s\",\n", label);
    printf("      \"count\": %d,\n", w.count);
    printf("      \"sigma_mean_1h\":  %.4f,\n", (double)w.sigma_mean_1h);
    printf("      \"sigma_mean_24h\": %.4f,\n", (double)w.sigma_mean_24h);
    printf("      \"sigma_mean_7d\":  %.4f,\n", (double)w.sigma_mean_7d);
    printf("      \"sigma_trend_per_day\": %.6f,\n",
           (double)w.sigma_trend_per_day);
    printf("      \"escal_rate_24h\": %.4f,\n", (double)w.escal_rate_24h);
    printf("      \"escal_trend_per_day\": %.6f,\n",
           (double)w.escal_trend_per_day);
    printf("      \"cost_today_eur\": %.4f,\n", (double)w.cost_today_eur);
    printf("      \"alert_sigma\": %s,\n", w.alert_sigma ? "true" : "false");
    printf("      \"alert_escal\": %s,\n", w.alert_escal ? "true" : "false");
    printf("      \"health\": \"%s\",\n", cos_watchdog_health_str(w.health));
    printf("      \"plan\": { \"remedy\": \"%s\", \"iterations\": %d,"
           " \"reason\": \"%s\" }\n    }",
           cos_watchdog_remedy_str(plan.remedy),
           plan.iterations, plan.reason);
}

int main(void) {
    int st = cos_watchdog_self_test();

    printf("{\n");
    printf("  \"kernel\": \"sigma_watchdog\",\n");
    printf("  \"self_test\": %d,\n", st);
    printf("  \"tau_sigma_up\": %.4f,\n", (double)COS_WATCHDOG_TAU_SIGMA_UP);
    printf("  \"tau_escal_up\": %.4f,\n", (double)COS_WATCHDOG_TAU_ESCAL_UP);
    printf("  \"scenarios\": [\n");
    int first = 1;
    run_scenario("drifting", 1, &first);
    run_scenario("steady",   0, &first);
    printf("\n  ]\n");
    printf("}\n");
    return st == 0 ? 0 : 1;
}
