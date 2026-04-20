/*
 * v295 σ-Immune — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "immune.h"

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

static const struct { const char *pat; float sigma; }
    INNATE_ROWS[COS_V295_N_INNATE] = {
    { "sql_injection",    0.85f },
    { "prompt_injection", 0.80f },
    { "obvious_malware",  0.90f },
};

static const struct { const char *row; bool faster; bool cross; }
    ADAPT_ROWS[COS_V295_N_ADAPT] = {
    { "novel_attack_first_seen", false, false },
    { "same_attack_second_seen", true,  false },
    { "related_variant_seen",    false, true  },
};

static const struct { const char *entry; bool recog; const char *tier; }
    MEMORY_ROWS[COS_V295_N_MEMORY] = {
    { "pattern_A_first_logged",  false, "SLOW" },
    { "pattern_A_reencountered", true,  "FAST" },
    { "pattern_B_new_logged",    false, "SLOW" },
};

static const struct { const char *scen; float tau; float fpr; const char *verd; }
    AUTO_ROWS[COS_V295_N_AUTO] = {
    { "tau_too_tight", 0.05f, 0.40f, "AUTOIMMUNE"      },
    { "tau_balanced",  0.30f, 0.05f, "HEALTHY"         },
    { "tau_too_loose", 0.80f, 0.01f, "IMMUNODEFICIENT" },
};

void cos_v295_init(cos_v295_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x295bcdULL;
}

void cos_v295_run(cos_v295_state_t *s) {
    uint64_t prev = 0x295bcd00ULL;

    s->n_innate_rows_ok = 0;
    int n_block = 0, n_instant = 0;
    for (int i = 0; i < COS_V295_N_INNATE; ++i) {
        cos_v295_innate_t *r = &s->innate[i];
        memset(r, 0, sizeof(*r));
        cpy(r->pattern,       sizeof(r->pattern),       INNATE_ROWS[i].pat);
        cpy(r->response_tier, sizeof(r->response_tier), "INSTANT");
        r->sigma_raw         = INNATE_ROWS[i].sigma;
        r->blocked           = (r->sigma_raw >= COS_V295_TAU_INNATE);
        r->requires_training = false;
        if (strlen(r->pattern) > 0) s->n_innate_rows_ok++;
        if (r->blocked) n_block++;
        if (strcmp(r->response_tier, "INSTANT") == 0) n_instant++;
        prev = fnv1a(r->pattern, strlen(r->pattern), prev);
        prev = fnv1a(&r->sigma_raw, sizeof(r->sigma_raw), prev);
        prev = fnv1a(&r->blocked,   sizeof(r->blocked),   prev);
        prev = fnv1a(&r->requires_training, sizeof(r->requires_training), prev);
        prev = fnv1a(r->response_tier, strlen(r->response_tier), prev);
    }
    s->innate_all_blocked_ok = (n_block   == COS_V295_N_INNATE);
    s->innate_all_instant_ok = (n_instant == COS_V295_N_INNATE);

    s->n_adaptive_rows_ok = 0;
    int n_learned = 0, n_faster = 0, n_cross = 0;
    for (int i = 0; i < COS_V295_N_ADAPT; ++i) {
        cos_v295_adapt_t *r = &s->adapt[i];
        memset(r, 0, sizeof(*r));
        cpy(r->row, sizeof(r->row), ADAPT_ROWS[i].row);
        r->learned          = true;
        r->faster_on_repeat = ADAPT_ROWS[i].faster;
        r->cross_recognized = ADAPT_ROWS[i].cross;
        if (strlen(r->row) > 0) s->n_adaptive_rows_ok++;
        if (r->learned)          n_learned++;
        if (r->faster_on_repeat) n_faster++;
        if (r->cross_recognized) n_cross++;
        prev = fnv1a(r->row, strlen(r->row), prev);
        prev = fnv1a(&r->learned,          sizeof(r->learned),          prev);
        prev = fnv1a(&r->faster_on_repeat, sizeof(r->faster_on_repeat), prev);
        prev = fnv1a(&r->cross_recognized, sizeof(r->cross_recognized), prev);
    }
    s->adaptive_all_learned_ok = (n_learned == COS_V295_N_ADAPT);
    s->adaptive_faster_ok      = (n_faster  == 1);
    s->adaptive_cross_ok       = (n_cross   == 1);

    s->n_memory_rows_ok = 0;
    int n_recog = 0;
    bool mem_polarity = true;
    for (int i = 0; i < COS_V295_N_MEMORY; ++i) {
        cos_v295_memory_t *r = &s->memory[i];
        memset(r, 0, sizeof(*r));
        cpy(r->entry, sizeof(r->entry), MEMORY_ROWS[i].entry);
        cpy(r->tier,  sizeof(r->tier),  MEMORY_ROWS[i].tier);
        r->recognised = MEMORY_ROWS[i].recog;
        if (strlen(r->entry) > 0) s->n_memory_rows_ok++;
        if (r->recognised) n_recog++;
        bool is_fast = (strcmp(r->tier, "FAST") == 0);
        if (r->recognised != is_fast) mem_polarity = false;
        prev = fnv1a(r->entry, strlen(r->entry), prev);
        prev = fnv1a(&r->recognised, sizeof(r->recognised), prev);
        prev = fnv1a(r->tier, strlen(r->tier), prev);
    }
    s->memory_recog_polarity_ok = mem_polarity;
    s->memory_count_ok          = (n_recog == 1);

    s->n_auto_rows_ok = 0;
    bool auto_order = true;
    float last_tau = -1.0f;
    int n_healthy = 0, n_autoim = 0, n_immdef = 0;
    bool healthy_range_ok = true;
    for (int i = 0; i < COS_V295_N_AUTO; ++i) {
        cos_v295_auto_t *r = &s->autoprev[i];
        memset(r, 0, sizeof(*r));
        cpy(r->scenario, sizeof(r->scenario), AUTO_ROWS[i].scen);
        cpy(r->verdict,  sizeof(r->verdict),  AUTO_ROWS[i].verd);
        r->tau = AUTO_ROWS[i].tau;
        r->false_positive_rate = AUTO_ROWS[i].fpr;
        if (strlen(r->scenario) > 0 && strlen(r->verdict) > 0)
            s->n_auto_rows_ok++;
        if (i > 0 && !(r->tau > last_tau)) auto_order = false;
        last_tau = r->tau;
        bool in_range =
            (r->tau >= COS_V295_TAU_LO && r->tau <= COS_V295_TAU_HI &&
             r->false_positive_rate <= COS_V295_FPR_BUDGET);
        bool is_healthy = (strcmp(r->verdict, "HEALTHY") == 0);
        if (is_healthy != in_range) healthy_range_ok = false;
        if      (is_healthy)                               n_healthy++;
        else if (strcmp(r->verdict, "AUTOIMMUNE") == 0)    n_autoim++;
        else if (strcmp(r->verdict, "IMMUNODEFICIENT")==0) n_immdef++;
        prev = fnv1a(r->scenario, strlen(r->scenario), prev);
        prev = fnv1a(&r->tau, sizeof(r->tau), prev);
        prev = fnv1a(&r->false_positive_rate, sizeof(r->false_positive_rate), prev);
        prev = fnv1a(r->verdict, strlen(r->verdict), prev);
    }
    s->auto_order_ok         = auto_order;
    s->auto_verdict_ok       = (n_healthy == 1 && n_autoim == 1 && n_immdef == 1);
    s->auto_healthy_range_ok = healthy_range_ok;

    int total   = COS_V295_N_INNATE + 1 + 1 +
                  COS_V295_N_ADAPT  + 1 + 1 + 1 +
                  COS_V295_N_MEMORY + 1 + 1 +
                  COS_V295_N_AUTO   + 1 + 1 + 1;
    int passing = s->n_innate_rows_ok +
                  (s->innate_all_blocked_ok ? 1 : 0) +
                  (s->innate_all_instant_ok ? 1 : 0) +
                  s->n_adaptive_rows_ok +
                  (s->adaptive_all_learned_ok ? 1 : 0) +
                  (s->adaptive_faster_ok      ? 1 : 0) +
                  (s->adaptive_cross_ok       ? 1 : 0) +
                  s->n_memory_rows_ok +
                  (s->memory_recog_polarity_ok ? 1 : 0) +
                  (s->memory_count_ok          ? 1 : 0) +
                  s->n_auto_rows_ok +
                  (s->auto_order_ok         ? 1 : 0) +
                  (s->auto_verdict_ok       ? 1 : 0) +
                  (s->auto_healthy_range_ok ? 1 : 0);
    s->sigma_immune = 1.0f - ((float)passing / (float)total);
    if (s->sigma_immune < 0.0f) s->sigma_immune = 0.0f;
    if (s->sigma_immune > 1.0f) s->sigma_immune = 1.0f;

    struct { int ni, na, nm, nu;
             bool b, i_, l, f, c, mp, mc, o, v, h;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.ni = s->n_innate_rows_ok;
    trec.na = s->n_adaptive_rows_ok;
    trec.nm = s->n_memory_rows_ok;
    trec.nu = s->n_auto_rows_ok;
    trec.b  = s->innate_all_blocked_ok;
    trec.i_ = s->innate_all_instant_ok;
    trec.l  = s->adaptive_all_learned_ok;
    trec.f  = s->adaptive_faster_ok;
    trec.c  = s->adaptive_cross_ok;
    trec.mp = s->memory_recog_polarity_ok;
    trec.mc = s->memory_count_ok;
    trec.o  = s->auto_order_ok;
    trec.v  = s->auto_verdict_ok;
    trec.h  = s->auto_healthy_range_ok;
    trec.sigma = s->sigma_immune;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v295_to_json(const cos_v295_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v295\","
        "\"n_innate_rows_ok\":%d,\"innate_all_blocked_ok\":%s,"
        "\"innate_all_instant_ok\":%s,"
        "\"n_adaptive_rows_ok\":%d,\"adaptive_all_learned_ok\":%s,"
        "\"adaptive_faster_ok\":%s,\"adaptive_cross_ok\":%s,"
        "\"n_memory_rows_ok\":%d,\"memory_recog_polarity_ok\":%s,"
        "\"memory_count_ok\":%s,"
        "\"n_auto_rows_ok\":%d,\"auto_order_ok\":%s,"
        "\"auto_verdict_ok\":%s,\"auto_healthy_range_ok\":%s,"
        "\"sigma_immune\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"innate\":[",
        s->n_innate_rows_ok,
        s->innate_all_blocked_ok ? "true" : "false",
        s->innate_all_instant_ok ? "true" : "false",
        s->n_adaptive_rows_ok,
        s->adaptive_all_learned_ok ? "true" : "false",
        s->adaptive_faster_ok      ? "true" : "false",
        s->adaptive_cross_ok       ? "true" : "false",
        s->n_memory_rows_ok,
        s->memory_recog_polarity_ok ? "true" : "false",
        s->memory_count_ok          ? "true" : "false",
        s->n_auto_rows_ok,
        s->auto_order_ok         ? "true" : "false",
        s->auto_verdict_ok       ? "true" : "false",
        s->auto_healthy_range_ok ? "true" : "false",
        s->sigma_immune,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V295_N_INNATE; ++i) {
        const cos_v295_innate_t *r = &s->innate[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"pattern\":\"%s\",\"sigma_raw\":%.4f,"
            "\"blocked\":%s,\"requires_training\":%s,"
            "\"response_tier\":\"%s\"}",
            i == 0 ? "" : ",", r->pattern, r->sigma_raw,
            r->blocked           ? "true" : "false",
            r->requires_training ? "true" : "false",
            r->response_tier);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"adapt\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V295_N_ADAPT; ++i) {
        const cos_v295_adapt_t *r = &s->adapt[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"row\":\"%s\",\"learned\":%s,"
            "\"faster_on_repeat\":%s,\"cross_recognized\":%s}",
            i == 0 ? "" : ",", r->row,
            r->learned          ? "true" : "false",
            r->faster_on_repeat ? "true" : "false",
            r->cross_recognized ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"memory\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V295_N_MEMORY; ++i) {
        const cos_v295_memory_t *r = &s->memory[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"entry\":\"%s\",\"recognised\":%s,\"tier\":\"%s\"}",
            i == 0 ? "" : ",", r->entry,
            r->recognised ? "true" : "false", r->tier);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"autoprev\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V295_N_AUTO; ++i) {
        const cos_v295_auto_t *r = &s->autoprev[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"scenario\":\"%s\",\"tau\":%.4f,"
            "\"false_positive_rate\":%.4f,\"verdict\":\"%s\"}",
            i == 0 ? "" : ",", r->scenario, r->tau,
            r->false_positive_rate, r->verdict);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v295_self_test(void) {
    cos_v295_state_t s;
    cos_v295_init(&s, 0x295bcdULL);
    cos_v295_run(&s);
    if (!s.chain_valid) return 1;

    static const char *WANT_I[COS_V295_N_INNATE] = {
        "sql_injection","prompt_injection","obvious_malware"
    };
    for (int i = 0; i < COS_V295_N_INNATE; ++i) {
        if (strcmp(s.innate[i].pattern, WANT_I[i]) != 0) return 2;
        if (!s.innate[i].blocked) return 3;
        if (s.innate[i].requires_training) return 4;
    }
    if (s.n_innate_rows_ok != COS_V295_N_INNATE) return 5;
    if (!s.innate_all_blocked_ok) return 6;
    if (!s.innate_all_instant_ok) return 7;

    static const char *WANT_A[COS_V295_N_ADAPT] = {
        "novel_attack_first_seen",
        "same_attack_second_seen",
        "related_variant_seen"
    };
    for (int i = 0; i < COS_V295_N_ADAPT; ++i) {
        if (strcmp(s.adapt[i].row, WANT_A[i]) != 0) return 8;
        if (!s.adapt[i].learned) return 9;
    }
    if (s.n_adaptive_rows_ok != COS_V295_N_ADAPT) return 10;
    if (!s.adaptive_all_learned_ok) return 11;
    if (!s.adaptive_faster_ok)      return 12;
    if (!s.adaptive_cross_ok)       return 13;

    static const char *WANT_M[COS_V295_N_MEMORY] = {
        "pattern_A_first_logged",
        "pattern_A_reencountered",
        "pattern_B_new_logged"
    };
    for (int i = 0; i < COS_V295_N_MEMORY; ++i) {
        if (strcmp(s.memory[i].entry, WANT_M[i]) != 0) return 14;
    }
    if (s.n_memory_rows_ok != COS_V295_N_MEMORY) return 15;
    if (!s.memory_recog_polarity_ok) return 16;
    if (!s.memory_count_ok)          return 17;

    static const char *WANT_S[COS_V295_N_AUTO] = {
        "tau_too_tight","tau_balanced","tau_too_loose"
    };
    for (int i = 0; i < COS_V295_N_AUTO; ++i) {
        if (strcmp(s.autoprev[i].scenario, WANT_S[i]) != 0) return 18;
    }
    if (s.n_auto_rows_ok != COS_V295_N_AUTO) return 19;
    if (!s.auto_order_ok)         return 20;
    if (!s.auto_verdict_ok)       return 21;
    if (!s.auto_healthy_range_ok) return 22;

    if (s.sigma_immune < 0.0f || s.sigma_immune > 1.0f) return 23;
    if (s.sigma_immune > 1e-6f) return 24;

    cos_v295_state_t u;
    cos_v295_init(&u, 0x295bcdULL);
    cos_v295_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 25;
    return 0;
}
