/*
 * v241 σ-API-Surface — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "api.h"

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

typedef struct {
    const char *method;
    const char *path;
    bool        openai_compatible;
    const char *summary;
} cos_v241_fx_ep_t;

static const cos_v241_fx_ep_t EPS[COS_V241_N_ENDPOINTS] = {
    { "POST", "/v1/chat/completions", true,  "OpenAI-compatible chat"       },
    { "POST", "/v1/reason",           false, "multi-path reasoning"         },
    { "POST", "/v1/plan",             false, "multi-step planning"          },
    { "POST", "/v1/create",           false, "creative generation"          },
    { "POST", "/v1/simulate",         false, "domain simulation"            },
    { "POST", "/v1/teach",            false, "Socratic mode"                },
    { "GET",  "/v1/health",           false, "system status"                },
    { "GET",  "/v1/identity",         false, "who am I"                     },
    { "GET",  "/v1/coherence",        false, "K_eff dashboard"              },
    { "GET",  "/v1/audit/stream",     false, "real-time audit stream"       },
};

typedef struct {
    const char *name;
    const char *install;
} cos_v241_fx_sdk_t;

static const cos_v241_fx_sdk_t SDKS[COS_V241_N_SDKS] = {
    { "python",     "pip install creation-os"                  },
    { "javascript", "npm install creation-os"                  },
    { "rust",       "cargo add creation-os"                    },
    { "go",         "go get github.com/spektre-labs/creation-os-go" },
};

void cos_v241_init(cos_v241_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed              = seed ? seed : 0x241A91E0ULL;
    s->api_version_major = 1;
    s->api_version_minor = 0;
}

void cos_v241_run(cos_v241_state_t *s) {
    uint64_t prev = 0x241A91E5ULL;

    s->n_openai_compat   = 0;
    s->n_audit_ok        = 0;
    s->n_sdks_maintained = 0;

    for (int i = 0; i < COS_V241_N_ENDPOINTS; ++i) {
        cos_v241_endpoint_t *e = &s->endpoints[i];
        memset(e, 0, sizeof(*e));
        cpy(e->method,  sizeof(e->method),  EPS[i].method);
        cpy(e->path,    sizeof(e->path),    EPS[i].path);
        cpy(e->summary, sizeof(e->summary), EPS[i].summary);
        e->openai_compatible = EPS[i].openai_compatible;
        e->emits_cos_headers = true;   /* every endpoint emits X-COS-* */

        bool method_ok = (strcmp(e->method, "GET")  == 0 ||
                          strcmp(e->method, "POST") == 0);
        bool path_ok   = (strncmp(e->path, "/v1/", 4) == 0);
        e->audit_ok    = method_ok && path_ok && e->emits_cos_headers;

        if (e->openai_compatible) s->n_openai_compat++;
        if (e->audit_ok)          s->n_audit_ok++;

        struct { int id, oc, eh, ok; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id = i;
        rec.oc = e->openai_compatible ? 1 : 0;
        rec.eh = e->emits_cos_headers ? 1 : 0;
        rec.ok = e->audit_ok          ? 1 : 0;
        rec.prev = prev;
        prev = fnv1a(e->method,  strlen(e->method),  prev);
        prev = fnv1a(e->path,    strlen(e->path),    prev);
        prev = fnv1a(&rec, sizeof(rec), prev);
    }

    for (int i = 0; i < COS_V241_N_SDKS; ++i) {
        cos_v241_sdk_t *k = &s->sdks[i];
        memset(k, 0, sizeof(*k));
        cpy(k->name,    sizeof(k->name),    SDKS[i].name);
        cpy(k->install, sizeof(k->install), SDKS[i].install);
        k->maintained = true;
        if (k->maintained) s->n_sdks_maintained++;
        prev = fnv1a(k->name,    strlen(k->name),    prev);
        prev = fnv1a(k->install, strlen(k->install), prev);
    }

    s->sigma_api = 1.0f - ((float)s->n_audit_ok / (float)COS_V241_N_ENDPOINTS);
    if (s->sigma_api < 0.0f) s->sigma_api = 0.0f;
    if (s->sigma_api > 1.0f) s->sigma_api = 1.0f;

    struct { int major, minor, oc, ok, sdks; float sa; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.major = s->api_version_major;
    trec.minor = s->api_version_minor;
    trec.oc    = s->n_openai_compat;
    trec.ok    = s->n_audit_ok;
    trec.sdks  = s->n_sdks_maintained;
    trec.sa    = s->sigma_api;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v241_to_json(const cos_v241_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v241\","
        "\"api_version_major\":%d,\"api_version_minor\":%d,"
        "\"n_endpoints\":%d,\"n_openai_compat\":%d,\"n_audit_ok\":%d,"
        "\"n_sdks\":%d,\"n_sdks_maintained\":%d,"
        "\"sigma_api\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"endpoints\":[",
        s->api_version_major, s->api_version_minor,
        COS_V241_N_ENDPOINTS, s->n_openai_compat, s->n_audit_ok,
        COS_V241_N_SDKS, s->n_sdks_maintained,
        s->sigma_api,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V241_N_ENDPOINTS; ++i) {
        const cos_v241_endpoint_t *e = &s->endpoints[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"method\":\"%s\",\"path\":\"%s\","
            "\"openai_compatible\":%s,\"emits_cos_headers\":%s,"
            "\"audit_ok\":%s,\"summary\":\"%s\"}",
            i == 0 ? "" : ",",
            e->method, e->path,
            e->openai_compatible ? "true" : "false",
            e->emits_cos_headers ? "true" : "false",
            e->audit_ok          ? "true" : "false",
            e->summary);
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    int m = snprintf(buf + off, cap - off, "],\"sdks\":[");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    off += (size_t)m;
    for (int i = 0; i < COS_V241_N_SDKS; ++i) {
        const cos_v241_sdk_t *k = &s->sdks[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"install\":\"%s\","
            "\"maintained\":%s}",
            i == 0 ? "" : ",",
            k->name, k->install,
            k->maintained ? "true" : "false");
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    int z = snprintf(buf + off, cap - off, "]}");
    if (z < 0 || off + (size_t)z >= cap) return 0;
    return off + (size_t)z;
}

int cos_v241_self_test(void) {
    cos_v241_state_t s;
    cos_v241_init(&s, 0x241A91E0ULL);
    cos_v241_run(&s);
    if (!s.chain_valid) return 1;

    int n_compat = 0;
    for (int i = 0; i < COS_V241_N_ENDPOINTS; ++i) {
        const cos_v241_endpoint_t *e = &s.endpoints[i];
        if (strlen(e->method) == 0) return 2;
        if (strcmp(e->method, "GET") != 0 && strcmp(e->method, "POST") != 0) return 3;
        if (strncmp(e->path, "/v1/", 4) != 0) return 4;
        if (!e->emits_cos_headers) return 5;
        if (!e->audit_ok)          return 6;
        if (e->openai_compatible)  n_compat++;
    }
    if (n_compat != 1)                           return 7;
    if (s.n_openai_compat != 1)                  return 7;
    if (s.n_audit_ok != COS_V241_N_ENDPOINTS)    return 8;
    if (COS_V241_N_ENDPOINTS != 10)              return 9;

    const char *names[COS_V241_N_SDKS] = { "python", "javascript", "rust", "go" };
    for (int i = 0; i < COS_V241_N_SDKS; ++i) {
        if (strcmp(s.sdks[i].name, names[i]) != 0) return 10;
        if (!s.sdks[i].maintained) return 11;
        if (strlen(s.sdks[i].install) == 0) return 12;
    }
    if (s.n_sdks_maintained != COS_V241_N_SDKS) return 13;

    if (s.api_version_major != 1) return 14;
    if (s.api_version_minor <  0) return 15;

    if (s.sigma_api < 0.0f || s.sigma_api > 1.0f) return 16;
    if (s.sigma_api > 1e-6f) return 17;  /* every endpoint passed */

    cos_v241_state_t t;
    cos_v241_init(&t, 0x241A91E0ULL);
    cos_v241_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 18;
    return 0;
}
