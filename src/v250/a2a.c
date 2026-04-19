/*
 * v250 σ-A2A — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "a2a.h"

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

static const char *CAPS[COS_V250_N_CAPABILITIES] = {
    "reason","plan","create","simulate","teach","coherence"
};

static const struct { const char *type; float sigma; }
    TASKS[COS_V250_N_TASKS] = {
    { "factual_reason",  0.18f },   /* ACCEPT    */
    { "plan",            0.34f },   /* ACCEPT    */
    { "creative",        0.62f },   /* NEGOTIATE */
    { "moral_dilemma",   0.81f },   /* REFUSE    */
};

static const struct { const char *name; float trust; const char *presence; }
    PARTNERS[COS_V250_N_PARTNERS] = {
    { "alice", 0.12f, "LIVE"     },
    { "bob",   0.27f, "LIVE"     },
    { "carol", 0.19f, "RESTORED" },
};

void cos_v250_init(cos_v250_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed       = seed ? seed : 0x250A2A7EULL;
    s->tau_neg    = 0.50f;
    s->tau_refuse = 0.75f;
}

void cos_v250_run(cos_v250_state_t *s) {
    uint64_t prev = 0x250A2A73ULL;

    cos_v250_card_t *c = &s->card;
    memset(c, 0, sizeof(*c));
    cpy(c->name,     sizeof(c->name),     "Creation OS");
    for (int i = 0; i < COS_V250_N_CAPABILITIES; ++i)
        cpy(c->capabilities[i], sizeof(c->capabilities[i]), CAPS[i]);
    c->n_capabilities     = COS_V250_N_CAPABILITIES;
    c->sigma_mean         = 0.23f;
    c->sigma_calibration  = 0.94f;
    cpy(c->presence, sizeof(c->presence), "LIVE");
    c->scsl               = true;
    cpy(c->endpoint, sizeof(c->endpoint),
        "https://creation-os.example/v1/a2a");
    c->card_ok =
        (strlen(c->name) > 0)                 &&
        (c->n_capabilities == COS_V250_N_CAPABILITIES) &&
        (c->sigma_mean >= 0.0f && c->sigma_mean <= 1.0f) &&
        (c->sigma_calibration >= 0.0f && c->sigma_calibration <= 1.0f) &&
        (strcmp(c->presence, "LIVE") == 0)    &&
        c->scsl                                &&
        (strlen(c->endpoint) > 0);

    prev = fnv1a(c->name, strlen(c->name), prev);
    for (int i = 0; i < c->n_capabilities; ++i)
        prev = fnv1a(c->capabilities[i], strlen(c->capabilities[i]), prev);
    prev = fnv1a(&c->sigma_mean,        sizeof(c->sigma_mean),        prev);
    prev = fnv1a(&c->sigma_calibration, sizeof(c->sigma_calibration), prev);
    prev = fnv1a(c->presence, strlen(c->presence), prev);
    prev = fnv1a(c->endpoint, strlen(c->endpoint), prev);

    /* Count present required fields: name + 6 caps + mean + cal + presence + scsl + endpoint = 12 */
    s->n_card_fields_ok = 0;
    if (strlen(c->name) > 0)                                    s->n_card_fields_ok++;
    if (c->n_capabilities == COS_V250_N_CAPABILITIES)           s->n_card_fields_ok++;
    if (c->sigma_mean >= 0.0f && c->sigma_mean <= 1.0f)         s->n_card_fields_ok++;
    if (c->sigma_calibration >= 0.0f && c->sigma_calibration <= 1.0f) s->n_card_fields_ok++;
    if (strcmp(c->presence, "LIVE") == 0)                       s->n_card_fields_ok++;
    if (c->scsl)                                                s->n_card_fields_ok++;
    /* n_card_fields_ok in [0, 6] */

    s->n_accept = s->n_negotiate = s->n_refuse = 0;
    for (int i = 0; i < COS_V250_N_TASKS; ++i) {
        cos_v250_task_t *t = &s->tasks[i];
        memset(t, 0, sizeof(*t));
        cpy(t->task_type, sizeof(t->task_type), TASKS[i].type);
        t->sigma_product = TASKS[i].sigma;
        if      (t->sigma_product > s->tau_refuse) { t->decision = COS_V250_DEC_REFUSE;    s->n_refuse++;    }
        else if (t->sigma_product > s->tau_neg)    { t->decision = COS_V250_DEC_NEGOTIATE; s->n_negotiate++; }
        else                                       { t->decision = COS_V250_DEC_ACCEPT;    s->n_accept++;    }
        prev = fnv1a(t->task_type, strlen(t->task_type), prev);
        prev = fnv1a(&t->sigma_product, sizeof(t->sigma_product), prev);
        prev = fnv1a(&t->decision,      sizeof(t->decision),      prev);
    }
    int delegation_ok = s->n_accept + s->n_negotiate + s->n_refuse;

    s->n_partners_ok = 0;
    for (int i = 0; i < COS_V250_N_PARTNERS; ++i) {
        cos_v250_partner_t *p = &s->partners[i];
        memset(p, 0, sizeof(*p));
        cpy(p->name,     sizeof(p->name),     PARTNERS[i].name);
        cpy(p->presence, sizeof(p->presence), PARTNERS[i].presence);
        p->sigma_trust = PARTNERS[i].trust;
        if (p->sigma_trust >= 0.0f && p->sigma_trust <= 1.0f)
            s->n_partners_ok++;
        prev = fnv1a(p->name,     strlen(p->name),     prev);
        prev = fnv1a(p->presence, strlen(p->presence), prev);
        prev = fnv1a(&p->sigma_trust, sizeof(p->sigma_trust), prev);
    }

    int total   = COS_V250_N_CAPABILITIES + COS_V250_N_TASKS +
                  COS_V250_N_PARTNERS;
    int passing = s->n_card_fields_ok + delegation_ok + s->n_partners_ok;
    s->sigma_a2a = 1.0f - ((float)passing / (float)total);
    if (s->sigma_a2a < 0.0f) s->sigma_a2a = 0.0f;
    if (s->sigma_a2a > 1.0f) s->sigma_a2a = 1.0f;

    struct { int co, na, nn, nf, np; float sa, tn, tr; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.co = s->n_card_fields_ok;
    trec.na = s->n_accept;
    trec.nn = s->n_negotiate;
    trec.nf = s->n_refuse;
    trec.np = s->n_partners_ok;
    trec.sa = s->sigma_a2a;
    trec.tn = s->tau_neg;
    trec.tr = s->tau_refuse;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *dec_name(cos_v250_decision_t d) {
    switch (d) {
    case COS_V250_DEC_ACCEPT:    return "ACCEPT";
    case COS_V250_DEC_NEGOTIATE: return "NEGOTIATE";
    case COS_V250_DEC_REFUSE:    return "REFUSE";
    }
    return "UNKNOWN";
}

size_t cos_v250_to_json(const cos_v250_state_t *s, char *buf, size_t cap) {
    const cos_v250_card_t *c = &s->card;
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v250\","
        "\"tau_neg\":%.3f,\"tau_refuse\":%.3f,"
        "\"n_card_fields_ok\":%d,\"n_accept\":%d,"
        "\"n_negotiate\":%d,\"n_refuse\":%d,"
        "\"n_partners_ok\":%d,"
        "\"sigma_a2a\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"agent_card\":{"
        "\"name\":\"%s\","
        "\"n_capabilities\":%d,"
        "\"sigma_profile\":{\"mean\":%.4f,\"calibration\":%.4f},"
        "\"presence\":\"%s\",\"scsl\":%s,\"endpoint\":\"%s\","
        "\"card_ok\":%s,\"capabilities\":[",
        s->tau_neg, s->tau_refuse,
        s->n_card_fields_ok, s->n_accept,
        s->n_negotiate, s->n_refuse, s->n_partners_ok,
        s->sigma_a2a,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash,
        c->name, c->n_capabilities,
        c->sigma_mean, c->sigma_calibration,
        c->presence, c->scsl ? "true" : "false", c->endpoint,
        c->card_ok ? "true" : "false");
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < c->n_capabilities; ++i) {
        int z = snprintf(buf + off, cap - off, "%s\"%s\"",
                         i == 0 ? "" : ",", c->capabilities[i]);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "]},\"tasks\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V250_N_TASKS; ++i) {
        const cos_v250_task_t *t = &s->tasks[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"task_type\":\"%s\",\"sigma_product\":%.4f,"
            "\"decision\":\"%s\"}",
            i == 0 ? "" : ",", t->task_type, t->sigma_product,
            dec_name(t->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"partners\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V250_N_PARTNERS; ++i) {
        const cos_v250_partner_t *p = &s->partners[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"sigma_trust\":%.4f,"
            "\"presence\":\"%s\"}",
            i == 0 ? "" : ",", p->name, p->sigma_trust, p->presence);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v250_self_test(void) {
    cos_v250_state_t s;
    cos_v250_init(&s, 0x250A2A7EULL);
    cos_v250_run(&s);
    if (!s.chain_valid) return 1;

    const cos_v250_card_t *c = &s.card;
    if (strcmp(c->name, "Creation OS") != 0) return 2;
    if (c->n_capabilities != COS_V250_N_CAPABILITIES) return 3;
    const char *wc[COS_V250_N_CAPABILITIES] = {
        "reason","plan","create","simulate","teach","coherence"
    };
    for (int i = 0; i < COS_V250_N_CAPABILITIES; ++i)
        if (strcmp(c->capabilities[i], wc[i]) != 0) return 4;
    if (c->sigma_mean < 0.0f || c->sigma_mean > 1.0f) return 5;
    if (c->sigma_calibration < 0.0f || c->sigma_calibration > 1.0f) return 6;
    if (strcmp(c->presence, "LIVE") != 0) return 7;
    if (!c->scsl) return 8;
    if (strlen(c->endpoint) == 0) return 9;
    if (!c->card_ok) return 10;
    if (s.n_card_fields_ok < COS_V250_N_CAPABILITIES) return 11;

    for (int i = 0; i < COS_V250_N_TASKS; ++i) {
        const cos_v250_task_t *t = &s.tasks[i];
        if (t->sigma_product < 0.0f || t->sigma_product > 1.0f) return 12;
        cos_v250_decision_t expect;
        if      (t->sigma_product > s.tau_refuse) expect = COS_V250_DEC_REFUSE;
        else if (t->sigma_product > s.tau_neg)    expect = COS_V250_DEC_NEGOTIATE;
        else                                      expect = COS_V250_DEC_ACCEPT;
        if (t->decision != expect) return 13;
    }
    if (s.n_accept    < 1) return 14;
    if (s.n_negotiate < 1) return 15;
    if (s.n_refuse    < 1) return 16;

    const char *pn[COS_V250_N_PARTNERS] = { "alice","bob","carol" };
    for (int i = 0; i < COS_V250_N_PARTNERS; ++i) {
        if (strcmp(s.partners[i].name, pn[i]) != 0) return 17;
        if (s.partners[i].sigma_trust < 0.0f ||
            s.partners[i].sigma_trust > 1.0f) return 18;
    }
    if (s.n_partners_ok != COS_V250_N_PARTNERS) return 19;

    if (s.sigma_a2a < 0.0f || s.sigma_a2a > 1.0f) return 20;
    if (s.sigma_a2a > 1e-6f) return 21;

    cos_v250_state_t t;
    cos_v250_init(&t, 0x250A2A7EULL);
    cos_v250_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 22;
    return 0;
}
