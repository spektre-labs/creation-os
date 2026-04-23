/*
 * Green score — composite 0–100 sustainability index from measured energy.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "green_score.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#define CREATION_OS_ENABLE_SELF_TESTS 0
#endif

/* Literature-style counterfactuals for comparison displays (research only). */
#define COS_GREEN_CLOUD_KWH_PER_QUERY  0.003f
#define COS_GREEN_LOCAL_REF_KWH        0.00001f

static float env_ratio(const char *key, float def)
{
    const char *e = getenv(key);
    return (e && e[0]) ? (float)atof(e) : def;
}

static float clamp01(float x)
{
    if (x < 0.f)
        return 0.f;
    if (x > 1.f)
        return 1.f;
    return x;
}

static char grade_from_score(float s)
{
    if (s > 90.f)
        return 'A';
    if (s > 75.f)
        return 'B';
    if (s > 60.f)
        return 'C';
    if (s > 40.f)
        return 'D';
    return 'F';
}

struct cos_green_score cos_green_calculate(
    const struct cos_energy_measurement *lifetime)
{
    struct cos_green_score g;
    float                  joules;
    float                  tok_per_j;
    float                  eff_n;
    float                  kwh_loc;
    float                  co2_actual_g;
    float                  co2_cloud_total_g;
    float                  grid;
    int64_t                nq;

    memset(&g, 0, sizeof g);

    g.cache_hit_ratio      = clamp01(env_ratio("COS_GREEN_CACHE_HIT", 0.25f));
    g.abstain_ratio        = clamp01(env_ratio("COS_GREEN_ABSTAIN", 0.08f));
    g.spike_suppress_ratio = clamp01(env_ratio("COS_GREEN_SPIKE", 0.15f));
    g.locality_ratio       = clamp01(env_ratio("COS_GREEN_LOCALITY", 0.92f));

    joules = lifetime ? lifetime->estimated_joules : 0.f;
    if (!lifetime || joules <= 1e-12f
        || lifetime->tokens_generated <= 0) {
        g.energy_efficiency = 0.f;
        eff_n               = 0.f;
        tok_per_j           = 0.f;
    } else {
        tok_per_j           = (float)lifetime->tokens_generated / joules;
        g.energy_efficiency = tok_per_j;
        eff_n               = clamp01(tok_per_j / 500.f);
    }

    kwh_loc        = joules / 3600000.f;
    grid           = env_ratio("COS_ENERGY_GRID_G", 230.f);
    co2_actual_g = lifetime ? lifetime->co2_grams : 0.f;
    nq           = cos_energy_lifetime_query_count();

    co2_cloud_total_g =
        (float)((double)nq * (double)COS_GREEN_CLOUD_KWH_PER_QUERY
                * (double)grid);

    if (kwh_loc > 1e-18f && COS_GREEN_CLOUD_KWH_PER_QUERY > 0.f) {
        g.vs_gpt5_ratio =
            COS_GREEN_CLOUD_KWH_PER_QUERY
            / (kwh_loc + COS_GREEN_LOCAL_REF_KWH * 1e-6f);
        if (g.vs_gpt5_ratio > 50000.f)
            g.vs_gpt5_ratio = 50000.f;
    } else {
        g.vs_gpt5_ratio = 300.f;
    }

    g.vs_cloud_ratio = g.locality_ratio * g.vs_gpt5_ratio
                       * (1.f + g.cache_hit_ratio);

    if (nq > 0 && co2_cloud_total_g > co2_actual_g) {
        g.co2_saved_lifetime_kg =
            (co2_cloud_total_g - co2_actual_g) / 1000.f;
    } else {
        g.co2_saved_lifetime_kg = 0.f;
    }
    if (!isfinite(g.co2_saved_lifetime_kg))
        g.co2_saved_lifetime_kg = 0.f;

    g.green_score =
        22.f * eff_n + 18.f * g.cache_hit_ratio
        + 13.f * g.abstain_ratio + 13.f * g.spike_suppress_ratio
        + 18.f * g.locality_ratio + 16.f * clamp01(g.vs_gpt5_ratio / 400.f);

    if (!isfinite(g.green_score))
        g.green_score = 0.f;
    if (g.green_score > 100.f)
        g.green_score = 100.f;

    g.grade = grade_from_score(g.green_score);

    return g;
}

void cos_green_print(const struct cos_green_score *g)
{
    if (!g)
        return;
    printf(
        "green_score: %.2f (%c)\n"
        "  tokens/J: %.8f | cache %.0f%% | abstain %.0f%% | spike %.0f%% | "
        "local %.0f%%\n"
        "  vs GPT-class cloud kWh/query ≈ %.1fx greener | CO2 saved (est.) "
        "%.6f kg\n",
        (double)g->green_score, g->grade, (double)g->energy_efficiency,
        (double)(g->cache_hit_ratio * 100.f),
        (double)(g->abstain_ratio * 100.f),
        (double)(g->spike_suppress_ratio * 100.f),
        (double)(g->locality_ratio * 100.f), (double)g->vs_gpt5_ratio,
        (double)g->co2_saved_lifetime_kg);
}

char *cos_green_to_json(const struct cos_green_score *g)
{
    char *out;
    int   n;

    if (!g)
        return NULL;
    out = (char *)malloc(768);
    if (!out)
        return NULL;
    n = snprintf(out, 768,
                 "{\"energy_efficiency\":%.10f,"
                 "\"cache_hit_ratio\":%.6f,"
                 "\"abstain_ratio\":%.6f,"
                 "\"spike_suppress_ratio\":%.6f,"
                 "\"locality_ratio\":%.6f,"
                 "\"green_score\":%.6f,"
                 "\"grade\":\"%c\","
                 "\"vs_gpt5_ratio\":%.6f,"
                 "\"vs_cloud_ratio\":%.6f,"
                 "\"co2_saved_lifetime_kg\":%.10f}",
                 (double)g->energy_efficiency, (double)g->cache_hit_ratio,
                 (double)g->abstain_ratio, (double)g->spike_suppress_ratio,
                 (double)g->locality_ratio, (double)g->green_score, g->grade,
                 (double)g->vs_gpt5_ratio, (double)g->vs_cloud_ratio,
                 (double)g->co2_saved_lifetime_kg);
    if (n <= 0 || n >= 768) {
        free(out);
        return NULL;
    }
    return out;
}

#if CREATION_OS_ENABLE_SELF_TESTS
static int gs_fail(const char *m)
{
    fprintf(stderr, "green_score self-test: %s\n", m);
    return 1;
}
#endif

int cos_green_self_test(void)
{
#if CREATION_OS_ENABLE_SELF_TESTS
    struct cos_energy_measurement l;
    struct cos_green_score         g;
    char                          *js;

    memset(&l, 0, sizeof l);
    l.estimated_joules      = 0.05f;
    l.tokens_generated      = 40;
    l.co2_grams             = 0.002f;
    g                       = cos_green_calculate(&l);
    if (g.green_score < 0.f || g.green_score > 100.f)
        return gs_fail("score_range");

    js = cos_green_to_json(&g);
    if (!js || !strstr(js, "green_score"))
        return gs_fail("json");
    free(js);

    fprintf(stderr, "green_score self-test: OK\n");
#endif
    return 0;
}
