/*
 * v296 σ-Antifragile — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "antifragile.h"

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

static const struct { const char *lab; float stress; float sigma; }
    STRESS_ROWS[COS_V296_N_STRESS] = {
    { "cycle_1", 1.0f, 0.50f },
    { "cycle_2", 2.0f, 0.35f },
    { "cycle_3", 3.0f, 0.25f },
};

static const struct { const char *reg; float m; float sd;
                      const char *trend; const char *cls; }
    VOL_ROWS[COS_V296_N_VOL] = {
    { "unstable",    0.40f, 0.20f, "NONE",       "UNSTABLE"    },
    { "stable",      0.30f, 0.02f, "NONE",       "STABLE"      },
    { "antifragile", 0.30f, 0.08f, "DECREASING", "ANTIFRAGILE" },
};

static const struct { const char *lab; float noise; bool vac; bool trained; }
    VAC_ROWS[COS_V296_N_VAC] = {
    { "dose_small",  0.10f, true,  false },
    { "dose_medium", 0.30f, true,  false },
    { "real_attack", 0.80f, false, true  },
};

static const struct { const char *lab; float share; float tau; bool kept; }
    BB_ROWS[COS_V296_N_BB] = {
    { "safe_mode",         0.90f, 0.15f, true  },
    { "experimental_mode", 0.10f, 0.70f, true  },
    { "middle_compromise", 0.00f, 0.40f, false },
};

static bool vol_expected(const char *cls, float sd, const char *trend) {
    bool decreasing = (strcmp(trend, "DECREASING") == 0);
    if (strcmp(cls, "ANTIFRAGILE") == 0)
        return sd > COS_V296_STD_STABILITY && decreasing;
    if (strcmp(cls, "STABLE") == 0)
        return sd <= COS_V296_STD_STABILITY && !decreasing;
    if (strcmp(cls, "UNSTABLE") == 0)
        return sd > COS_V296_STD_UNSTABLE && !decreasing;
    return false;
}

void cos_v296_init(cos_v296_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x296afeULL;
}

void cos_v296_run(cos_v296_state_t *s) {
    uint64_t prev = 0x296afe00ULL;

    s->n_stress_rows_ok = 0;
    bool stress_mono = true;
    float last_stress = -1.0f, last_sigma = 2.0f;
    for (int i = 0; i < COS_V296_N_STRESS; ++i) {
        cos_v296_stress_t *r = &s->stress[i];
        memset(r, 0, sizeof(*r));
        cpy(r->label, sizeof(r->label), STRESS_ROWS[i].lab);
        r->stress_level = STRESS_ROWS[i].stress;
        r->sigma_after  = STRESS_ROWS[i].sigma;
        if (strlen(r->label) > 0) s->n_stress_rows_ok++;
        if (i > 0) {
            if (!(r->stress_level > last_stress &&
                  r->sigma_after  < last_sigma))
                stress_mono = false;
        }
        last_stress = r->stress_level;
        last_sigma  = r->sigma_after;
        prev = fnv1a(r->label, strlen(r->label), prev);
        prev = fnv1a(&r->stress_level, sizeof(r->stress_level), prev);
        prev = fnv1a(&r->sigma_after,  sizeof(r->sigma_after),  prev);
    }
    s->stress_monotone_ok = stress_mono;

    s->n_vol_rows_ok = 0;
    bool vol_classify = true;
    int n_cls_distinct = 0;
    const char *seen[COS_V296_N_VOL] = { NULL, NULL, NULL };
    for (int i = 0; i < COS_V296_N_VOL; ++i) {
        cos_v296_vol_t *r = &s->vol[i];
        memset(r, 0, sizeof(*r));
        cpy(r->regime,         sizeof(r->regime),         VOL_ROWS[i].reg);
        cpy(r->trend,          sizeof(r->trend),          VOL_ROWS[i].trend);
        cpy(r->classification, sizeof(r->classification), VOL_ROWS[i].cls);
        r->sigma_mean = VOL_ROWS[i].m;
        r->sigma_std  = VOL_ROWS[i].sd;
        if (strlen(r->regime) > 0) s->n_vol_rows_ok++;
        if (!vol_expected(r->classification, r->sigma_std, r->trend))
            vol_classify = false;
        bool dup = false;
        for (int k = 0; k < i; ++k)
            if (seen[k] && strcmp(seen[k], r->classification) == 0) dup = true;
        if (!dup) { seen[i] = VOL_ROWS[i].cls; n_cls_distinct++; }
        prev = fnv1a(r->regime, strlen(r->regime), prev);
        prev = fnv1a(&r->sigma_mean, sizeof(r->sigma_mean), prev);
        prev = fnv1a(&r->sigma_std,  sizeof(r->sigma_std),  prev);
        prev = fnv1a(r->trend,          strlen(r->trend),          prev);
        prev = fnv1a(r->classification, strlen(r->classification), prev);
    }
    s->vol_classify_ok = (vol_classify && n_cls_distinct == COS_V296_N_VOL);

    s->n_vac_rows_ok = 0;
    bool vac_order = true;
    int n_survived = 0, n_vac = 0, n_trained_real = 0;
    float last_noise = -1.0f;
    for (int i = 0; i < COS_V296_N_VAC; ++i) {
        cos_v296_vac_t *r = &s->vac[i];
        memset(r, 0, sizeof(*r));
        cpy(r->label, sizeof(r->label), VAC_ROWS[i].lab);
        r->noise           = VAC_ROWS[i].noise;
        r->is_vaccine      = VAC_ROWS[i].vac;
        r->because_trained = VAC_ROWS[i].trained;
        r->survived        = true;
        if (strlen(r->label) > 0) s->n_vac_rows_ok++;
        if (i > 0 && !(r->noise > last_noise)) vac_order = false;
        last_noise = r->noise;
        if (r->survived)   n_survived++;
        if (r->is_vaccine) n_vac++;
        else if (r->because_trained) n_trained_real++;
        prev = fnv1a(r->label, strlen(r->label), prev);
        prev = fnv1a(&r->noise,           sizeof(r->noise),           prev);
        prev = fnv1a(&r->is_vaccine,      sizeof(r->is_vaccine),      prev);
        prev = fnv1a(&r->survived,        sizeof(r->survived),        prev);
        prev = fnv1a(&r->because_trained, sizeof(r->because_trained), prev);
    }
    s->vac_noise_order_ok   = vac_order;
    s->vac_survived_ok      = (n_survived == COS_V296_N_VAC);
    s->vac_vaccine_count_ok = (n_vac == 2);
    s->vac_trained_ok       = (n_trained_real == 1);

    s->n_bb_rows_ok = 0;
    float share_safe = 0.0f, share_exp = 0.0f, share_mid = 0.0f;
    float tau_safe = -1.0f, tau_exp = -1.0f;
    bool kept_polarity = true;
    for (int i = 0; i < COS_V296_N_BB; ++i) {
        cos_v296_bb_t *r = &s->bb[i];
        memset(r, 0, sizeof(*r));
        cpy(r->allocation, sizeof(r->allocation), BB_ROWS[i].lab);
        r->share = BB_ROWS[i].share;
        r->tau   = BB_ROWS[i].tau;
        r->kept  = BB_ROWS[i].kept;
        if (strlen(r->allocation) > 0) s->n_bb_rows_ok++;
        if (strcmp(r->allocation, "safe_mode") == 0) {
            share_safe = r->share; tau_safe = r->tau;
            if (!r->kept) kept_polarity = false;
        } else if (strcmp(r->allocation, "experimental_mode") == 0) {
            share_exp = r->share; tau_exp = r->tau;
            if (!r->kept) kept_polarity = false;
        } else if (strcmp(r->allocation, "middle_compromise") == 0) {
            share_mid = r->share;
            if (r->kept) kept_polarity = false;
        }
        prev = fnv1a(r->allocation, strlen(r->allocation), prev);
        prev = fnv1a(&r->share, sizeof(r->share), prev);
        prev = fnv1a(&r->tau,   sizeof(r->tau),   prev);
        prev = fnv1a(&r->kept,  sizeof(r->kept),  prev);
    }
    s->bb_share_sum_ok     = (fabsf(share_safe + share_exp - 1.0f) <= 1e-3f);
    s->bb_middle_zero_ok   = (fabsf(share_mid) <= 1e-3f);
    s->bb_tau_order_ok     = (tau_safe >= 0.0f && tau_exp >= 0.0f &&
                              tau_safe < tau_exp);
    s->bb_kept_polarity_ok = kept_polarity;

    int total   = COS_V296_N_STRESS + 1 +
                  COS_V296_N_VOL    + 1 +
                  COS_V296_N_VAC    + 1 + 1 + 1 + 1 +
                  COS_V296_N_BB     + 1 + 1 + 1 + 1;
    int passing = s->n_stress_rows_ok +
                  (s->stress_monotone_ok ? 1 : 0) +
                  s->n_vol_rows_ok +
                  (s->vol_classify_ok ? 1 : 0) +
                  s->n_vac_rows_ok +
                  (s->vac_noise_order_ok   ? 1 : 0) +
                  (s->vac_survived_ok      ? 1 : 0) +
                  (s->vac_vaccine_count_ok ? 1 : 0) +
                  (s->vac_trained_ok       ? 1 : 0) +
                  s->n_bb_rows_ok +
                  (s->bb_share_sum_ok     ? 1 : 0) +
                  (s->bb_middle_zero_ok   ? 1 : 0) +
                  (s->bb_tau_order_ok     ? 1 : 0) +
                  (s->bb_kept_polarity_ok ? 1 : 0);
    s->sigma_antifragile = 1.0f - ((float)passing / (float)total);
    if (s->sigma_antifragile < 0.0f) s->sigma_antifragile = 0.0f;
    if (s->sigma_antifragile > 1.0f) s->sigma_antifragile = 1.0f;

    struct { int ns, nv, nc, nb;
             bool sm, vc, vo, vs, vvc, vt, bs, bm, bt, bk;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.ns = s->n_stress_rows_ok;
    trec.nv = s->n_vol_rows_ok;
    trec.nc = s->n_vac_rows_ok;
    trec.nb = s->n_bb_rows_ok;
    trec.sm = s->stress_monotone_ok;
    trec.vc = s->vol_classify_ok;
    trec.vo = s->vac_noise_order_ok;
    trec.vs = s->vac_survived_ok;
    trec.vvc = s->vac_vaccine_count_ok;
    trec.vt = s->vac_trained_ok;
    trec.bs = s->bb_share_sum_ok;
    trec.bm = s->bb_middle_zero_ok;
    trec.bt = s->bb_tau_order_ok;
    trec.bk = s->bb_kept_polarity_ok;
    trec.sigma = s->sigma_antifragile;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v296_to_json(const cos_v296_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v296\","
        "\"n_stress_rows_ok\":%d,\"stress_monotone_ok\":%s,"
        "\"n_vol_rows_ok\":%d,\"vol_classify_ok\":%s,"
        "\"n_vac_rows_ok\":%d,\"vac_noise_order_ok\":%s,"
        "\"vac_survived_ok\":%s,\"vac_vaccine_count_ok\":%s,"
        "\"vac_trained_ok\":%s,"
        "\"n_bb_rows_ok\":%d,\"bb_share_sum_ok\":%s,"
        "\"bb_middle_zero_ok\":%s,\"bb_tau_order_ok\":%s,"
        "\"bb_kept_polarity_ok\":%s,"
        "\"sigma_antifragile\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"stress\":[",
        s->n_stress_rows_ok,
        s->stress_monotone_ok ? "true" : "false",
        s->n_vol_rows_ok,
        s->vol_classify_ok ? "true" : "false",
        s->n_vac_rows_ok,
        s->vac_noise_order_ok   ? "true" : "false",
        s->vac_survived_ok      ? "true" : "false",
        s->vac_vaccine_count_ok ? "true" : "false",
        s->vac_trained_ok       ? "true" : "false",
        s->n_bb_rows_ok,
        s->bb_share_sum_ok     ? "true" : "false",
        s->bb_middle_zero_ok   ? "true" : "false",
        s->bb_tau_order_ok     ? "true" : "false",
        s->bb_kept_polarity_ok ? "true" : "false",
        s->sigma_antifragile,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V296_N_STRESS; ++i) {
        const cos_v296_stress_t *r = &s->stress[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"label\":\"%s\",\"stress_level\":%.4f,"
            "\"sigma_after\":%.4f}",
            i == 0 ? "" : ",", r->label,
            r->stress_level, r->sigma_after);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"vol\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V296_N_VOL; ++i) {
        const cos_v296_vol_t *r = &s->vol[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"regime\":\"%s\",\"sigma_mean\":%.4f,"
            "\"sigma_std\":%.4f,\"trend\":\"%s\","
            "\"classification\":\"%s\"}",
            i == 0 ? "" : ",", r->regime,
            r->sigma_mean, r->sigma_std, r->trend, r->classification);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"vac\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V296_N_VAC; ++i) {
        const cos_v296_vac_t *r = &s->vac[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"label\":\"%s\",\"noise\":%.4f,"
            "\"is_vaccine\":%s,\"survived\":%s,"
            "\"because_trained\":%s}",
            i == 0 ? "" : ",", r->label, r->noise,
            r->is_vaccine      ? "true" : "false",
            r->survived        ? "true" : "false",
            r->because_trained ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"bb\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V296_N_BB; ++i) {
        const cos_v296_bb_t *r = &s->bb[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"allocation\":\"%s\",\"share\":%.4f,"
            "\"tau\":%.4f,\"kept\":%s}",
            i == 0 ? "" : ",", r->allocation,
            r->share, r->tau, r->kept ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v296_self_test(void) {
    cos_v296_state_t s;
    cos_v296_init(&s, 0x296afeULL);
    cos_v296_run(&s);
    if (!s.chain_valid) return 1;

    static const char *WANT_S[COS_V296_N_STRESS] = {
        "cycle_1","cycle_2","cycle_3"
    };
    for (int i = 0; i < COS_V296_N_STRESS; ++i) {
        if (strcmp(s.stress[i].label, WANT_S[i]) != 0) return 2;
    }
    if (s.n_stress_rows_ok != COS_V296_N_STRESS) return 3;
    if (!s.stress_monotone_ok) return 4;

    static const char *WANT_V[COS_V296_N_VOL] = {
        "unstable","stable","antifragile"
    };
    for (int i = 0; i < COS_V296_N_VOL; ++i) {
        if (strcmp(s.vol[i].regime, WANT_V[i]) != 0) return 5;
    }
    if (s.n_vol_rows_ok != COS_V296_N_VOL) return 6;
    if (!s.vol_classify_ok) return 7;

    static const char *WANT_C[COS_V296_N_VAC] = {
        "dose_small","dose_medium","real_attack"
    };
    for (int i = 0; i < COS_V296_N_VAC; ++i) {
        if (strcmp(s.vac[i].label, WANT_C[i]) != 0) return 8;
        if (!s.vac[i].survived) return 9;
    }
    if (s.n_vac_rows_ok != COS_V296_N_VAC) return 10;
    if (!s.vac_noise_order_ok)   return 11;
    if (!s.vac_survived_ok)      return 12;
    if (!s.vac_vaccine_count_ok) return 13;
    if (!s.vac_trained_ok)       return 14;
    if (!s.vac[2].because_trained) return 15;

    static const char *WANT_B[COS_V296_N_BB] = {
        "safe_mode","experimental_mode","middle_compromise"
    };
    for (int i = 0; i < COS_V296_N_BB; ++i) {
        if (strcmp(s.bb[i].allocation, WANT_B[i]) != 0) return 16;
    }
    if (s.n_bb_rows_ok != COS_V296_N_BB) return 17;
    if (!s.bb_share_sum_ok)     return 18;
    if (!s.bb_middle_zero_ok)   return 19;
    if (!s.bb_tau_order_ok)     return 20;
    if (!s.bb_kept_polarity_ok) return 21;
    if (s.bb[2].kept) return 22;

    if (s.sigma_antifragile < 0.0f || s.sigma_antifragile > 1.0f) return 23;
    if (s.sigma_antifragile > 1e-6f) return 24;

    cos_v296_state_t u;
    cos_v296_init(&u, 0x296afeULL);
    cos_v296_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 25;
    return 0;
}
