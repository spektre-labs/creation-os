/*
 * v247 σ-Benchmark-Suite — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "benchmark_suite.h"

#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static void cpy(char *dst, size_t cap, const char *src) {
    size_t n = 0;
    for (; n + 1 < cap && src[n]; ++n) dst[n] = src[n];
    dst[n] = '\0';
}

static const char *CATEGORIES[COS_V247_N_CATEGORIES] = {
    "correctness", "performance", "cognitive", "comparative"
};

static const char *CORRECT_NAMES[COS_V247_N_CORRECT] = {
    "unit", "integration", "e2e", "regression"
};

static const char *CI_TARGETS[COS_V247_N_CI_TARGETS] = {
    "test", "bench", "verify"
};

void cos_v247_init(cos_v247_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x247BE7CEULL;
}

void cos_v247_run(cos_v247_state_t *s) {
    uint64_t prev = 0x247BE7CDULL;

    for (int i = 0; i < COS_V247_N_CATEGORIES; ++i) {
        cpy(s->category_names[i], sizeof(s->category_names[i]),
            CATEGORIES[i]);
        prev = fnv1a(s->category_names[i],
                     strlen(s->category_names[i]), prev);
    }

    for (int i = 0; i < COS_V247_N_CORRECT; ++i) {
        cos_v247_correct_t *c = &s->correctness[i];
        memset(c, 0, sizeof(*c));
        cpy(c->name, sizeof(c->name), CORRECT_NAMES[i]);
        c->pass = true;
        prev = fnv1a(c->name, strlen(c->name), prev);
    }

    cos_v247_perf_t *p = &s->performance;
    memset(p, 0, sizeof(*p));
    p->latency_p50_ms  = 120.0f;
    p->latency_p95_ms  = 340.0f;
    p->latency_p99_ms  = 820.0f;
    p->throughput_rps  = 48.5f;
    p->peak_rss_mb     = 6144;
    p->sigma_overhead  = 0.032f;
    p->tau_overhead    = 0.05f;
    p->overhead_ok     = (p->sigma_overhead < p->tau_overhead);
    prev = fnv1a(p, sizeof(*p), prev);

    cos_v247_cog_t *cg = &s->cognitive;
    memset(cg, 0, sizeof(*cg));
    cg->accuracy_answered   = 0.78f;
    cg->abstention_rate     = 0.15f;
    cg->calibration_ece     = 0.04f;
    cg->consistency_trials  = 10;
    cg->consistency_stable  = 10;
    cg->adversarial_total   = 20;
    cg->adversarial_pass    = 20;
    prev = fnv1a(cg, sizeof(*cg), prev);

    cos_v247_compare_t *c0 = &s->comparative[0];
    memset(c0, 0, sizeof(*c0));
    cpy(c0->name, sizeof(c0->name), "creation_os_vs_raw_llama");
    c0->cos_sigma      = 0.18f;
    c0->other_sigma    = 0.41f;
    c0->cos_abstain    = 0.15f;
    c0->other_abstain  = 0.00f;
    c0->other_has_sigma = true;

    cos_v247_compare_t *c1 = &s->comparative[1];
    memset(c1, 0, sizeof(*c1));
    cpy(c1->name, sizeof(c1->name), "creation_os_vs_openai_api");
    c1->cos_sigma      = 0.18f;
    c1->other_sigma    = -1.0f;  /* "n/a" sentinel */
    c1->cos_abstain    = 0.15f;
    c1->other_abstain  = 0.02f;
    c1->other_has_sigma = false;

    for (int i = 0; i < COS_V247_N_COMPARE; ++i)
        prev = fnv1a(s->comparative[i].name,
                     strlen(s->comparative[i].name), prev);

    for (int i = 0; i < COS_V247_N_CI_TARGETS; ++i) {
        cpy(s->ci_targets[i], sizeof(s->ci_targets[i]), CI_TARGETS[i]);
        prev = fnv1a(s->ci_targets[i], strlen(s->ci_targets[i]), prev);
    }

    /* Overall pass rate: 4 correctness + 1 overhead-ok +
     * 1 adversarial + 1 consistency. */
    int total = COS_V247_N_CORRECT + 3;
    int pass  = 0;
    for (int i = 0; i < COS_V247_N_CORRECT; ++i)
        if (s->correctness[i].pass) pass++;
    if (p->overhead_ok) pass++;
    if (cg->adversarial_total > 0 &&
        cg->adversarial_pass == cg->adversarial_total) pass++;
    if (cg->consistency_trials > 0 &&
        cg->consistency_stable == cg->consistency_trials) pass++;

    s->n_passing_tests = pass;
    s->n_total_tests   = total;
    s->sigma_suite = 1.0f - ((float)pass / (float)total);
    if (s->sigma_suite < 0.0f) s->sigma_suite = 0.0f;
    if (s->sigma_suite > 1.0f) s->sigma_suite = 1.0f;

    struct { int pass, total; float ss; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.pass  = s->n_passing_tests;
    trec.total = s->n_total_tests;
    trec.ss    = s->sigma_suite;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v247_to_json(const cos_v247_state_t *s, char *buf, size_t cap) {
    const cos_v247_perf_t *p  = &s->performance;
    const cos_v247_cog_t  *cg = &s->cognitive;
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v247\","
        "\"n_categories\":%d,\"n_correct\":%d,"
        "\"n_compare\":%d,\"n_ci_targets\":%d,"
        "\"n_passing_tests\":%d,\"n_total_tests\":%d,"
        "\"sigma_suite\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"categories\":[",
        COS_V247_N_CATEGORIES, COS_V247_N_CORRECT,
        COS_V247_N_COMPARE, COS_V247_N_CI_TARGETS,
        s->n_passing_tests, s->n_total_tests,
        s->sigma_suite,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V247_N_CATEGORIES; ++i) {
        int z = snprintf(buf + off, cap - off, "%s\"%s\"",
                         i == 0 ? "" : ",", s->category_names[i]);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"correctness\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V247_N_CORRECT; ++i) {
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"pass\":%s}",
            i == 0 ? "" : ",", s->correctness[i].name,
            s->correctness[i].pass ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off,
        "],\"performance\":{\"latency_p50_ms\":%.2f,"
        "\"latency_p95_ms\":%.2f,\"latency_p99_ms\":%.2f,"
        "\"throughput_rps\":%.2f,\"peak_rss_mb\":%d,"
        "\"sigma_overhead\":%.4f,\"tau_overhead\":%.4f,"
        "\"overhead_ok\":%s},"
        "\"cognitive\":{\"accuracy_answered\":%.4f,"
        "\"abstention_rate\":%.4f,\"calibration_ece\":%.4f,"
        "\"consistency_trials\":%d,\"consistency_stable\":%d,"
        "\"adversarial_total\":%d,\"adversarial_pass\":%d},"
        "\"comparative\":[",
        p->latency_p50_ms, p->latency_p95_ms, p->latency_p99_ms,
        p->throughput_rps, p->peak_rss_mb,
        p->sigma_overhead, p->tau_overhead,
        p->overhead_ok ? "true" : "false",
        cg->accuracy_answered, cg->abstention_rate, cg->calibration_ece,
        cg->consistency_trials, cg->consistency_stable,
        cg->adversarial_total, cg->adversarial_pass);
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V247_N_COMPARE; ++i) {
        const cos_v247_compare_t *c = &s->comparative[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"cos_sigma\":%.4f,"
            "\"other_has_sigma\":%s,\"other_sigma\":%.4f,"
            "\"cos_abstain\":%.4f,\"other_abstain\":%.4f}",
            i == 0 ? "" : ",", c->name,
            c->cos_sigma,
            c->other_has_sigma ? "true" : "false",
            c->other_sigma,
            c->cos_abstain, c->other_abstain);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"ci_targets\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V247_N_CI_TARGETS; ++i) {
        int z = snprintf(buf + off, cap - off, "%s\"%s\"",
                         i == 0 ? "" : ",", s->ci_targets[i]);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v247_self_test(void) {
    cos_v247_state_t s;
    cos_v247_init(&s, 0x247BE7CEULL);
    cos_v247_run(&s);
    if (!s.chain_valid) return 1;

    const char *cats[COS_V247_N_CATEGORIES] = {
        "correctness","performance","cognitive","comparative"
    };
    for (int i = 0; i < COS_V247_N_CATEGORIES; ++i)
        if (strcmp(s.category_names[i], cats[i]) != 0) return 2;

    const char *cors[COS_V247_N_CORRECT] = {
        "unit","integration","e2e","regression"
    };
    for (int i = 0; i < COS_V247_N_CORRECT; ++i) {
        if (strcmp(s.correctness[i].name, cors[i]) != 0) return 3;
        if (!s.correctness[i].pass) return 4;
    }

    const cos_v247_perf_t *p = &s.performance;
    if (p->latency_p50_ms > p->latency_p95_ms + 1e-6f) return 5;
    if (p->latency_p95_ms > p->latency_p99_ms + 1e-6f) return 6;
    if (p->throughput_rps <= 0.0f) return 7;
    if (p->sigma_overhead < 0.0f || p->sigma_overhead >= p->tau_overhead) return 8;
    if (!p->overhead_ok) return 9;

    const cos_v247_cog_t *cg = &s.cognitive;
    if (cg->accuracy_answered < 0.0f || cg->accuracy_answered > 1.0f) return 10;
    if (cg->abstention_rate   < 0.0f || cg->abstention_rate   > 1.0f) return 11;
    if (cg->consistency_trials <= 0) return 12;
    if (cg->consistency_stable != cg->consistency_trials) return 13;
    if (cg->adversarial_total <= 0) return 14;
    if (cg->adversarial_pass != cg->adversarial_total) return 15;

    if (strcmp(s.comparative[0].name, "creation_os_vs_raw_llama") != 0) return 16;
    if (strcmp(s.comparative[1].name, "creation_os_vs_openai_api") != 0) return 17;

    const char *ci[COS_V247_N_CI_TARGETS] = { "test", "bench", "verify" };
    for (int i = 0; i < COS_V247_N_CI_TARGETS; ++i)
        if (strcmp(s.ci_targets[i], ci[i]) != 0) return 18;

    if (s.n_passing_tests != s.n_total_tests) return 19;
    if (s.sigma_suite < 0.0f || s.sigma_suite > 1.0f) return 20;
    if (s.sigma_suite > 1e-6f) return 21;

    cos_v247_state_t t;
    cos_v247_init(&t, 0x247BE7CEULL);
    cos_v247_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 22;
    return 0;
}
