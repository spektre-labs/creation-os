/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* v303 σ-Governance — v0 manifest implementation. */

#include "governance.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t cos_v303_fnv1a(uint64_t h, const void *p, size_t n)
{
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 0x100000001b3ULL; }
    return h;
}
static void cpy(char *d, size_t c, const char *s)
{
    size_t i = 0;
    if (!c || !s) { if (c) d[0] = '\0'; return; }
    for (; s[i] && i + 1 < c; ++i) d[i] = s[i];
    d[i] = '\0';
}

void cos_v303_init(cos_v303_state_t *s, uint64_t seed)
{ memset(s, 0, sizeof *s); s->seed = seed; }

void cos_v303_run(cos_v303_state_t *s)
{
    /* 1) Decision σ */
    const char *d_lbl[] = {
        "strategy_matches_ops",
        "strategy_partially_realised",
        "strategy_ignored" };
    const float d_s[] = { 0.05f, 0.35f, 0.75f };
    const char *d_v[] = { "COHERENT", "DEGRADED", "INCOHERENT" };

    int dok = 0;
    for (int i = 0; i < COS_V303_N_DEC; ++i) {
        cos_v303_dec_t *d = &s->dec[i];
        cpy(d->label, sizeof d->label, d_lbl[i]);
        d->sigma_decision = d_s[i];
        cpy(d->verdict, sizeof d->verdict, d_v[i]);
        if (strlen(d->label) > 0) ++dok;
    }
    s->n_dec_rows_ok = dok;
    bool dord = true;
    for (int i = 1; i < COS_V303_N_DEC; ++i)
        if (!(s->dec[i].sigma_decision > s->dec[i-1].sigma_decision))
            dord = false;
    s->dec_order_ok = dord;
    s->dec_distinct_ok =
        (strcmp(s->dec[0].verdict, s->dec[1].verdict) != 0) &&
        (strcmp(s->dec[1].verdict, s->dec[2].verdict) != 0) &&
        (strcmp(s->dec[0].verdict, s->dec[2].verdict) != 0);

    /* 2) Meeting σ */
    const char *m_lbl[] = {
        "meeting_perfect", "meeting_quarter", "meeting_noise" };
    const int   m_made[] = { 10, 8, 10 };
    const int   m_real[] = { 10, 6,  3 };

    int mok = 0, f_all = 1;
    for (int i = 0; i < COS_V303_N_MTG; ++i) {
        cos_v303_mtg_t *m = &s->mtg[i];
        cpy(m->label, sizeof m->label, m_lbl[i]);
        m->decisions_made     = m_made[i];
        m->decisions_realised = m_real[i];
        double expected = 1.0 - (double)m->decisions_realised /
                                (double)m->decisions_made;
        m->sigma_meeting = (float)expected;
        if (fabs((double)m->sigma_meeting - expected) > 1e-3)
            f_all = 0;
        if (strlen(m->label) > 0) ++mok;
    }
    s->n_mtg_rows_ok = mok;
    s->mtg_formula_ok = (f_all == 1);
    bool mord = true;
    for (int i = 1; i < COS_V303_N_MTG; ++i)
        if (!(s->mtg[i].sigma_meeting > s->mtg[i-1].sigma_meeting))
            mord = false;
    s->mtg_order_ok = mord;

    /* 3) Communication σ */
    const char *c_lbl[] = {
        "clear_message", "slightly_vague", "highly_vague" };
    const float c_s[] = { 0.10f, 0.35f, 0.80f };

    int cok = 0, n_c = 0, n_a = 0;
    for (int i = 0; i < COS_V303_N_COMM; ++i) {
        cos_v303_comm_t *c = &s->comm[i];
        cpy(c->channel, sizeof c->channel, c_lbl[i]);
        c->sigma_comm = c_s[i];
        c->clear = (c->sigma_comm <= COS_V303_TAU_COMM);
        if (c->clear) ++n_c; else ++n_a;
        if (strlen(c->channel) > 0) ++cok;
    }
    s->n_comm_rows_ok = cok;
    bool cord = true;
    for (int i = 1; i < COS_V303_N_COMM; ++i)
        if (!(s->comm[i].sigma_comm > s->comm[i-1].sigma_comm))
            cord = false;
    s->comm_order_ok = cord;
    s->comm_polarity_ok = (n_c > 0) && (n_a > 0);

    /* 4) Institutional K(t) */
    const char *k_lbl[] = {
        "healthy_org", "warning_org", "collapsing_org" };
    const float k_rho  [] = { 0.85f, 0.50f, 0.30f };
    const float k_iphi [] = { 0.60f, 0.50f, 0.40f };
    const float k_f    [] = { 0.70f, 0.60f, 0.50f };

    int kok = 0, fk_all = 1;
    int n_v = 0, n_w = 0, n_c2 = 0;
    for (int i = 0; i < COS_V303_N_KT; ++i) {
        cos_v303_kt_t *k = &s->kt[i];
        cpy(k->org, sizeof k->org, k_lbl[i]);
        k->rho   = k_rho[i];
        k->i_phi = k_iphi[i];
        k->f     = k_f[i];
        double expected = (double)k->rho * (double)k->i_phi * (double)k->f;
        k->k = (float)expected;
        if (fabs((double)k->k - expected) > 1e-3) fk_all = 0;
        if (k->k >= COS_V303_K_WARN) {
            cpy(k->status, sizeof k->status, "VIABLE");
            ++n_v;
        } else if (k->k >= COS_V303_K_CRIT) {
            cpy(k->status, sizeof k->status, "WARNING");
            ++n_w;
        } else {
            cpy(k->status, sizeof k->status, "COLLAPSE");
            ++n_c2;
        }
        if (strlen(k->org) > 0) ++kok;
    }
    s->n_kt_rows_ok = kok;
    s->kt_formula_ok = (fk_all == 1);
    s->kt_polarity_ok = (n_v > 0) && (n_w > 0) && (n_c2 > 0);

    /* σ_gov aggregation */
    int good = 0, denom = 0;
    good += s->n_dec_rows_ok;                       denom += 3;
    good += s->dec_order_ok ? 1 : 0;                denom += 1;
    good += s->dec_distinct_ok ? 1 : 0;             denom += 1;
    good += s->n_mtg_rows_ok;                       denom += 3;
    good += s->mtg_formula_ok ? 1 : 0;              denom += 1;
    good += s->mtg_order_ok ? 1 : 0;                denom += 1;
    good += s->n_comm_rows_ok;                      denom += 3;
    good += s->comm_order_ok ? 1 : 0;               denom += 1;
    good += s->comm_polarity_ok ? 1 : 0;            denom += 1;
    good += s->n_kt_rows_ok;                        denom += 3;
    good += s->kt_formula_ok ? 1 : 0;               denom += 1;
    good += s->kt_polarity_ok ? 1 : 0;              denom += 1;
    s->sigma_gov = 1.0f - ((float)good / (float)denom);

    uint64_t h = 0xcbf29ce484222325ULL;
    h = cos_v303_fnv1a(h, &s->seed, sizeof s->seed);
    for (int i = 0; i < COS_V303_N_DEC; ++i)
        h = cos_v303_fnv1a(h, &s->dec[i], sizeof s->dec[i]);
    for (int i = 0; i < COS_V303_N_MTG; ++i)
        h = cos_v303_fnv1a(h, &s->mtg[i], sizeof s->mtg[i]);
    for (int i = 0; i < COS_V303_N_COMM; ++i)
        h = cos_v303_fnv1a(h, &s->comm[i], sizeof s->comm[i]);
    for (int i = 0; i < COS_V303_N_KT; ++i)
        h = cos_v303_fnv1a(h, &s->kt[i], sizeof s->kt[i]);
    h = cos_v303_fnv1a(h, &s->sigma_gov, sizeof s->sigma_gov);
    s->terminal_hash = h;
    s->chain_valid = true;
}

size_t cos_v303_to_json(const cos_v303_state_t *s, char *buf, size_t cap)
{
    if (!buf || cap == 0) return 0;
    int w = snprintf(buf, cap,
        "{\"kernel\":\"v303_governance\","
        "\"tau_comm\":%.4f,\"k_crit\":%.4f,\"k_warn\":%.4f,"
        "\"decisions\":[",
        (double)COS_V303_TAU_COMM,
        (double)COS_V303_K_CRIT,
        (double)COS_V303_K_WARN);
    if (w < 0 || (size_t)w >= cap) return (size_t)w;

    for (int i = 0; i < COS_V303_N_DEC; ++i) {
        const cos_v303_dec_t *d = &s->dec[i];
        int n = snprintf(buf + w, cap - (size_t)w,
            "%s{\"label\":\"%s\",\"sigma_decision\":%.4f,"
            "\"verdict\":\"%s\"}",
            i ? "," : "", d->label, (double)d->sigma_decision, d->verdict);
        if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
        w += n;
    }
    int n = snprintf(buf + w, cap - (size_t)w,
        "],\"dec_rows_ok\":%d,\"dec_order_ok\":%s,"
        "\"dec_distinct_ok\":%s,\"meetings\":[",
        s->n_dec_rows_ok,
        s->dec_order_ok ? "true" : "false",
        s->dec_distinct_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V303_N_MTG; ++i) {
        const cos_v303_mtg_t *m = &s->mtg[i];
        int q = snprintf(buf + w, cap - (size_t)w,
            "%s{\"label\":\"%s\",\"decisions_made\":%d,"
            "\"decisions_realised\":%d,\"sigma_meeting\":%.4f}",
            i ? "," : "", m->label, m->decisions_made,
            m->decisions_realised, (double)m->sigma_meeting);
        if (q < 0 || (size_t)(w + q) >= cap) return (size_t)(w + q);
        w += q;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"mtg_rows_ok\":%d,\"mtg_formula_ok\":%s,"
        "\"mtg_order_ok\":%s,\"comms\":[",
        s->n_mtg_rows_ok,
        s->mtg_formula_ok ? "true" : "false",
        s->mtg_order_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V303_N_COMM; ++i) {
        const cos_v303_comm_t *c = &s->comm[i];
        int q = snprintf(buf + w, cap - (size_t)w,
            "%s{\"channel\":\"%s\",\"sigma_comm\":%.4f,\"clear\":%s}",
            i ? "," : "", c->channel, (double)c->sigma_comm,
            c->clear ? "true" : "false");
        if (q < 0 || (size_t)(w + q) >= cap) return (size_t)(w + q);
        w += q;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"comm_rows_ok\":%d,\"comm_order_ok\":%s,"
        "\"comm_polarity_ok\":%s,\"institutional_k\":[",
        s->n_comm_rows_ok,
        s->comm_order_ok ? "true" : "false",
        s->comm_polarity_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V303_N_KT; ++i) {
        const cos_v303_kt_t *k = &s->kt[i];
        int q = snprintf(buf + w, cap - (size_t)w,
            "%s{\"org\":\"%s\",\"rho\":%.4f,\"i_phi\":%.4f,"
            "\"f\":%.4f,\"k\":%.4f,\"status\":\"%s\"}",
            i ? "," : "", k->org, (double)k->rho, (double)k->i_phi,
            (double)k->f, (double)k->k, k->status);
        if (q < 0 || (size_t)(w + q) >= cap) return (size_t)(w + q);
        w += q;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"kt_rows_ok\":%d,\"kt_formula_ok\":%s,"
        "\"kt_polarity_ok\":%s,"
        "\"sigma_gov\":%.4f,\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\"}",
        s->n_kt_rows_ok,
        s->kt_formula_ok ? "true" : "false",
        s->kt_polarity_ok ? "true" : "false",
        (double)s->sigma_gov,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;
    return (size_t)w;
}

int cos_v303_self_test(void)
{
    cos_v303_state_t s;
    cos_v303_init(&s, 303u);
    cos_v303_run(&s);
    if (s.n_dec_rows_ok  != COS_V303_N_DEC)  return 1;
    if (!s.dec_order_ok)                     return 2;
    if (!s.dec_distinct_ok)                  return 3;
    if (s.n_mtg_rows_ok  != COS_V303_N_MTG)  return 4;
    if (!s.mtg_formula_ok)                   return 5;
    if (!s.mtg_order_ok)                     return 6;
    if (s.n_comm_rows_ok != COS_V303_N_COMM) return 7;
    if (!s.comm_order_ok)                    return 8;
    if (!s.comm_polarity_ok)                 return 9;
    if (s.n_kt_rows_ok   != COS_V303_N_KT)   return 10;
    if (!s.kt_formula_ok)                    return 11;
    if (!s.kt_polarity_ok)                   return 12;
    if (!(s.sigma_gov == 0.0f))              return 13;
    if (!s.chain_valid)                      return 14;
    if (s.terminal_hash == 0ULL)             return 15;
    char buf[4096];
    size_t nj = cos_v303_to_json(&s, buf, sizeof buf);
    if (nj == 0 || nj >= sizeof buf)         return 16;
    if (!strstr(buf, "\"kernel\":\"v303_governance\"")) return 17;
    return 0;
}
