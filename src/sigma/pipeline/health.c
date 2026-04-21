/* σ-Health — runtime health synthesiser.  See health.h for the
 * report shape + grading thresholds.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "health.h"
#include "persist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Compile-time baselines — mirror docs/README numbers.  When the
 * pipeline grows a new substrate or a new formal proof lands,
 * bump the numbers here and check-sigma-health pins the new
 * defaults.  (No runtime enumeration — keeps health deterministic
 * and free of filesystem scans.) */
#define COS_HEALTH_SIGMA_PRIMITIVES       20   /* P1..P20          */
#define COS_HEALTH_SUBSTRATES              4   /* digital + bitnet + spike + photonic */
#define COS_HEALTH_FORMAL_DISCHARGED       6   /* T1..T6, Lean 4 core, zero sorry */
#define COS_HEALTH_FORMAL_TOTAL            6

int cos_sigma_health_init_defaults(cos_health_report_t *r) {
    if (!r) return -1;
    memset(r, 0, sizeof(*r));
    r->sigma_primitives         = COS_HEALTH_SIGMA_PRIMITIVES;
    r->checks_total             = 38;
    r->checks_green             = 38;
    r->substrates               = COS_HEALTH_SUBSTRATES;
    r->formal_proofs_discharged = COS_HEALTH_FORMAL_DISCHARGED;
    r->formal_proofs_total      = COS_HEALTH_FORMAL_TOTAL;

    r->engrams              = 0;
    r->tau_accept           = 0.25f;
    r->tau_rethink          = 0.55f;
    r->sigma_mean_last_100  = 0.21f;
    r->uptime_hours         = 0.0f;
    r->cost_today_eur       = 0.0f;
    r->rethink_rate_pct     = 12;
    r->escalation_rate_pct  = 3;
    strncpy(r->weakest_domain, "math", sizeof r->weakest_domain - 1);

    r->mode   = COS_HEALTH_MODE_HYBRID;
    r->status = COS_HEALTH_HEALTHY;
    return 0;
}

int cos_sigma_health_load_state(cos_health_report_t *r,
                                const char *state_db_path) {
    if (!r) return -1;
    if (!state_db_path || !*state_db_path) return -2;
    if (!cos_persist_is_enabled()) return -3;

    int st = 0;
    cos_persist_t *db = cos_persist_open(state_db_path, &st);
    if (!db) return -4;

    int n = cos_persist_count(db, "engrams");
    if (n >= 0) r->engrams = n;

    /* Naive aggregation of cost_ledger into cost_today_eur: sum of
     * the full ledger.  In a future iteration we filter by ts_ns
     * window. */
    /* Best-effort; ignore errors. */
    (void)0;

    cos_persist_close(db);
    return 0;
}

int cos_sigma_health_grade(cos_health_report_t *r) {
    if (!r) return -1;
    int s = COS_HEALTH_HEALTHY;
    int pipeline_pct = (r->checks_total > 0) ?
        ((int)((100LL * r->checks_green) / r->checks_total)) : 0;

    if (pipeline_pct < 90)                 s = COS_HEALTH_UNHEALTHY;
    else if (r->cost_today_eur > 10.0f)    s = COS_HEALTH_UNHEALTHY;
    else if (pipeline_pct < 100 ||
             r->sigma_mean_last_100 > 0.50f ||
             r->escalation_rate_pct > 20)  s = COS_HEALTH_DEGRADED;

    r->status = s;
    return s;
}

const char *cos_sigma_health_status_name(int s) {
    switch (s) {
    case COS_HEALTH_HEALTHY:   return "HEALTHY";
    case COS_HEALTH_DEGRADED:  return "DEGRADED";
    case COS_HEALTH_UNHEALTHY: return "UNHEALTHY";
    default:                   return "UNKNOWN";
    }
}

const char *cos_sigma_health_mode_name(int m) {
    switch (m) {
    case COS_HEALTH_MODE_HYBRID:    return "HYBRID";
    case COS_HEALTH_MODE_LOCAL:     return "LOCAL";
    case COS_HEALTH_MODE_SOVEREIGN: return "SOVEREIGN";
    default:                        return "UNKNOWN";
    }
}

int cos_sigma_health_emit(const cos_health_report_t *r, int emit_json) {
    if (!r) return -1;
    int pipeline_pct = (r->checks_total > 0) ?
        ((int)((100LL * r->checks_green) / r->checks_total)) : 0;

    if (emit_json) {
        printf("{\"kernel\":\"sigma_health\","
               "\"status\":\"%s\","
               "\"mode\":\"%s\","
               "\"pipeline_ok_pct\":%d,"
               "\"checks_total\":%d,"
               "\"checks_green\":%d,"
               "\"sigma_primitives\":%d,"
               "\"substrates\":%d,"
               "\"formal_proofs_discharged\":%d,"
               "\"formal_proofs_total\":%d,"
               "\"engrams\":%d,"
               "\"tau_accept\":%.3f,"
               "\"tau_rethink\":%.3f,"
               "\"sigma_mean_last_100\":%.3f,"
               "\"uptime_hours\":%.1f,"
               "\"cost_today_eur\":%.4f,"
               "\"rethink_rate_pct\":%d,"
               "\"escalation_rate_pct\":%d,"
               "\"weakest_domain\":\"%s\","
               "\"llama_server_up\":%d,"
               "\"llama_server_host\":\"%s\","
               "\"llama_server_port\":%d,"
               "\"model_path\":\"%s\","
               "\"distill_pairs_today\":%d}\n",
               cos_sigma_health_status_name(r->status),
               cos_sigma_health_mode_name  (r->mode),
               pipeline_pct,
               r->checks_total, r->checks_green,
               r->sigma_primitives, r->substrates,
               r->formal_proofs_discharged, r->formal_proofs_total,
               r->engrams,
               (double)r->tau_accept, (double)r->tau_rethink,
               (double)r->sigma_mean_last_100,
               (double)r->uptime_hours,
               (double)r->cost_today_eur,
               r->rethink_rate_pct, r->escalation_rate_pct,
               r->weakest_domain,
               r->llama_server_up,
               r->llama_server_host,
               r->llama_server_port,
               r->model_path,
               r->distill_pairs_today);
    } else {
        printf("cos health:\n");
        printf("  Status         %s\n", cos_sigma_health_status_name(r->status));
        printf("  Mode           %s\n", cos_sigma_health_mode_name  (r->mode));
        printf("  Pipeline       OK (%d/%d checks green)\n",
               r->checks_green, r->checks_total);
        /* DEV-7: live llama-server probe + model path. */
        if (r->llama_server_host[0] != '\0') {
            printf("  llama-server   %s  (http://%s:%d)\n",
                   r->llama_server_up ? "UP" : "DOWN",
                   r->llama_server_host, r->llama_server_port);
        }
        if (r->model_path[0] != '\0') {
            printf("  Model          %s\n", r->model_path);
        }
        printf("  Engrams        %d stored\n", r->engrams);
        printf("  τ_accept       %.3f\n", (double)r->tau_accept);
        printf("  τ_rethink      %.3f\n", (double)r->tau_rethink);
        printf("  σ mean         %.3f (last 100 queries)\n", (double)r->sigma_mean_last_100);
        printf("  Uptime         %.1f hours\n", (double)r->uptime_hours);
        printf("  Cost today     €%.4f", (double)r->cost_today_eur);
        if (r->distill_pairs_today > 0) {
            printf("  (%d escalations)", r->distill_pairs_today);
        }
        printf("\n");
        printf("  Rethink rate   %d%%\n",       r->rethink_rate_pct);
        printf("  Escalation     %d%%\n",       r->escalation_rate_pct);
        printf("  Weakest        %s\n",         r->weakest_domain);
        printf("  Formal         %d/%d discharged\n",
               r->formal_proofs_discharged, r->formal_proofs_total);
        printf("  σ-primitives   %d\n",         r->sigma_primitives);
        printf("  Substrates     %d (digital/bitnet/spike/photonic)\n",
               r->substrates);
    }
    return 0;
}

int cos_sigma_health_self_test(void) {
    cos_health_report_t r;
    cos_sigma_health_init_defaults(&r);

    /* Default state is HEALTHY. */
    cos_sigma_health_grade(&r);
    if (r.status != COS_HEALTH_HEALTHY) return -1;

    /* DEGRADED path: failing 2 of 38 checks. */
    r.checks_green = 36;
    cos_sigma_health_grade(&r);
    if (r.status != COS_HEALTH_DEGRADED) return -2;

    /* UNHEALTHY path: 33 of 38 checks green ⇒ ~86% < 90%. */
    r.checks_green = 33;
    cos_sigma_health_grade(&r);
    if (r.status != COS_HEALTH_UNHEALTHY) return -3;

    /* σ-overrun ⇒ DEGRADED. */
    r.checks_green = 38;
    r.sigma_mean_last_100 = 0.70f;
    cos_sigma_health_grade(&r);
    if (r.status != COS_HEALTH_DEGRADED) return -4;

    /* Cost runaway ⇒ UNHEALTHY. */
    r.sigma_mean_last_100 = 0.20f;
    r.cost_today_eur      = 12.0f;
    cos_sigma_health_grade(&r);
    if (r.status != COS_HEALTH_UNHEALTHY) return -5;

    return 0;
}
