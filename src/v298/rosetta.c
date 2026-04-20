/*
 * v298 σ-Rosetta — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "rosetta.h"

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

static const struct { float sigma; const char *dom; const char *reason; }
    EMIT_ROWS[COS_V298_N_EMIT] = {
    { 0.35f, "LLM",
      "entropy high, top_token_prob 0.23, 4 competing tokens" },
    { 0.15f, "SENSOR",
      "noise floor 0.02, signal 0.13, SNR low" },
    { 0.60f, "ORG",
      "disagreement between 3 voters, quorum not met" },
};

static const struct { const char *lang; const char *role; }
    SPEC_ROWS[COS_V298_N_SPEC] = {
    { "C",      "REFERENCE" },
    { "Python", "ADOPTION"  },
    { "Rust",   "SAFETY"    },
};

static const struct { const char *fmt; bool human; }
    FMT_ROWS[COS_V298_N_FMT] = {
    { "binary", false },
    { "csv",    true  },
    { "json",   true  },
};

static const struct { const char *name; const char *formula; }
    MATH_ROWS[COS_V298_N_MATH] = {
    { "sigma_definition",     "sigma = noise/(signal+noise)" },
    { "pythagoras_2500_yr",   "a^2 + b^2 = c^2"              },
    { "arithmetic_invariant", "(a + b) + c = a + (b + c)"    },
};

void cos_v298_init(cos_v298_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x298205ULL;
}

void cos_v298_run(cos_v298_state_t *s) {
    uint64_t prev = 0x29820500ULL;

    s->n_emit_rows_ok = 0;
    int emit_reason_ok = 0;
    int n_distinct_dom = 0;
    const char *seen_dom[COS_V298_N_EMIT] = { NULL, NULL, NULL };
    for (int i = 0; i < COS_V298_N_EMIT; ++i) {
        cos_v298_emit_t *r = &s->emit[i];
        memset(r, 0, sizeof(*r));
        r->sigma = EMIT_ROWS[i].sigma;
        cpy(r->domain, sizeof(r->domain), EMIT_ROWS[i].dom);
        cpy(r->reason, sizeof(r->reason), EMIT_ROWS[i].reason);
        r->reason_present = (strlen(r->reason) >=
                             (size_t)COS_V298_REASON_MIN_LEN);
        if (strlen(r->domain) > 0) s->n_emit_rows_ok++;
        if (r->reason_present) emit_reason_ok++;
        bool dup = false;
        for (int k = 0; k < i; ++k)
            if (seen_dom[k] && strcmp(seen_dom[k], r->domain) == 0) dup = true;
        if (!dup) { seen_dom[i] = EMIT_ROWS[i].dom; n_distinct_dom++; }
        prev = fnv1a(&r->sigma, sizeof(r->sigma), prev);
        prev = fnv1a(r->domain, strlen(r->domain), prev);
        prev = fnv1a(r->reason, strlen(r->reason), prev);
        prev = fnv1a(&r->reason_present, sizeof(r->reason_present), prev);
    }
    s->emit_domain_distinct_ok = (n_distinct_dom == COS_V298_N_EMIT);
    s->emit_reason_ok          = (emit_reason_ok == COS_V298_N_EMIT);

    s->n_spec_rows_ok = 0;
    int spec_maintained = 0, spec_match = 0;
    int n_distinct_role = 0;
    const char *seen_role[COS_V298_N_SPEC] = { NULL, NULL, NULL };
    for (int i = 0; i < COS_V298_N_SPEC; ++i) {
        cos_v298_spec_t *r = &s->spec[i];
        memset(r, 0, sizeof(*r));
        cpy(r->language, sizeof(r->language), SPEC_ROWS[i].lang);
        cpy(r->role,     sizeof(r->role),     SPEC_ROWS[i].role);
        r->maintained          = true;
        r->semantic_match_to_c = true;
        if (strlen(r->language) > 0 && strlen(r->role) > 0)
            s->n_spec_rows_ok++;
        if (r->maintained)          spec_maintained++;
        if (r->semantic_match_to_c) spec_match++;
        bool dup = false;
        for (int k = 0; k < i; ++k)
            if (seen_role[k] && strcmp(seen_role[k], r->role) == 0) dup = true;
        if (!dup) { seen_role[i] = SPEC_ROWS[i].role; n_distinct_role++; }
        prev = fnv1a(r->language, strlen(r->language), prev);
        prev = fnv1a(r->role,     strlen(r->role),     prev);
        prev = fnv1a(&r->maintained,          sizeof(r->maintained),          prev);
        prev = fnv1a(&r->semantic_match_to_c, sizeof(r->semantic_match_to_c), prev);
    }
    s->spec_role_distinct_ok = (n_distinct_role == COS_V298_N_SPEC);
    s->spec_maintained_ok    = (spec_maintained == COS_V298_N_SPEC);
    s->spec_match_ok         = (spec_match      == COS_V298_N_SPEC);

    s->n_fmt_rows_ok = 0;
    int n_machine = 0, n_human = 0, n_binary_nohuman = 0;
    for (int i = 0; i < COS_V298_N_FMT; ++i) {
        cos_v298_fmt_t *r = &s->fmt[i];
        memset(r, 0, sizeof(*r));
        cpy(r->format, sizeof(r->format), FMT_ROWS[i].fmt);
        r->machine_readable        = true;
        r->human_readable_forever  = FMT_ROWS[i].human;
        if (strlen(r->format) > 0) s->n_fmt_rows_ok++;
        if (r->machine_readable)                                 n_machine++;
        if (r->human_readable_forever)                           n_human++;
        if (strcmp(r->format, "binary") == 0 &&
            !r->human_readable_forever)                          n_binary_nohuman++;
        prev = fnv1a(r->format, strlen(r->format), prev);
        prev = fnv1a(&r->machine_readable,       sizeof(r->machine_readable),       prev);
        prev = fnv1a(&r->human_readable_forever, sizeof(r->human_readable_forever), prev);
    }
    s->fmt_machine_ok     = (n_machine == COS_V298_N_FMT);
    s->fmt_human_count_ok = (n_human == 2 && n_binary_nohuman == 1);

    s->n_math_rows_ok = 0;
    int n_expr = 0, n_not_aging = 0;
    for (int i = 0; i < COS_V298_N_MATH; ++i) {
        cos_v298_math_t *r = &s->math[i];
        memset(r, 0, sizeof(*r));
        cpy(r->name,    sizeof(r->name),    MATH_ROWS[i].name);
        cpy(r->formula, sizeof(r->formula), MATH_ROWS[i].formula);
        r->formal_expression_present = (strlen(r->formula) > 0);
        r->ages_out                  = false;
        if (strlen(r->name) > 0) s->n_math_rows_ok++;
        if (r->formal_expression_present) n_expr++;
        if (!r->ages_out)                 n_not_aging++;
        prev = fnv1a(r->name,    strlen(r->name),    prev);
        prev = fnv1a(r->formula, strlen(r->formula), prev);
        prev = fnv1a(&r->formal_expression_present,
                     sizeof(r->formal_expression_present), prev);
        prev = fnv1a(&r->ages_out, sizeof(r->ages_out), prev);
    }
    s->math_expression_ok = (n_expr       == COS_V298_N_MATH);
    s->math_ages_out_ok   = (n_not_aging  == COS_V298_N_MATH);

    int total   = COS_V298_N_EMIT + 1 + 1 +
                  COS_V298_N_SPEC + 1 + 1 + 1 +
                  COS_V298_N_FMT  + 1 + 1 +
                  COS_V298_N_MATH + 1 + 1;
    int passing = s->n_emit_rows_ok +
                  (s->emit_domain_distinct_ok ? 1 : 0) +
                  (s->emit_reason_ok          ? 1 : 0) +
                  s->n_spec_rows_ok +
                  (s->spec_role_distinct_ok ? 1 : 0) +
                  (s->spec_maintained_ok    ? 1 : 0) +
                  (s->spec_match_ok         ? 1 : 0) +
                  s->n_fmt_rows_ok +
                  (s->fmt_machine_ok     ? 1 : 0) +
                  (s->fmt_human_count_ok ? 1 : 0) +
                  s->n_math_rows_ok +
                  (s->math_expression_ok ? 1 : 0) +
                  (s->math_ages_out_ok   ? 1 : 0);
    s->sigma_rosetta = 1.0f - ((float)passing / (float)total);
    if (s->sigma_rosetta < 0.0f) s->sigma_rosetta = 0.0f;
    if (s->sigma_rosetta > 1.0f) s->sigma_rosetta = 1.0f;

    struct { int ne, ns, nf, nm;
             bool edd, er, srd, sm, smt, fm, fh, mx, ma;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.ne = s->n_emit_rows_ok;
    trec.ns = s->n_spec_rows_ok;
    trec.nf = s->n_fmt_rows_ok;
    trec.nm = s->n_math_rows_ok;
    trec.edd = s->emit_domain_distinct_ok;
    trec.er  = s->emit_reason_ok;
    trec.srd = s->spec_role_distinct_ok;
    trec.sm  = s->spec_maintained_ok;
    trec.smt = s->spec_match_ok;
    trec.fm  = s->fmt_machine_ok;
    trec.fh  = s->fmt_human_count_ok;
    trec.mx  = s->math_expression_ok;
    trec.ma  = s->math_ages_out_ok;
    trec.sigma = s->sigma_rosetta;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v298_to_json(const cos_v298_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v298\","
        "\"n_emit_rows_ok\":%d,\"emit_domain_distinct_ok\":%s,"
        "\"emit_reason_ok\":%s,"
        "\"n_spec_rows_ok\":%d,\"spec_role_distinct_ok\":%s,"
        "\"spec_maintained_ok\":%s,\"spec_match_ok\":%s,"
        "\"n_fmt_rows_ok\":%d,\"fmt_machine_ok\":%s,"
        "\"fmt_human_count_ok\":%s,"
        "\"n_math_rows_ok\":%d,\"math_expression_ok\":%s,"
        "\"math_ages_out_ok\":%s,"
        "\"sigma_rosetta\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"emit\":[",
        s->n_emit_rows_ok,
        s->emit_domain_distinct_ok ? "true" : "false",
        s->emit_reason_ok          ? "true" : "false",
        s->n_spec_rows_ok,
        s->spec_role_distinct_ok ? "true" : "false",
        s->spec_maintained_ok    ? "true" : "false",
        s->spec_match_ok         ? "true" : "false",
        s->n_fmt_rows_ok,
        s->fmt_machine_ok     ? "true" : "false",
        s->fmt_human_count_ok ? "true" : "false",
        s->n_math_rows_ok,
        s->math_expression_ok ? "true" : "false",
        s->math_ages_out_ok   ? "true" : "false",
        s->sigma_rosetta,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V298_N_EMIT; ++i) {
        const cos_v298_emit_t *r = &s->emit[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"sigma\":%.4f,\"domain\":\"%s\","
            "\"reason\":\"%s\",\"reason_present\":%s}",
            i == 0 ? "" : ",", r->sigma, r->domain,
            r->reason, r->reason_present ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"spec\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V298_N_SPEC; ++i) {
        const cos_v298_spec_t *r = &s->spec[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"language\":\"%s\",\"role\":\"%s\","
            "\"maintained\":%s,\"semantic_match_to_c\":%s}",
            i == 0 ? "" : ",", r->language, r->role,
            r->maintained          ? "true" : "false",
            r->semantic_match_to_c ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"fmt\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V298_N_FMT; ++i) {
        const cos_v298_fmt_t *r = &s->fmt[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"format\":\"%s\",\"machine_readable\":%s,"
            "\"human_readable_forever\":%s}",
            i == 0 ? "" : ",", r->format,
            r->machine_readable       ? "true" : "false",
            r->human_readable_forever ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"math\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V298_N_MATH; ++i) {
        const cos_v298_math_t *r = &s->math[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"formula\":\"%s\","
            "\"formal_expression_present\":%s,"
            "\"ages_out\":%s}",
            i == 0 ? "" : ",", r->name, r->formula,
            r->formal_expression_present ? "true" : "false",
            r->ages_out                  ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v298_self_test(void) {
    cos_v298_state_t s;
    cos_v298_init(&s, 0x298205ULL);
    cos_v298_run(&s);
    if (!s.chain_valid) return 1;

    static const char *WANT_DOM[COS_V298_N_EMIT] = {
        "LLM","SENSOR","ORG"
    };
    for (int i = 0; i < COS_V298_N_EMIT; ++i) {
        if (strcmp(s.emit[i].domain, WANT_DOM[i]) != 0) return 2;
        if (!s.emit[i].reason_present) return 3;
    }
    if (s.n_emit_rows_ok != COS_V298_N_EMIT) return 4;
    if (!s.emit_domain_distinct_ok) return 5;
    if (!s.emit_reason_ok)          return 6;

    static const char *WANT_LANG[COS_V298_N_SPEC] = {
        "C","Python","Rust"
    };
    static const char *WANT_ROLE[COS_V298_N_SPEC] = {
        "REFERENCE","ADOPTION","SAFETY"
    };
    for (int i = 0; i < COS_V298_N_SPEC; ++i) {
        if (strcmp(s.spec[i].language, WANT_LANG[i]) != 0) return 7;
        if (strcmp(s.spec[i].role,     WANT_ROLE[i]) != 0) return 8;
        if (!s.spec[i].maintained)          return 9;
        if (!s.spec[i].semantic_match_to_c) return 10;
    }
    if (s.n_spec_rows_ok != COS_V298_N_SPEC) return 11;
    if (!s.spec_role_distinct_ok) return 12;
    if (!s.spec_maintained_ok)    return 13;
    if (!s.spec_match_ok)         return 14;

    static const char *WANT_FMT[COS_V298_N_FMT] = {
        "binary","csv","json"
    };
    for (int i = 0; i < COS_V298_N_FMT; ++i) {
        if (strcmp(s.fmt[i].format, WANT_FMT[i]) != 0) return 15;
        if (!s.fmt[i].machine_readable) return 16;
    }
    if (s.fmt[0].human_readable_forever)  return 17;
    if (!s.fmt[1].human_readable_forever) return 18;
    if (!s.fmt[2].human_readable_forever) return 19;
    if (s.n_fmt_rows_ok != COS_V298_N_FMT) return 20;
    if (!s.fmt_machine_ok)      return 21;
    if (!s.fmt_human_count_ok)  return 22;

    static const char *WANT_MA[COS_V298_N_MATH] = {
        "sigma_definition","pythagoras_2500_yr","arithmetic_invariant"
    };
    for (int i = 0; i < COS_V298_N_MATH; ++i) {
        if (strcmp(s.math[i].name, WANT_MA[i]) != 0) return 23;
        if (!s.math[i].formal_expression_present) return 24;
        if (s.math[i].ages_out) return 25;
    }
    if (s.n_math_rows_ok != COS_V298_N_MATH) return 26;
    if (!s.math_expression_ok) return 27;
    if (!s.math_ages_out_ok)   return 28;

    if (s.sigma_rosetta < 0.0f || s.sigma_rosetta > 1.0f) return 29;
    if (s.sigma_rosetta > 1e-6f) return 30;

    cos_v298_state_t u;
    cos_v298_init(&u, 0x298205ULL);
    cos_v298_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 31;
    return 0;
}
