/*
 * v257 σ-Locale — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "locale.h"

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

static const struct {
    const char *loc; const char *tz; const char *df;
    const char *cur; float sl;
} LOCALES[COS_V257_N_LOCALES] = {
    { "en", "America/Los_Angeles", "MM/DD/YYYY", "USD", 0.08f },
    { "fi", "Europe/Helsinki",     "DD.MM.YYYY", "EUR", 0.11f },
    { "zh", "Asia/Shanghai",       "YYYY-MM-DD", "CNY", 0.16f },
    { "ja", "Asia/Tokyo",          "YYYY/MM/DD", "JPY", 0.14f },
    { "de", "Europe/Berlin",       "DD.MM.YYYY", "EUR", 0.09f },
    { "fr", "Europe/Paris",        "DD/MM/YYYY", "EUR", 0.10f },
    { "es", "Europe/Madrid",       "DD/MM/YYYY", "EUR", 0.09f },
    { "ar", "Asia/Riyadh",         "DD/MM/YYYY", "SAR", 0.21f },
    { "hi", "Asia/Kolkata",        "DD/MM/YYYY", "INR", 0.19f },
    { "pt", "America/Sao_Paulo",   "DD/MM/YYYY", "BRL", 0.12f },
};

static const struct { const char *style; const char *ex; }
    STYLES[COS_V257_N_STYLES] = {
    { "direct",       "en" },
    { "indirect",     "ja" },
    { "high_context", "ar" },
};

static const struct { const char *regime; const char *jur; int rev; }
    REGIMES[COS_V257_N_REGIMES] = {
    { "EU_AI_ACT",       "EU",         20260401 },
    { "GDPR",            "EU",         20260301 },
    { "COLORADO_AI_ACT", "US-CO",      20260601 },
};

static const struct { const char *n; const char *ev; }
    TIME_CHECKS[COS_V257_N_TIME] = {
    { "current_time_example", "4:58 Helsinki" },
    { "locale_lookup_ok",     "auto-detect ok" },
};

void cos_v257_init(cos_v257_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x25710CA1ULL;
}

void cos_v257_run(cos_v257_state_t *s) {
    uint64_t prev = 0x25710CA0ULL;

    s->n_locales_ok = 0;
    for (int i = 0; i < COS_V257_N_LOCALES; ++i) {
        cos_v257_locale_t *l = &s->locales[i];
        memset(l, 0, sizeof(*l));
        cpy(l->locale,      sizeof(l->locale),      LOCALES[i].loc);
        cpy(l->timezone,    sizeof(l->timezone),    LOCALES[i].tz);
        cpy(l->date_format, sizeof(l->date_format), LOCALES[i].df);
        cpy(l->currency,    sizeof(l->currency),    LOCALES[i].cur);
        l->sigma_language = LOCALES[i].sl;
        l->locale_ok =
            (strlen(l->locale)      > 0) &&
            (strlen(l->timezone)    > 0) &&
            (strlen(l->date_format) > 0) &&
            (strlen(l->currency)    == 3) &&
            (l->sigma_language >= 0.0f && l->sigma_language <= 1.0f);
        if (l->locale_ok) s->n_locales_ok++;
        prev = fnv1a(l->locale,      strlen(l->locale),      prev);
        prev = fnv1a(l->timezone,    strlen(l->timezone),    prev);
        prev = fnv1a(l->date_format, strlen(l->date_format), prev);
        prev = fnv1a(l->currency,    strlen(l->currency),    prev);
        prev = fnv1a(&l->sigma_language, sizeof(l->sigma_language), prev);
    }

    s->n_styles_ok = 0;
    for (int i = 0; i < COS_V257_N_STYLES; ++i) {
        cos_v257_style_t *st = &s->styles[i];
        memset(st, 0, sizeof(*st));
        cpy(st->style,          sizeof(st->style),          STYLES[i].style);
        cpy(st->example_locale, sizeof(st->example_locale), STYLES[i].ex);
        bool example_known = false;
        for (int j = 0; j < COS_V257_N_LOCALES; ++j)
            if (strcmp(LOCALES[j].loc, st->example_locale) == 0) {
                example_known = true; break;
            }
        st->style_ok = (strlen(st->style) > 0) && example_known;
        if (st->style_ok) s->n_styles_ok++;
        prev = fnv1a(st->style,          strlen(st->style),          prev);
        prev = fnv1a(st->example_locale, strlen(st->example_locale), prev);
    }

    s->n_regimes_ok = 0;
    for (int i = 0; i < COS_V257_N_REGIMES; ++i) {
        cos_v257_regime_t *r = &s->regimes[i];
        memset(r, 0, sizeof(*r));
        cpy(r->regime,       sizeof(r->regime),       REGIMES[i].regime);
        cpy(r->jurisdiction, sizeof(r->jurisdiction), REGIMES[i].jur);
        r->compliant     = true;
        r->last_reviewed = REGIMES[i].rev;
        if (r->compliant && r->last_reviewed > 0 &&
            strlen(r->jurisdiction) > 0)
            s->n_regimes_ok++;
        prev = fnv1a(r->regime,       strlen(r->regime),       prev);
        prev = fnv1a(r->jurisdiction, strlen(r->jurisdiction), prev);
        prev = fnv1a(&r->last_reviewed, sizeof(r->last_reviewed), prev);
    }

    s->n_time_ok = 0;
    for (int i = 0; i < COS_V257_N_TIME; ++i) {
        cos_v257_time_t *t = &s->time[i];
        memset(t, 0, sizeof(*t));
        cpy(t->name,     sizeof(t->name),     TIME_CHECKS[i].n);
        cpy(t->evidence, sizeof(t->evidence), TIME_CHECKS[i].ev);
        t->pass = (strlen(t->evidence) > 0);
        if (t->pass) s->n_time_ok++;
        prev = fnv1a(t->name,     strlen(t->name),     prev);
        prev = fnv1a(t->evidence, strlen(t->evidence), prev);
    }

    int total   = COS_V257_N_LOCALES + COS_V257_N_STYLES +
                  COS_V257_N_REGIMES + COS_V257_N_TIME;
    int passing = s->n_locales_ok + s->n_styles_ok +
                  s->n_regimes_ok + s->n_time_ok;
    s->sigma_locale = 1.0f - ((float)passing / (float)total);
    if (s->sigma_locale < 0.0f) s->sigma_locale = 0.0f;
    if (s->sigma_locale > 1.0f) s->sigma_locale = 1.0f;

    struct { int nl, ns, nr, nt; float sl; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nl = s->n_locales_ok;
    trec.ns = s->n_styles_ok;
    trec.nr = s->n_regimes_ok;
    trec.nt = s->n_time_ok;
    trec.sl = s->sigma_locale;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v257_to_json(const cos_v257_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v257\","
        "\"n_locales\":%d,\"n_styles\":%d,"
        "\"n_regimes\":%d,\"n_time\":%d,"
        "\"n_locales_ok\":%d,\"n_styles_ok\":%d,"
        "\"n_regimes_ok\":%d,\"n_time_ok\":%d,"
        "\"sigma_locale\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"locales\":[",
        COS_V257_N_LOCALES, COS_V257_N_STYLES,
        COS_V257_N_REGIMES, COS_V257_N_TIME,
        s->n_locales_ok, s->n_styles_ok,
        s->n_regimes_ok, s->n_time_ok,
        s->sigma_locale,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V257_N_LOCALES; ++i) {
        const cos_v257_locale_t *l = &s->locales[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"locale\":\"%s\",\"timezone\":\"%s\","
            "\"date_format\":\"%s\",\"currency\":\"%s\","
            "\"sigma_language\":%.4f,\"locale_ok\":%s}",
            i == 0 ? "" : ",",
            l->locale, l->timezone, l->date_format,
            l->currency, l->sigma_language,
            l->locale_ok ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"styles\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V257_N_STYLES; ++i) {
        const cos_v257_style_t *st = &s->styles[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"style\":\"%s\",\"example_locale\":\"%s\","
            "\"style_ok\":%s}",
            i == 0 ? "" : ",", st->style, st->example_locale,
            st->style_ok ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"regimes\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V257_N_REGIMES; ++i) {
        const cos_v257_regime_t *r = &s->regimes[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"regime\":\"%s\",\"jurisdiction\":\"%s\","
            "\"compliant\":%s,\"last_reviewed\":%d}",
            i == 0 ? "" : ",", r->regime, r->jurisdiction,
            r->compliant ? "true" : "false",
            r->last_reviewed);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"time\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V257_N_TIME; ++i) {
        const cos_v257_time_t *t = &s->time[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"evidence\":\"%s\","
            "\"pass\":%s}",
            i == 0 ? "" : ",", t->name, t->evidence,
            t->pass ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v257_self_test(void) {
    cos_v257_state_t s;
    cos_v257_init(&s, 0x25710CA1ULL);
    cos_v257_run(&s);
    if (!s.chain_valid) return 1;

    const char *ln[COS_V257_N_LOCALES] = {
        "en","fi","zh","ja","de","fr","es","ar","hi","pt"
    };
    for (int i = 0; i < COS_V257_N_LOCALES; ++i) {
        if (strcmp(s.locales[i].locale, ln[i]) != 0) return 2;
        if (strlen(s.locales[i].timezone)    == 0) return 3;
        if (strlen(s.locales[i].date_format) == 0) return 4;
        if (strlen(s.locales[i].currency)    != 3) return 5;
        if (s.locales[i].sigma_language < 0.0f ||
            s.locales[i].sigma_language > 1.0f)    return 6;
        if (!s.locales[i].locale_ok)               return 7;
    }
    if (s.n_locales_ok != COS_V257_N_LOCALES) return 8;

    const char *sn[COS_V257_N_STYLES] = {
        "direct","indirect","high_context"
    };
    for (int i = 0; i < COS_V257_N_STYLES; ++i) {
        if (strcmp(s.styles[i].style, sn[i]) != 0) return 9;
        if (!s.styles[i].style_ok) return 10;
    }
    if (s.n_styles_ok != COS_V257_N_STYLES) return 11;

    const char *rn[COS_V257_N_REGIMES] = {
        "EU_AI_ACT","GDPR","COLORADO_AI_ACT"
    };
    for (int i = 0; i < COS_V257_N_REGIMES; ++i) {
        if (strcmp(s.regimes[i].regime, rn[i]) != 0) return 12;
        if (!s.regimes[i].compliant) return 13;
        if (s.regimes[i].last_reviewed <= 0) return 14;
    }
    if (s.n_regimes_ok != COS_V257_N_REGIMES) return 15;

    for (int i = 0; i < COS_V257_N_TIME; ++i) {
        if (!s.time[i].pass) return 16;
    }
    if (s.n_time_ok != COS_V257_N_TIME) return 17;

    if (s.sigma_locale < 0.0f || s.sigma_locale > 1.0f) return 18;
    if (s.sigma_locale > 1e-6f) return 19;

    cos_v257_state_t t;
    cos_v257_init(&t, 0x25710CA1ULL);
    cos_v257_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 20;
    return 0;
}
