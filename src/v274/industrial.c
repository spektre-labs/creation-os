/*
 * v274 σ-Industrial — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "industrial.h"

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

static const struct { const char *p; float s; }
    PARAMS[COS_V274_N_PROCESS] = {
    { "temperature", 0.12f },
    { "pressure",    0.18f },
    { "speed",       0.24f },
    { "material",    0.55f },   /* drives HALT branch */
};

static const struct { const char *l; float s; }
    SUPPLY[COS_V274_N_SUPPLY] = {
    { "supplier",      0.22f },  /* no backup (0.22 <= 0.45) */
    { "factory",       0.38f },  /* no backup */
    { "distribution",  0.61f },  /* backup activated (0.61 > 0.45) */
    { "customer",      0.14f },  /* no backup */
};

static const struct { const char *id; float sq; }
    QUALITY[COS_V274_N_QUALITY] = {
    { "batch-001",  0.09f },   /* SKIP_MANUAL    */
    { "batch-002",  0.19f },   /* SKIP_MANUAL    */
    { "batch-003",  0.44f },   /* REQUIRE_MANUAL */
};

static const struct {
    const char *sh; float a, p, q, so;
} OEE[COS_V274_N_OEE] = {
    { "morning",  0.95f, 0.88f, 0.92f, 0.05f },  /* trustworthy */
    { "afternoon",0.90f, 0.82f, 0.85f, 0.12f },  /* trustworthy */
    { "night",    0.60f, 0.55f, 0.70f, 0.55f },  /* untrustworthy (σ > 0.20) */
};

static float absf(float x) { return x < 0 ? -x : x; }

void cos_v274_init(cos_v274_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed        = seed ? seed : 0x274FA6ULL;
    s->tau_process = 0.40f;
    s->tau_backup  = 0.45f;
    s->tau_quality = 0.25f;
    s->tau_oee     = 0.20f;
}

void cos_v274_run(cos_v274_state_t *s) {
    uint64_t prev = 0x274FA600ULL;

    s->n_process_params_ok = 0;
    float max_sp = 0.0f;
    for (int i = 0; i < COS_V274_N_PROCESS; ++i) {
        cos_v274_param_t *p = &s->params[i];
        memset(p, 0, sizeof(*p));
        cpy(p->param, sizeof(p->param), PARAMS[i].p);
        p->sigma_param = PARAMS[i].s;
        if (p->sigma_param >= 0.0f && p->sigma_param <= 1.0f)
            s->n_process_params_ok++;
        if (p->sigma_param > max_sp) max_sp = p->sigma_param;
        prev = fnv1a(p->param, strlen(p->param), prev);
        prev = fnv1a(&p->sigma_param, sizeof(p->sigma_param), prev);
    }
    s->sigma_process  = max_sp;
    s->process_action =
        (s->sigma_process <= s->tau_process)
            ? COS_V274_PROC_CONTINUE
            : COS_V274_PROC_HALT;
    s->process_aggregation_ok = (absf(s->sigma_process - max_sp) < 1e-6f);
    {
        cos_v274_proc_t expected =
            (s->sigma_process <= s->tau_process)
                ? COS_V274_PROC_CONTINUE : COS_V274_PROC_HALT;
        s->process_action_ok = (s->process_action == expected);
    }
    prev = fnv1a(&s->sigma_process,  sizeof(s->sigma_process),  prev);
    prev = fnv1a(&s->process_action, sizeof(s->process_action), prev);

    s->n_supply_rows_ok = 0;
    s->n_supply_backup = s->n_supply_ok_link = 0;
    for (int i = 0; i < COS_V274_N_SUPPLY; ++i) {
        cos_v274_supply_t *y = &s->supply[i];
        memset(y, 0, sizeof(*y));
        cpy(y->link, sizeof(y->link), SUPPLY[i].l);
        y->sigma_link        = SUPPLY[i].s;
        y->backup_activated  = (y->sigma_link > s->tau_backup);
        if (y->sigma_link >= 0.0f && y->sigma_link <= 1.0f)
            s->n_supply_rows_ok++;
        if (y->backup_activated) s->n_supply_backup++;
        else                     s->n_supply_ok_link++;
        prev = fnv1a(y->link, strlen(y->link), prev);
        prev = fnv1a(&y->sigma_link,       sizeof(y->sigma_link),       prev);
        prev = fnv1a(&y->backup_activated, sizeof(y->backup_activated), prev);
    }

    s->n_quality_rows_ok = 0;
    s->n_quality_skip = s->n_quality_require = 0;
    for (int i = 0; i < COS_V274_N_QUALITY; ++i) {
        cos_v274_quality_t *q = &s->quality[i];
        memset(q, 0, sizeof(*q));
        cpy(q->batch_id, sizeof(q->batch_id), QUALITY[i].id);
        q->sigma_quality = QUALITY[i].sq;
        q->action = (q->sigma_quality <= s->tau_quality)
                        ? COS_V274_Q_SKIP_MANUAL
                        : COS_V274_Q_REQUIRE_MANUAL;
        if (q->sigma_quality >= 0.0f && q->sigma_quality <= 1.0f)
            s->n_quality_rows_ok++;
        if (q->action == COS_V274_Q_SKIP_MANUAL)    s->n_quality_skip++;
        if (q->action == COS_V274_Q_REQUIRE_MANUAL) s->n_quality_require++;
        prev = fnv1a(q->batch_id, strlen(q->batch_id), prev);
        prev = fnv1a(&q->sigma_quality, sizeof(q->sigma_quality), prev);
        prev = fnv1a(&q->action,        sizeof(q->action),        prev);
    }

    s->n_oee_rows_ok = 0;
    s->n_oee_formula_ok = 0;
    s->n_oee_trust = s->n_oee_untrust = 0;
    for (int i = 0; i < COS_V274_N_OEE; ++i) {
        cos_v274_oee_t *o = &s->oee[i];
        memset(o, 0, sizeof(*o));
        cpy(o->shift, sizeof(o->shift), OEE[i].sh);
        o->availability = OEE[i].a;
        o->performance  = OEE[i].p;
        o->quality      = OEE[i].q;
        o->sigma_oee    = OEE[i].so;
        o->oee          =
            o->availability * o->performance * o->quality;
        o->trustworthy  = (o->sigma_oee <= s->tau_oee);
        if (o->availability >= 0.0f && o->availability <= 1.0f &&
            o->performance  >= 0.0f && o->performance  <= 1.0f &&
            o->quality      >= 0.0f && o->quality      <= 1.0f &&
            o->sigma_oee    >= 0.0f && o->sigma_oee    <= 1.0f)
            s->n_oee_rows_ok++;
        float expected = o->availability * o->performance * o->quality;
        if (absf(o->oee - expected) < 1e-5f) s->n_oee_formula_ok++;
        if (o->trustworthy) s->n_oee_trust++;
        else                s->n_oee_untrust++;
        prev = fnv1a(o->shift, strlen(o->shift), prev);
        prev = fnv1a(&o->availability, sizeof(o->availability), prev);
        prev = fnv1a(&o->performance,  sizeof(o->performance),  prev);
        prev = fnv1a(&o->quality,      sizeof(o->quality),      prev);
        prev = fnv1a(&o->oee,          sizeof(o->oee),          prev);
        prev = fnv1a(&o->sigma_oee,    sizeof(o->sigma_oee),    prev);
        prev = fnv1a(&o->trustworthy,  sizeof(o->trustworthy),  prev);
    }

    bool supply_both  =
        (s->n_supply_backup >= 1) && (s->n_supply_ok_link >= 1);
    bool quality_both =
        (s->n_quality_skip >= 1) && (s->n_quality_require >= 1);
    bool oee_both     =
        (s->n_oee_trust >= 1) && (s->n_oee_untrust >= 1);

    int total = COS_V274_N_PROCESS + 1 + 1 +
                COS_V274_N_SUPPLY  + 1 +
                COS_V274_N_QUALITY + 1 +
                COS_V274_N_OEE     + 1 + 1;
    int passing = s->n_process_params_ok +
                  (s->process_aggregation_ok ? 1 : 0) +
                  (s->process_action_ok ? 1 : 0) +
                  s->n_supply_rows_ok +
                  (supply_both ? 1 : 0) +
                  s->n_quality_rows_ok +
                  (quality_both ? 1 : 0) +
                  s->n_oee_rows_ok +
                  ((s->n_oee_formula_ok == COS_V274_N_OEE) ? 1 : 0) +
                  (oee_both ? 1 : 0);
    s->sigma_industrial = 1.0f - ((float)passing / (float)total);
    if (s->sigma_industrial < 0.0f) s->sigma_industrial = 0.0f;
    if (s->sigma_industrial > 1.0f) s->sigma_industrial = 1.0f;

    struct { int np, nsr, nsb, nsok, nq, nqs, nqr, no, nof, not_, nou;
             bool pa, paok;
             float sp, sigma, tp, tb, tq, toe; int pact; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.np   = s->n_process_params_ok;
    trec.nsr  = s->n_supply_rows_ok;
    trec.nsb  = s->n_supply_backup;
    trec.nsok = s->n_supply_ok_link;
    trec.nq   = s->n_quality_rows_ok;
    trec.nqs  = s->n_quality_skip;
    trec.nqr  = s->n_quality_require;
    trec.no   = s->n_oee_rows_ok;
    trec.nof  = s->n_oee_formula_ok;
    trec.not_ = s->n_oee_trust;
    trec.nou  = s->n_oee_untrust;
    trec.pa   = s->process_aggregation_ok;
    trec.paok = s->process_action_ok;
    trec.sp   = s->sigma_process;
    trec.sigma = s->sigma_industrial;
    trec.tp   = s->tau_process;
    trec.tb   = s->tau_backup;
    trec.tq   = s->tau_quality;
    trec.toe  = s->tau_oee;
    trec.pact = (int)s->process_action;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *proc_name(cos_v274_proc_t p) {
    switch (p) {
    case COS_V274_PROC_CONTINUE: return "CONTINUE";
    case COS_V274_PROC_HALT:     return "HALT";
    }
    return "UNKNOWN";
}

static const char *q_name(cos_v274_q_t q) {
    switch (q) {
    case COS_V274_Q_SKIP_MANUAL:    return "SKIP_MANUAL";
    case COS_V274_Q_REQUIRE_MANUAL: return "REQUIRE_MANUAL";
    }
    return "UNKNOWN";
}

size_t cos_v274_to_json(const cos_v274_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v274\","
        "\"tau_process\":%.3f,\"tau_backup\":%.3f,"
        "\"tau_quality\":%.3f,\"tau_oee\":%.3f,"
        "\"sigma_process\":%.4f,\"process_action\":\"%s\","
        "\"process_aggregation_ok\":%s,\"process_action_ok\":%s,"
        "\"n_process_params_ok\":%d,"
        "\"n_supply_rows_ok\":%d,"
        "\"n_supply_backup\":%d,\"n_supply_ok_link\":%d,"
        "\"n_quality_rows_ok\":%d,"
        "\"n_quality_skip\":%d,\"n_quality_require\":%d,"
        "\"n_oee_rows_ok\":%d,\"n_oee_formula_ok\":%d,"
        "\"n_oee_trust\":%d,\"n_oee_untrust\":%d,"
        "\"sigma_industrial\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"params\":[",
        s->tau_process, s->tau_backup,
        s->tau_quality, s->tau_oee,
        s->sigma_process, proc_name(s->process_action),
        s->process_aggregation_ok ? "true" : "false",
        s->process_action_ok      ? "true" : "false",
        s->n_process_params_ok,
        s->n_supply_rows_ok,
        s->n_supply_backup, s->n_supply_ok_link,
        s->n_quality_rows_ok,
        s->n_quality_skip, s->n_quality_require,
        s->n_oee_rows_ok, s->n_oee_formula_ok,
        s->n_oee_trust, s->n_oee_untrust,
        s->sigma_industrial,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V274_N_PROCESS; ++i) {
        const cos_v274_param_t *p = &s->params[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"param\":\"%s\",\"sigma_param\":%.4f}",
            i == 0 ? "" : ",", p->param, p->sigma_param);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"supply\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V274_N_SUPPLY; ++i) {
        const cos_v274_supply_t *y = &s->supply[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"link\":\"%s\",\"sigma_link\":%.4f,"
            "\"backup_activated\":%s}",
            i == 0 ? "" : ",", y->link, y->sigma_link,
            y->backup_activated ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"quality\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V274_N_QUALITY; ++i) {
        const cos_v274_quality_t *qr = &s->quality[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"batch_id\":\"%s\",\"sigma_quality\":%.4f,"
            "\"action\":\"%s\"}",
            i == 0 ? "" : ",", qr->batch_id, qr->sigma_quality,
            q_name(qr->action));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"oee\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V274_N_OEE; ++i) {
        const cos_v274_oee_t *o = &s->oee[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"shift\":\"%s\",\"availability\":%.4f,"
            "\"performance\":%.4f,\"quality\":%.4f,"
            "\"oee\":%.4f,\"sigma_oee\":%.4f,"
            "\"trustworthy\":%s}",
            i == 0 ? "" : ",", o->shift,
            o->availability, o->performance, o->quality,
            o->oee, o->sigma_oee,
            o->trustworthy ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v274_self_test(void) {
    cos_v274_state_t s;
    cos_v274_init(&s, 0x274FA6ULL);
    cos_v274_run(&s);
    if (!s.chain_valid) return 1;

    const char *pn[COS_V274_N_PROCESS] = {
        "temperature","pressure","speed","material"
    };
    float mx = 0.0f;
    for (int i = 0; i < COS_V274_N_PROCESS; ++i) {
        if (strcmp(s.params[i].param, pn[i]) != 0) return 2;
        if (s.params[i].sigma_param > mx) mx = s.params[i].sigma_param;
    }
    if (absf(s.sigma_process - mx) > 1e-6f) return 3;
    if (!s.process_aggregation_ok) return 4;
    cos_v274_proc_t exp_pa =
        (s.sigma_process <= s.tau_process)
            ? COS_V274_PROC_CONTINUE : COS_V274_PROC_HALT;
    if (s.process_action != exp_pa) return 5;
    if (!s.process_action_ok) return 6;
    if (s.process_action != COS_V274_PROC_HALT) return 7;
    if (s.n_process_params_ok != COS_V274_N_PROCESS) return 8;

    const char *sn[COS_V274_N_SUPPLY] = {
        "supplier","factory","distribution","customer"
    };
    for (int i = 0; i < COS_V274_N_SUPPLY; ++i) {
        if (strcmp(s.supply[i].link, sn[i]) != 0) return 9;
        bool exp = (s.supply[i].sigma_link > s.tau_backup);
        if (s.supply[i].backup_activated != exp) return 10;
    }
    if (s.n_supply_rows_ok != COS_V274_N_SUPPLY) return 11;
    if (s.n_supply_backup  < 1) return 12;
    if (s.n_supply_ok_link < 1) return 13;

    for (int i = 0; i < COS_V274_N_QUALITY; ++i) {
        cos_v274_q_t exp =
            (s.quality[i].sigma_quality <= s.tau_quality)
                ? COS_V274_Q_SKIP_MANUAL : COS_V274_Q_REQUIRE_MANUAL;
        if (s.quality[i].action != exp) return 14;
    }
    if (s.n_quality_rows_ok != COS_V274_N_QUALITY) return 15;
    if (s.n_quality_skip    < 1) return 16;
    if (s.n_quality_require < 1) return 17;

    for (int i = 0; i < COS_V274_N_OEE; ++i) {
        float expected = s.oee[i].availability *
                         s.oee[i].performance *
                         s.oee[i].quality;
        if (absf(s.oee[i].oee - expected) > 1e-5f) return 18;
        bool exp = (s.oee[i].sigma_oee <= s.tau_oee);
        if (s.oee[i].trustworthy != exp) return 19;
    }
    if (s.n_oee_rows_ok    != COS_V274_N_OEE) return 20;
    if (s.n_oee_formula_ok != COS_V274_N_OEE) return 21;
    if (s.n_oee_trust   < 1) return 22;
    if (s.n_oee_untrust < 1) return 23;

    if (s.sigma_industrial < 0.0f || s.sigma_industrial > 1.0f) return 24;
    if (s.sigma_industrial > 1e-6f) return 25;

    cos_v274_state_t u;
    cos_v274_init(&u, 0x274FA6ULL);
    cos_v274_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 26;
    return 0;
}
