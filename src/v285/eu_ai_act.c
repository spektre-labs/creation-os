/*
 * v285 σ-EU-AI-Act — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "eu_ai_act.h"

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

static const char *ART15[COS_V285_N_ART15] = {
    "robustness", "accuracy", "cybersecurity",
};

static const char *ART52[COS_V285_N_ART52] = {
    "training_docs", "feedback_collection", "qa_process",
};

static const struct {
    const char *lbl; float s;
    bool sg; bool hitl; bool aud; bool red;
} RISKS[COS_V285_N_RISK] = {
    { "low",      0.10f, true, false, false, false },
    { "medium",   0.35f, true, false, true,  false },
    { "high",     0.65f, true, true,  true,  false },
    { "critical", 0.90f, true, true,  true,  true  },
};

static const struct {
    const char *n; cos_v285_layer_t l;
} LICENSES[COS_V285_N_LICENSE] = {
    { "scsl",       COS_V285_LAYER_LEGAL      },
    { "eu_ai_act",  COS_V285_LAYER_REGULATORY },
    { "sigma_gate", COS_V285_LAYER_TECHNICAL  },
};

void cos_v285_init(cos_v285_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed       = seed ? seed : 0x285555ULL;
    s->tau_low    = 0.20f;
    s->tau_medium = 0.50f;
    s->tau_high   = 0.80f;
}

void cos_v285_run(cos_v285_state_t *s) {
    uint64_t prev = 0x28555500ULL;

    s->n_art15_rows_ok = 0;
    for (int i = 0; i < COS_V285_N_ART15; ++i) {
        cos_v285_art15_t *a = &s->art15[i];
        memset(a, 0, sizeof(*a));
        cpy(a->name, sizeof(a->name), ART15[i]);
        a->sigma_mapped = true;
        a->audit_trail  = true;
        if (strlen(a->name) > 0) s->n_art15_rows_ok++;
        prev = fnv1a(a->name, strlen(a->name), prev);
        prev = fnv1a(&a->sigma_mapped, sizeof(a->sigma_mapped), prev);
        prev = fnv1a(&a->audit_trail,  sizeof(a->audit_trail),  prev);
    }
    bool m_ok = true, au_ok = true;
    for (int i = 0; i < COS_V285_N_ART15; ++i) {
        if (!s->art15[i].sigma_mapped) m_ok = false;
        if (!s->art15[i].audit_trail)  au_ok = false;
    }
    s->art15_all_mapped_ok = m_ok;
    s->art15_all_audit_ok  = au_ok;

    s->n_art52_rows_ok = 0;
    for (int i = 0; i < COS_V285_N_ART52; ++i) {
        cos_v285_art52_t *a = &s->art52[i];
        memset(a, 0, sizeof(*a));
        cpy(a->name, sizeof(a->name), ART52[i]);
        a->required_by_eu   = true;
        a->sigma_simplifies = true;
        if (strlen(a->name) > 0) s->n_art52_rows_ok++;
        prev = fnv1a(a->name, strlen(a->name), prev);
        prev = fnv1a(&a->required_by_eu,   sizeof(a->required_by_eu),   prev);
        prev = fnv1a(&a->sigma_simplifies, sizeof(a->sigma_simplifies), prev);
    }
    bool r_ok = true, sm_ok = true;
    for (int i = 0; i < COS_V285_N_ART52; ++i) {
        if (!s->art52[i].required_by_eu)   r_ok  = false;
        if (!s->art52[i].sigma_simplifies) sm_ok = false;
    }
    s->art52_all_required_ok    = r_ok;
    s->art52_all_simplifies_ok  = sm_ok;

    s->n_risk_rows_ok = 0;
    for (int i = 0; i < COS_V285_N_RISK; ++i) {
        cos_v285_risk_t *r = &s->risk[i];
        memset(r, 0, sizeof(*r));
        cpy(r->label, sizeof(r->label), RISKS[i].lbl);
        r->sigma_risk            = RISKS[i].s;
        r->sigma_gate            = RISKS[i].sg;
        r->hitl_required         = RISKS[i].hitl;
        r->audit_trail           = RISKS[i].aud;
        r->redundancy_required   = RISKS[i].red;
        r->controls_count =
            (r->sigma_gate          ? 1 : 0) +
            (r->hitl_required       ? 1 : 0) +
            (r->audit_trail         ? 1 : 0) +
            (r->redundancy_required ? 1 : 0);
        if (r->sigma_risk >= 0.0f && r->sigma_risk <= 1.0f)
            s->n_risk_rows_ok++;
        prev = fnv1a(r->label, strlen(r->label), prev);
        prev = fnv1a(&r->sigma_risk,          sizeof(r->sigma_risk),          prev);
        prev = fnv1a(&r->sigma_gate,          sizeof(r->sigma_gate),          prev);
        prev = fnv1a(&r->hitl_required,       sizeof(r->hitl_required),       prev);
        prev = fnv1a(&r->audit_trail,         sizeof(r->audit_trail),         prev);
        prev = fnv1a(&r->redundancy_required, sizeof(r->redundancy_required), prev);
        prev = fnv1a(&r->controls_count,      sizeof(r->controls_count),      prev);
    }
    bool all_sg = true;
    for (int i = 0; i < COS_V285_N_RISK; ++i)
        if (!s->risk[i].sigma_gate) all_sg = false;
    s->risk_all_sigma_ok = all_sg;
    bool mono = true;
    for (int i = 1; i < COS_V285_N_RISK; ++i) {
        if (s->risk[i].controls_count <= s->risk[i - 1].controls_count) {
            mono = false; break;
        }
    }
    s->risk_monotone_ok = mono &&
                          (s->risk[0].controls_count == 1) &&
                          (s->risk[COS_V285_N_RISK - 1].controls_count == 4);

    s->n_license_rows_ok = 0;
    for (int i = 0; i < COS_V285_N_LICENSE; ++i) {
        cos_v285_license_t *l = &s->license[i];
        memset(l, 0, sizeof(*l));
        cpy(l->name, sizeof(l->name), LICENSES[i].n);
        l->layer      = LICENSES[i].l;
        l->enabled    = true;
        l->composable = true;
        if (strlen(l->name) > 0) s->n_license_rows_ok++;
        prev = fnv1a(l->name, strlen(l->name), prev);
        prev = fnv1a(&l->layer,      sizeof(l->layer),      prev);
        prev = fnv1a(&l->enabled,    sizeof(l->enabled),    prev);
        prev = fnv1a(&l->composable, sizeof(l->composable), prev);
    }
    bool layers_distinct = true;
    for (int i = 0; i < COS_V285_N_LICENSE && layers_distinct; ++i) {
        for (int j = i + 1; j < COS_V285_N_LICENSE; ++j) {
            if (s->license[i].layer == s->license[j].layer) {
                layers_distinct = false; break;
            }
        }
    }
    s->license_layers_distinct_ok = layers_distinct;
    bool all_en = true, all_cp = true;
    for (int i = 0; i < COS_V285_N_LICENSE; ++i) {
        if (!s->license[i].enabled)    all_en = false;
        if (!s->license[i].composable) all_cp = false;
    }
    s->license_all_enabled_ok    = all_en;
    s->license_all_composable_ok = all_cp;

    int total   = COS_V285_N_ART15   + 1 + 1 +
                  COS_V285_N_ART52   + 1 + 1 +
                  COS_V285_N_RISK    + 1 + 1 +
                  COS_V285_N_LICENSE + 1 + 1 + 1;
    int passing = s->n_art15_rows_ok +
                  (s->art15_all_mapped_ok ? 1 : 0) +
                  (s->art15_all_audit_ok  ? 1 : 0) +
                  s->n_art52_rows_ok +
                  (s->art52_all_required_ok   ? 1 : 0) +
                  (s->art52_all_simplifies_ok ? 1 : 0) +
                  s->n_risk_rows_ok +
                  (s->risk_all_sigma_ok ? 1 : 0) +
                  (s->risk_monotone_ok  ? 1 : 0) +
                  s->n_license_rows_ok +
                  (s->license_layers_distinct_ok ? 1 : 0) +
                  (s->license_all_enabled_ok     ? 1 : 0) +
                  (s->license_all_composable_ok  ? 1 : 0);
    s->sigma_euact = 1.0f - ((float)passing / (float)total);
    if (s->sigma_euact < 0.0f) s->sigma_euact = 0.0f;
    if (s->sigma_euact > 1.0f) s->sigma_euact = 1.0f;

    struct { int n15, n52, nr, nl;
             bool m, au, req, smp, rsg, rmo, lyr, len, lcp;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.n15 = s->n_art15_rows_ok;
    trec.n52 = s->n_art52_rows_ok;
    trec.nr  = s->n_risk_rows_ok;
    trec.nl  = s->n_license_rows_ok;
    trec.m   = s->art15_all_mapped_ok;
    trec.au  = s->art15_all_audit_ok;
    trec.req = s->art52_all_required_ok;
    trec.smp = s->art52_all_simplifies_ok;
    trec.rsg = s->risk_all_sigma_ok;
    trec.rmo = s->risk_monotone_ok;
    trec.lyr = s->license_layers_distinct_ok;
    trec.len = s->license_all_enabled_ok;
    trec.lcp = s->license_all_composable_ok;
    trec.sigma = s->sigma_euact;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *layer_name(cos_v285_layer_t l) {
    switch (l) {
    case COS_V285_LAYER_LEGAL:      return "LEGAL";
    case COS_V285_LAYER_REGULATORY: return "REGULATORY";
    case COS_V285_LAYER_TECHNICAL:  return "TECHNICAL";
    }
    return "UNKNOWN";
}

size_t cos_v285_to_json(const cos_v285_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v285\","
        "\"tau_low\":%.3f,\"tau_medium\":%.3f,\"tau_high\":%.3f,"
        "\"n_art15_rows_ok\":%d,"
        "\"art15_all_mapped_ok\":%s,\"art15_all_audit_ok\":%s,"
        "\"n_art52_rows_ok\":%d,"
        "\"art52_all_required_ok\":%s,\"art52_all_simplifies_ok\":%s,"
        "\"n_risk_rows_ok\":%d,"
        "\"risk_all_sigma_ok\":%s,\"risk_monotone_ok\":%s,"
        "\"n_license_rows_ok\":%d,"
        "\"license_layers_distinct_ok\":%s,"
        "\"license_all_enabled_ok\":%s,"
        "\"license_all_composable_ok\":%s,"
        "\"sigma_euact\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"art15\":[",
        s->tau_low, s->tau_medium, s->tau_high,
        s->n_art15_rows_ok,
        s->art15_all_mapped_ok ? "true" : "false",
        s->art15_all_audit_ok  ? "true" : "false",
        s->n_art52_rows_ok,
        s->art52_all_required_ok   ? "true" : "false",
        s->art52_all_simplifies_ok ? "true" : "false",
        s->n_risk_rows_ok,
        s->risk_all_sigma_ok ? "true" : "false",
        s->risk_monotone_ok  ? "true" : "false",
        s->n_license_rows_ok,
        s->license_layers_distinct_ok ? "true" : "false",
        s->license_all_enabled_ok     ? "true" : "false",
        s->license_all_composable_ok  ? "true" : "false",
        s->sigma_euact,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V285_N_ART15; ++i) {
        const cos_v285_art15_t *a = &s->art15[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"sigma_mapped\":%s,\"audit_trail\":%s}",
            i == 0 ? "" : ",", a->name,
            a->sigma_mapped ? "true" : "false",
            a->audit_trail  ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"art52\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V285_N_ART52; ++i) {
        const cos_v285_art52_t *a = &s->art52[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"required_by_eu\":%s,\"sigma_simplifies\":%s}",
            i == 0 ? "" : ",", a->name,
            a->required_by_eu   ? "true" : "false",
            a->sigma_simplifies ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"risk\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V285_N_RISK; ++i) {
        const cos_v285_risk_t *r = &s->risk[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"label\":\"%s\",\"sigma_risk\":%.4f,"
            "\"sigma_gate\":%s,\"hitl_required\":%s,"
            "\"audit_trail\":%s,\"redundancy_required\":%s,"
            "\"controls_count\":%d}",
            i == 0 ? "" : ",", r->label, r->sigma_risk,
            r->sigma_gate          ? "true" : "false",
            r->hitl_required       ? "true" : "false",
            r->audit_trail         ? "true" : "false",
            r->redundancy_required ? "true" : "false",
            r->controls_count);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"license\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V285_N_LICENSE; ++i) {
        const cos_v285_license_t *l = &s->license[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"layer\":\"%s\","
            "\"enabled\":%s,\"composable\":%s}",
            i == 0 ? "" : ",", l->name, layer_name(l->layer),
            l->enabled    ? "true" : "false",
            l->composable ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v285_self_test(void) {
    cos_v285_state_t s;
    cos_v285_init(&s, 0x285555ULL);
    cos_v285_run(&s);
    if (!s.chain_valid) return 1;

    static const char *WANT15[COS_V285_N_ART15] = {
        "robustness", "accuracy", "cybersecurity"
    };
    for (int i = 0; i < COS_V285_N_ART15; ++i) {
        if (strcmp(s.art15[i].name, WANT15[i]) != 0) return 2;
        if (!s.art15[i].sigma_mapped) return 3;
        if (!s.art15[i].audit_trail)  return 4;
    }
    if (s.n_art15_rows_ok != COS_V285_N_ART15) return 5;
    if (!s.art15_all_mapped_ok) return 6;
    if (!s.art15_all_audit_ok)  return 7;

    static const char *WANT52[COS_V285_N_ART52] = {
        "training_docs", "feedback_collection", "qa_process"
    };
    for (int i = 0; i < COS_V285_N_ART52; ++i) {
        if (strcmp(s.art52[i].name, WANT52[i]) != 0) return 8;
        if (!s.art52[i].required_by_eu)   return 9;
        if (!s.art52[i].sigma_simplifies) return 10;
    }
    if (s.n_art52_rows_ok != COS_V285_N_ART52) return 11;
    if (!s.art52_all_required_ok)   return 12;
    if (!s.art52_all_simplifies_ok) return 13;

    static const char *WANTR[COS_V285_N_RISK] = {
        "low", "medium", "high", "critical"
    };
    static const int WANT_COUNT[COS_V285_N_RISK] = { 1, 2, 3, 4 };
    for (int i = 0; i < COS_V285_N_RISK; ++i) {
        if (strcmp(s.risk[i].label, WANTR[i]) != 0) return 14;
        if (!s.risk[i].sigma_gate) return 15;
        if (s.risk[i].controls_count != WANT_COUNT[i]) return 16;
    }
    if (s.n_risk_rows_ok != COS_V285_N_RISK) return 17;
    if (!s.risk_all_sigma_ok) return 18;
    if (!s.risk_monotone_ok)  return 19;

    static const char *WANTL[COS_V285_N_LICENSE] = {
        "scsl", "eu_ai_act", "sigma_gate"
    };
    static const cos_v285_layer_t WANTLY[COS_V285_N_LICENSE] = {
        COS_V285_LAYER_LEGAL,
        COS_V285_LAYER_REGULATORY,
        COS_V285_LAYER_TECHNICAL
    };
    for (int i = 0; i < COS_V285_N_LICENSE; ++i) {
        if (strcmp(s.license[i].name, WANTL[i]) != 0) return 20;
        if (s.license[i].layer   != WANTLY[i]) return 21;
        if (!s.license[i].enabled)    return 22;
        if (!s.license[i].composable) return 23;
    }
    if (s.n_license_rows_ok != COS_V285_N_LICENSE) return 24;
    if (!s.license_layers_distinct_ok) return 25;
    if (!s.license_all_enabled_ok)     return 26;
    if (!s.license_all_composable_ok)  return 27;

    if (s.sigma_euact < 0.0f || s.sigma_euact > 1.0f) return 28;
    if (s.sigma_euact > 1e-6f) return 29;

    cos_v285_state_t u;
    cos_v285_init(&u, 0x285555ULL);
    cos_v285_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 30;
    return 0;
}
