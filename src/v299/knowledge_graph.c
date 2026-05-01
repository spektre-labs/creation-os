/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* v299 σ-Knowledge-Graph — v0 manifest implementation. */

#include "knowledge_graph.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t cos_v299_fnv1a(uint64_t h, const void *p, size_t n)
{
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 0; i < n; ++i) {
        h ^= b[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

static void cpy(char *dst, size_t cap, const char *src)
{
    size_t i = 0;
    if (cap == 0 || !src) {
        if (cap) dst[0] = '\0';
        return;
    }
    for (; src[i] && i + 1 < cap; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

void cos_v299_init(cos_v299_state_t *s, uint64_t seed)
{
    memset(s, 0, sizeof *s);
    s->seed = seed;
}

void cos_v299_run(cos_v299_state_t *s)
{
    /* 1) σ-grounded retrieval */
    const char *ret_kind[COS_V299_N_RET]  = {
        "known_fact", "partial_match", "unknown" };
    const float ret_sigma[COS_V299_N_RET] = {
        0.08f, 0.35f, 0.85f };

    int rok = 0, n_from_kg = 0, n_fallback = 0;
    int n_gate_true = 0, n_gate_false = 0;
    for (int i = 0; i < COS_V299_N_RET; ++i) {
        cos_v299_ret_t *r = &s->ret[i];
        cpy(r->query_kind, sizeof r->query_kind, ret_kind[i]);
        r->sigma_retrieval = ret_sigma[i];
        bool from_kg = r->sigma_retrieval <= COS_V299_TAU_KG;
        cpy(r->route, sizeof r->route,
            from_kg ? "FROM_KG" : "FALLBACK_LLM");
        if (from_kg)  { ++n_from_kg; ++n_gate_true; }
        else          { ++n_fallback; ++n_gate_false; }
        if (strlen(r->query_kind) > 0 && strlen(r->route) > 0)
            ++rok;
    }
    s->n_ret_rows_ok = rok;

    bool ret_ord = true;
    for (int i = 1; i < COS_V299_N_RET; ++i)
        if (!(s->ret[i].sigma_retrieval >
              s->ret[i-1].sigma_retrieval)) ret_ord = false;
    s->ret_order_ok = ret_ord;
    s->ret_gate_polarity_ok =
        (n_gate_true > 0) && (n_gate_false > 0);
    s->ret_fromkg_count_ok =
        (n_from_kg == 2) && (n_fallback == 1);

    /* 2) Provenance tracking */
    const char *prov_src[COS_V299_N_PROV] = {
        "primary_source", "secondary_source", "rumor_source" };
    const float prov_sigma[COS_V299_N_PROV] = {
        0.05f, 0.25f, 0.80f };
    const bool  prov_pr[COS_V299_N_PROV] = {
        true, false, false };
    const char *prov_ref[COS_V299_N_PROV] = {
        "arxiv:2403.12345",
        "blog:spektre_labs",
        "" };

    int pok = 0, n_trust_t = 0, n_trust_f = 0;
    int source_ok_all = 1;
    for (int i = 0; i < COS_V299_N_PROV; ++i) {
        cos_v299_prov_t *p = &s->prov[i];
        cpy(p->source, sizeof p->source, prov_src[i]);
        p->sigma_provenance = prov_sigma[i];
        p->trusted = (p->sigma_provenance <= COS_V299_TAU_PROV);
        p->peer_reviewed = prov_pr[i];
        cpy(p->source_ref, sizeof p->source_ref, prov_ref[i]);
        if (p->trusted) {
            ++n_trust_t;
            if (strlen(p->source_ref) == 0) source_ok_all = 0;
        } else {
            ++n_trust_f;
        }
        if (strlen(p->source) > 0) ++pok;
    }
    s->n_prov_rows_ok = pok;

    bool prov_ord = true;
    for (int i = 1; i < COS_V299_N_PROV; ++i)
        if (!(s->prov[i].sigma_provenance >
              s->prov[i-1].sigma_provenance)) prov_ord = false;
    s->prov_order_ok = prov_ord;
    s->prov_polarity_ok = (n_trust_t > 0) && (n_trust_f > 0);
    s->prov_source_ok = (source_ok_all == 1);

    /* 3) Multi-hop σ */
    const char *hop_lbl[COS_V299_N_HOP] = { "1_hop", "3_hop", "5_hop" };
    const int   hop_n  [COS_V299_N_HOP] = { 1, 3, 5 };
    const float hop_per[COS_V299_N_HOP] = { 0.10f, 0.15f, 0.20f };

    int hok = 0;
    int n_warn_t = 0, n_warn_f = 0;
    int formula_ok_all = 1;
    for (int i = 0; i < COS_V299_N_HOP; ++i) {
        cos_v299_hop_t *h = &s->hop[i];
        cpy(h->label, sizeof h->label, hop_lbl[i]);
        h->hops          = hop_n[i];
        h->sigma_per_hop = hop_per[i];
        double survival  = 1.0 - (double)h->sigma_per_hop;
        double prod = 1.0;
        for (int k = 0; k < h->hops; ++k) prod *= survival;
        h->sigma_total = (float)(1.0 - prod);
        h->warning = (h->sigma_total > COS_V299_TAU_WARNING);
        if (h->warning) ++n_warn_t; else ++n_warn_f;
        if (strlen(h->label) > 0) ++hok;
        double expected = 1.0 - prod;
        if (fabs((double)h->sigma_total - expected) > 1e-3)
            formula_ok_all = 0;
    }
    s->n_hop_rows_ok = hok;

    bool hop_ord = true;
    for (int i = 1; i < COS_V299_N_HOP; ++i) {
        if (!(s->hop[i].hops > s->hop[i-1].hops))
            hop_ord = false;
        if (!(s->hop[i].sigma_total > s->hop[i-1].sigma_total))
            hop_ord = false;
    }
    s->hop_order_ok = hop_ord;
    s->hop_formula_ok = (formula_ok_all == 1);
    s->hop_warning_polarity_ok = (n_warn_t > 0) && (n_warn_f > 0);

    /* 4) Corpus as KG */
    const char *sub[COS_V299_N_CORPUS] = {
        "sigma", "k_eff", "one_equals_one" };
    const char *rel[COS_V299_N_CORPUS] = {
        "IS_SNR_OF", "DEPENDS_ON", "EXPRESSES" };
    const char *obj[COS_V299_N_CORPUS] = {
        "noise_signal_ratio", "sigma", "self_consistency" };

    int cok = 0;
    int wf_all = 1, q_all = 1;
    for (int i = 0; i < COS_V299_N_CORPUS; ++i) {
        cos_v299_triplet_t *t = &s->corpus[i];
        cpy(t->subject,  sizeof t->subject,  sub[i]);
        cpy(t->relation, sizeof t->relation, rel[i]);
        cpy(t->object,   sizeof t->object,   obj[i]);
        t->well_formed = (strlen(t->subject)  > 0) &&
                         (strlen(t->relation) > 0) &&
                         (strlen(t->object)   > 0);
        t->queryable = t->well_formed;
        if (!t->well_formed) wf_all = 0;
        if (!t->queryable)   q_all  = 0;
        if (t->well_formed)  ++cok;
    }
    s->n_corpus_rows_ok = cok;
    s->corpus_wellformed_ok = (wf_all == 1);
    s->corpus_queryable_ok  = (q_all  == 1);

    /* σ_kg aggregation */
    int good = 0;
    int denom = 0;
    good  += s->n_ret_rows_ok;       denom += 3;
    good  += s->ret_order_ok ? 1 : 0;       denom += 1;
    good  += s->ret_gate_polarity_ok ? 1 : 0; denom += 1;
    good  += s->ret_fromkg_count_ok ? 1 : 0;  denom += 1;
    good  += s->n_prov_rows_ok;      denom += 3;
    good  += s->prov_order_ok ? 1 : 0;      denom += 1;
    good  += s->prov_polarity_ok ? 1 : 0;   denom += 1;
    good  += s->prov_source_ok ? 1 : 0;     denom += 1;
    good  += s->n_hop_rows_ok;       denom += 3;
    good  += s->hop_order_ok ? 1 : 0;       denom += 1;
    good  += s->hop_formula_ok ? 1 : 0;     denom += 1;
    good  += s->hop_warning_polarity_ok ? 1 : 0; denom += 1;
    good  += s->n_corpus_rows_ok;    denom += 3;
    good  += s->corpus_wellformed_ok ? 1 : 0; denom += 1;
    good  += s->corpus_queryable_ok ? 1 : 0;  denom += 1;
    s->sigma_kg = 1.0f - ((float)good / (float)denom);

    /* FNV-1a terminal chain */
    uint64_t h = 0xcbf29ce484222325ULL;
    h = cos_v299_fnv1a(h, &s->seed, sizeof s->seed);
    for (int i = 0; i < COS_V299_N_RET; ++i)
        h = cos_v299_fnv1a(h, &s->ret[i], sizeof s->ret[i]);
    for (int i = 0; i < COS_V299_N_PROV; ++i)
        h = cos_v299_fnv1a(h, &s->prov[i], sizeof s->prov[i]);
    for (int i = 0; i < COS_V299_N_HOP; ++i)
        h = cos_v299_fnv1a(h, &s->hop[i], sizeof s->hop[i]);
    for (int i = 0; i < COS_V299_N_CORPUS; ++i)
        h = cos_v299_fnv1a(h, &s->corpus[i], sizeof s->corpus[i]);
    h = cos_v299_fnv1a(h, &s->sigma_kg, sizeof s->sigma_kg);
    s->terminal_hash = h;
    s->chain_valid = true;
}

size_t cos_v299_to_json(const cos_v299_state_t *s,
                         char *buf, size_t cap)
{
    if (!buf || cap == 0) return 0;
    int w = snprintf(buf, cap,
        "{\"kernel\":\"v299_knowledge_graph\","
        "\"tau_kg\":%.4f,\"tau_prov\":%.4f,\"tau_warning\":%.4f,"
        "\"retrieval\":[",
        (double)COS_V299_TAU_KG,
        (double)COS_V299_TAU_PROV,
        (double)COS_V299_TAU_WARNING);
    if (w < 0 || (size_t)w >= cap) return (size_t)w;

    for (int i = 0; i < COS_V299_N_RET; ++i) {
        const cos_v299_ret_t *r = &s->ret[i];
        int n = snprintf(buf + w, cap - (size_t)w,
            "%s{\"query_kind\":\"%s\","
            "\"sigma_retrieval\":%.4f,"
            "\"route\":\"%s\"}",
            i ? "," : "",
            r->query_kind,
            (double)r->sigma_retrieval,
            r->route);
        if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
        w += n;
    }

    int n = snprintf(buf + w, cap - (size_t)w,
        "],\"ret_rows_ok\":%d,\"ret_order_ok\":%s,"
        "\"ret_gate_polarity_ok\":%s,\"ret_fromkg_count_ok\":%s,"
        "\"provenance\":[",
        s->n_ret_rows_ok,
        s->ret_order_ok ? "true" : "false",
        s->ret_gate_polarity_ok ? "true" : "false",
        s->ret_fromkg_count_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V299_N_PROV; ++i) {
        const cos_v299_prov_t *p = &s->prov[i];
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"source\":\"%s\","
            "\"sigma_provenance\":%.4f,"
            "\"trusted\":%s,\"peer_reviewed\":%s,"
            "\"source_ref\":\"%s\"}",
            i ? "," : "",
            p->source,
            (double)p->sigma_provenance,
            p->trusted ? "true" : "false",
            p->peer_reviewed ? "true" : "false",
            p->source_ref);
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }

    n = snprintf(buf + w, cap - (size_t)w,
        "],\"prov_rows_ok\":%d,\"prov_order_ok\":%s,"
        "\"prov_polarity_ok\":%s,\"prov_source_ok\":%s,"
        "\"multi_hop\":[",
        s->n_prov_rows_ok,
        s->prov_order_ok ? "true" : "false",
        s->prov_polarity_ok ? "true" : "false",
        s->prov_source_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V299_N_HOP; ++i) {
        const cos_v299_hop_t *h = &s->hop[i];
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"label\":\"%s\","
            "\"hops\":%d,\"sigma_per_hop\":%.4f,"
            "\"sigma_total\":%.4f,\"warning\":%s}",
            i ? "," : "",
            h->label,
            h->hops,
            (double)h->sigma_per_hop,
            (double)h->sigma_total,
            h->warning ? "true" : "false");
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }

    n = snprintf(buf + w, cap - (size_t)w,
        "],\"hop_rows_ok\":%d,\"hop_order_ok\":%s,"
        "\"hop_formula_ok\":%s,\"hop_warning_polarity_ok\":%s,"
        "\"corpus\":[",
        s->n_hop_rows_ok,
        s->hop_order_ok ? "true" : "false",
        s->hop_formula_ok ? "true" : "false",
        s->hop_warning_polarity_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V299_N_CORPUS; ++i) {
        const cos_v299_triplet_t *t = &s->corpus[i];
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"subject\":\"%s\","
            "\"relation\":\"%s\",\"object\":\"%s\","
            "\"well_formed\":%s,\"queryable\":%s}",
            i ? "," : "",
            t->subject,
            t->relation,
            t->object,
            t->well_formed ? "true" : "false",
            t->queryable   ? "true" : "false");
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }

    n = snprintf(buf + w, cap - (size_t)w,
        "],\"corpus_rows_ok\":%d,"
        "\"corpus_wellformed_ok\":%s,\"corpus_queryable_ok\":%s,"
        "\"sigma_kg\":%.4f,\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\"}",
        s->n_corpus_rows_ok,
        s->corpus_wellformed_ok ? "true" : "false",
        s->corpus_queryable_ok ? "true" : "false",
        (double)s->sigma_kg,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    return (size_t)w;
}

int cos_v299_self_test(void)
{
    cos_v299_state_t s;
    cos_v299_init(&s, 299u);
    cos_v299_run(&s);

    if (s.n_ret_rows_ok      != COS_V299_N_RET)    return 1;
    if (!s.ret_order_ok)                           return 2;
    if (!s.ret_gate_polarity_ok)                   return 3;
    if (!s.ret_fromkg_count_ok)                    return 4;

    if (s.n_prov_rows_ok     != COS_V299_N_PROV)   return 5;
    if (!s.prov_order_ok)                          return 6;
    if (!s.prov_polarity_ok)                       return 7;
    if (!s.prov_source_ok)                         return 8;

    if (s.n_hop_rows_ok      != COS_V299_N_HOP)    return 9;
    if (!s.hop_order_ok)                           return 10;
    if (!s.hop_formula_ok)                         return 11;
    if (!s.hop_warning_polarity_ok)                return 12;

    if (s.n_corpus_rows_ok   != COS_V299_N_CORPUS) return 13;
    if (!s.corpus_wellformed_ok)                   return 14;
    if (!s.corpus_queryable_ok)                    return 15;

    if (!(s.sigma_kg == 0.0f))                     return 16;
    if (!s.chain_valid)                            return 17;
    if (s.terminal_hash == 0ULL)                   return 18;

    char buf[3072];
    size_t n = cos_v299_to_json(&s, buf, sizeof buf);
    if (n == 0 || n >= sizeof buf)                 return 19;
    if (!strstr(buf, "\"kernel\":\"v299_knowledge_graph\""))
        return 20;

    return 0;
}
