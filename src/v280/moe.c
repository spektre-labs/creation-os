/*
 * v280 σ-MoE — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "moe.h"

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

static const struct { int id; float s; }
    ROUTES[COS_V280_N_ROUTE] = {
    { 0, 0.09f },   /* TOP_K     */
    { 1, 0.28f },   /* TOP_K     */
    { 2, 0.52f },   /* DIVERSIFY */
    { 3, 0.81f },   /* DIVERSIFY */
};

static const struct { const char *t; float e; }
    SIGS[COS_V280_N_SIG] = {
    { "code",     0.12f },   /* KNOWN */
    { "math",     0.33f },   /* KNOWN */
    { "creative", 0.64f },   /* NOVEL */
};

static const struct { const char *lbl; float s; }
    PREFETCHES[COS_V280_N_PREFETCH] = {
    { "low",  0.10f },   /* AGGRESSIVE   */
    { "mid",  0.38f },   /* BALANCED     */
    { "high", 0.72f },   /* CONSERVATIVE */
};

static const struct { const char *n; float s; }
    MOBIES[COS_V280_N_MOBIE] = {
    { "expert_0", 0.11f },   /* BIT1 */
    { "expert_1", 0.36f },   /* BIT2 */
    { "expert_2", 0.74f },   /* BIT4 */
};

void cos_v280_init(cos_v280_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed              = seed ? seed : 0x280666ULL;
    s->tau_route         = 0.35f;
    s->tau_sig           = 0.40f;
    s->tau_prefetch_low  = 0.20f;
    s->tau_prefetch_mid  = 0.50f;
    s->tau_shift_low     = 0.20f;
    s->tau_shift_mid     = 0.50f;
}

void cos_v280_run(cos_v280_state_t *s) {
    uint64_t prev = 0x28066600ULL;

    s->n_route_rows_ok = 0;
    s->n_route_topk = s->n_route_diversify = 0;
    for (int i = 0; i < COS_V280_N_ROUTE; ++i) {
        cos_v280_route_row_t *r = &s->route[i];
        memset(r, 0, sizeof(*r));
        r->token_id      = ROUTES[i].id;
        r->sigma_routing = ROUTES[i].s;
        r->decision      = (r->sigma_routing <= s->tau_route)
                               ? COS_V280_ROUTE_TOP_K
                               : COS_V280_ROUTE_DIVERSIFY;
        if (r->sigma_routing >= 0.0f && r->sigma_routing <= 1.0f)
            s->n_route_rows_ok++;
        if (r->decision == COS_V280_ROUTE_TOP_K) s->n_route_topk++;
        else                                     s->n_route_diversify++;
        prev = fnv1a(&r->token_id,      sizeof(r->token_id),      prev);
        prev = fnv1a(&r->sigma_routing, sizeof(r->sigma_routing), prev);
        prev = fnv1a(&r->decision,      sizeof(r->decision),      prev);
    }

    s->n_sig_rows_ok = 0;
    s->n_sig_known = s->n_sig_novel = 0;
    for (int i = 0; i < COS_V280_N_SIG; ++i) {
        cos_v280_sig_t *g = &s->sig[i];
        memset(g, 0, sizeof(*g));
        cpy(g->task, sizeof(g->task), SIGS[i].t);
        g->routing_entropy = SIGS[i].e;
        g->familiar        = (g->routing_entropy <= s->tau_sig)
                                 ? COS_V280_FAM_KNOWN
                                 : COS_V280_FAM_NOVEL;
        if (g->routing_entropy >= 0.0f && g->routing_entropy <= 1.0f)
            s->n_sig_rows_ok++;
        if (g->familiar == COS_V280_FAM_KNOWN) s->n_sig_known++;
        else                                   s->n_sig_novel++;
        prev = fnv1a(g->task, strlen(g->task), prev);
        prev = fnv1a(&g->routing_entropy, sizeof(g->routing_entropy), prev);
        prev = fnv1a(&g->familiar,        sizeof(g->familiar),        prev);
    }

    s->n_prefetch_rows_ok = 0;
    s->n_prefetch_agg = s->n_prefetch_bal = s->n_prefetch_cons = 0;
    for (int i = 0; i < COS_V280_N_PREFETCH; ++i) {
        cos_v280_prefetch_t *pf = &s->prefetch[i];
        memset(pf, 0, sizeof(*pf));
        cpy(pf->label, sizeof(pf->label), PREFETCHES[i].lbl);
        pf->sigma_prefetch = PREFETCHES[i].s;
        if      (pf->sigma_prefetch <= s->tau_prefetch_low) pf->strategy = COS_V280_STRAT_AGGRESSIVE;
        else if (pf->sigma_prefetch <= s->tau_prefetch_mid) pf->strategy = COS_V280_STRAT_BALANCED;
        else                                                pf->strategy = COS_V280_STRAT_CONSERVATIVE;
        if (pf->sigma_prefetch >= 0.0f && pf->sigma_prefetch <= 1.0f)
            s->n_prefetch_rows_ok++;
        if (pf->strategy == COS_V280_STRAT_AGGRESSIVE)   s->n_prefetch_agg++;
        if (pf->strategy == COS_V280_STRAT_BALANCED)     s->n_prefetch_bal++;
        if (pf->strategy == COS_V280_STRAT_CONSERVATIVE) s->n_prefetch_cons++;
        prev = fnv1a(pf->label, strlen(pf->label), prev);
        prev = fnv1a(&pf->sigma_prefetch, sizeof(pf->sigma_prefetch), prev);
        prev = fnv1a(&pf->strategy,       sizeof(pf->strategy),       prev);
    }

    s->n_mobie_rows_ok = 0;
    s->n_mobie_bit1 = s->n_mobie_bit2 = s->n_mobie_bit4 = 0;
    for (int i = 0; i < COS_V280_N_MOBIE; ++i) {
        cos_v280_mobie_t *m = &s->mobie[i];
        memset(m, 0, sizeof(*m));
        cpy(m->name, sizeof(m->name), MOBIES[i].n);
        m->sigma_shift = MOBIES[i].s;
        if      (m->sigma_shift <= s->tau_shift_low) m->bits = COS_V280_BITS_BIT1;
        else if (m->sigma_shift <= s->tau_shift_mid) m->bits = COS_V280_BITS_BIT2;
        else                                         m->bits = COS_V280_BITS_BIT4;
        if (m->sigma_shift >= 0.0f && m->sigma_shift <= 1.0f)
            s->n_mobie_rows_ok++;
        if (m->bits == COS_V280_BITS_BIT1) s->n_mobie_bit1++;
        if (m->bits == COS_V280_BITS_BIT2) s->n_mobie_bit2++;
        if (m->bits == COS_V280_BITS_BIT4) s->n_mobie_bit4++;
        prev = fnv1a(m->name, strlen(m->name), prev);
        prev = fnv1a(&m->sigma_shift, sizeof(m->sigma_shift), prev);
        prev = fnv1a(&m->bits,        sizeof(m->bits),        prev);
    }

    bool route_both = (s->n_route_topk >= 1) && (s->n_route_diversify >= 1);
    bool sig_both   = (s->n_sig_known  >= 1) && (s->n_sig_novel       >= 1);
    bool prefetch_all =
        (s->n_prefetch_agg  == 1) &&
        (s->n_prefetch_bal  == 1) &&
        (s->n_prefetch_cons == 1);
    bool mobie_all =
        (s->n_mobie_bit1 == 1) &&
        (s->n_mobie_bit2 == 1) &&
        (s->n_mobie_bit4 == 1);

    int total   = COS_V280_N_ROUTE    + 1 +
                  COS_V280_N_SIG      + 1 +
                  COS_V280_N_PREFETCH + 1 +
                  COS_V280_N_MOBIE    + 1;
    int passing = s->n_route_rows_ok +
                  (route_both ? 1 : 0) +
                  s->n_sig_rows_ok +
                  (sig_both ? 1 : 0) +
                  s->n_prefetch_rows_ok +
                  (prefetch_all ? 1 : 0) +
                  s->n_mobie_rows_ok +
                  (mobie_all ? 1 : 0);
    s->sigma_moe = 1.0f - ((float)passing / (float)total);
    if (s->sigma_moe < 0.0f) s->sigma_moe = 0.0f;
    if (s->sigma_moe > 1.0f) s->sigma_moe = 1.0f;

    struct { int nr, nrt, nrd, ns, nsk, nsn,
                 np, npa, npb, npc,
                 nm, nm1, nm2, nm4;
             float sigma, tr, tsg, tpl, tpm, tsl, tsm;
             uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nr  = s->n_route_rows_ok;
    trec.nrt = s->n_route_topk;
    trec.nrd = s->n_route_diversify;
    trec.ns  = s->n_sig_rows_ok;
    trec.nsk = s->n_sig_known;
    trec.nsn = s->n_sig_novel;
    trec.np  = s->n_prefetch_rows_ok;
    trec.npa = s->n_prefetch_agg;
    trec.npb = s->n_prefetch_bal;
    trec.npc = s->n_prefetch_cons;
    trec.nm  = s->n_mobie_rows_ok;
    trec.nm1 = s->n_mobie_bit1;
    trec.nm2 = s->n_mobie_bit2;
    trec.nm4 = s->n_mobie_bit4;
    trec.sigma = s->sigma_moe;
    trec.tr  = s->tau_route;
    trec.tsg = s->tau_sig;
    trec.tpl = s->tau_prefetch_low;
    trec.tpm = s->tau_prefetch_mid;
    trec.tsl = s->tau_shift_low;
    trec.tsm = s->tau_shift_mid;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *route_name(cos_v280_route_t r) {
    switch (r) {
    case COS_V280_ROUTE_TOP_K:     return "TOP_K";
    case COS_V280_ROUTE_DIVERSIFY: return "DIVERSIFY";
    }
    return "UNKNOWN";
}

static const char *fam_name(cos_v280_fam_t f) {
    switch (f) {
    case COS_V280_FAM_KNOWN: return "KNOWN";
    case COS_V280_FAM_NOVEL: return "NOVEL";
    }
    return "UNKNOWN";
}

static const char *strat_name(cos_v280_strat_t st) {
    switch (st) {
    case COS_V280_STRAT_AGGRESSIVE:   return "AGGRESSIVE";
    case COS_V280_STRAT_BALANCED:     return "BALANCED";
    case COS_V280_STRAT_CONSERVATIVE: return "CONSERVATIVE";
    }
    return "UNKNOWN";
}

static const char *bits_name(cos_v280_bits_t b) {
    switch (b) {
    case COS_V280_BITS_BIT1: return "BIT1";
    case COS_V280_BITS_BIT2: return "BIT2";
    case COS_V280_BITS_BIT4: return "BIT4";
    }
    return "UNKNOWN";
}

size_t cos_v280_to_json(const cos_v280_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v280\","
        "\"tau_route\":%.3f,\"tau_sig\":%.3f,"
        "\"tau_prefetch_low\":%.3f,\"tau_prefetch_mid\":%.3f,"
        "\"tau_shift_low\":%.3f,\"tau_shift_mid\":%.3f,"
        "\"n_route_rows_ok\":%d,"
        "\"n_route_topk\":%d,\"n_route_diversify\":%d,"
        "\"n_sig_rows_ok\":%d,"
        "\"n_sig_known\":%d,\"n_sig_novel\":%d,"
        "\"n_prefetch_rows_ok\":%d,"
        "\"n_prefetch_agg\":%d,\"n_prefetch_bal\":%d,\"n_prefetch_cons\":%d,"
        "\"n_mobie_rows_ok\":%d,"
        "\"n_mobie_bit1\":%d,\"n_mobie_bit2\":%d,\"n_mobie_bit4\":%d,"
        "\"sigma_moe\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"route\":[",
        s->tau_route, s->tau_sig,
        s->tau_prefetch_low, s->tau_prefetch_mid,
        s->tau_shift_low, s->tau_shift_mid,
        s->n_route_rows_ok,
        s->n_route_topk, s->n_route_diversify,
        s->n_sig_rows_ok,
        s->n_sig_known, s->n_sig_novel,
        s->n_prefetch_rows_ok,
        s->n_prefetch_agg, s->n_prefetch_bal, s->n_prefetch_cons,
        s->n_mobie_rows_ok,
        s->n_mobie_bit1, s->n_mobie_bit2, s->n_mobie_bit4,
        s->sigma_moe,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V280_N_ROUTE; ++i) {
        const cos_v280_route_row_t *r = &s->route[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"token_id\":%d,\"sigma_routing\":%.4f,\"decision\":\"%s\"}",
            i == 0 ? "" : ",", r->token_id, r->sigma_routing, route_name(r->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"sig\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V280_N_SIG; ++i) {
        const cos_v280_sig_t *g = &s->sig[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"task\":\"%s\",\"routing_entropy\":%.4f,\"familiar\":\"%s\"}",
            i == 0 ? "" : ",", g->task, g->routing_entropy, fam_name(g->familiar));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"prefetch\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V280_N_PREFETCH; ++i) {
        const cos_v280_prefetch_t *pf = &s->prefetch[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"label\":\"%s\",\"sigma_prefetch\":%.4f,\"strategy\":\"%s\"}",
            i == 0 ? "" : ",", pf->label, pf->sigma_prefetch, strat_name(pf->strategy));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"mobie\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V280_N_MOBIE; ++i) {
        const cos_v280_mobie_t *m = &s->mobie[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"sigma_shift\":%.4f,\"bits\":\"%s\"}",
            i == 0 ? "" : ",", m->name, m->sigma_shift, bits_name(m->bits));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int mx = snprintf(buf + off, cap - off, "]}");
    if (mx < 0 || off + (size_t)mx >= cap) return 0;
    return off + (size_t)mx;
}

int cos_v280_self_test(void) {
    cos_v280_state_t s;
    cos_v280_init(&s, 0x280666ULL);
    cos_v280_run(&s);
    if (!s.chain_valid) return 1;

    for (int i = 0; i < COS_V280_N_ROUTE; ++i) {
        cos_v280_route_t exp = (s.route[i].sigma_routing <= s.tau_route)
                                   ? COS_V280_ROUTE_TOP_K
                                   : COS_V280_ROUTE_DIVERSIFY;
        if (s.route[i].decision != exp) return 2;
    }
    if (s.n_route_rows_ok != COS_V280_N_ROUTE) return 3;
    if (s.n_route_topk      < 1) return 4;
    if (s.n_route_diversify < 1) return 5;

    static const char *WANT_TASK[COS_V280_N_SIG] = { "code", "math", "creative" };
    for (int i = 0; i < COS_V280_N_SIG; ++i) {
        if (strcmp(s.sig[i].task, WANT_TASK[i]) != 0) return 6;
        cos_v280_fam_t exp = (s.sig[i].routing_entropy <= s.tau_sig)
                                 ? COS_V280_FAM_KNOWN
                                 : COS_V280_FAM_NOVEL;
        if (s.sig[i].familiar != exp) return 7;
    }
    if (s.n_sig_rows_ok != COS_V280_N_SIG) return 8;
    if (s.n_sig_known < 1) return 9;
    if (s.n_sig_novel < 1) return 10;

    static const char *WANT_PF[COS_V280_N_PREFETCH] = { "low", "mid", "high" };
    for (int i = 0; i < COS_V280_N_PREFETCH; ++i) {
        if (strcmp(s.prefetch[i].label, WANT_PF[i]) != 0) return 11;
        cos_v280_strat_t exp;
        if      (s.prefetch[i].sigma_prefetch <= s.tau_prefetch_low) exp = COS_V280_STRAT_AGGRESSIVE;
        else if (s.prefetch[i].sigma_prefetch <= s.tau_prefetch_mid) exp = COS_V280_STRAT_BALANCED;
        else                                                         exp = COS_V280_STRAT_CONSERVATIVE;
        if (s.prefetch[i].strategy != exp) return 12;
    }
    if (s.n_prefetch_rows_ok != COS_V280_N_PREFETCH) return 13;
    if (s.n_prefetch_agg  != 1) return 14;
    if (s.n_prefetch_bal  != 1) return 15;
    if (s.n_prefetch_cons != 1) return 16;

    static const char *WANT_EXP[COS_V280_N_MOBIE] = { "expert_0", "expert_1", "expert_2" };
    for (int i = 0; i < COS_V280_N_MOBIE; ++i) {
        if (strcmp(s.mobie[i].name, WANT_EXP[i]) != 0) return 17;
        cos_v280_bits_t exp;
        if      (s.mobie[i].sigma_shift <= s.tau_shift_low) exp = COS_V280_BITS_BIT1;
        else if (s.mobie[i].sigma_shift <= s.tau_shift_mid) exp = COS_V280_BITS_BIT2;
        else                                                exp = COS_V280_BITS_BIT4;
        if (s.mobie[i].bits != exp) return 18;
    }
    if (s.n_mobie_rows_ok != COS_V280_N_MOBIE) return 19;
    if (s.n_mobie_bit1 != 1) return 20;
    if (s.n_mobie_bit2 != 1) return 21;
    if (s.n_mobie_bit4 != 1) return 22;

    if (s.sigma_moe < 0.0f || s.sigma_moe > 1.0f) return 23;
    if (s.sigma_moe > 1e-6f) return 24;

    cos_v280_state_t u;
    cos_v280_init(&u, 0x280666ULL);
    cos_v280_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 25;
    return 0;
}
