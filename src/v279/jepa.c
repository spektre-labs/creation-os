/*
 * v279 σ-JEPA — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "jepa.h"

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

static const struct { int step; float s; }
    PREDICTS[COS_V279_N_PREDICT] = {
    { 0, 0.07f },   /* ACT     */
    { 1, 0.24f },   /* ACT     */
    { 2, 0.46f },   /* OBSERVE */
    { 3, 0.82f },   /* OBSERVE */
};

/* entropy_z and sigma_latent both strictly decrease and
 * stay within ±0.05 of each other per checkpoint. */
static const struct { const char *lbl; float ez; float sl; }
    LATENTS[COS_V279_N_LATENT] = {
    { "early", 0.72f, 0.70f },
    { "mid",   0.41f, 0.44f },
    { "late",  0.08f, 0.09f },
};

static const struct { const char *n; float w; }
    LOSSES[COS_V279_N_LOSS] = {
    { "prediction",  0.70f },
    { "regularizer", 0.30f },
};

static const struct { const char *src; const char *ev; }
    VALIDATION[COS_V279_N_VALIDATION] = {
    { "lecun_jepa_2022",
      "Meta AI JEPA, representation-space prediction" },
    { "leworldmodel_2026_03",
      "LeWorldModel 03/2026, stable end-to-end JEPA"  },
};

void cos_v279_init(cos_v279_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed         = seed ? seed : 0x279555ULL;
    s->tau_predict  = 0.30f;
    s->converge_eps = 0.05f;
}

void cos_v279_run(cos_v279_state_t *s) {
    uint64_t prev = 0x27955500ULL;

    s->n_predict_rows_ok = 0;
    s->n_predict_act = s->n_predict_observe = 0;
    for (int i = 0; i < COS_V279_N_PREDICT; ++i) {
        cos_v279_predict_t *p = &s->predict[i];
        memset(p, 0, sizeof(*p));
        p->step             = PREDICTS[i].step;
        p->sigma_prediction = PREDICTS[i].s;
        p->decision         = (p->sigma_prediction <= s->tau_predict)
                                  ? COS_V279_DEC_ACT
                                  : COS_V279_DEC_OBSERVE;
        if (p->sigma_prediction >= 0.0f && p->sigma_prediction <= 1.0f)
            s->n_predict_rows_ok++;
        if (p->decision == COS_V279_DEC_ACT) s->n_predict_act++;
        else                                 s->n_predict_observe++;
        prev = fnv1a(&p->step,             sizeof(p->step),             prev);
        prev = fnv1a(&p->sigma_prediction, sizeof(p->sigma_prediction), prev);
        prev = fnv1a(&p->decision,         sizeof(p->decision),         prev);
    }

    s->n_latent_rows_ok = 0;
    for (int i = 0; i < COS_V279_N_LATENT; ++i) {
        cos_v279_latent_t *l = &s->latent[i];
        memset(l, 0, sizeof(*l));
        cpy(l->label, sizeof(l->label), LATENTS[i].lbl);
        l->entropy_z    = LATENTS[i].ez;
        l->sigma_latent = LATENTS[i].sl;
        if (l->entropy_z    >= 0.0f && l->entropy_z    <= 1.0f &&
            l->sigma_latent >= 0.0f && l->sigma_latent <= 1.0f)
            s->n_latent_rows_ok++;
        prev = fnv1a(l->label, strlen(l->label), prev);
        prev = fnv1a(&l->entropy_z,    sizeof(l->entropy_z),    prev);
        prev = fnv1a(&l->sigma_latent, sizeof(l->sigma_latent), prev);
    }
    bool ez_mono = true, sl_mono = true, conv_ok = true;
    for (int i = 1; i < COS_V279_N_LATENT; ++i) {
        if (!(s->latent[i].entropy_z    < s->latent[i-1].entropy_z))    ez_mono = false;
        if (!(s->latent[i].sigma_latent < s->latent[i-1].sigma_latent)) sl_mono = false;
    }
    for (int i = 0; i < COS_V279_N_LATENT; ++i) {
        float d = s->latent[i].entropy_z - s->latent[i].sigma_latent;
        if (d < 0.0f) d = -d;
        if (d > s->converge_eps) conv_ok = false;
    }
    s->latent_monotone_entropy_ok = ez_mono;
    s->latent_monotone_sigma_ok   = sl_mono;
    s->latent_converge_ok         = conv_ok;

    s->n_loss_rows_ok = 0;
    float wsum = 0.0f;
    for (int i = 0; i < COS_V279_N_LOSS; ++i) {
        cos_v279_loss_t *l = &s->loss[i];
        memset(l, 0, sizeof(*l));
        cpy(l->name, sizeof(l->name), LOSSES[i].n);
        l->weight  = LOSSES[i].w;
        l->enabled = (strlen(l->name) > 0) && (l->weight > 0.0f) && (l->weight < 1.0f);
        if (l->enabled) s->n_loss_rows_ok++;
        wsum += l->weight;
        prev = fnv1a(l->name,      strlen(l->name),      prev);
        prev = fnv1a(&l->enabled,  sizeof(l->enabled),   prev);
        prev = fnv1a(&l->weight,   sizeof(l->weight),    prev);
    }
    s->loss_distinct_ok =
        (s->n_loss_rows_ok == COS_V279_N_LOSS) &&
        (strcmp(s->loss[0].name, s->loss[1].name) != 0);
    float dw = wsum - 1.0f; if (dw < 0.0f) dw = -dw;
    s->loss_weights_ok = (dw <= 1e-3f);

    s->n_validation_rows_ok = 0;
    for (int i = 0; i < COS_V279_N_VALIDATION; ++i) {
        cos_v279_citation_t *c = &s->validation[i];
        memset(c, 0, sizeof(*c));
        cpy(c->source,   sizeof(c->source),   VALIDATION[i].src);
        cpy(c->evidence, sizeof(c->evidence), VALIDATION[i].ev);
        c->present = (strlen(c->source) > 0) && (strlen(c->evidence) > 0);
        if (c->present) s->n_validation_rows_ok++;
        prev = fnv1a(c->source,   strlen(c->source),   prev);
        prev = fnv1a(c->evidence, strlen(c->evidence), prev);
        prev = fnv1a(&c->present, sizeof(c->present),  prev);
    }
    s->validation_distinct_ok =
        (s->n_validation_rows_ok == COS_V279_N_VALIDATION) &&
        (strcmp(s->validation[0].source, s->validation[1].source) != 0);

    bool predict_both =
        (s->n_predict_act >= 1) && (s->n_predict_observe >= 1);

    int total   = COS_V279_N_PREDICT + 1 +
                  COS_V279_N_LATENT + 1 + 1 + 1 +
                  COS_V279_N_LOSS + 1 + 1 +
                  COS_V279_N_VALIDATION + 1;
    int passing = s->n_predict_rows_ok +
                  (predict_both ? 1 : 0) +
                  s->n_latent_rows_ok +
                  (s->latent_monotone_entropy_ok ? 1 : 0) +
                  (s->latent_monotone_sigma_ok   ? 1 : 0) +
                  (s->latent_converge_ok         ? 1 : 0) +
                  s->n_loss_rows_ok +
                  (s->loss_distinct_ok ? 1 : 0) +
                  (s->loss_weights_ok  ? 1 : 0) +
                  s->n_validation_rows_ok +
                  (s->validation_distinct_ok ? 1 : 0);
    s->sigma_jepa = 1.0f - ((float)passing / (float)total);
    if (s->sigma_jepa < 0.0f) s->sigma_jepa = 0.0f;
    if (s->sigma_jepa > 1.0f) s->sigma_jepa = 1.0f;

    struct { int np, na, no, nl, nl2, nv;
             bool ez, sl, cv, ld, lw, vd;
             float sigma, tp, eps; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.np  = s->n_predict_rows_ok;
    trec.na  = s->n_predict_act;
    trec.no  = s->n_predict_observe;
    trec.nl  = s->n_latent_rows_ok;
    trec.nl2 = s->n_loss_rows_ok;
    trec.nv  = s->n_validation_rows_ok;
    trec.ez  = s->latent_monotone_entropy_ok;
    trec.sl  = s->latent_monotone_sigma_ok;
    trec.cv  = s->latent_converge_ok;
    trec.ld  = s->loss_distinct_ok;
    trec.lw  = s->loss_weights_ok;
    trec.vd  = s->validation_distinct_ok;
    trec.sigma = s->sigma_jepa;
    trec.tp  = s->tau_predict;
    trec.eps = s->converge_eps;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *dec_name(cos_v279_dec_t d) {
    switch (d) {
    case COS_V279_DEC_ACT:     return "ACT";
    case COS_V279_DEC_OBSERVE: return "OBSERVE";
    }
    return "UNKNOWN";
}

size_t cos_v279_to_json(const cos_v279_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v279\","
        "\"tau_predict\":%.3f,\"converge_eps\":%.3f,"
        "\"n_predict_rows_ok\":%d,"
        "\"n_predict_act\":%d,\"n_predict_observe\":%d,"
        "\"n_latent_rows_ok\":%d,"
        "\"latent_monotone_entropy_ok\":%s,"
        "\"latent_monotone_sigma_ok\":%s,"
        "\"latent_converge_ok\":%s,"
        "\"n_loss_rows_ok\":%d,"
        "\"loss_distinct_ok\":%s,\"loss_weights_ok\":%s,"
        "\"n_validation_rows_ok\":%d,"
        "\"validation_distinct_ok\":%s,"
        "\"sigma_jepa\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"predict\":[",
        s->tau_predict, s->converge_eps,
        s->n_predict_rows_ok,
        s->n_predict_act, s->n_predict_observe,
        s->n_latent_rows_ok,
        s->latent_monotone_entropy_ok ? "true" : "false",
        s->latent_monotone_sigma_ok   ? "true" : "false",
        s->latent_converge_ok         ? "true" : "false",
        s->n_loss_rows_ok,
        s->loss_distinct_ok ? "true" : "false",
        s->loss_weights_ok  ? "true" : "false",
        s->n_validation_rows_ok,
        s->validation_distinct_ok ? "true" : "false",
        s->sigma_jepa,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V279_N_PREDICT; ++i) {
        const cos_v279_predict_t *p = &s->predict[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"step\":%d,\"sigma_prediction\":%.4f,\"decision\":\"%s\"}",
            i == 0 ? "" : ",", p->step, p->sigma_prediction, dec_name(p->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"latent\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V279_N_LATENT; ++i) {
        const cos_v279_latent_t *l = &s->latent[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"label\":\"%s\",\"entropy_z\":%.4f,\"sigma_latent\":%.4f}",
            i == 0 ? "" : ",", l->label, l->entropy_z, l->sigma_latent);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"loss\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V279_N_LOSS; ++i) {
        const cos_v279_loss_t *l = &s->loss[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"enabled\":%s,\"weight\":%.4f}",
            i == 0 ? "" : ",", l->name,
            l->enabled ? "true" : "false", l->weight);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"validation\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V279_N_VALIDATION; ++i) {
        const cos_v279_citation_t *c = &s->validation[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"source\":\"%s\",\"evidence\":\"%s\",\"present\":%s}",
            i == 0 ? "" : ",", c->source, c->evidence,
            c->present ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v279_self_test(void) {
    cos_v279_state_t s;
    cos_v279_init(&s, 0x279555ULL);
    cos_v279_run(&s);
    if (!s.chain_valid) return 1;

    for (int i = 0; i < COS_V279_N_PREDICT; ++i) {
        cos_v279_dec_t exp =
            (s.predict[i].sigma_prediction <= s.tau_predict)
                ? COS_V279_DEC_ACT : COS_V279_DEC_OBSERVE;
        if (s.predict[i].decision != exp) return 2;
    }
    if (s.n_predict_rows_ok != COS_V279_N_PREDICT) return 3;
    if (s.n_predict_act     < 1) return 4;
    if (s.n_predict_observe < 1) return 5;

    if (s.n_latent_rows_ok != COS_V279_N_LATENT) return 6;
    if (!s.latent_monotone_entropy_ok) return 7;
    if (!s.latent_monotone_sigma_ok)   return 8;
    if (!s.latent_converge_ok)         return 9;
    if (strcmp(s.latent[0].label, "early") != 0) return 10;
    if (strcmp(s.latent[1].label, "mid")   != 0) return 11;
    if (strcmp(s.latent[2].label, "late")  != 0) return 12;

    if (s.n_loss_rows_ok != COS_V279_N_LOSS) return 13;
    if (!s.loss_distinct_ok) return 14;
    if (!s.loss_weights_ok)  return 15;
    if (strcmp(s.loss[0].name, "prediction")  != 0) return 16;
    if (strcmp(s.loss[1].name, "regularizer") != 0) return 17;

    if (s.n_validation_rows_ok != COS_V279_N_VALIDATION) return 18;
    if (!s.validation_distinct_ok) return 19;
    if (strcmp(s.validation[0].source, "lecun_jepa_2022")      != 0) return 20;
    if (strcmp(s.validation[1].source, "leworldmodel_2026_03") != 0) return 21;

    if (s.sigma_jepa < 0.0f || s.sigma_jepa > 1.0f) return 22;
    if (s.sigma_jepa > 1e-6f) return 23;

    cos_v279_state_t u;
    cos_v279_init(&u, 0x279555ULL);
    cos_v279_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 24;
    return 0;
}
