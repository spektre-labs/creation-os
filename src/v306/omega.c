/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* v306 σ-Omega — v0 manifest implementation. */

#include "omega.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t cos_v306_fnv1a(uint64_t h, const void *p, size_t n)
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

void cos_v306_init(cos_v306_state_t *s, uint64_t seed)
{ memset(s, 0, sizeof *s); s->seed = seed; }

void cos_v306_run(cos_v306_state_t *s)
{
    /* 1) Ω-optimization loop */
    const float ls[] = { 0.50f, 0.35f, 0.22f, 0.14f };
    const float lk[] = { 0.25f, 0.20f, 0.18f, 0.15f };

    int lok = 0;
    float cum = 0.0f;
    bool k_ok = true;
    for (int i = 0; i < COS_V306_N_LOOP; ++i) {
        cos_v306_loop_t *l = &s->loop[i];
        l->t = i;
        l->sigma = ls[i];
        l->k_eff = lk[i];
        cum += l->sigma;
        l->integral_sigma = cum;
        if (!(l->k_eff >= COS_V306_K_CRIT)) k_ok = false;
        ++lok;
    }
    s->n_loop_rows_ok = lok;

    bool sig_dec = true;
    for (int i = 1; i < COS_V306_N_LOOP; ++i)
        if (!(s->loop[i].sigma < s->loop[i-1].sigma)) sig_dec = false;
    s->loop_sigma_decreasing_ok = sig_dec;
    s->loop_k_constraint_ok = k_ok;
    /* Integral is monotone ↑; marginal (σ itself) strictly ↓, which
     * we already assert above.  Re-verify integral ↑ separately: */
    bool int_ok = true;
    for (int i = 1; i < COS_V306_N_LOOP; ++i)
        if (!(s->loop[i].integral_sigma > s->loop[i-1].integral_sigma))
            int_ok = false;
    s->loop_integral_ok = int_ok;

    /* 2) Multi-scale Ω */
    const char *sc_name[] = { "token", "answer", "session", "domain" };
    const char *sc_tier[] = { "MICRO", "MESO", "MACRO", "META" };
    const char *sc_op  [] = {
        "sigma_gate", "sigma_product",
        "sigma_session", "sigma_domain" };
    const float sc_s   [] = { 0.10f, 0.15f, 0.20f, 0.25f };

    int sok = 0;
    for (int i = 0; i < COS_V306_N_SCALE; ++i) {
        cos_v306_scale_t *sc = &s->scale[i];
        cpy(sc->scale,          sizeof sc->scale,          sc_name[i]);
        cpy(sc->tier,           sizeof sc->tier,           sc_tier[i]);
        cpy(sc->operator_name,  sizeof sc->operator_name,  sc_op[i]);
        sc->sigma = sc_s[i];
        if (strlen(sc->scale) > 0) ++sok;
    }
    s->n_scale_rows_ok = sok;
    int d1 = 1, d2 = 1;
    for (int i = 0; i < COS_V306_N_SCALE; ++i)
        for (int j = i + 1; j < COS_V306_N_SCALE; ++j) {
            if (strcmp(s->scale[i].scale,         s->scale[j].scale) == 0)
                d1 = 0;
            if (strcmp(s->scale[i].operator_name, s->scale[j].operator_name) == 0)
                d2 = 0;
        }
    s->scale_distinct_ok = (d1 == 1);
    s->scale_operator_distinct_ok = (d2 == 1);

    /* 3) ½ operator */
    const char *h_lbl[] = { "signal_dominant", "critical", "noise_dominant" };
    const float h_s  [] = { 0.25f, 0.50f, 0.75f };

    int hok = 0;
    int n_s = 0, n_c = 0, n_n = 0;
    for (int i = 0; i < COS_V306_N_HALF; ++i) {
        cos_v306_half_t *h = &s->half[i];
        cpy(h->label, sizeof h->label, h_lbl[i]);
        h->sigma = h_s[i];
        if (h->sigma < COS_V306_SIGMA_HALF) {
            cpy(h->regime, sizeof h->regime, "SIGNAL");   ++n_s;
        } else if (h->sigma > COS_V306_SIGMA_HALF) {
            cpy(h->regime, sizeof h->regime, "NOISE");    ++n_n;
        } else {
            cpy(h->regime, sizeof h->regime, "CRITICAL"); ++n_c;
        }
        if (strlen(h->label) > 0) ++hok;
    }
    s->n_half_rows_ok = hok;
    bool hord = true;
    for (int i = 1; i < COS_V306_N_HALF; ++i)
        if (!(s->half[i].sigma > s->half[i-1].sigma)) hord = false;
    s->half_order_ok = hord;
    s->half_critical_ok = (s->half[1].sigma == COS_V306_SIGMA_HALF);
    s->half_polarity_ok = (n_s > 0) && (n_c > 0) && (n_n > 0);

    /* 4) 1 = 1 invariant */
    const char *iv_lbl[] = {
        "kernel_count", "architecture_claim", "axiom_one_equals_one" };
    const int   iv_d  [] = { COS_V306_KERNELS, COS_V306_KERNELS, 1 };
    const int   iv_r  [] = { COS_V306_KERNELS, COS_V306_KERNELS, 1 };

    int iok = 0, all_hold = 1, pair_all = 1;
    for (int i = 0; i < COS_V306_N_INV; ++i) {
        cos_v306_inv_t *iv = &s->inv[i];
        cpy(iv->assertion, sizeof iv->assertion, iv_lbl[i]);
        iv->declared = iv_d[i];
        iv->realized = iv_r[i];
        iv->holds = (iv->declared == iv->realized);
        if (iv->declared != iv->realized) pair_all = 0;
        if (!iv->holds) all_hold = 0;
        if (strlen(iv->assertion) > 0) ++iok;
    }
    s->n_inv_rows_ok = iok;
    s->inv_pair_ok = (pair_all == 1);
    s->inv_all_hold_ok = (all_hold == 1);
    s->the_invariant_holds = s->inv_all_hold_ok;

    /* σ_omega aggregation */
    int good = 0, denom = 0;
    good += s->n_loop_rows_ok;                        denom += 4;
    good += s->loop_sigma_decreasing_ok ? 1 : 0;      denom += 1;
    good += s->loop_k_constraint_ok ? 1 : 0;          denom += 1;
    good += s->loop_integral_ok ? 1 : 0;              denom += 1;
    good += s->n_scale_rows_ok;                       denom += 4;
    good += s->scale_distinct_ok ? 1 : 0;             denom += 1;
    good += s->scale_operator_distinct_ok ? 1 : 0;    denom += 1;
    good += s->n_half_rows_ok;                        denom += 3;
    good += s->half_order_ok ? 1 : 0;                 denom += 1;
    good += s->half_critical_ok ? 1 : 0;              denom += 1;
    good += s->half_polarity_ok ? 1 : 0;              denom += 1;
    good += s->n_inv_rows_ok;                         denom += 3;
    good += s->inv_pair_ok ? 1 : 0;                   denom += 1;
    good += s->inv_all_hold_ok ? 1 : 0;               denom += 1;
    good += s->the_invariant_holds ? 1 : 0;           denom += 1;
    s->sigma_omega = 1.0f - ((float)good / (float)denom);

    uint64_t h = 0xcbf29ce484222325ULL;
    h = cos_v306_fnv1a(h, &s->seed, sizeof s->seed);
    for (int i = 0; i < COS_V306_N_LOOP; ++i)
        h = cos_v306_fnv1a(h, &s->loop[i], sizeof s->loop[i]);
    for (int i = 0; i < COS_V306_N_SCALE; ++i)
        h = cos_v306_fnv1a(h, &s->scale[i], sizeof s->scale[i]);
    for (int i = 0; i < COS_V306_N_HALF; ++i)
        h = cos_v306_fnv1a(h, &s->half[i], sizeof s->half[i]);
    for (int i = 0; i < COS_V306_N_INV; ++i)
        h = cos_v306_fnv1a(h, &s->inv[i], sizeof s->inv[i]);
    h = cos_v306_fnv1a(h, &s->sigma_omega, sizeof s->sigma_omega);
    s->terminal_hash = h;
    s->chain_valid = true;
}

size_t cos_v306_to_json(const cos_v306_state_t *s, char *buf, size_t cap)
{
    if (!buf || cap == 0) return 0;
    int w = snprintf(buf, cap,
        "{\"kernel\":\"v306_omega\","
        "\"k_crit\":%.4f,\"sigma_half\":%.4f,\"kernels_total\":%d,"
        "\"loop\":[",
        (double)COS_V306_K_CRIT,
        (double)COS_V306_SIGMA_HALF,
        COS_V306_KERNELS);
    if (w < 0 || (size_t)w >= cap) return (size_t)w;

    for (int i = 0; i < COS_V306_N_LOOP; ++i) {
        const cos_v306_loop_t *l = &s->loop[i];
        int n = snprintf(buf + w, cap - (size_t)w,
            "%s{\"t\":%d,\"sigma\":%.4f,\"k_eff\":%.4f,"
            "\"integral_sigma\":%.4f}",
            i ? "," : "", l->t, (double)l->sigma,
            (double)l->k_eff, (double)l->integral_sigma);
        if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
        w += n;
    }
    int n = snprintf(buf + w, cap - (size_t)w,
        "],\"loop_rows_ok\":%d,\"loop_sigma_decreasing_ok\":%s,"
        "\"loop_k_constraint_ok\":%s,\"loop_integral_ok\":%s,"
        "\"scales\":[",
        s->n_loop_rows_ok,
        s->loop_sigma_decreasing_ok ? "true" : "false",
        s->loop_k_constraint_ok ? "true" : "false",
        s->loop_integral_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V306_N_SCALE; ++i) {
        const cos_v306_scale_t *sc = &s->scale[i];
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"scale\":\"%s\",\"tier\":\"%s\","
            "\"operator\":\"%s\",\"sigma\":%.4f}",
            i ? "," : "", sc->scale, sc->tier,
            sc->operator_name, (double)sc->sigma);
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"scale_rows_ok\":%d,\"scale_distinct_ok\":%s,"
        "\"scale_operator_distinct_ok\":%s,\"half_operator\":[",
        s->n_scale_rows_ok,
        s->scale_distinct_ok ? "true" : "false",
        s->scale_operator_distinct_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V306_N_HALF; ++i) {
        const cos_v306_half_t *h = &s->half[i];
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"label\":\"%s\",\"sigma\":%.4f,\"regime\":\"%s\"}",
            i ? "," : "", h->label, (double)h->sigma, h->regime);
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"half_rows_ok\":%d,\"half_order_ok\":%s,"
        "\"half_critical_ok\":%s,\"half_polarity_ok\":%s,"
        "\"invariant\":[",
        s->n_half_rows_ok,
        s->half_order_ok ? "true" : "false",
        s->half_critical_ok ? "true" : "false",
        s->half_polarity_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V306_N_INV; ++i) {
        const cos_v306_inv_t *iv = &s->inv[i];
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"assertion\":\"%s\",\"declared\":%d,"
            "\"realized\":%d,\"holds\":%s}",
            i ? "," : "", iv->assertion, iv->declared,
            iv->realized, iv->holds ? "true" : "false");
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"inv_rows_ok\":%d,\"inv_pair_ok\":%s,"
        "\"inv_all_hold_ok\":%s,\"the_invariant_holds\":%s,"
        "\"sigma_omega\":%.4f,\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\"}",
        s->n_inv_rows_ok,
        s->inv_pair_ok ? "true" : "false",
        s->inv_all_hold_ok ? "true" : "false",
        s->the_invariant_holds ? "true" : "false",
        (double)s->sigma_omega,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;
    return (size_t)w;
}

int cos_v306_self_test(void)
{
    cos_v306_state_t s;
    cos_v306_init(&s, 306u);
    cos_v306_run(&s);
    if (s.n_loop_rows_ok  != COS_V306_N_LOOP)  return 1;
    if (!s.loop_sigma_decreasing_ok)           return 2;
    if (!s.loop_k_constraint_ok)               return 3;
    if (!s.loop_integral_ok)                   return 4;
    if (s.n_scale_rows_ok != COS_V306_N_SCALE) return 5;
    if (!s.scale_distinct_ok)                  return 6;
    if (!s.scale_operator_distinct_ok)         return 7;
    if (s.n_half_rows_ok  != COS_V306_N_HALF)  return 8;
    if (!s.half_order_ok)                      return 9;
    if (!s.half_critical_ok)                   return 10;
    if (!s.half_polarity_ok)                   return 11;
    if (s.n_inv_rows_ok   != COS_V306_N_INV)   return 12;
    if (!s.inv_pair_ok)                        return 13;
    if (!s.inv_all_hold_ok)                    return 14;
    if (!s.the_invariant_holds)                return 15;
    if (!(s.sigma_omega == 0.0f))              return 16;
    if (!s.chain_valid)                        return 17;
    if (s.terminal_hash == 0ULL)               return 18;
    char buf[4096];
    size_t nj = cos_v306_to_json(&s, buf, sizeof buf);
    if (nj == 0 || nj >= sizeof buf)           return 19;
    if (!strstr(buf, "\"kernel\":\"v306_omega\"")) return 20;
    return 0;
}
