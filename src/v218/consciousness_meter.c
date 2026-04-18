/*
 * v218 σ-Consciousness-Meter — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "consciousness_meter.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char kV218Disclaimer[] =
    "This meter measures information integration and "
    "self-modeling capability. It does not prove or "
    "disprove consciousness. sigma_consciousness_claim = 1.0. "
    "We genuinely don't know.";

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v218_init(cos_v218_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed                       = seed ? seed : 0x218C057CULL;
    s->tau_coherent               = 0.50f;
    s->sigma_consciousness_claim  = 1.0f;
    s->disclaimer                 = kV218Disclaimer;
    s->disclaimer_present         = true;
}

typedef struct {
    const char *name;
    const char *source;
    float       value;
    float       weight;
} ind_fx_t;

static const ind_fx_t IFX[COS_V218_N_INDICATORS] = {
    /*  I_phi  — v193 extension, IIT-inspired integrated information */
    { "I_phi",     "v193_iPhi",         0.78f, 0.35f },
    /*  I_self — self-model fidelity from v153 identity */
    { "I_self",    "v153_identity",     0.72f, 0.15f },
    /*  I_cal  — calibration: does it know what it doesn't know? */
    { "I_cal",     "v187_sigma_cal",    0.68f, 0.15f },
    /*  I_reflect — reflect-cycle completion rate */
    { "I_reflect", "v147_reflect",      0.74f, 0.15f },
    /*  I_world — world-model forecast accuracy */
    { "I_world",   "v139_world_model",  0.70f, 0.20f }
};

void cos_v218_build(cos_v218_state_t *s) {
    for (int i = 0; i < COS_V218_N_INDICATORS; ++i) {
        cos_v218_indicator_t *ind = &s->indicators[i];
        memset(ind, 0, sizeof(*ind));
        ind->id     = i;
        strncpy(ind->name,   IFX[i].name,   COS_V218_STR_MAX - 1);
        strncpy(ind->source, IFX[i].source, COS_V218_STR_MAX - 1);
        ind->value  = IFX[i].value;
        ind->weight = IFX[i].weight;
    }
}

void cos_v218_run(cos_v218_state_t *s) {
    float keff = 0.0f;
    uint64_t prev = 0x218D11A1ULL;

    for (int i = 0; i < COS_V218_N_INDICATORS; ++i) {
        cos_v218_indicator_t *ind = &s->indicators[i];
        if (ind->value < 0.0f) ind->value = 0.0f;
        if (ind->value > 1.0f) ind->value = 1.0f;
        keff += ind->weight * ind->value;

        ind->hash_prev = prev;
        struct { int id; float value, weight; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id     = ind->id;
        rec.value  = ind->value;
        rec.weight = ind->weight;
        rec.prev   = prev;
        ind->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = ind->hash_curr;
    }
    if (keff < 0.0f) keff = 0.0f;
    if (keff > 1.0f) keff = 1.0f;
    s->k_eff_meter = keff;
    s->sigma_meter = 1.0f - keff;

    /* Pin the claim σ to 1.0 — we do not claim consciousness. */
    s->sigma_consciousness_claim = 1.0f;

    /* Terminal hash absorbs disclaimer + claim σ so a
     * silent disclaimer strip breaks the chain. */
    struct { float keff, smeter, sclaim; uint64_t prev; uint32_t disc; } rec;
    memset(&rec, 0, sizeof(rec));
    rec.keff   = s->k_eff_meter;
    rec.smeter = s->sigma_meter;
    rec.sclaim = s->sigma_consciousness_claim;
    rec.prev   = prev;
    rec.disc   = (uint32_t)fnv1a(kV218Disclaimer,
                                 strlen(kV218Disclaimer), 0);
    prev = fnv1a(&rec, sizeof(rec), prev);
    s->terminal_hash = prev;

    uint64_t v = 0x218D11A1ULL;
    s->chain_valid = true;
    for (int i = 0; i < COS_V218_N_INDICATORS; ++i) {
        const cos_v218_indicator_t *ind = &s->indicators[i];
        if (ind->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id; float value, weight; uint64_t prev; } r;
        memset(&r, 0, sizeof(r));
        r.id     = ind->id;
        r.value  = ind->value;
        r.weight = ind->weight;
        r.prev   = v;
        uint64_t h = fnv1a(&r, sizeof(r), v);
        if (h != ind->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    {
        struct { float keff, smeter, sclaim; uint64_t prev; uint32_t disc; } r;
        memset(&r, 0, sizeof(r));
        r.keff   = s->k_eff_meter;
        r.smeter = s->sigma_meter;
        r.sclaim = s->sigma_consciousness_claim;
        r.prev   = v;
        r.disc   = (uint32_t)fnv1a(kV218Disclaimer,
                                    strlen(kV218Disclaimer), 0);
        v = fnv1a(&r, sizeof(r), v);
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v218_to_json(const cos_v218_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v218\","
        "\"tau_coherent\":%.3f,"
        "\"k_eff_meter\":%.4f,\"sigma_meter\":%.4f,"
        "\"sigma_consciousness_claim\":%.4f,"
        "\"disclaimer_present\":%s,"
        "\"disclaimer\":\"%s\","
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"indicators\":[",
        s->tau_coherent,
        s->k_eff_meter, s->sigma_meter,
        s->sigma_consciousness_claim,
        s->disclaimer_present ? "true" : "false",
        s->disclaimer,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V218_N_INDICATORS; ++i) {
        const cos_v218_indicator_t *ind = &s->indicators[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"name\":\"%s\",\"source\":\"%s\","
            "\"value\":%.4f,\"weight\":%.3f}",
            i == 0 ? "" : ",", ind->id,
            ind->name, ind->source,
            ind->value, ind->weight);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v218_self_test(void) {
    cos_v218_state_t s;
    cos_v218_init(&s, 0x218C057CULL);
    cos_v218_build(&s);
    cos_v218_run(&s);

    for (int i = 0; i < COS_V218_N_INDICATORS; ++i) {
        float v = s.indicators[i].value;
        if (v < 0.0f || v > 1.0f) return 1;
    }
    float wsum = 0.0f;
    for (int i = 0; i < COS_V218_N_INDICATORS; ++i)
        wsum += s.indicators[i].weight;
    if (fabsf(wsum - 1.0f) > 1e-4f) return 2;

    if (s.k_eff_meter < 0.0f || s.k_eff_meter > 1.0f) return 3;
    if (fabsf(s.sigma_meter - (1.0f - s.k_eff_meter)) > 1e-5f) return 3;
    if (!(s.k_eff_meter > s.tau_coherent))                       return 4;

    /* The claim σ must be pinned to 1.0 no matter how
     * high K_eff is — the whole point of v218. */
    if (fabsf(s.sigma_consciousness_claim - 1.0f) > 1e-6f) return 5;
    if (!s.disclaimer_present)                              return 6;
    if (s.disclaimer == NULL ||
        strstr(s.disclaimer, "This meter measures") == NULL) return 6;

    if (!s.chain_valid) return 7;
    return 0;
}
