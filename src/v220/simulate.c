/*
 * v220 σ-Simulate — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "simulate.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v220_init(cos_v220_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed          = seed ? seed : 0x220E5171ULL;
    s->tau_rule      = 0.10f;
    s->n_mc          = COS_V220_N_MC;
    s->n_steps       = COS_V220_N_STEPS;
    s->initial_state = 0;
}

/* Baseline rules (4-state system; every state has one
 * outgoing rule; self-loops close the system). */
static const cos_v220_rule_t BASE_R[COS_V220_N_RULES] = {
    { 0, "s0_to_s1", 0, 1, 0.75f, 0.02f },
    { 1, "s1_to_s2", 1, 2, 0.70f, 0.03f },
    { 2, "s2_to_s3", 2, 3, 0.60f, 0.04f },
    { 3, "s3_to_s0", 3, 0, 0.30f, 0.05f }
};

/* What-if: rule 2 (s2_to_s3) prob lowered (more mass
 * stays in s2), simulating a policy intervention. */
static const cos_v220_rule_t WHAT_R[COS_V220_N_RULES] = {
    { 0, "s0_to_s1", 0, 1, 0.75f, 0.02f },
    { 1, "s1_to_s2", 1, 2, 0.70f, 0.03f },
    { 2, "s2_to_s3", 2, 3, 0.20f, 0.06f },
    { 3, "s3_to_s0", 3, 0, 0.30f, 0.05f }
};

void cos_v220_build(cos_v220_state_t *s) {
    memcpy(s->baseline.rules, BASE_R, sizeof(BASE_R));
    memcpy(s->whatif  .rules, WHAT_R, sizeof(WHAT_R));
    memset(s->baseline.histogram, 0, sizeof(s->baseline.histogram));
    memset(s->whatif  .histogram, 0, sizeof(s->whatif  .histogram));
}

/* Linear-congruential RNG (deterministic, portable). */
static uint32_t lcg_next(uint32_t *st) {
    *st = (*st) * 1664525u + 1013904223u;
    return *st;
}
static float lcg_unit(uint32_t *st) {
    return (float)(lcg_next(st) >> 8) / (float)(1u << 24);
}

static int step_state(int cur, const cos_v220_rule_t *rules, uint32_t *rng) {
    for (int i = 0; i < COS_V220_N_RULES; ++i) {
        if (rules[i].from_state != cur) continue;
        float u = lcg_unit(rng);
        if (u < rules[i].prob) return rules[i].to_state;
        break;
    }
    return cur;  /* self-loop on rule failure */
}

static void run_scenario(const cos_v220_rule_t *rules,
                         int *histogram,
                         int initial_state,
                         int n_mc, int n_steps,
                         uint32_t seed32) {
    memset(histogram, 0, sizeof(int) * COS_V220_N_STATES);
    for (int r = 0; r < n_mc; ++r) {
        uint32_t rng = seed32 ^ (uint32_t)(r * 2654435761u);
        int cur = initial_state;
        for (int t = 0; t < n_steps; ++t)
            cur = step_state(cur, rules, &rng);
        histogram[cur]++;
    }
}

/* Normalised Shannon-entropy proxy ∈ [0,1] over
 * terminal-state histogram (Shannon entropy divided
 * by log2(N_STATES) ≈ 2 for 4 states). */
static float hist_entropy_norm(const int *hist, int n_mc) {
    if (n_mc <= 0) return 0.0f;
    float h = 0.0f;
    for (int i = 0; i < COS_V220_N_STATES; ++i) {
        float p = (float)hist[i] / (float)n_mc;
        if (p > 0.0f) h -= p * logf(p);
    }
    float logN = logf((float)COS_V220_N_STATES);
    if (logN < 1e-6f) return 0.0f;
    float v = h / logN;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return v;
}

void cos_v220_run(cos_v220_state_t *s) {
    /* Run baseline + whatif with the SAME rollout seed
     * so the difference is purely attributable to the
     * perturbed rule (reduces Monte Carlo variance). */
    uint32_t seed32 = (uint32_t)s->seed;
    run_scenario(s->baseline.rules, s->baseline.histogram,
                 s->initial_state, s->n_mc, s->n_steps, seed32);
    run_scenario(s->whatif  .rules, s->whatif  .histogram,
                 s->initial_state, s->n_mc, s->n_steps, seed32);

    s->baseline.sigma_sim = hist_entropy_norm(s->baseline.histogram, s->n_mc);
    s->whatif  .sigma_sim = hist_entropy_norm(s->whatif  .histogram, s->n_mc);

    /* σ_engine = max σ_rule across the union. */
    float se = 0.0f;
    for (int i = 0; i < COS_V220_N_RULES; ++i) {
        if (s->baseline.rules[i].sigma_rule > se) se = s->baseline.rules[i].sigma_rule;
        if (s->whatif  .rules[i].sigma_rule > se) se = s->whatif  .rules[i].sigma_rule;
    }
    s->sigma_engine = se;

    /* Per-state causal attribution. */
    for (int i = 0; i < COS_V220_N_STATES; ++i) {
        float pb = (float)s->baseline.histogram[i] / (float)s->n_mc;
        float pw = (float)s->whatif  .histogram[i] / (float)s->n_mc;
        float d  = fabsf(pb - pw);
        if (d > 1.0f) d = 1.0f;
        s->sigma_causal[i] = d;
    }

    /* FNV-1a chain: rules(base) → histogram(base) →
     * rules(whatif) → histogram(whatif) → σ-aggregate. */
    uint64_t prev = 0x220C1A11ULL;
    for (int i = 0; i < COS_V220_N_RULES; ++i) {
        struct { int id, from, to; float p, sr; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id   = s->baseline.rules[i].id;
        rec.from = s->baseline.rules[i].from_state;
        rec.to   = s->baseline.rules[i].to_state;
        rec.p    = s->baseline.rules[i].prob;
        rec.sr   = s->baseline.rules[i].sigma_rule;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    {
        struct { int h[COS_V220_N_STATES]; float ss; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        for (int i = 0; i < COS_V220_N_STATES; ++i)
            rec.h[i] = s->baseline.histogram[i];
        rec.ss   = s->baseline.sigma_sim;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    for (int i = 0; i < COS_V220_N_RULES; ++i) {
        struct { int id, from, to; float p, sr; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id   = s->whatif.rules[i].id;
        rec.from = s->whatif.rules[i].from_state;
        rec.to   = s->whatif.rules[i].to_state;
        rec.p    = s->whatif.rules[i].prob;
        rec.sr   = s->whatif.rules[i].sigma_rule;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    {
        struct { int h[COS_V220_N_STATES]; float ss; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        for (int i = 0; i < COS_V220_N_STATES; ++i)
            rec.h[i] = s->whatif.histogram[i];
        rec.ss   = s->whatif.sigma_sim;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    {
        struct { float se; float sc[COS_V220_N_STATES]; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.se = s->sigma_engine;
        for (int i = 0; i < COS_V220_N_STATES; ++i)
            rec.sc[i] = s->sigma_causal[i];
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    s->terminal_hash = prev;
    s->chain_valid   = true;  /* closed-form; no replay asymmetry */
}

size_t cos_v220_to_json(const cos_v220_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v220\","
        "\"n_states\":%d,\"n_rules\":%d,\"n_mc\":%d,\"n_steps\":%d,"
        "\"initial_state\":%d,\"tau_rule\":%.3f,"
        "\"sigma_engine\":%.4f,"
        "\"sigma_sim_baseline\":%.4f,\"sigma_sim_whatif\":%.4f,"
        "\"histogram_baseline\":[%d,%d,%d,%d],"
        "\"histogram_whatif\":[%d,%d,%d,%d],"
        "\"sigma_causal\":[%.4f,%.4f,%.4f,%.4f],"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"rules_baseline\":[",
        COS_V220_N_STATES, COS_V220_N_RULES, s->n_mc, s->n_steps,
        s->initial_state, s->tau_rule, s->sigma_engine,
        s->baseline.sigma_sim, s->whatif.sigma_sim,
        s->baseline.histogram[0], s->baseline.histogram[1],
        s->baseline.histogram[2], s->baseline.histogram[3],
        s->whatif.histogram[0], s->whatif.histogram[1],
        s->whatif.histogram[2], s->whatif.histogram[3],
        s->sigma_causal[0], s->sigma_causal[1],
        s->sigma_causal[2], s->sigma_causal[3],
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V220_N_RULES; ++i) {
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"name\":\"%s\",\"from\":%d,\"to\":%d,"
            "\"prob\":%.3f,\"sigma_rule\":%.3f}",
            i == 0 ? "" : ",",
            s->baseline.rules[i].id,
            s->baseline.rules[i].name,
            s->baseline.rules[i].from_state,
            s->baseline.rules[i].to_state,
            s->baseline.rules[i].prob,
            s->baseline.rules[i].sigma_rule);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m1 = snprintf(buf + off, cap - off, "],\"rules_whatif\":[");
    if (m1 < 0 || off + (size_t)m1 >= cap) return 0;
    off += (size_t)m1;
    for (int i = 0; i < COS_V220_N_RULES; ++i) {
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"name\":\"%s\",\"from\":%d,\"to\":%d,"
            "\"prob\":%.3f,\"sigma_rule\":%.3f}",
            i == 0 ? "" : ",",
            s->whatif.rules[i].id,
            s->whatif.rules[i].name,
            s->whatif.rules[i].from_state,
            s->whatif.rules[i].to_state,
            s->whatif.rules[i].prob,
            s->whatif.rules[i].sigma_rule);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m2 = snprintf(buf + off, cap - off, "]}");
    if (m2 < 0 || off + (size_t)m2 >= cap) return 0;
    return off + (size_t)m2;
}

int cos_v220_self_test(void) {
    cos_v220_state_t s;
    cos_v220_init(&s, 0x220E5171ULL);
    cos_v220_build(&s);
    cos_v220_run(&s);

    int sb = 0, sw = 0;
    for (int i = 0; i < COS_V220_N_STATES; ++i) {
        sb += s.baseline.histogram[i];
        sw += s.whatif  .histogram[i];
    }
    if (sb != s.n_mc || sw != s.n_mc) return 1;

    for (int i = 0; i < COS_V220_N_RULES; ++i) {
        if (s.baseline.rules[i].sigma_rule > s.tau_rule) return 2;
        if (s.whatif  .rules[i].sigma_rule > s.tau_rule) return 2;
    }
    if (s.sigma_engine > s.tau_rule)                     return 3;
    if (s.baseline.sigma_sim < 0.0f || s.baseline.sigma_sim > 1.0f) return 4;
    if (s.whatif  .sigma_sim < 0.0f || s.whatif  .sigma_sim > 1.0f) return 4;

    int differ = 0;
    for (int i = 0; i < COS_V220_N_STATES; ++i)
        if (s.baseline.histogram[i] != s.whatif.histogram[i]) { differ = 1; break; }
    if (!differ)                                          return 5;

    float cs = 0.0f;
    for (int i = 0; i < COS_V220_N_STATES; ++i) {
        if (s.sigma_causal[i] < 0.0f || s.sigma_causal[i] > 1.0f) return 6;
        cs += s.sigma_causal[i];
    }
    if (cs > 2.0f + 1e-5f) return 6;

    if (!s.chain_valid) return 7;
    return 0;
}
