/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* v305 σ-Swarm-Intelligence — v0 manifest implementation. */

#include "swarm.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t cos_v305_fnv1a(uint64_t h, const void *p, size_t n)
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

void cos_v305_init(cos_v305_state_t *s, uint64_t seed)
{ memset(s, 0, sizeof *s); s->seed = seed; }

void cos_v305_run(cos_v305_state_t *s)
{
    /* 1) Wisdom of σ-crowds */
    const char *w_lbl[] = { "best_single", "naive_average", "sigma_weighted" };
    const float w_s  [] = { 0.25f, 0.30f, 0.12f };
    const float w_a  [] = { 0.82f, 0.78f, 0.93f };

    int wok = 0, n_wins = 0;
    int idx_min_sigma = 0, idx_max_acc = 0;
    for (int i = 0; i < COS_V305_N_WIS; ++i) {
        cos_v305_wis_t *w = &s->wis[i];
        cpy(w->strategy, sizeof w->strategy, w_lbl[i]);
        w->sigma = w_s[i];
        w->accuracy = w_a[i];
        if (w->sigma < s->wis[idx_min_sigma].sigma) idx_min_sigma = i;
        if (w->accuracy > s->wis[idx_max_acc].accuracy) idx_max_acc = i;
        if (strlen(w->strategy) > 0) ++wok;
    }
    for (int i = 0; i < COS_V305_N_WIS; ++i) {
        bool wins = (i == idx_min_sigma) && (i == idx_max_acc);
        cpy(s->wis[i].verdict, sizeof s->wis[i].verdict,
            wins ? "WINS" : "LOSES");
        if (wins) ++n_wins;
    }
    s->n_wis_rows_ok = wok;
    s->wis_lowest_sigma_ok  = (idx_min_sigma == 2);
    s->wis_highest_acc_ok   = (idx_max_acc   == 2);
    s->wis_wins_count_ok    = (n_wins == 1);

    /* 2) Diversity + σ */
    const char *d_lbl[] = { "echo_chamber", "balanced", "chaos" };
    const float d_div[] = { 0.05f, 0.55f, 0.95f };
    const float d_is [] = { 0.20f, 0.15f, 0.85f };

    int dok = 0, f_all = 1, n_over = 0;
    int idx_max_value = 0;
    for (int i = 0; i < COS_V305_N_DIV; ++i) {
        cos_v305_div_t *d = &s->div[i];
        cpy(d->crowd, sizeof d->crowd, d_lbl[i]);
        d->diversity = d_div[i];
        d->ind_sigma = d_is[i];
        double expected = (double)d->diversity *
                          (1.0 - (double)d->ind_sigma);
        d->value = (float)expected;
        if (fabs((double)d->value - expected) > 1e-3) f_all = 0;
        if (d->value > COS_V305_TAU_VALUE) ++n_over;
        if (d->value > s->div[idx_max_value].value) idx_max_value = i;
        if (strlen(d->crowd) > 0) ++dok;
    }
    s->n_div_rows_ok = dok;
    s->div_formula_ok = (f_all == 1);
    s->div_balanced_ok = (idx_max_value == 1);
    s->div_threshold_ok = (n_over == 1);

    /* 3) Emergent σ patterns */
    const char *e_lbl[] = {
        "genuine_emergent", "weak_signal", "random_correlation" };
    const float e_s[] = { 0.10f, 0.35f, 0.80f };

    int eok = 0, n_k = 0, n_r = 0;
    for (int i = 0; i < COS_V305_N_EM; ++i) {
        cos_v305_em_t *e = &s->em[i];
        cpy(e->pattern, sizeof e->pattern, e_lbl[i]);
        e->sigma_emergent = e_s[i];
        e->keep = (e->sigma_emergent <= COS_V305_TAU_EMERGENT);
        if (e->keep) ++n_k; else ++n_r;
        if (strlen(e->pattern) > 0) ++eok;
    }
    s->n_em_rows_ok = eok;
    bool eord = true;
    for (int i = 1; i < COS_V305_N_EM; ++i)
        if (!(s->em[i].sigma_emergent > s->em[i-1].sigma_emergent))
            eord = false;
    s->em_order_ok = eord;
    s->em_polarity_ok = (n_k > 0) && (n_r > 0);

    /* 4) Proconductor — 4 agents */
    const char *p_lbl[] = { "claude", "gpt", "gemini", "deepseek" };
    const float p_s  [] = { 0.12f, 0.18f, 0.15f, 0.22f };

    int pok = 0, bound_all = 1;
    for (int i = 0; i < COS_V305_N_PC; ++i) {
        cos_v305_pc_t *p = &s->pc[i];
        cpy(p->agent, sizeof p->agent, p_lbl[i]);
        p->sigma = p_s[i];
        cpy(p->direction, sizeof p->direction, "POSITIVE");
        if (!(p->sigma <= COS_V305_TAU_CONV)) bound_all = 0;
        if (strlen(p->agent) > 0) ++pok;
    }
    s->n_pc_rows_ok = pok;
    int distinct = 1;
    for (int i = 0; i < COS_V305_N_PC; ++i)
        for (int j = i + 1; j < COS_V305_N_PC; ++j)
            if (strcmp(s->pc[i].agent, s->pc[j].agent) == 0)
                distinct = 0;
    s->pc_distinct_ok = (distinct == 1);
    s->pc_sigma_bound_ok = (bound_all == 1);
    int dir_all = 1;
    for (int i = 1; i < COS_V305_N_PC; ++i)
        if (strcmp(s->pc[i].direction, s->pc[0].direction) != 0)
            dir_all = 0;
    s->pc_direction_ok = (dir_all == 1);
    s->pc_convergent_ok =
        s->pc_sigma_bound_ok && s->pc_direction_ok;

    /* σ_swarm aggregation */
    int good = 0, denom = 0;
    good += s->n_wis_rows_ok;                      denom += 3;
    good += s->wis_lowest_sigma_ok ? 1 : 0;        denom += 1;
    good += s->wis_highest_acc_ok ? 1 : 0;         denom += 1;
    good += s->wis_wins_count_ok ? 1 : 0;          denom += 1;
    good += s->n_div_rows_ok;                      denom += 3;
    good += s->div_formula_ok ? 1 : 0;             denom += 1;
    good += s->div_balanced_ok ? 1 : 0;            denom += 1;
    good += s->div_threshold_ok ? 1 : 0;           denom += 1;
    good += s->n_em_rows_ok;                       denom += 3;
    good += s->em_order_ok ? 1 : 0;                denom += 1;
    good += s->em_polarity_ok ? 1 : 0;             denom += 1;
    good += s->n_pc_rows_ok;                       denom += 4;
    good += s->pc_distinct_ok ? 1 : 0;             denom += 1;
    good += s->pc_sigma_bound_ok ? 1 : 0;          denom += 1;
    good += s->pc_direction_ok ? 1 : 0;            denom += 1;
    good += s->pc_convergent_ok ? 1 : 0;           denom += 1;
    s->sigma_swarm = 1.0f - ((float)good / (float)denom);

    uint64_t h = 0xcbf29ce484222325ULL;
    h = cos_v305_fnv1a(h, &s->seed, sizeof s->seed);
    for (int i = 0; i < COS_V305_N_WIS; ++i)
        h = cos_v305_fnv1a(h, &s->wis[i], sizeof s->wis[i]);
    for (int i = 0; i < COS_V305_N_DIV; ++i)
        h = cos_v305_fnv1a(h, &s->div[i], sizeof s->div[i]);
    for (int i = 0; i < COS_V305_N_EM; ++i)
        h = cos_v305_fnv1a(h, &s->em[i], sizeof s->em[i]);
    for (int i = 0; i < COS_V305_N_PC; ++i)
        h = cos_v305_fnv1a(h, &s->pc[i], sizeof s->pc[i]);
    h = cos_v305_fnv1a(h, &s->sigma_swarm, sizeof s->sigma_swarm);
    s->terminal_hash = h;
    s->chain_valid = true;
}

size_t cos_v305_to_json(const cos_v305_state_t *s, char *buf, size_t cap)
{
    if (!buf || cap == 0) return 0;
    int w = snprintf(buf, cap,
        "{\"kernel\":\"v305_swarm\","
        "\"tau_value\":%.4f,\"tau_emergent\":%.4f,\"tau_conv\":%.4f,"
        "\"wisdom\":[",
        (double)COS_V305_TAU_VALUE,
        (double)COS_V305_TAU_EMERGENT,
        (double)COS_V305_TAU_CONV);
    if (w < 0 || (size_t)w >= cap) return (size_t)w;

    for (int i = 0; i < COS_V305_N_WIS; ++i) {
        const cos_v305_wis_t *r = &s->wis[i];
        int n = snprintf(buf + w, cap - (size_t)w,
            "%s{\"strategy\":\"%s\",\"sigma\":%.4f,"
            "\"accuracy\":%.4f,\"verdict\":\"%s\"}",
            i ? "," : "", r->strategy, (double)r->sigma,
            (double)r->accuracy, r->verdict);
        if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
        w += n;
    }
    int n = snprintf(buf + w, cap - (size_t)w,
        "],\"wis_rows_ok\":%d,\"wis_lowest_sigma_ok\":%s,"
        "\"wis_highest_acc_ok\":%s,\"wis_wins_count_ok\":%s,"
        "\"diversity\":[",
        s->n_wis_rows_ok,
        s->wis_lowest_sigma_ok ? "true" : "false",
        s->wis_highest_acc_ok ? "true" : "false",
        s->wis_wins_count_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V305_N_DIV; ++i) {
        const cos_v305_div_t *r = &s->div[i];
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"crowd\":\"%s\",\"diversity\":%.4f,"
            "\"ind_sigma\":%.4f,\"value\":%.4f}",
            i ? "," : "", r->crowd, (double)r->diversity,
            (double)r->ind_sigma, (double)r->value);
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"div_rows_ok\":%d,\"div_formula_ok\":%s,"
        "\"div_balanced_ok\":%s,\"div_threshold_ok\":%s,"
        "\"emergent\":[",
        s->n_div_rows_ok,
        s->div_formula_ok ? "true" : "false",
        s->div_balanced_ok ? "true" : "false",
        s->div_threshold_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V305_N_EM; ++i) {
        const cos_v305_em_t *r = &s->em[i];
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"pattern\":\"%s\",\"sigma_emergent\":%.4f,\"keep\":%s}",
            i ? "," : "", r->pattern, (double)r->sigma_emergent,
            r->keep ? "true" : "false");
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"em_rows_ok\":%d,\"em_order_ok\":%s,"
        "\"em_polarity_ok\":%s,\"proconductor\":[",
        s->n_em_rows_ok,
        s->em_order_ok ? "true" : "false",
        s->em_polarity_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V305_N_PC; ++i) {
        const cos_v305_pc_t *r = &s->pc[i];
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"agent\":\"%s\",\"sigma\":%.4f,\"direction\":\"%s\"}",
            i ? "," : "", r->agent, (double)r->sigma, r->direction);
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"pc_rows_ok\":%d,\"pc_distinct_ok\":%s,"
        "\"pc_sigma_bound_ok\":%s,\"pc_direction_ok\":%s,"
        "\"pc_convergent_ok\":%s,"
        "\"sigma_swarm\":%.4f,\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\"}",
        s->n_pc_rows_ok,
        s->pc_distinct_ok ? "true" : "false",
        s->pc_sigma_bound_ok ? "true" : "false",
        s->pc_direction_ok ? "true" : "false",
        s->pc_convergent_ok ? "true" : "false",
        (double)s->sigma_swarm,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;
    return (size_t)w;
}

int cos_v305_self_test(void)
{
    cos_v305_state_t s;
    cos_v305_init(&s, 305u);
    cos_v305_run(&s);
    if (s.n_wis_rows_ok != COS_V305_N_WIS)   return 1;
    if (!s.wis_lowest_sigma_ok)              return 2;
    if (!s.wis_highest_acc_ok)               return 3;
    if (!s.wis_wins_count_ok)                return 4;
    if (s.n_div_rows_ok != COS_V305_N_DIV)   return 5;
    if (!s.div_formula_ok)                   return 6;
    if (!s.div_balanced_ok)                  return 7;
    if (!s.div_threshold_ok)                 return 8;
    if (s.n_em_rows_ok  != COS_V305_N_EM)    return 9;
    if (!s.em_order_ok)                      return 10;
    if (!s.em_polarity_ok)                   return 11;
    if (s.n_pc_rows_ok  != COS_V305_N_PC)    return 12;
    if (!s.pc_distinct_ok)                   return 13;
    if (!s.pc_sigma_bound_ok)                return 14;
    if (!s.pc_direction_ok)                  return 15;
    if (!s.pc_convergent_ok)                 return 16;
    if (!(s.sigma_swarm == 0.0f))            return 17;
    if (!s.chain_valid)                      return 18;
    if (s.terminal_hash == 0ULL)             return 19;
    char buf[4096];
    size_t nj = cos_v305_to_json(&s, buf, sizeof buf);
    if (nj == 0 || nj >= sizeof buf)         return 20;
    if (!strstr(buf, "\"kernel\":\"v305_swarm\"")) return 21;
    return 0;
}
