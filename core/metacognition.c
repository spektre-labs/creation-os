#include "metacognition.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define META_SIG_HIST 1000

static int    g_total;
static int    g_tier_counts[8];
static int    g_verified;
static int    g_killed;
static int    g_unknown;
static float  g_sigma_hist[META_SIG_HIST];
static int    g_sigma_idx;
static int    g_sigma_filled;

static int g_epi_hyp;
static int g_epi_attested;
static int g_epi_shadow;
static int g_epi_fail;

void meta_reset(void) {
    g_total = 0;
    memset(g_tier_counts, 0, sizeof g_tier_counts);
    g_verified = 0;
    g_killed = 0;
    g_unknown = 0;
    memset(g_sigma_hist, 0, sizeof g_sigma_hist);
    g_sigma_idx = 0;
    g_sigma_filled = 0;
    g_epi_hyp = g_epi_attested = g_epi_shadow = g_epi_fail = 0;
}

void meta_update_from_response(const alm_response_t *r) {
    if (!r)
        return;
    g_total++;
    int t = r->tier;
    if (t < 0)
        t = 0;
    if (t >= 8)
        t = 7;
    g_tier_counts[t]++;
    if (r->attest == ALM_ATTESTED)
        g_verified++;
    else if (r->attest == ALM_HALLUCINATION)
        g_killed++;
    else if (r->attest == ALM_UNKNOWN)
        g_unknown++;
    g_sigma_hist[g_sigma_idx % META_SIG_HIST] = r->sigma;
    g_sigma_idx++;
    if (g_sigma_filled < META_SIG_HIST)
        g_sigma_filled++;
}

int meta_total_queries(void) { return g_total; }

float meta_confidence(void) {
    if (g_total == 0)
        return 0.f;
    return (float)g_verified / (float)g_total;
}

float meta_sigma_trend(void) {
    if (g_sigma_filled < 100)
        return 0.f;
    int n = 50;
    float recent = 0.f, old = 0.f;
    for (int i = 0; i < n; i++) {
        int ir = (g_sigma_idx - 1 - i + META_SIG_HIST * 1000) % META_SIG_HIST;
        int io = (g_sigma_idx - 1 - n - i + META_SIG_HIST * 1000) % META_SIG_HIST;
        recent += g_sigma_hist[ir];
        old += g_sigma_hist[io];
    }
    return (old / (float)n) - (recent / (float)n);
}

void meta_llm_and_tier_stats(int *out_llm_steps, float *out_llm_pct, int tier_counts_out[8]) {
    if (out_llm_steps)
        *out_llm_steps = g_tier_counts[2];
    if (out_llm_pct && g_total > 0)
        *out_llm_pct = 100.f * (float)g_tier_counts[2] / (float)g_total;
    else if (out_llm_pct)
        *out_llm_pct = 0.f;
    if (tier_counts_out)
        memcpy(tier_counts_out, g_tier_counts, sizeof g_tier_counts);
}

void meta_print_report(void) {
    printf("\n=== METACOGNITION REPORT ===\n");
    printf("Total queries:     %d\n", g_total);
    if (g_total == 0) {
        printf("(no data)\n");
        return;
    }
    printf("Tier 0 (instant):  %d (%.1f%%)\n", g_tier_counts[0],
           100.0 * (double)g_tier_counts[0] / (double)g_total);
    printf("Tier 1 (near):     %d (%.1f%%)\n", g_tier_counts[1],
           100.0 * (double)g_tier_counts[1] / (double)g_total);
    printf("Tier 2 (LLM ok):   %d (%.1f%%)\n", g_tier_counts[2],
           100.0 * (double)g_tier_counts[2] / (double)g_total);
    printf("Tier 3+ (reject):  %d (%.1f%%)\n", g_tier_counts[3],
           100.0 * (double)g_tier_counts[3] / (double)g_total);
    printf("Verified (ATTEST): %d (%.1f%%)\n", g_verified,
           100.0 * (double)g_verified / (double)g_total);
    printf("Killed (HALLU):    %d\n", g_killed);
    printf("Unknown:           %d\n", g_unknown);
    printf("Confidence:        %.1f%%\n", (double)meta_confidence() * 100.0);
    float tr = meta_sigma_trend();
    printf("sigma trend:       %.4f (%s)\n", (double)tr, tr > 0.f ? "IMPROVING" : "FLAT/DEGRADING");
    printf("LLM usage (tier 2 only): %.1f%%\n", 100.0 * (double)g_tier_counts[2] / (double)g_total);
    if (g_epi_hyp > 0) {
        float sr = (float)g_epi_attested / (float)g_epi_hyp;
        printf("--- Epistemic (Δ) ---\n");
        printf("Hypotheses run:    %d\n", g_epi_hyp);
        printf("Attested σ<0.1:   %d\n", g_epi_attested);
        printf("Shadowed:          %d\n", g_epi_shadow);
        printf("Failures:          %d\n", g_epi_fail);
        printf("Epistemic success: %.1f%%\n", 100.0 * (double)sr);
    }
}

void meta_epistemic_hypothesis_logged(const char *key_a, const char *key_b, float dist) {
    (void)key_a;
    (void)key_b;
    (void)dist;
    g_epi_hyp++;
}

void meta_epistemic_success_logged(void) { g_epi_attested++; }

void meta_epistemic_failure_logged(void) { g_epi_fail++; }

void meta_epistemic_shadow_logged(void) { g_epi_shadow++; }

void meta_epistemic_get_stats(int *out_hyp, int *out_attested, int *out_shadow, int *out_fail,
                              float *out_success_rate) {
    if (out_hyp)
        *out_hyp = g_epi_hyp;
    if (out_attested)
        *out_attested = g_epi_attested;
    if (out_shadow)
        *out_shadow = g_epi_shadow;
    if (out_fail)
        *out_fail = g_epi_fail;
    if (out_success_rate)
        *out_success_rate = (g_epi_hyp > 0) ? (float)g_epi_attested / (float)g_epi_hyp : 0.f;
}
