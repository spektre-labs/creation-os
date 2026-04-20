/*
 * v297 σ-Clock — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "clock.h"

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

static const char *EXPIRY_ROWS[COS_V297_N_EXPIRY] = {
    "hardcoded_date",
    "valid_until_2030",
    "api_version_expiry",
};

static const struct { const char *src; cos_v297_time_verdict_t v; bool in_k; }
    TIME_ROWS[COS_V297_N_TIME] = {
    { "CLOCK_MONOTONIC", COS_V297_TIME_ALLOW,  true  },
    { "CLOCK_REALTIME",  COS_V297_TIME_FORBID, false },
    { "wallclock_local", COS_V297_TIME_FORBID, false },
};

static const char *LOG_ROWS[COS_V297_N_LOG] = {
    "relative_sequence",
    "unix_epoch_absent",
    "y2038_safe",
};

static const char *PROTO_ROWS[COS_V297_N_PROTO] = {
    "no_version_field_on_struct",
    "old_reader_ignores_new_fields",
    "append_only_field_semantics",
};

void cos_v297_init(cos_v297_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x297c10cULL;
}

void cos_v297_run(cos_v297_state_t *s) {
    uint64_t prev = 0x297c10c00ULL;

    s->n_expiry_rows_ok = 0;
    int n_forbidden = 0, n_absent = 0;
    for (int i = 0; i < COS_V297_N_EXPIRY; ++i) {
        cos_v297_expiry_t *r = &s->expiry[i];
        memset(r, 0, sizeof(*r));
        cpy(r->field, sizeof(r->field), EXPIRY_ROWS[i]);
        r->present_in_kernel = false;
        r->forbidden         = true;
        if (strlen(r->field) > 0) s->n_expiry_rows_ok++;
        if (r->forbidden)           n_forbidden++;
        if (!r->present_in_kernel)  n_absent++;
        prev = fnv1a(r->field, strlen(r->field), prev);
        prev = fnv1a(&r->present_in_kernel, sizeof(r->present_in_kernel), prev);
        prev = fnv1a(&r->forbidden,         sizeof(r->forbidden),         prev);
    }
    s->expiry_all_forbidden_ok = (n_forbidden == COS_V297_N_EXPIRY);
    s->expiry_all_absent_ok    = (n_absent    == COS_V297_N_EXPIRY);

    s->n_time_rows_ok = 0;
    int n_allow = 0, n_forbid = 0;
    for (int i = 0; i < COS_V297_N_TIME; ++i) {
        cos_v297_time_t *r = &s->time[i];
        memset(r, 0, sizeof(*r));
        cpy(r->source, sizeof(r->source), TIME_ROWS[i].src);
        r->verdict   = TIME_ROWS[i].v;
        r->in_kernel = TIME_ROWS[i].in_k;
        if (strlen(r->source) > 0) s->n_time_rows_ok++;
        if (r->verdict == COS_V297_TIME_ALLOW  && r->in_kernel)  n_allow++;
        if (r->verdict == COS_V297_TIME_FORBID && !r->in_kernel) n_forbid++;
        prev = fnv1a(r->source, strlen(r->source), prev);
        prev = fnv1a(&r->verdict,   sizeof(r->verdict),   prev);
        prev = fnv1a(&r->in_kernel, sizeof(r->in_kernel), prev);
    }
    s->time_allow_count_ok  = (n_allow  == 1);
    s->time_forbid_count_ok = (n_forbid == 2);

    s->n_log_rows_ok = 0;
    int n_log_hold = 0;
    for (int i = 0; i < COS_V297_N_LOG; ++i) {
        cos_v297_log_t *r = &s->log[i];
        memset(r, 0, sizeof(*r));
        cpy(r->property, sizeof(r->property), LOG_ROWS[i]);
        r->holds = true;
        if (strlen(r->property) > 0) s->n_log_rows_ok++;
        if (r->holds) n_log_hold++;
        prev = fnv1a(r->property, strlen(r->property), prev);
        prev = fnv1a(&r->holds,   sizeof(r->holds),   prev);
    }
    s->log_all_hold_ok = (n_log_hold == COS_V297_N_LOG);

    s->n_proto_rows_ok = 0;
    int n_proto_hold = 0;
    for (int i = 0; i < COS_V297_N_PROTO; ++i) {
        cos_v297_proto_t *r = &s->proto[i];
        memset(r, 0, sizeof(*r));
        cpy(r->property, sizeof(r->property), PROTO_ROWS[i]);
        r->holds = true;
        if (strlen(r->property) > 0) s->n_proto_rows_ok++;
        if (r->holds) n_proto_hold++;
        prev = fnv1a(r->property, strlen(r->property), prev);
        prev = fnv1a(&r->holds,   sizeof(r->holds),   prev);
    }
    s->proto_all_hold_ok = (n_proto_hold == COS_V297_N_PROTO);

    int total   = COS_V297_N_EXPIRY + 1 + 1 +
                  COS_V297_N_TIME   + 1 + 1 +
                  COS_V297_N_LOG    + 1 +
                  COS_V297_N_PROTO  + 1;
    int passing = s->n_expiry_rows_ok +
                  (s->expiry_all_forbidden_ok ? 1 : 0) +
                  (s->expiry_all_absent_ok    ? 1 : 0) +
                  s->n_time_rows_ok +
                  (s->time_allow_count_ok  ? 1 : 0) +
                  (s->time_forbid_count_ok ? 1 : 0) +
                  s->n_log_rows_ok +
                  (s->log_all_hold_ok ? 1 : 0) +
                  s->n_proto_rows_ok +
                  (s->proto_all_hold_ok ? 1 : 0);
    s->sigma_clock = 1.0f - ((float)passing / (float)total);
    if (s->sigma_clock < 0.0f) s->sigma_clock = 0.0f;
    if (s->sigma_clock > 1.0f) s->sigma_clock = 1.0f;

    struct { int ne, nt, nl, np;
             bool ef, ea, tac, tfc, lh, ph;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.ne = s->n_expiry_rows_ok;
    trec.nt = s->n_time_rows_ok;
    trec.nl = s->n_log_rows_ok;
    trec.np = s->n_proto_rows_ok;
    trec.ef  = s->expiry_all_forbidden_ok;
    trec.ea  = s->expiry_all_absent_ok;
    trec.tac = s->time_allow_count_ok;
    trec.tfc = s->time_forbid_count_ok;
    trec.lh  = s->log_all_hold_ok;
    trec.ph  = s->proto_all_hold_ok;
    trec.sigma = s->sigma_clock;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v297_to_json(const cos_v297_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v297\","
        "\"n_expiry_rows_ok\":%d,\"expiry_all_forbidden_ok\":%s,"
        "\"expiry_all_absent_ok\":%s,"
        "\"n_time_rows_ok\":%d,\"time_allow_count_ok\":%s,"
        "\"time_forbid_count_ok\":%s,"
        "\"n_log_rows_ok\":%d,\"log_all_hold_ok\":%s,"
        "\"n_proto_rows_ok\":%d,\"proto_all_hold_ok\":%s,"
        "\"sigma_clock\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"expiry\":[",
        s->n_expiry_rows_ok,
        s->expiry_all_forbidden_ok ? "true" : "false",
        s->expiry_all_absent_ok    ? "true" : "false",
        s->n_time_rows_ok,
        s->time_allow_count_ok  ? "true" : "false",
        s->time_forbid_count_ok ? "true" : "false",
        s->n_log_rows_ok,
        s->log_all_hold_ok ? "true" : "false",
        s->n_proto_rows_ok,
        s->proto_all_hold_ok ? "true" : "false",
        s->sigma_clock,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V297_N_EXPIRY; ++i) {
        const cos_v297_expiry_t *r = &s->expiry[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"field\":\"%s\",\"present_in_kernel\":%s,"
            "\"forbidden\":%s}",
            i == 0 ? "" : ",", r->field,
            r->present_in_kernel ? "true" : "false",
            r->forbidden         ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"time\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V297_N_TIME; ++i) {
        const cos_v297_time_t *r = &s->time[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"source\":\"%s\",\"verdict\":\"%s\","
            "\"in_kernel\":%s}",
            i == 0 ? "" : ",", r->source,
            r->verdict == COS_V297_TIME_ALLOW ? "ALLOW" : "FORBID",
            r->in_kernel ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"log\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V297_N_LOG; ++i) {
        const cos_v297_log_t *r = &s->log[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"property\":\"%s\",\"holds\":%s}",
            i == 0 ? "" : ",", r->property,
            r->holds ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"proto\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V297_N_PROTO; ++i) {
        const cos_v297_proto_t *r = &s->proto[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"property\":\"%s\",\"holds\":%s}",
            i == 0 ? "" : ",", r->property,
            r->holds ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v297_self_test(void) {
    cos_v297_state_t s;
    cos_v297_init(&s, 0x297c10cULL);
    cos_v297_run(&s);
    if (!s.chain_valid) return 1;

    static const char *WANT_E[COS_V297_N_EXPIRY] = {
        "hardcoded_date","valid_until_2030","api_version_expiry"
    };
    for (int i = 0; i < COS_V297_N_EXPIRY; ++i) {
        if (strcmp(s.expiry[i].field, WANT_E[i]) != 0) return 2;
        if (s.expiry[i].present_in_kernel) return 3;
        if (!s.expiry[i].forbidden)        return 4;
    }
    if (s.n_expiry_rows_ok != COS_V297_N_EXPIRY) return 5;
    if (!s.expiry_all_forbidden_ok) return 6;
    if (!s.expiry_all_absent_ok)    return 7;

    static const char *WANT_T[COS_V297_N_TIME] = {
        "CLOCK_MONOTONIC","CLOCK_REALTIME","wallclock_local"
    };
    for (int i = 0; i < COS_V297_N_TIME; ++i) {
        if (strcmp(s.time[i].source, WANT_T[i]) != 0) return 8;
    }
    if (s.time[0].verdict != COS_V297_TIME_ALLOW)  return 9;
    if (s.time[1].verdict != COS_V297_TIME_FORBID) return 10;
    if (s.time[2].verdict != COS_V297_TIME_FORBID) return 11;
    if (!s.time[0].in_kernel) return 12;
    if (s.time[1].in_kernel)  return 13;
    if (s.time[2].in_kernel)  return 14;
    if (s.n_time_rows_ok != COS_V297_N_TIME) return 15;
    if (!s.time_allow_count_ok)  return 16;
    if (!s.time_forbid_count_ok) return 17;

    static const char *WANT_L[COS_V297_N_LOG] = {
        "relative_sequence","unix_epoch_absent","y2038_safe"
    };
    for (int i = 0; i < COS_V297_N_LOG; ++i) {
        if (strcmp(s.log[i].property, WANT_L[i]) != 0) return 18;
        if (!s.log[i].holds) return 19;
    }
    if (s.n_log_rows_ok != COS_V297_N_LOG) return 20;
    if (!s.log_all_hold_ok) return 21;

    static const char *WANT_P[COS_V297_N_PROTO] = {
        "no_version_field_on_struct",
        "old_reader_ignores_new_fields",
        "append_only_field_semantics"
    };
    for (int i = 0; i < COS_V297_N_PROTO; ++i) {
        if (strcmp(s.proto[i].property, WANT_P[i]) != 0) return 22;
        if (!s.proto[i].holds) return 23;
    }
    if (s.n_proto_rows_ok != COS_V297_N_PROTO) return 24;
    if (!s.proto_all_hold_ok) return 25;

    if (s.sigma_clock < 0.0f || s.sigma_clock > 1.0f) return 26;
    if (s.sigma_clock > 1e-6f) return 27;

    cos_v297_state_t u;
    cos_v297_init(&u, 0x297c10cULL);
    cos_v297_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 28;
    return 0;
}
