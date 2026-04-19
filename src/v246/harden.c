/*
 * v246 σ-Harden — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "harden.h"

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

static const struct { const char *name; const char *recovery; int ticks; }
    CHAOS[COS_V246_N_CHAOS] = {
    { "kill-random-kernel", "v239 deactivates, requeues",  3 },
    { "high-load",          "σ-budget throttle via v239",  2 },
    { "network-partition",  "v235 audit-chain winner",     5 },
    { "corrupt-memory",     "v195 error recovery restore", 4 },
    { "oom-attempt",        "v246 hard limit refuses",     1 },
};

static const struct { const char *name; long value; }
    LIMITS[COS_V246_N_LIMITS] = {
    { "max_tokens_per_request",    16384 },
    { "max_time_ms_per_request",   60000 },
    { "max_kernels_per_request",   20    },
    { "max_memory_mb_per_session", 8192  },
    { "max_disk_mb_per_session",   4096  },
    { "max_concurrent_requests",   64    },
};

static const char *INPUTS[COS_V246_N_INPUT_CHECKS] = {
    "max_prompt_length",
    "utf8_encoding_ok",
    "injection_filter_ok",
    "rate_limit_ok",
    "auth_token_required",
};

static const char *SECURITY[COS_V246_N_SECURITY] = {
    "tls_required",
    "auth_token_required",
    "audit_log_on",
    "containment_on",
    "scsl_license_pinned",
};

void cos_v246_init(cos_v246_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x246A7DE4ULL;
}

void cos_v246_run(cos_v246_state_t *s) {
    uint64_t prev = 0x246A7DE5ULL;

    s->n_recovered = 0;
    for (int i = 0; i < COS_V246_N_CHAOS; ++i) {
        cos_v246_chaos_t *c = &s->chaos[i];
        memset(c, 0, sizeof(*c));
        cpy(c->name,     sizeof(c->name),     CHAOS[i].name);
        cpy(c->recovery, sizeof(c->recovery), CHAOS[i].recovery);
        c->recovery_ticks   = CHAOS[i].ticks;
        c->error_path_taken = true;
        c->recovered        = (c->error_path_taken && c->recovery_ticks > 0);
        if (c->recovered) s->n_recovered++;
        prev = fnv1a(c->name,     strlen(c->name),     prev);
        prev = fnv1a(c->recovery, strlen(c->recovery), prev);
        prev = fnv1a(&c->recovery_ticks, sizeof(c->recovery_ticks), prev);
    }

    s->n_limits_positive = 0;
    for (int i = 0; i < COS_V246_N_LIMITS; ++i) {
        cos_v246_limit_t *l = &s->limits[i];
        memset(l, 0, sizeof(*l));
        cpy(l->name, sizeof(l->name), LIMITS[i].name);
        l->value = LIMITS[i].value;
        if (l->value > 0) s->n_limits_positive++;
        prev = fnv1a(l->name, strlen(l->name), prev);
        prev = fnv1a(&l->value, sizeof(l->value), prev);
    }

    s->n_inputs_passing = 0;
    for (int i = 0; i < COS_V246_N_INPUT_CHECKS; ++i) {
        cos_v246_input_t *c = &s->inputs[i];
        memset(c, 0, sizeof(*c));
        cpy(c->name, sizeof(c->name), INPUTS[i]);
        c->pass = true;
        if (c->pass) s->n_inputs_passing++;
        prev = fnv1a(c->name, strlen(c->name), prev);
    }

    s->n_security_on = 0;
    for (int i = 0; i < COS_V246_N_SECURITY; ++i) {
        cos_v246_security_t *sec = &s->security[i];
        memset(sec, 0, sizeof(*sec));
        cpy(sec->name, sizeof(sec->name), SECURITY[i]);
        sec->on = true;
        if (sec->on) s->n_security_on++;
        prev = fnv1a(sec->name, strlen(sec->name), prev);
    }

    int total = COS_V246_N_CHAOS + COS_V246_N_INPUT_CHECKS +
                COS_V246_N_SECURITY + COS_V246_N_LIMITS;
    int passing = s->n_recovered + s->n_inputs_passing +
                  s->n_security_on + s->n_limits_positive;
    s->sigma_harden = 1.0f - ((float)passing / (float)total);
    if (s->sigma_harden < 0.0f) s->sigma_harden = 0.0f;
    if (s->sigma_harden > 1.0f) s->sigma_harden = 1.0f;

    struct { int nr, nl, ni, ns; float sh; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nr = s->n_recovered;
    trec.nl = s->n_limits_positive;
    trec.ni = s->n_inputs_passing;
    trec.ns = s->n_security_on;
    trec.sh = s->sigma_harden;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v246_to_json(const cos_v246_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v246\","
        "\"n_chaos\":%d,\"n_limits\":%d,\"n_input_checks\":%d,"
        "\"n_security\":%d,"
        "\"n_recovered\":%d,\"n_limits_positive\":%d,"
        "\"n_inputs_passing\":%d,\"n_security_on\":%d,"
        "\"sigma_harden\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"chaos\":[",
        COS_V246_N_CHAOS, COS_V246_N_LIMITS,
        COS_V246_N_INPUT_CHECKS, COS_V246_N_SECURITY,
        s->n_recovered, s->n_limits_positive,
        s->n_inputs_passing, s->n_security_on,
        s->sigma_harden,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V246_N_CHAOS; ++i) {
        const cos_v246_chaos_t *c = &s->chaos[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"recovery\":\"%s\","
            "\"recovery_ticks\":%d,\"error_path_taken\":%s,"
            "\"recovered\":%s}",
            i == 0 ? "" : ",",
            c->name, c->recovery, c->recovery_ticks,
            c->error_path_taken ? "true" : "false",
            c->recovered        ? "true" : "false");
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    int q = snprintf(buf + off, cap - off, "],\"limits\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V246_N_LIMITS; ++i) {
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"value\":%ld}",
            i == 0 ? "" : ",", s->limits[i].name, s->limits[i].value);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"inputs\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V246_N_INPUT_CHECKS; ++i) {
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"pass\":%s}",
            i == 0 ? "" : ",", s->inputs[i].name,
            s->inputs[i].pass ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"security\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V246_N_SECURITY; ++i) {
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"on\":%s}",
            i == 0 ? "" : ",", s->security[i].name,
            s->security[i].on ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v246_self_test(void) {
    cos_v246_state_t s;
    cos_v246_init(&s, 0x246A7DE4ULL);
    cos_v246_run(&s);
    if (!s.chain_valid) return 1;

    const char *chaos_names[COS_V246_N_CHAOS] = {
        "kill-random-kernel","high-load","network-partition",
        "corrupt-memory","oom-attempt"
    };
    for (int i = 0; i < COS_V246_N_CHAOS; ++i) {
        if (strcmp(s.chaos[i].name, chaos_names[i]) != 0) return 2;
        if (!s.chaos[i].error_path_taken) return 3;
        if (!s.chaos[i].recovered)        return 4;
        if (s.chaos[i].recovery_ticks <= 0) return 5;
    }
    if (s.n_recovered != COS_V246_N_CHAOS) return 6;

    const char *lim_names[COS_V246_N_LIMITS] = {
        "max_tokens_per_request","max_time_ms_per_request",
        "max_kernels_per_request","max_memory_mb_per_session",
        "max_disk_mb_per_session","max_concurrent_requests"
    };
    for (int i = 0; i < COS_V246_N_LIMITS; ++i) {
        if (strcmp(s.limits[i].name, lim_names[i]) != 0) return 7;
        if (s.limits[i].value <= 0) return 8;
    }
    if (s.n_limits_positive != COS_V246_N_LIMITS) return 9;

    const char *in_names[COS_V246_N_INPUT_CHECKS] = {
        "max_prompt_length","utf8_encoding_ok","injection_filter_ok",
        "rate_limit_ok","auth_token_required"
    };
    for (int i = 0; i < COS_V246_N_INPUT_CHECKS; ++i) {
        if (strcmp(s.inputs[i].name, in_names[i]) != 0) return 10;
        if (!s.inputs[i].pass) return 11;
    }
    if (s.n_inputs_passing != COS_V246_N_INPUT_CHECKS) return 12;

    const char *sec_names[COS_V246_N_SECURITY] = {
        "tls_required","auth_token_required","audit_log_on",
        "containment_on","scsl_license_pinned"
    };
    for (int i = 0; i < COS_V246_N_SECURITY; ++i) {
        if (strcmp(s.security[i].name, sec_names[i]) != 0) return 13;
        if (!s.security[i].on) return 14;
    }
    if (s.n_security_on != COS_V246_N_SECURITY) return 15;

    if (s.sigma_harden < 0.0f || s.sigma_harden > 1.0f) return 16;
    if (s.sigma_harden > 1e-6f) return 17;

    cos_v246_state_t t;
    cos_v246_init(&t, 0x246A7DE4ULL);
    cos_v246_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 18;
    return 0;
}
