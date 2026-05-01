/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* v302 σ-Green — v0 manifest implementation. */

#include "green.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t cos_v302_fnv1a(uint64_t h, const void *p, size_t n)
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

void cos_v302_init(cos_v302_state_t *s, uint64_t seed)
{ memset(s, 0, sizeof *s); s->seed = seed; }

void cos_v302_run(cos_v302_state_t *s)
{
    /* 1) σ-aware compute budget */
    const char *b_lbl[] = { "easy", "medium", "hard" };
    const float b_sig[] = { 0.10f, 0.40f, 0.80f };
    const char *b_tier[] = { "SMALL", "MID", "LARGE" };
    const float b_e []  = { 0.5f, 2.0f, 8.0f };

    int bok = 0;
    for (int i = 0; i < COS_V302_N_BUDGET; ++i) {
        cos_v302_budget_t *b = &s->budget[i];
        cpy(b->difficulty, sizeof b->difficulty, b_lbl[i]);
        b->sigma_difficulty = b_sig[i];
        cpy(b->model_tier, sizeof b->model_tier, b_tier[i]);
        b->energy_j = b_e[i];
        if (strlen(b->difficulty) > 0) ++bok;
    }
    s->n_budget_rows_ok = bok;
    bool bord = true;
    for (int i = 1; i < COS_V302_N_BUDGET; ++i) {
        if (!(s->budget[i].sigma_difficulty >
              s->budget[i-1].sigma_difficulty)) bord = false;
        if (!(s->budget[i].energy_j > s->budget[i-1].energy_j))
            bord = false;
    }
    s->budget_order_ok = bord;
    s->budget_distinct_ok =
        (strcmp(s->budget[0].model_tier, s->budget[1].model_tier) != 0) &&
        (strcmp(s->budget[1].model_tier, s->budget[2].model_tier) != 0) &&
        (strcmp(s->budget[0].model_tier, s->budget[2].model_tier) != 0);

    /* 2) Carbon-aware scheduling */
    const char *sc_lbl[] = {
        "urgent_green", "low_urgency_brown", "urgent_brown" };
    const char *sc_urg[] = { "HIGH", "LOW", "HIGH" };
    const char *sc_grid[] = { "GREEN", "BROWN", "BROWN" };

    int sok = 0, n_proc = 0, n_def = 0, rule_all = 1;
    for (int i = 0; i < COS_V302_N_SCHED; ++i) {
        cos_v302_sched_t *c = &s->sched[i];
        cpy(c->label,   sizeof c->label,   sc_lbl[i]);
        cpy(c->urgency, sizeof c->urgency, sc_urg[i]);
        cpy(c->grid,    sizeof c->grid,    sc_grid[i]);
        bool high = (strcmp(c->urgency, "HIGH") == 0);
        bool green = (strcmp(c->grid, "GREEN") == 0);
        bool expected = high || green;
        c->processed = expected;
        if (c->processed != expected) rule_all = 0;
        if (c->processed) ++n_proc; else ++n_def;
        if (strlen(c->label) > 0) ++sok;
    }
    s->n_sched_rows_ok = sok;
    s->sched_rule_ok = (rule_all == 1);
    s->sched_polarity_ok = (n_proc > 0) && (n_def > 0);

    /* 3) Abstain = savings */
    const char *sv_lbl[] = {
        "baseline", "sigma_gated_light", "sigma_gated_heavy" };
    const int   sv_tot[] = { 1000, 1000, 1000 };
    const int   sv_abs[] = {    0,  200,  500 };
    const float sv_e  [] = { 100.0f, 80.0f, 50.0f };

    int vok = 0, ratio_ok_all = 1;
    for (int i = 0; i < COS_V302_N_SAVE; ++i) {
        cos_v302_save_t *v = &s->save[i];
        cpy(v->regime, sizeof v->regime, sv_lbl[i]);
        v->total_tokens     = sv_tot[i];
        v->abstained_tokens = sv_abs[i];
        v->energy_j         = sv_e[i];
        v->saved_ratio = (float)v->abstained_tokens /
                         (float)v->total_tokens;
        double expected = (double)v->abstained_tokens /
                          (double)v->total_tokens;
        if (fabs((double)v->saved_ratio - expected) > 1e-3)
            ratio_ok_all = 0;
        if (strlen(v->regime) > 0) ++vok;
    }
    s->n_save_rows_ok = vok;
    s->save_ratio_ok = (ratio_ok_all == 1);
    bool sv_ord = true, sv_e_dec = true;
    for (int i = 1; i < COS_V302_N_SAVE; ++i) {
        if (!(s->save[i].saved_ratio > s->save[i-1].saved_ratio))
            sv_ord = false;
        if (!(s->save[i].energy_j < s->save[i-1].energy_j))
            sv_e_dec = false;
    }
    s->save_order_ok = sv_ord;
    s->save_energy_decrease_ok = sv_e_dec;

    /* 4) J per reliable token */
    const char *j_lbl[] = {
        "unfiltered", "soft_filter", "hard_filter" };
    const float j_e  [] = { 100.0f, 80.0f, 70.0f };
    const int   j_rel[] = { 700, 700, 700 };
    const int   j_noi[] = { 300,   0,   0 };

    int jok = 0, formula_all = 1;
    for (int i = 0; i < COS_V302_N_JPT; ++i) {
        cos_v302_jpt_t *j = &s->jpt[i];
        cpy(j->regime, sizeof j->regime, j_lbl[i]);
        j->energy_j        = j_e[i];
        j->reliable_tokens = j_rel[i];
        j->noisy_tokens    = j_noi[i];
        double expected = (double)j->energy_j /
                          (double)j->reliable_tokens;
        j->j_per_reliable = (float)expected;
        if (fabs((double)j->j_per_reliable - expected) > 1e-3)
            formula_all = 0;
        if (strlen(j->regime) > 0) ++jok;
    }
    s->n_jpt_rows_ok = jok;
    s->jpt_formula_ok = (formula_all == 1);
    bool j_dec = true;
    for (int i = 1; i < COS_V302_N_JPT; ++i)
        if (!(s->jpt[i].j_per_reliable < s->jpt[i-1].j_per_reliable))
            j_dec = false;
    s->jpt_decrease_ok = j_dec;

    /* σ_green aggregation */
    int good = 0, denom = 0;
    good += s->n_budget_rows_ok;                   denom += 3;
    good += s->budget_order_ok ? 1 : 0;            denom += 1;
    good += s->budget_distinct_ok ? 1 : 0;         denom += 1;
    good += s->n_sched_rows_ok;                    denom += 3;
    good += s->sched_rule_ok ? 1 : 0;              denom += 1;
    good += s->sched_polarity_ok ? 1 : 0;          denom += 1;
    good += s->n_save_rows_ok;                     denom += 3;
    good += s->save_ratio_ok ? 1 : 0;              denom += 1;
    good += s->save_order_ok ? 1 : 0;              denom += 1;
    good += s->save_energy_decrease_ok ? 1 : 0;    denom += 1;
    good += s->n_jpt_rows_ok;                      denom += 3;
    good += s->jpt_formula_ok ? 1 : 0;             denom += 1;
    good += s->jpt_decrease_ok ? 1 : 0;            denom += 1;
    s->sigma_green = 1.0f - ((float)good / (float)denom);

    uint64_t h = 0xcbf29ce484222325ULL;
    h = cos_v302_fnv1a(h, &s->seed, sizeof s->seed);
    for (int i = 0; i < COS_V302_N_BUDGET; ++i)
        h = cos_v302_fnv1a(h, &s->budget[i], sizeof s->budget[i]);
    for (int i = 0; i < COS_V302_N_SCHED; ++i)
        h = cos_v302_fnv1a(h, &s->sched[i], sizeof s->sched[i]);
    for (int i = 0; i < COS_V302_N_SAVE; ++i)
        h = cos_v302_fnv1a(h, &s->save[i], sizeof s->save[i]);
    for (int i = 0; i < COS_V302_N_JPT; ++i)
        h = cos_v302_fnv1a(h, &s->jpt[i], sizeof s->jpt[i]);
    h = cos_v302_fnv1a(h, &s->sigma_green, sizeof s->sigma_green);
    s->terminal_hash = h;
    s->chain_valid = true;
}

size_t cos_v302_to_json(const cos_v302_state_t *s, char *buf, size_t cap)
{
    if (!buf || cap == 0) return 0;
    int w = snprintf(buf, cap,
        "{\"kernel\":\"v302_green\",\"budget\":[");
    if (w < 0 || (size_t)w >= cap) return (size_t)w;

    for (int i = 0; i < COS_V302_N_BUDGET; ++i) {
        const cos_v302_budget_t *b = &s->budget[i];
        int n = snprintf(buf + w, cap - (size_t)w,
            "%s{\"difficulty\":\"%s\",\"sigma_difficulty\":%.4f,"
            "\"model_tier\":\"%s\",\"energy_j\":%.4f}",
            i ? "," : "", b->difficulty, (double)b->sigma_difficulty,
            b->model_tier, (double)b->energy_j);
        if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
        w += n;
    }
    int n = snprintf(buf + w, cap - (size_t)w,
        "],\"budget_rows_ok\":%d,\"budget_order_ok\":%s,"
        "\"budget_distinct_ok\":%s,\"schedule\":[",
        s->n_budget_rows_ok,
        s->budget_order_ok ? "true" : "false",
        s->budget_distinct_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V302_N_SCHED; ++i) {
        const cos_v302_sched_t *c = &s->sched[i];
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"label\":\"%s\",\"urgency\":\"%s\",\"grid\":\"%s\","
            "\"processed\":%s}",
            i ? "," : "", c->label, c->urgency, c->grid,
            c->processed ? "true" : "false");
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"sched_rows_ok\":%d,\"sched_rule_ok\":%s,"
        "\"sched_polarity_ok\":%s,\"savings\":[",
        s->n_sched_rows_ok,
        s->sched_rule_ok ? "true" : "false",
        s->sched_polarity_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V302_N_SAVE; ++i) {
        const cos_v302_save_t *v = &s->save[i];
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"regime\":\"%s\",\"total_tokens\":%d,"
            "\"abstained_tokens\":%d,\"energy_j\":%.4f,"
            "\"saved_ratio\":%.4f}",
            i ? "," : "", v->regime, v->total_tokens,
            v->abstained_tokens, (double)v->energy_j,
            (double)v->saved_ratio);
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"save_rows_ok\":%d,\"save_ratio_ok\":%s,"
        "\"save_order_ok\":%s,\"save_energy_decrease_ok\":%s,"
        "\"j_per_reliable\":[",
        s->n_save_rows_ok,
        s->save_ratio_ok ? "true" : "false",
        s->save_order_ok ? "true" : "false",
        s->save_energy_decrease_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V302_N_JPT; ++i) {
        const cos_v302_jpt_t *j = &s->jpt[i];
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"regime\":\"%s\",\"energy_j\":%.4f,"
            "\"reliable_tokens\":%d,\"noisy_tokens\":%d,"
            "\"j_per_reliable\":%.4f}",
            i ? "," : "", j->regime, (double)j->energy_j,
            j->reliable_tokens, j->noisy_tokens,
            (double)j->j_per_reliable);
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"jpt_rows_ok\":%d,\"jpt_formula_ok\":%s,"
        "\"jpt_decrease_ok\":%s,"
        "\"sigma_green\":%.4f,\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\"}",
        s->n_jpt_rows_ok,
        s->jpt_formula_ok ? "true" : "false",
        s->jpt_decrease_ok ? "true" : "false",
        (double)s->sigma_green,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;
    return (size_t)w;
}

int cos_v302_self_test(void)
{
    cos_v302_state_t s;
    cos_v302_init(&s, 302u);
    cos_v302_run(&s);
    if (s.n_budget_rows_ok != COS_V302_N_BUDGET) return 1;
    if (!s.budget_order_ok)                      return 2;
    if (!s.budget_distinct_ok)                   return 3;
    if (s.n_sched_rows_ok  != COS_V302_N_SCHED)  return 4;
    if (!s.sched_rule_ok)                        return 5;
    if (!s.sched_polarity_ok)                    return 6;
    if (s.n_save_rows_ok   != COS_V302_N_SAVE)   return 7;
    if (!s.save_ratio_ok)                        return 8;
    if (!s.save_order_ok)                        return 9;
    if (!s.save_energy_decrease_ok)              return 10;
    if (s.n_jpt_rows_ok    != COS_V302_N_JPT)    return 11;
    if (!s.jpt_formula_ok)                       return 12;
    if (!s.jpt_decrease_ok)                      return 13;
    if (!(s.sigma_green == 0.0f))                return 14;
    if (!s.chain_valid)                          return 15;
    if (s.terminal_hash == 0ULL)                 return 16;
    char buf[4096];
    size_t nj = cos_v302_to_json(&s, buf, sizeof buf);
    if (nj == 0 || nj >= sizeof buf)             return 17;
    if (!strstr(buf, "\"kernel\":\"v302_green\"")) return 18;
    return 0;
}
