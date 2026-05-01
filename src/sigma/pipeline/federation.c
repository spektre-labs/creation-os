/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Federation — σ-weighted Δweight aggregation with poison guard. */

#include "federation.h"

#include <math.h>
#include <string.h>

int cos_sigma_fed_init(cos_fed_t *f,
                       cos_fed_update_t *storage, int cap,
                       float sigma_reject, float poison_max_scale)
{
    if (!f || !storage || cap <= 0) return -1;
    if (!(sigma_reject > 0.0f && sigma_reject <= 1.0f))        return -2;
    if (!(poison_max_scale >= 1.0f))                            return -3;
    memset(f, 0, sizeof *f);
    f->slots           = storage;
    f->cap             = cap;
    f->sigma_reject    = sigma_reject;
    f->poison_max_scale = poison_max_scale;
    memset(storage, 0, sizeof(cos_fed_update_t) * (size_t)cap);
    return 0;
}

static float l2_norm(const float *v, int n) {
    double s = 0.0;
    for (int i = 0; i < n; ++i) {
        double x = (double)v[i];
        if (x != x) return NAN;
        s += x * x;
    }
    return (float)sqrt(s);
}

int cos_sigma_fed_add(cos_fed_t *f, const char *id,
                      const float *delta_w, int n_params,
                      float sigma_node, int n_samples)
{
    if (!f || !id || !delta_w || n_params <= 0) return -1;
    if (!(sigma_node >= 0.0f && sigma_node <= 1.0f))            return -2;
    if (f->count >= f->cap)                                      return -3;
    if (n_samples < 0)                                           return -4;
    if (f->count == 0) {
        f->n_params = n_params;
    } else if (f->n_params != n_params) {
        return -5;
    }
    float norm = l2_norm(delta_w, n_params);
    if (norm != norm) return -6;   /* NaN in payload */
    cos_fed_update_t *u = &f->slots[f->count++];
    memset(u, 0, sizeof *u);
    strncpy(u->node_id, id, COS_FED_NODE_ID_CAP - 1);
    u->delta_w    = delta_w;
    u->n_params   = n_params;
    u->sigma_node = sigma_node;
    u->n_samples  = n_samples;
    u->norm_l2    = norm;
    return 0;
}

/* O(n²) median OK for v0; caller populations are N ≈ tens. */
static float median_norm(const cos_fed_update_t *u, int n) {
    float buf[256];
    if (n > 256) n = 256;
    for (int i = 0; i < n; ++i) buf[i] = u[i].norm_l2;
    /* insertion sort */
    for (int i = 1; i < n; ++i) {
        float v = buf[i];
        int j = i - 1;
        while (j >= 0 && buf[j] > v) { buf[j+1] = buf[j]; --j; }
        buf[j+1] = v;
    }
    if (n == 0) return 0.0f;
    return (n & 1) ? buf[n/2] : 0.5f * (buf[n/2 - 1] + buf[n/2]);
}

int cos_sigma_fed_aggregate(cos_fed_t *f,
                            float *out_delta, cos_fed_report_t *out_rep)
{
    if (!f || !out_delta || !out_rep) return -1;
    if (f->count == 0)                 return -2;
    memset(out_rep, 0, sizeof *out_rep);
    for (int j = 0; j < f->n_params; ++j) out_delta[j] = 0.0f;

    /* Pass 1: σ gate + norm collection. */
    float max_norm = 0.0f;
    for (int i = 0; i < f->count; ++i) {
        cos_fed_update_t *u = &f->slots[i];
        u->accepted = 0;
        u->flagged_sigma = 0;
        u->flagged_poison = 0;
        u->weight = 0.0f;
        if (u->sigma_node >= f->sigma_reject) {
            u->flagged_sigma = 1;
            out_rep->n_rejected_sigma++;
            continue;
        }
        if (u->norm_l2 > max_norm) max_norm = u->norm_l2;
        u->accepted = 1;
    }
    out_rep->max_norm = max_norm;

    /* Pass 2: poison gate using median over σ-accepted contributors. */
    {
        cos_fed_update_t tmp[256];
        int m = 0;
        for (int i = 0; i < f->count && m < 256; ++i) {
            if (f->slots[i].accepted) tmp[m++] = f->slots[i];
        }
        float median = median_norm(tmp, m);
        out_rep->median_norm = median;
        float cap = (median <= 0.0f) ? max_norm : median * f->poison_max_scale;
        if (cap <= 0.0f) cap = 1e30f;
        for (int i = 0; i < f->count; ++i) {
            cos_fed_update_t *u = &f->slots[i];
            if (!u->accepted) continue;
            if (u->norm_l2 > cap) {
                u->accepted = 0;
                u->flagged_poison = 1;
                out_rep->n_rejected_poison++;
            }
        }
    }

    /* Pass 3: compute weights, total weight, aggregate. */
    float total_w = 0.0f;
    for (int i = 0; i < f->count; ++i) {
        cos_fed_update_t *u = &f->slots[i];
        if (!u->accepted) continue;
        u->weight = (1.0f - u->sigma_node) * (float)u->n_samples;
        if (u->weight < 0.0f) u->weight = 0.0f;
        total_w += u->weight;
        out_rep->n_accepted++;
    }
    out_rep->total_weight = total_w;

    if (total_w <= 0.0f) {
        out_rep->abstained = 1;
        out_rep->delta_l2_norm = 0.0f;
        return 0;
    }
    for (int i = 0; i < f->count; ++i) {
        cos_fed_update_t *u = &f->slots[i];
        if (!u->accepted) continue;
        float alpha = u->weight / total_w;
        for (int j = 0; j < f->n_params; ++j) {
            out_delta[j] += alpha * u->delta_w[j];
        }
    }
    out_rep->delta_l2_norm = l2_norm(out_delta, f->n_params);
    return 0;
}

/* ---------- self-test ---------- */

static float fabsf6(float x) { return x < 0.0f ? -x : x; }

static int check_guards(void) {
    cos_fed_t f;
    cos_fed_update_t slots[2];
    if (cos_sigma_fed_init(NULL, slots, 2, 0.8f, 3.0f) == 0) return 10;
    if (cos_sigma_fed_init(&f,   slots, 2, 0.0f, 3.0f) == 0) return 11;
    if (cos_sigma_fed_init(&f,   slots, 2, 0.8f, 0.5f) == 0) return 12;
    return 0;
}

static int check_weighted_mean(void) {
    cos_fed_t f;
    cos_fed_update_t slots[2];
    cos_sigma_fed_init(&f, slots, 2, 0.80f, 10.0f);
    float da[3] = { 1.0f, 2.0f, 3.0f };
    float db[3] = { 4.0f, 6.0f, 8.0f };
    /* σ_a=0.10 (trusted, 100 samples), σ_b=0.50 (half-trusted, 50). */
    cos_sigma_fed_add(&f, "a", da, 3, 0.10f, 100);
    cos_sigma_fed_add(&f, "b", db, 3, 0.50f,  50);
    float out[3];
    cos_fed_report_t rep;
    if (cos_sigma_fed_aggregate(&f, out, &rep) != 0) return 20;
    /* w_a = 0.90·100 = 90 ; w_b = 0.50·50 = 25 ; total = 115 */
    float alpha_a = 90.0f / 115.0f;
    float alpha_b = 25.0f / 115.0f;
    float expect[3] = {
        alpha_a*1.0f + alpha_b*4.0f,
        alpha_a*2.0f + alpha_b*6.0f,
        alpha_a*3.0f + alpha_b*8.0f,
    };
    for (int i = 0; i < 3; ++i) {
        if (fabsf6(out[i] - expect[i]) > 1e-4f) return 21;
    }
    if (rep.n_accepted != 2)           return 22;
    if (rep.n_rejected_sigma != 0)     return 23;
    if (rep.n_rejected_poison != 0)    return 24;
    if (rep.abstained)                 return 25;
    return 0;
}

static int check_sigma_gate(void) {
    cos_fed_t f;
    cos_fed_update_t slots[3];
    cos_sigma_fed_init(&f, slots, 3, 0.70f, 10.0f);
    float da[2] = { 1.0f, 1.0f };
    float db[2] = { 10.0f, 10.0f };  /* malicious Δ */
    float dc[2] = { 2.0f, 2.0f };
    cos_sigma_fed_add(&f, "a", da, 2, 0.10f,  50);
    cos_sigma_fed_add(&f, "b", db, 2, 0.95f, 100);  /* σ ≥ 0.70 → rejected */
    cos_sigma_fed_add(&f, "c", dc, 2, 0.20f,  50);
    float out[2]; cos_fed_report_t rep;
    cos_sigma_fed_aggregate(&f, out, &rep);
    if (rep.n_accepted != 2)          return 30;
    if (rep.n_rejected_sigma != 1)    return 31;
    /* Malicious update must not contribute. */
    if (out[0] > 5.0f || out[1] > 5.0f) return 32;
    return 0;
}

static int check_poison_gate(void) {
    cos_fed_t f;
    cos_fed_update_t slots[4];
    cos_sigma_fed_init(&f, slots, 4, 0.80f, 3.0f);
    /* Cohort: norms ≈ 1, 1.2, 1.1; one poison with norm ≈ 100. */
    float d1[2] = { 0.7f, 0.7f };        /* ≈ √0.98 ≈ 0.99 */
    float d2[2] = { 0.9f, 0.6f };        /* ≈ √1.17 ≈ 1.08 */
    float d3[2] = { 0.8f, 0.7f };        /* ≈ √1.13 ≈ 1.06 */
    float d4[2] = { 80.0f, 60.0f };      /* norm 100 → poison  */
    cos_sigma_fed_add(&f, "n1", d1, 2, 0.20f, 50);
    cos_sigma_fed_add(&f, "n2", d2, 2, 0.20f, 50);
    cos_sigma_fed_add(&f, "n3", d3, 2, 0.20f, 50);
    cos_sigma_fed_add(&f, "n4", d4, 2, 0.20f, 50);
    float out[2]; cos_fed_report_t rep;
    cos_sigma_fed_aggregate(&f, out, &rep);
    if (rep.n_rejected_poison != 1)   return 40;
    if (rep.n_accepted != 3)          return 41;
    /* Aggregate should be nowhere near the poison scale. */
    if (out[0] > 2.0f)                return 42;
    return 0;
}

static int check_abstain(void) {
    cos_fed_t f;
    cos_fed_update_t slots[2];
    cos_sigma_fed_init(&f, slots, 2, 0.50f, 10.0f);
    float d1[2] = { 1.0f, 1.0f };
    float d2[2] = { 2.0f, 2.0f };
    cos_sigma_fed_add(&f, "a", d1, 2, 0.90f, 100); /* over σ floor */
    cos_sigma_fed_add(&f, "b", d2, 2, 0.80f, 100); /* over σ floor */
    float out[2]; cos_fed_report_t rep;
    cos_sigma_fed_aggregate(&f, out, &rep);
    if (!rep.abstained)               return 50;
    if (rep.n_accepted != 0)          return 51;
    if (out[0] != 0.0f || out[1] != 0.0f) return 52;
    return 0;
}

int cos_sigma_fed_self_test(void) {
    int rc;
    if ((rc = check_guards()))       return rc;
    if ((rc = check_weighted_mean()))return rc;
    if ((rc = check_sigma_gate()))   return rc;
    if ((rc = check_poison_gate()))  return rc;
    if ((rc = check_abstain()))      return rc;
    return 0;
}
