/* cos cost — zero-cloud sovereignty ledger summary.
 *
 * Two modes:
 *
 *   (default)  Canonical "what does σ-gated Creation OS save?"
 *              projection: 85 local + 15 cloud calls per day,
 *              same as the sigma_sovereign self-test demo.
 *
 *   --from-benchmark   Re-run the benchmark fixture internally,
 *                      then report the actual ledger from the
 *                      pipeline_codex configuration.  This is
 *                      what `make check-cos-cost` pins.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "pipeline.h"
#include "codex.h"
#include "engram.h"
#include "sovereign.h"
#include "agent.h"
#include "stub_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *verdict_str(cos_sovereign_verdict_t v) {
    switch (v) {
        case COS_SOVEREIGN_FULL:      return "FULL";
        case COS_SOVEREIGN_HYBRID:    return "HYBRID";
        case COS_SOVEREIGN_DEPENDENT: return "DEPENDENT";
        default:                      return "?";
    }
}

static void print_report(const cos_sigma_sovereign_t *s, int calls_per_day) {
    float frac   = cos_sigma_sovereign_fraction(s);
    double per   = cos_sigma_sovereign_eur_per_call(s);
    double month = cos_sigma_sovereign_monthly_eur_estimate(s, calls_per_day);
    double eur   = s->eur_local_total + s->eur_cloud_total;
    cos_sovereign_verdict_t v = cos_sigma_sovereign_verdict(s);

    /* Cloud-only counterfactual: assume the exact same workload
     * would have cost €0.012 / call (Claude-Haiku tier). */
    double cf_month = 0.012 * (double)calls_per_day * 30.0;
    double saved = (cf_month > 0.0)
                  ? 100.0 * (1.0 - month / cf_month) : 0.0;

    printf("Creation OS — zero-cloud sovereignty ledger\n"
           "  calls         : %u local + %u cloud + %u abstain = %u\n"
           "  local fraction: %.1f%%  (verdict: %s)\n"
           "  cost now      : €%.4f total, €%.6f / call\n"
           "  projection @ %d calls/day × 30:\n"
           "     Creation OS  €%.2f / month\n"
           "     cloud-only   €%.2f / month\n"
           "     saved        %.1f%%\n"
           "  data out      : %llu bytes sent, %llu bytes received\n",
           s->n_local, s->n_cloud, s->n_abstain,
           s->n_local + s->n_cloud + s->n_abstain,
           (double)(frac * 100.0f), verdict_str(v),
           eur, per,
           calls_per_day, month, cf_month, saved,
           (unsigned long long)s->bytes_sent_cloud,
           (unsigned long long)s->bytes_recv_cloud);
}

static void canonical_ledger(cos_sigma_sovereign_t *s) {
    cos_sigma_sovereign_init(s, 0.85f);
    for (int i = 0; i < 85; ++i) cos_sigma_sovereign_record_local(s, 0.0001);
    for (int i = 0; i < 15; ++i) cos_sigma_sovereign_record_cloud(s, 0.012,
                                                                  1024, 4096);
}

static void from_benchmark(cos_sigma_sovereign_t *out_sv) {
    static const char *FIX[] = {
        "low:what is 2+2?",     "low:capital of France?",
        "low:define gravity",   "low:describe entropy",
        "low:what is a prime?", "low:what is recursion?",
        "improve:explain GANs", "improve:explain entanglement",
        "mid:prove P=NP",       "high:unfalsifiable question",
    };
    enum { N = 10 };

    cos_sigma_sovereign_init(out_sv, 0.85f);
    cos_sigma_agent_t ag;
    cos_sigma_agent_init(&ag, 0.80f, 0.10f);
    cos_sigma_codex_t codex;
    memset(&codex, 0, sizeof(codex));
    cos_sigma_codex_load(NULL, &codex);

    enum { CAP = 32 };
    cos_sigma_engram_entry_t slots[CAP];
    memset(slots, 0, sizeof(slots));
    cos_sigma_engram_t engram;
    cos_sigma_engram_init(&engram, slots, CAP, 0.25f, 200, 20);

    cos_pipeline_config_t cfg;
    cos_sigma_pipeline_config_defaults(&cfg);
    cfg.tau_accept   = 0.40f;
    cfg.tau_rethink  = 0.60f;
    cfg.max_rethink  = 3;
    cfg.mode         = COS_PIPELINE_MODE_HYBRID;
    cfg.codex        = &codex;
    cfg.engram       = &engram;
    cfg.sovereign    = out_sv;
    cfg.agent        = &ag;
    cfg.generate     = cos_cli_stub_generate;
    cfg.escalate     = cos_cli_stub_escalate;

    for (int i = 0; i < N; ++i) {
        cos_pipeline_result_t r;
        cos_sigma_pipeline_run(&cfg, FIX[i], &r);
    }

    cos_sigma_pipeline_free_engram_values(&engram);
    cos_sigma_codex_free(&codex);
}

int main(int argc, char **argv) {
    int from_bench = 0;
    int calls_per_day = 100;
    int json = 0;
    for (int i = 1; i < argc; ++i) {
        if      (strcmp(argv[i], "--from-benchmark") == 0) from_bench = 1;
        else if (strcmp(argv[i], "--json")           == 0) json = 1;
        else if (strcmp(argv[i], "--calls-per-day") == 0 && i + 1 < argc)
            calls_per_day = atoi(argv[++i]);
        else if (strcmp(argv[i], "--help") == 0) {
            printf("cos cost [--from-benchmark] [--calls-per-day N] [--json]\n");
            return 0;
        }
    }

    cos_sigma_sovereign_t sv;
    if (from_bench) from_benchmark(&sv);
    else            canonical_ledger(&sv);

    if (json) {
        float frac = cos_sigma_sovereign_fraction(&sv);
        double per = cos_sigma_sovereign_eur_per_call(&sv);
        double mo  = cos_sigma_sovereign_monthly_eur_estimate(&sv, calls_per_day);
        double cf  = 0.012 * calls_per_day * 30.0;
        double sv_pct = (cf > 0.0) ? 100.0 * (1.0 - mo / cf) : 0.0;
        printf("{\"source\":\"%s\",\"calls_per_day\":%d,"
               "\"n_local\":%u,\"n_cloud\":%u,\"n_abstain\":%u,"
               "\"sovereign_fraction\":%.4f,"
               "\"eur_per_call\":%.6f,"
               "\"monthly_eur\":%.4f,"
               "\"cloud_only_monthly_eur\":%.4f,"
               "\"saved_pct\":%.2f,"
               "\"verdict\":\"%s\"}\n",
               from_bench ? "benchmark" : "canonical",
               calls_per_day,
               sv.n_local, sv.n_cloud, sv.n_abstain,
               (double)frac, per, mo, cf, sv_pct,
               verdict_str(cos_sigma_sovereign_verdict(&sv)));
    } else {
        print_report(&sv, calls_per_day);
        printf("\n  1 = 1.\n");
    }
    return 0;
}
