/*
 * Energy accounting — CPU process time × TDP proxy → joules, CO₂, €.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "energy_accounting.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#if defined(__APPLE__)
#include <mach/mach_time.h>
#endif

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#define CREATION_OS_ENABLE_SELF_TESTS 0
#endif

#define COS_ENERGY_DEFAULT_GRID_G    230.f
#define COS_ENERGY_DEFAULT_EUR_KWH   0.15f
#define COS_ENERGY_DEFAULT_TDP       15.f

static struct cos_energy_config    g_cfg;
static struct cos_energy_measurement g_session;
static struct cos_energy_measurement g_lifetime;
static int64_t                       g_lifetime_tokens;
static int64_t                       g_lifetime_queries;
static int                           g_init;
static float                         g_ctx_sigma    = 0.5f;
static float                         g_ctx_reason   = 1.f;

static int64_t wall_ms_now(void)
{
#if defined(CLOCK_REALTIME)
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return (int64_t)time(NULL) * 1000LL;
    return (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
#else
    return (int64_t)time(NULL) * 1000LL;
#endif
}

static void energy_default_path(char *buf, size_t cap)
{
    const char *h = getenv("HOME");
    if (!h || !h[0])
        h = "/tmp";
    snprintf(buf, cap, "%s/.cos/energy_state.txt", h);
}

static int energy_mkparent(const char *path)
{
    char b[768];
    char *slash;

    snprintf(b, sizeof b, "%s", path);
    slash = strrchr(b, '/');
    if (!slash || slash == b)
        return 0;
    *slash = '\0';
    return mkdir(b, 0700) == 0 || errno == EEXIST ? 0 : -1;
}

static float joules_from_cpu_ms(float cpu_ms, float tdp_w)
{
    double cpu_s = (double)cpu_ms / 1000.0;
    return (float)(cpu_s * (double)tdp_w);
}

static void fill_measurement(struct cos_energy_measurement *m,
                             float cpu_ms, float wall_ms, int tokens)
{
    double kwh;
    double j;

    memset(m, 0, sizeof(*m));
    m->cpu_time_ms   = cpu_ms;
    m->wall_time_ms  = wall_ms > 0.f ? wall_ms : cpu_ms;
    m->tokens_generated = tokens;
    m->timestamp_ms     = wall_ms_now();

    j              = joules_from_cpu_ms(cpu_ms, g_cfg.tdp_watts);
    m->estimated_joules = (float)j;

    if (m->wall_time_ms > 1e-6f) {
        double ws       = (double)m->wall_time_ms / 1000.0;
        m->estimated_watts_avg = (float)(j / ws);
    } else {
        m->estimated_watts_avg = g_cfg.tdp_watts;
    }

    kwh            = j / 3600000.0;
    m->co2_grams   = (float)(kwh * (double)g_cfg.grid_co2_per_kwh);
    m->cost_eur    = (float)(kwh * (double)g_cfg.electricity_eur_per_kwh);

    if (tokens > 0) {
        m->joules_per_token = m->estimated_joules / (float)tokens;
        m->co2_per_token    = m->co2_grams / (float)tokens;
    }

    if (j > 1e-12) {
        m->sigma_per_joule      = (1.f - g_ctx_sigma) / (float)j;
        m->reasoning_per_joule  = g_ctx_reason / (float)j;
    }
}

int cos_energy_init(const struct cos_energy_config *config)
{
    memset(&g_cfg, 0, sizeof g_cfg);
    g_cfg.grid_co2_per_kwh       = COS_ENERGY_DEFAULT_GRID_G;
    g_cfg.electricity_eur_per_kwh = COS_ENERGY_DEFAULT_EUR_KWH;
    g_cfg.tdp_watts              = COS_ENERGY_DEFAULT_TDP;

    if (config != NULL) {
        if (config->grid_co2_per_kwh > 0.f)
            g_cfg.grid_co2_per_kwh = config->grid_co2_per_kwh;
        if (config->electricity_eur_per_kwh > 0.f)
            g_cfg.electricity_eur_per_kwh = config->electricity_eur_per_kwh;
        if (config->tdp_watts > 0.f)
            g_cfg.tdp_watts = config->tdp_watts;
    }

    {
        char p[512];
        const char *e = getenv("COS_ENERGY_GRID_G");
        const char *u = getenv("COS_ENERGY_EUR_KWH");
        const char *w = getenv("COS_ENERGY_TDP_W");

        if (e && e[0])
            g_cfg.grid_co2_per_kwh = (float)atof(e);
        if (u && u[0])
            g_cfg.electricity_eur_per_kwh = (float)atof(u);
        if (w && w[0])
            g_cfg.tdp_watts = (float)atof(w);

        energy_default_path(p, sizeof p);
        (void)cos_energy_load(p);
    }

    g_init = 1;
    return 0;
}

void cos_energy_reset_session(void)
{
    memset(&g_session, 0, sizeof g_session);
}

const struct cos_energy_measurement *cos_energy_session_total(void)
{
    return &g_session;
}

const struct cos_energy_measurement *cos_energy_lifetime_total(void)
{
    return &g_lifetime;
}

void cos_energy_context(float sigma_next, float reasoning_units)
{
    if (sigma_next >= 0.f && sigma_next <= 1.f)
        g_ctx_sigma = sigma_next;
    if (reasoning_units >= 0.f)
        g_ctx_reason = reasoning_units;
}

struct cos_energy_measurement cos_energy_measure(float cpu_time_ms,
                                                 int              tokens_generated)
{
    return cos_energy_measure_wall(cpu_time_ms, cpu_time_ms, tokens_generated);
}

struct cos_energy_measurement cos_energy_measure_wall(
    float cpu_time_ms, float wall_time_ms, int tokens_generated)
{
    struct cos_energy_measurement m;

    if (!g_init)
        (void)cos_energy_init(NULL);

    fill_measurement(&m, cpu_time_ms, wall_time_ms, tokens_generated);

    cos_energy_accumulate(&g_session, &m);
    cos_energy_accumulate(&g_lifetime, &m);
    g_lifetime_tokens += tokens_generated > 0 ? tokens_generated : 0;
    g_lifetime_queries++;

    return m;
}

void cos_energy_accumulate(struct cos_energy_measurement       *total,
                           const struct cos_energy_measurement *current)
{
    if (!total || !current)
        return;
    total->cpu_time_ms += current->cpu_time_ms;
    total->wall_time_ms += current->wall_time_ms;
    total->estimated_joules += current->estimated_joules;
    total->co2_grams += current->co2_grams;
    total->cost_eur += current->cost_eur;
    total->tokens_generated += current->tokens_generated;
    total->cache_savings_joules += current->cache_savings_joules;
    total->abstain_savings_joules += current->abstain_savings_joules;
    total->spike_savings_joules += current->spike_savings_joules;
    if (total->wall_time_ms > 1e-6f) {
        double ws = (double)total->wall_time_ms / 1000.0;
        total->estimated_watts_avg =
            (float)((double)total->estimated_joules / ws);
    }
    if (total->tokens_generated > 0) {
        total->joules_per_token =
            total->estimated_joules / (float)total->tokens_generated;
        total->co2_per_token =
            total->co2_grams / (float)total->tokens_generated;
    }
}

char *cos_energy_to_json(const struct cos_energy_measurement *m)
{
    char *out;
    int   n;

    if (!m)
        return NULL;
    out = (char *)malloc(1536);
    if (!out)
        return NULL;
    n = snprintf(out, 1536,
                 "{\"cpu_time_ms\":%.6f,\"wall_time_ms\":%.6f,"
                 "\"estimated_joules\":%.8f,\"estimated_watts_avg\":%.6f,"
                 "\"co2_grams\":%.8f,\"cost_eur\":%.10f,"
                 "\"tokens_generated\":%d,"
                 "\"joules_per_token\":%.8f,\"co2_per_token\":%.8f,"
                 "\"reasoning_per_joule\":%.8f,\"sigma_per_joule\":%.8f,"
                 "\"cache_savings_joules\":%.8f,"
                 "\"abstain_savings_joules\":%.8f,"
                 "\"spike_savings_joules\":%.8f,"
                 "\"timestamp_ms\":%lld}",
                 (double)m->cpu_time_ms, (double)m->wall_time_ms,
                 (double)m->estimated_joules,
                 (double)m->estimated_watts_avg, (double)m->co2_grams,
                 (double)m->cost_eur, m->tokens_generated,
                 (double)m->joules_per_token, (double)m->co2_per_token,
                 (double)m->reasoning_per_joule, (double)m->sigma_per_joule,
                 (double)m->cache_savings_joules,
                 (double)m->abstain_savings_joules,
                 (double)m->spike_savings_joules,
                 (long long)m->timestamp_ms);
    if (n <= 0 || n >= 1536) {
        free(out);
        return NULL;
    }
    return out;
}

void cos_energy_print_receipt(const struct cos_energy_measurement *m)
{
    if (!m)
        return;
    printf("[energy: %.4fJ | %.4fW | %.6fgCO2 | €%.8f | %.6fJ/tok]\n",
           (double)m->estimated_joules, (double)m->estimated_watts_avg,
           (double)m->co2_grams, (double)m->cost_eur,
           (double)m->joules_per_token);
}

void cos_energy_print_savings_line(const struct cos_energy_measurement *m)
{
    float total_saved = 0.f;

    if (!m)
        return;
    total_saved = m->cache_savings_joules + m->abstain_savings_joules
                  + m->spike_savings_joules;
    printf("[savings: cache=%.4fJ spike=%.4fJ abstain=%.4fJ total_saved=%.4fJ]\n",
           (double)m->cache_savings_joules,
           (double)m->spike_savings_joules,
           (double)m->abstain_savings_joules, (double)total_saved);
}

int cos_energy_persist(const char *path)
{
    FILE *f;
    const char *p = path;

    if (!p || !p[0]) {
        char b[512];
        energy_default_path(b, sizeof b);
        p = b;
    }

    if (energy_mkparent(p) != 0 && errno != EEXIST)
        return -1;

    f = fopen(p, "w");
    if (!f)
        return -2;

    fprintf(f,
            "lifetime_joules %.12f\n"
            "lifetime_co2_g %.12f\n"
            "lifetime_cost_eur %.12f\n"
            "lifetime_tokens %lld\n"
            "lifetime_queries %lld\n"
            "lifetime_cpu_ms %.12f\n"
            "lifetime_wall_ms %.12f\n"
            "lifetime_cache_save_j %.12f\n"
            "lifetime_abstain_save_j %.12f\n"
            "lifetime_spike_save_j %.12f\n",
            (double)g_lifetime.estimated_joules,
            (double)g_lifetime.co2_grams, (double)g_lifetime.cost_eur,
            (long long)g_lifetime_tokens, (long long)g_lifetime_queries,
            (double)g_lifetime.cpu_time_ms, (double)g_lifetime.wall_time_ms,
            (double)g_lifetime.cache_savings_joules,
            (double)g_lifetime.abstain_savings_joules,
            (double)g_lifetime.spike_savings_joules);
    fclose(f);
    return 0;
}

int cos_energy_load(const char *path)
{
    FILE *f;
    char  line[512];
    double v;

    f = fopen(path, "r");
    if (!f)
        return -1;

    while (fgets(line, sizeof line, f)) {
        long long llv;

        if (sscanf(line, "lifetime_joules %lf", &v) == 1)
            g_lifetime.estimated_joules = (float)v;
        else if (sscanf(line, "lifetime_co2_g %lf", &v) == 1)
            g_lifetime.co2_grams = (float)v;
        else if (sscanf(line, "lifetime_cost_eur %lf", &v) == 1)
            g_lifetime.cost_eur = (float)v;
        else if (sscanf(line, "lifetime_tokens %lld", &llv) == 1)
            g_lifetime_tokens = llv;
        else if (sscanf(line, "lifetime_queries %lld", &llv) == 1)
            g_lifetime_queries = llv;
        else if (sscanf(line, "lifetime_cpu_ms %lf", &v) == 1)
            g_lifetime.cpu_time_ms = (float)v;
        else if (sscanf(line, "lifetime_wall_ms %lf", &v) == 1)
            g_lifetime.wall_time_ms = (float)v;
        else if (sscanf(line, "lifetime_cache_save_j %lf", &v) == 1)
            g_lifetime.cache_savings_joules = (float)v;
        else if (sscanf(line, "lifetime_abstain_save_j %lf", &v) == 1)
            g_lifetime.abstain_savings_joules = (float)v;
        else if (sscanf(line, "lifetime_spike_save_j %lf", &v) == 1)
            g_lifetime.spike_savings_joules = (float)v;
    }
    fclose(f);
    if (g_lifetime_tokens > (long long)INT_MAX)
        g_lifetime.tokens_generated = INT_MAX;
    else
        g_lifetime.tokens_generated = (int)g_lifetime_tokens;
    return 0;
}

int64_t cos_energy_lifetime_query_count(void)
{
    return g_lifetime_queries;
}

int cos_energy_lifetime_report(char *report, int max_len)
{
    double jq;

    if (!report || max_len <= 0)
        return -1;
    jq = g_lifetime_queries > 0
             ? (double)g_lifetime.estimated_joules / (double)g_lifetime_queries
             : 0.0;
    snprintf(report, (size_t)max_len,
             "lifetime: %.6f J, %.6f gCO2, €%.8f, tokens=%lld queries=%lld "
             "avg_J/q=%.8f\n",
             (double)g_lifetime.estimated_joules, (double)g_lifetime.co2_grams,
             (double)g_lifetime.cost_eur, (long long)g_lifetime_tokens,
             (long long)g_lifetime_queries, jq);
    return 0;
}

static uint64_t monotonic_ns(void)
{
#if defined(__APPLE__)
    static mach_timebase_info_data_t tb;
    uint64_t t = mach_absolute_time();
    if (tb.denom == 0)
        mach_timebase_info(&tb);
    return t * (uint64_t)tb.numer / (uint64_t)tb.denom;
#elif defined(CLOCK_MONOTONIC)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#elif defined(CLOCK_REALTIME)
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#else
    return (uint64_t)clock() * 1000000ULL;
#endif
}

static uint64_t cpu_ns_now(void)
{
#if defined(CLOCK_PROCESS_CPUTIME_ID)
    struct timespec ts;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#else
    return monotonic_ns();
#endif
}

void cos_energy_timer_start(cos_energy_timer_t *t)
{
    if (!t)
        return;
    t->cpu_start_ns  = cpu_ns_now();
    t->wall_start_ns = monotonic_ns();
    t->started       = 1;
}

struct cos_energy_measurement cos_energy_timer_stop_and_measure(
    cos_energy_timer_t *t,
    int                 tokens_generated)
{
    uint64_t cpu1, w1;
    float    cpu_ms, wall_ms;

    if (!g_init)
        (void)cos_energy_init(NULL);

    if (!t || !t->started) {
        struct cos_energy_measurement z;
        memset(&z, 0, sizeof z);
        return z;
    }

    cpu1 = cpu_ns_now();
    w1   = monotonic_ns();

    cpu_ms = (float)((double)(cpu1 - t->cpu_start_ns) / 1000000.0);
    wall_ms =
        (float)((double)(w1 - t->wall_start_ns) / 1000000.0);

    t->started = 0;

    return cos_energy_measure_wall(cpu_ms, wall_ms, tokens_generated);
}

#if CREATION_OS_ENABLE_SELF_TESTS
static int energy_fail(const char *m)
{
    fprintf(stderr, "energy_accounting self-test: %s\n", m);
    return 1;
}
#endif

int cos_energy_self_test(void)
{
#if CREATION_OS_ENABLE_SELF_TESTS
    struct cos_energy_measurement a, b;
    char                         *js;

    cos_energy_reset_session();
    if (cos_energy_init(NULL) != 0)
        return energy_fail("init");

    a = cos_energy_measure_wall(100.f, 120.f, 50);
    if (a.estimated_joules <= 0.f)
        return energy_fail("joules");

    memset(&b, 0, sizeof b);
    cos_energy_accumulate(&b, &a);
    if (fabsf(b.estimated_joules - a.estimated_joules) > 1e-3f)
        return energy_fail("accum");

    js = cos_energy_to_json(&a);
    if (!js || !strstr(js, "estimated_joules"))
        return energy_fail("json");
    free(js);

    fprintf(stderr, "energy_accounting self-test: OK\n");
#endif
    return 0;
}
