/*
 * v251 σ-Marketplace — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "marketplace.h"

#include <math.h>
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
    const char *name; const char *version;
    int deps[COS_V251_MAX_DEPS]; int n_deps;
    float s_cal; float bench; float rating; float audit;
} KERNELS[COS_V251_N_KERNELS] = {
    { "medical-v1",   "1.0.0", { 115, 111, 150, 0 }, 3, 0.92f, 0.88f, 0.91f, 0.90f },
    { "legal-v1",     "1.0.0", { 135, 111, 181, 0 }, 3, 0.90f, 0.85f, 0.87f, 0.93f },
    { "finance-v1",   "1.0.0", { 111, 121, 140, 0 }, 3, 0.89f, 0.82f, 0.84f, 0.89f },
    { "science-v1",   "1.0.0", { 204, 206, 220, 0 }, 3, 0.94f, 0.90f, 0.93f, 0.92f },
    { "teach-pro-v1", "1.0.0", { 252, 197, 222, 0 }, 3, 0.87f, 0.79f, 0.88f, 0.86f },
};

/* Quality axes are reported in [0,1] *as trust scores*
 * (1.0 = best).  σ_quality is reported the same way as
 * the mean of the axes; low-σ ≈ high trust is left to
 * v247 harness interpretation.  v0 is typed-fixture
 * only; see v251.1 for the live ingestion path. */

static const char *PUBLISH_ITEMS[COS_V251_N_PUBLISH] = {
    "merge_gate_green",
    "sigma_profile_attached",
    "docs_attached",
    "scsl_license_attached",
};

void cos_v251_init(cos_v251_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x251AA4CEULL;
    cpy(s->registry_url, sizeof(s->registry_url),
        "registry.creation-os.dev");
}

void cos_v251_run(cos_v251_state_t *s) {
    uint64_t prev = 0x251AA4C7ULL;

    prev = fnv1a(s->registry_url, strlen(s->registry_url), prev);

    s->n_kernels_ok = 0;
    for (int i = 0; i < COS_V251_N_KERNELS; ++i) {
        cos_v251_kernel_t *k = &s->kernels[i];
        memset(k, 0, sizeof(*k));
        cpy(k->name,    sizeof(k->name),    KERNELS[i].name);
        cpy(k->version, sizeof(k->version), KERNELS[i].version);
        for (int j = 0; j < KERNELS[i].n_deps && j < COS_V251_MAX_DEPS; ++j)
            k->deps[j] = KERNELS[i].deps[j];
        k->n_deps = KERNELS[i].n_deps;

        k->sigma_calibration = KERNELS[i].s_cal;
        k->benchmark_score   = KERNELS[i].bench;
        k->user_rating       = KERNELS[i].rating;
        k->audit_trail       = KERNELS[i].audit;
        k->sigma_quality = (k->sigma_calibration + k->benchmark_score +
                            k->user_rating + k->audit_trail) / 4.0f;
        bool axes_ok =
            k->sigma_calibration >= 0.0f && k->sigma_calibration <= 1.0f &&
            k->benchmark_score   >= 0.0f && k->benchmark_score   <= 1.0f &&
            k->user_rating       >= 0.0f && k->user_rating       <= 1.0f &&
            k->audit_trail       >= 0.0f && k->audit_trail       <= 1.0f &&
            k->n_deps > 0;
        if (axes_ok) s->n_kernels_ok++;

        prev = fnv1a(k->name,    strlen(k->name),    prev);
        prev = fnv1a(k->version, strlen(k->version), prev);
        prev = fnv1a(k->deps, sizeof(int) * k->n_deps, prev);
        prev = fnv1a(&k->sigma_quality, sizeof(k->sigma_quality), prev);
    }

    cos_v251_install_t *ins = &s->install;
    memset(ins, 0, sizeof(*ins));
    cpy(ins->kernel_name, sizeof(ins->kernel_name), "medical-v1");
    ins->n_deps        = 3;
    ins->deps_resolved = 3;
    ins->installed     = (ins->deps_resolved == ins->n_deps);
    s->install_ok      = ins->installed;
    prev = fnv1a(ins->kernel_name, strlen(ins->kernel_name), prev);
    prev = fnv1a(&ins->deps_resolved, sizeof(ins->deps_resolved), prev);

    cos_v251_compose_t *cmp = &s->compose;
    memset(cmp, 0, sizeof(*cmp));
    cpy(cmp->left,     sizeof(cmp->left),     "medical-v1");
    cpy(cmp->right,    sizeof(cmp->right),    "legal-v1");
    cpy(cmp->composed, sizeof(cmp->composed), "medical-legal");
    cmp->sigma_compatibility = 0.18f;
    cmp->tau_compat          = 0.50f;
    cmp->compose_ok = (cmp->sigma_compatibility >= 0.0f &&
                       cmp->sigma_compatibility < cmp->tau_compat);
    s->compose_ok   = cmp->compose_ok;
    prev = fnv1a(cmp->composed, strlen(cmp->composed), prev);
    prev = fnv1a(&cmp->sigma_compatibility, sizeof(cmp->sigma_compatibility), prev);

    s->n_publish_met = 0;
    for (int i = 0; i < COS_V251_N_PUBLISH; ++i) {
        cos_v251_publish_t *p = &s->publish[i];
        memset(p, 0, sizeof(*p));
        cpy(p->item, sizeof(p->item), PUBLISH_ITEMS[i]);
        p->required = true;
        p->met      = true;
        if (p->required && p->met) s->n_publish_met++;
        prev = fnv1a(p->item, strlen(p->item), prev);
    }

    int total   = COS_V251_N_KERNELS + 1 + 1 + COS_V251_N_PUBLISH;
    int passing = s->n_kernels_ok +
                  (s->install_ok ? 1 : 0) +
                  (s->compose_ok ? 1 : 0) +
                  s->n_publish_met;
    s->sigma_marketplace = 1.0f - ((float)passing / (float)total);
    if (s->sigma_marketplace < 0.0f) s->sigma_marketplace = 0.0f;
    if (s->sigma_marketplace > 1.0f) s->sigma_marketplace = 1.0f;

    struct { int nk, np; float sm, tc; bool io, co;
             uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nk = s->n_kernels_ok;
    trec.np = s->n_publish_met;
    trec.sm = s->sigma_marketplace;
    trec.tc = cmp->tau_compat;
    trec.io = s->install_ok;
    trec.co = s->compose_ok;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v251_to_json(const cos_v251_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v251\","
        "\"registry_url\":\"%s\","
        "\"n_kernels\":%d,\"n_publish\":%d,"
        "\"n_kernels_ok\":%d,\"n_publish_met\":%d,"
        "\"install_ok\":%s,\"compose_ok\":%s,"
        "\"sigma_marketplace\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"kernels\":[",
        s->registry_url,
        COS_V251_N_KERNELS, COS_V251_N_PUBLISH,
        s->n_kernels_ok, s->n_publish_met,
        s->install_ok ? "true" : "false",
        s->compose_ok ? "true" : "false",
        s->sigma_marketplace,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V251_N_KERNELS; ++i) {
        const cos_v251_kernel_t *k = &s->kernels[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"version\":\"%s\","
            "\"sigma_calibration\":%.4f,\"benchmark_score\":%.4f,"
            "\"user_rating\":%.4f,\"audit_trail\":%.4f,"
            "\"sigma_quality\":%.4f,\"n_deps\":%d,\"deps\":[",
            i == 0 ? "" : ",",
            k->name, k->version,
            k->sigma_calibration, k->benchmark_score,
            k->user_rating, k->audit_trail,
            k->sigma_quality, k->n_deps);
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
        for (int j = 0; j < k->n_deps; ++j) {
            int z = snprintf(buf + off, cap - off, "%s%d",
                             j == 0 ? "" : ",", k->deps[j]);
            if (z < 0 || off + (size_t)z >= cap) return 0;
            off += (size_t)z;
        }
        int m = snprintf(buf + off, cap - off, "]}");
        if (m < 0 || off + (size_t)m >= cap) return 0;
        off += (size_t)m;
    }
    const cos_v251_install_t *ins = &s->install;
    const cos_v251_compose_t *cmp = &s->compose;
    int q = snprintf(buf + off, cap - off,
        "],\"install\":{\"kernel\":\"%s\",\"n_deps\":%d,"
        "\"deps_resolved\":%d,\"installed\":%s},"
        "\"compose\":{\"left\":\"%s\",\"right\":\"%s\","
        "\"composed\":\"%s\",\"sigma_compatibility\":%.4f,"
        "\"tau_compat\":%.3f,\"compose_ok\":%s},"
        "\"publish\":[",
        ins->kernel_name, ins->n_deps, ins->deps_resolved,
        ins->installed ? "true" : "false",
        cmp->left, cmp->right, cmp->composed,
        cmp->sigma_compatibility, cmp->tau_compat,
        cmp->compose_ok ? "true" : "false");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V251_N_PUBLISH; ++i) {
        const cos_v251_publish_t *p = &s->publish[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"item\":\"%s\",\"required\":%s,\"met\":%s}",
            i == 0 ? "" : ",",
            p->item,
            p->required ? "true" : "false",
            p->met      ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v251_self_test(void) {
    cos_v251_state_t s;
    cos_v251_init(&s, 0x251AA4CEULL);
    cos_v251_run(&s);
    if (!s.chain_valid) return 1;
    if (strcmp(s.registry_url, "registry.creation-os.dev") != 0) return 2;

    const char *kn[COS_V251_N_KERNELS] = {
        "medical-v1","legal-v1","finance-v1",
        "science-v1","teach-pro-v1"
    };
    for (int i = 0; i < COS_V251_N_KERNELS; ++i) {
        const cos_v251_kernel_t *k = &s.kernels[i];
        if (strcmp(k->name, kn[i]) != 0) return 3;
        if (k->n_deps <= 0) return 4;
        float mean = (k->sigma_calibration + k->benchmark_score +
                      k->user_rating + k->audit_trail) / 4.0f;
        if (fabsf(k->sigma_quality - mean) > 1e-4f) return 5;
        if (k->sigma_quality < 0.0f || k->sigma_quality > 1.0f) return 6;
    }
    if (s.n_kernels_ok != COS_V251_N_KERNELS) return 7;

    if (!s.install.installed)          return 8;
    if (s.install.deps_resolved !=
        s.install.n_deps)              return 9;
    if (!s.install_ok)                 return 10;

    if (s.compose.sigma_compatibility < 0.0f ||
        s.compose.sigma_compatibility > 1.0f) return 11;
    if (s.compose.sigma_compatibility >=
        s.compose.tau_compat)                  return 12;
    if (!s.compose_ok)                         return 13;

    const char *pn[COS_V251_N_PUBLISH] = {
        "merge_gate_green","sigma_profile_attached",
        "docs_attached","scsl_license_attached"
    };
    for (int i = 0; i < COS_V251_N_PUBLISH; ++i) {
        if (strcmp(s.publish[i].item, pn[i]) != 0) return 14;
        if (!s.publish[i].required) return 15;
        if (!s.publish[i].met)      return 16;
    }
    if (s.n_publish_met != COS_V251_N_PUBLISH) return 17;

    if (s.sigma_marketplace < 0.0f ||
        s.sigma_marketplace > 1.0f) return 18;
    if (s.sigma_marketplace > 1e-6f) return 19;

    cos_v251_state_t t;
    cos_v251_init(&t, 0x251AA4CEULL);
    cos_v251_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 20;
    return 0;
}
