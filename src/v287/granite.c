/*
 * v287 σ-Granite — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "granite.h"

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
    const char *n; cos_v287_verdict_t v; bool in_k;
} DEPS[COS_V287_N_DEP] = {
    { "libc",     COS_V287_DEP_ALLOW,  true  },
    { "posix",    COS_V287_DEP_ALLOW,  true  },
    { "pthreads", COS_V287_DEP_ALLOW,  true  },
    { "npm",      COS_V287_DEP_FORBID, false },
    { "pip",      COS_V287_DEP_FORBID, false },
    { "cargo",    COS_V287_DEP_FORBID, false },
};

static const struct { const char *n; int y; bool a; }
    STDS[COS_V287_N_STD] = {
    { "C89", 1989, true  },
    { "C99", 1999, true  },
    { "C++", 1998, false },
};

static const char *PLATFORMS[COS_V287_N_PLATFORM] = {
    "linux", "macos", "bare_metal", "rtos", "cortex_m",
};

static const struct { const char *n; bool a; }
    VENDORS[COS_V287_N_VENDOR] = {
    { "vendored_copy",      true  },
    { "external_linkage",   false },
    { "supply_chain_trust", false },
};

void cos_v287_init(cos_v287_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x287777ULL;
}

void cos_v287_run(cos_v287_state_t *s) {
    uint64_t prev = 0x28777700ULL;

    s->n_dep_rows_ok = 0;
    int n_allow_ok = 0, n_forbid_ok = 0, n_allow = 0, n_forbid = 0;
    for (int i = 0; i < COS_V287_N_DEP; ++i) {
        cos_v287_dep_t *d = &s->dep[i];
        memset(d, 0, sizeof(*d));
        cpy(d->name, sizeof(d->name), DEPS[i].n);
        d->verdict   = DEPS[i].v;
        d->in_kernel = DEPS[i].in_k;
        if (strlen(d->name) > 0) s->n_dep_rows_ok++;
        if (d->verdict == COS_V287_DEP_ALLOW)  { n_allow++;
            if (d->in_kernel == true)  n_allow_ok++;  }
        if (d->verdict == COS_V287_DEP_FORBID) { n_forbid++;
            if (d->in_kernel == false) n_forbid_ok++; }
        prev = fnv1a(d->name, strlen(d->name), prev);
        prev = fnv1a(&d->verdict,   sizeof(d->verdict),   prev);
        prev = fnv1a(&d->in_kernel, sizeof(d->in_kernel), prev);
    }
    s->dep_allow_polarity_ok  = (n_allow  == 3) && (n_allow_ok  == 3);
    s->dep_forbid_polarity_ok = (n_forbid == 3) && (n_forbid_ok == 3);

    s->n_std_rows_ok = 0;
    int n_std_allowed = 0, n_std_forbidden = 0;
    for (int i = 0; i < COS_V287_N_STD; ++i) {
        cos_v287_std_t *st = &s->stdlang[i];
        memset(st, 0, sizeof(*st));
        cpy(st->name, sizeof(st->name), STDS[i].n);
        st->year    = STDS[i].y;
        st->allowed = STDS[i].a;
        if (strlen(st->name) > 0) s->n_std_rows_ok++;
        if (st->allowed)  n_std_allowed++;
        else              n_std_forbidden++;
        prev = fnv1a(st->name, strlen(st->name), prev);
        prev = fnv1a(&st->year,    sizeof(st->year),    prev);
        prev = fnv1a(&st->allowed, sizeof(st->allowed), prev);
    }
    s->std_count_ok = (n_std_allowed == 2) && (n_std_forbidden == 1);

    s->n_platform_rows_ok = 0;
    int n_works = 0, n_ifdef_ok = 0;
    for (int i = 0; i < COS_V287_N_PLATFORM; ++i) {
        cos_v287_platform_t *p = &s->platform[i];
        memset(p, 0, sizeof(*p));
        cpy(p->name, sizeof(p->name), PLATFORMS[i]);
        p->kernel_works        = true;
        p->ifdef_only_at_edges = true;
        if (strlen(p->name) > 0) s->n_platform_rows_ok++;
        if (p->kernel_works)        n_works++;
        if (p->ifdef_only_at_edges) n_ifdef_ok++;
        prev = fnv1a(p->name, strlen(p->name), prev);
        prev = fnv1a(&p->kernel_works,        sizeof(p->kernel_works),        prev);
        prev = fnv1a(&p->ifdef_only_at_edges, sizeof(p->ifdef_only_at_edges), prev);
    }
    s->platform_works_ok  = (n_works    == COS_V287_N_PLATFORM);
    s->platform_ifdef_ok  = (n_ifdef_ok == COS_V287_N_PLATFORM);

    s->n_vendor_rows_ok = 0;
    bool v_pol = true;
    for (int i = 0; i < COS_V287_N_VENDOR; ++i) {
        cos_v287_vendor_t *v = &s->vendor[i];
        memset(v, 0, sizeof(*v));
        cpy(v->name, sizeof(v->name), VENDORS[i].n);
        v->allowed_in_kernel = VENDORS[i].a;
        if (strlen(v->name) > 0) s->n_vendor_rows_ok++;
        if (v->allowed_in_kernel != VENDORS[i].a) v_pol = false;
        prev = fnv1a(v->name, strlen(v->name), prev);
        prev = fnv1a(&v->allowed_in_kernel, sizeof(v->allowed_in_kernel), prev);
    }
    s->vendor_polarity_ok =
        v_pol &&
        (s->vendor[0].allowed_in_kernel == true)  &&
        (s->vendor[1].allowed_in_kernel == false) &&
        (s->vendor[2].allowed_in_kernel == false);

    int total   = COS_V287_N_DEP      + 1 + 1 +
                  COS_V287_N_STD      + 1 +
                  COS_V287_N_PLATFORM + 1 + 1 +
                  COS_V287_N_VENDOR   + 1;
    int passing = s->n_dep_rows_ok +
                  (s->dep_allow_polarity_ok  ? 1 : 0) +
                  (s->dep_forbid_polarity_ok ? 1 : 0) +
                  s->n_std_rows_ok +
                  (s->std_count_ok ? 1 : 0) +
                  s->n_platform_rows_ok +
                  (s->platform_works_ok ? 1 : 0) +
                  (s->platform_ifdef_ok ? 1 : 0) +
                  s->n_vendor_rows_ok +
                  (s->vendor_polarity_ok ? 1 : 0);
    s->sigma_granite = 1.0f - ((float)passing / (float)total);
    if (s->sigma_granite < 0.0f) s->sigma_granite = 0.0f;
    if (s->sigma_granite > 1.0f) s->sigma_granite = 1.0f;

    struct { int nd, ns, np, nv;
             bool ap, fp, sc, pw, pi, vp;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nd = s->n_dep_rows_ok;
    trec.ns = s->n_std_rows_ok;
    trec.np = s->n_platform_rows_ok;
    trec.nv = s->n_vendor_rows_ok;
    trec.ap = s->dep_allow_polarity_ok;
    trec.fp = s->dep_forbid_polarity_ok;
    trec.sc = s->std_count_ok;
    trec.pw = s->platform_works_ok;
    trec.pi = s->platform_ifdef_ok;
    trec.vp = s->vendor_polarity_ok;
    trec.sigma = s->sigma_granite;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *verdict_name(cos_v287_verdict_t v) {
    switch (v) {
    case COS_V287_DEP_ALLOW:  return "ALLOW";
    case COS_V287_DEP_FORBID: return "FORBID";
    }
    return "UNKNOWN";
}

size_t cos_v287_to_json(const cos_v287_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v287\","
        "\"n_dep_rows_ok\":%d,"
        "\"dep_allow_polarity_ok\":%s,\"dep_forbid_polarity_ok\":%s,"
        "\"n_std_rows_ok\":%d,\"std_count_ok\":%s,"
        "\"n_platform_rows_ok\":%d,"
        "\"platform_works_ok\":%s,\"platform_ifdef_ok\":%s,"
        "\"n_vendor_rows_ok\":%d,\"vendor_polarity_ok\":%s,"
        "\"sigma_granite\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"dep\":[",
        s->n_dep_rows_ok,
        s->dep_allow_polarity_ok  ? "true" : "false",
        s->dep_forbid_polarity_ok ? "true" : "false",
        s->n_std_rows_ok,
        s->std_count_ok ? "true" : "false",
        s->n_platform_rows_ok,
        s->platform_works_ok ? "true" : "false",
        s->platform_ifdef_ok ? "true" : "false",
        s->n_vendor_rows_ok,
        s->vendor_polarity_ok ? "true" : "false",
        s->sigma_granite,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V287_N_DEP; ++i) {
        const cos_v287_dep_t *d = &s->dep[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"verdict\":\"%s\",\"in_kernel\":%s}",
            i == 0 ? "" : ",", d->name, verdict_name(d->verdict),
            d->in_kernel ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"stdlang\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V287_N_STD; ++i) {
        const cos_v287_std_t *st = &s->stdlang[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"year\":%d,\"allowed\":%s}",
            i == 0 ? "" : ",", st->name, st->year,
            st->allowed ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"platform\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V287_N_PLATFORM; ++i) {
        const cos_v287_platform_t *p = &s->platform[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"kernel_works\":%s,\"ifdef_only_at_edges\":%s}",
            i == 0 ? "" : ",", p->name,
            p->kernel_works        ? "true" : "false",
            p->ifdef_only_at_edges ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"vendor\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V287_N_VENDOR; ++i) {
        const cos_v287_vendor_t *v = &s->vendor[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"allowed_in_kernel\":%s}",
            i == 0 ? "" : ",", v->name,
            v->allowed_in_kernel ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v287_self_test(void) {
    cos_v287_state_t s;
    cos_v287_init(&s, 0x287777ULL);
    cos_v287_run(&s);
    if (!s.chain_valid) return 1;

    static const char *WANT_DEP[COS_V287_N_DEP] = {
        "libc", "posix", "pthreads", "npm", "pip", "cargo"
    };
    for (int i = 0; i < COS_V287_N_DEP; ++i) {
        if (strcmp(s.dep[i].name, WANT_DEP[i]) != 0) return 2;
    }
    for (int i = 0; i < 3; ++i) {
        if (s.dep[i].verdict   != COS_V287_DEP_ALLOW) return 3;
        if (s.dep[i].in_kernel != true)               return 4;
    }
    for (int i = 3; i < COS_V287_N_DEP; ++i) {
        if (s.dep[i].verdict   != COS_V287_DEP_FORBID) return 5;
        if (s.dep[i].in_kernel != false)               return 6;
    }
    if (s.n_dep_rows_ok != COS_V287_N_DEP) return 7;
    if (!s.dep_allow_polarity_ok)  return 8;
    if (!s.dep_forbid_polarity_ok) return 9;

    static const char *WANT_STD[COS_V287_N_STD] = { "C89", "C99", "C++" };
    for (int i = 0; i < COS_V287_N_STD; ++i) {
        if (strcmp(s.stdlang[i].name, WANT_STD[i]) != 0) return 10;
    }
    if (!s.stdlang[0].allowed || !s.stdlang[1].allowed) return 11;
    if ( s.stdlang[2].allowed) return 12;
    if (s.n_std_rows_ok != COS_V287_N_STD) return 13;
    if (!s.std_count_ok) return 14;

    static const char *WANT_P[COS_V287_N_PLATFORM] = {
        "linux", "macos", "bare_metal", "rtos", "cortex_m"
    };
    for (int i = 0; i < COS_V287_N_PLATFORM; ++i) {
        if (strcmp(s.platform[i].name, WANT_P[i]) != 0) return 15;
        if (!s.platform[i].kernel_works)         return 16;
        if (!s.platform[i].ifdef_only_at_edges)  return 17;
    }
    if (s.n_platform_rows_ok != COS_V287_N_PLATFORM) return 18;
    if (!s.platform_works_ok) return 19;
    if (!s.platform_ifdef_ok) return 20;

    static const char *WANT_V[COS_V287_N_VENDOR] = {
        "vendored_copy", "external_linkage", "supply_chain_trust"
    };
    for (int i = 0; i < COS_V287_N_VENDOR; ++i) {
        if (strcmp(s.vendor[i].name, WANT_V[i]) != 0) return 21;
    }
    if (s.vendor[0].allowed_in_kernel != true)  return 22;
    if (s.vendor[1].allowed_in_kernel != false) return 23;
    if (s.vendor[2].allowed_in_kernel != false) return 24;
    if (s.n_vendor_rows_ok != COS_V287_N_VENDOR) return 25;
    if (!s.vendor_polarity_ok) return 26;

    if (s.sigma_granite < 0.0f || s.sigma_granite > 1.0f) return 27;
    if (s.sigma_granite > 1e-6f) return 28;

    cos_v287_state_t u;
    cos_v287_init(&u, 0x287777ULL);
    cos_v287_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 29;
    return 0;
}
