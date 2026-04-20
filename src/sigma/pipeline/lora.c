/*
 * σ-LoRA — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "lora.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================================================================
 * Helpers
 * ================================================================== */
static void lncpy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

/* Deterministic 64-bit LCG (Knuth / MMIX constants). */
static uint64_t lcg_next(uint64_t *s) {
    *s = (*s) * 6364136223846793005ULL + 1442695040888963407ULL;
    return *s;
}

/* Uniform in (-1, 1). */
static float lcg_uniform(uint64_t *s) {
    uint64_t r = lcg_next(s);
    double u = (double)(r >> 11) / (double)((uint64_t)1 << 53);
    return (float)(2.0 * u - 1.0);
}

/* ==================================================================
 * Lifecycle
 * ================================================================== */
int cos_lora_init(cos_lora_adapter_t *a,
                  const char *name,
                  int in_dim, int out_dim, int rank,
                  float alpha, uint64_t seed) {
    if (!a) return COS_LORA_ERR_ARG;
    if (in_dim  <= 0 || in_dim  > COS_LORA_MAX_IN_DIM)  return COS_LORA_ERR_ARG;
    if (out_dim <= 0 || out_dim > COS_LORA_MAX_OUT_DIM) return COS_LORA_ERR_ARG;
    if (rank    <= 0 || rank    > COS_LORA_MAX_RANK)    return COS_LORA_ERR_ARG;
    if (!(alpha > 0.0f)) return COS_LORA_ERR_ARG;

    memset(a, 0, sizeof *a);
    lncpy(a->name, sizeof a->name, name ? name : "adapter");
    a->in_dim  = in_dim;
    a->out_dim = out_dim;
    a->rank    = rank;
    a->alpha   = alpha;
    a->seed    = seed;

    size_t na = (size_t)rank    * (size_t)in_dim;
    size_t nb = (size_t)out_dim * (size_t)rank;
    a->A = (float *)calloc(na, sizeof *a->A);
    a->B = (float *)calloc(nb, sizeof *a->B);
    if (!a->A || !a->B) {
        free(a->A); free(a->B);
        a->A = a->B = NULL;
        return COS_LORA_ERR_OOM;
    }
    /* Init A ~ U(-1, 1) · 1/sqrt(in_dim). */
    uint64_t s = seed ? seed : 0x9E3779B97F4A7C15ULL;
    float scale = 1.0f / (float)sqrt((double)in_dim);
    for (size_t i = 0; i < na; i++) {
        a->A[i] = lcg_uniform(&s) * scale;
    }
    /* B stays zero so adapter is identity at t=0. */
    return COS_LORA_OK;
}

void cos_lora_free(cos_lora_adapter_t *a) {
    if (!a) return;
    free(a->A);
    free(a->B);
    memset(a, 0, sizeof *a);
}

void cos_lora_reset(cos_lora_adapter_t *a) {
    if (!a || !a->B) return;
    memset(a->B, 0, (size_t)a->out_dim * (size_t)a->rank * sizeof *a->B);
    a->training_steps = 0;
    a->sigma_before = 0.0f;
    a->sigma_after  = 0.0f;
    a->frozen = 0;
}

/* ==================================================================
 * Forward
 * ================================================================== */
int cos_lora_forward(const cos_lora_adapter_t *a,
                     const float *x,
                     const float *base,
                     float *y) {
    if (!a || !x || !y || !a->A || !a->B) return COS_LORA_ERR_ARG;
    int R = a->rank, IN = a->in_dim, OUT = a->out_dim;
    float scale = a->alpha / (float)R;

    /* z = A·x  (rank) */
    float z[COS_LORA_MAX_RANK];
    for (int r = 0; r < R; r++) {
        float s = 0.0f;
        const float *Ar = a->A + (size_t)r * IN;
        for (int i = 0; i < IN; i++) s += Ar[i] * x[i];
        z[r] = s;
    }
    /* h = B·z  (out_dim) */
    for (int o = 0; o < OUT; o++) {
        float s = 0.0f;
        const float *Bo = a->B + (size_t)o * R;
        for (int r = 0; r < R; r++) s += Bo[r] * z[r];
        y[o] = (base ? base[o] : 0.0f) + scale * s;
    }
    return COS_LORA_OK;
}

/* ==================================================================
 * Training
 * ================================================================== */
int cos_lora_train_step(cos_lora_adapter_t *a,
                        const float *x,
                        const float *target,
                        const float *base,
                        float lr,
                        float *out) {
    if (!a || !x || !target || !a->A || !a->B) return COS_LORA_ERR_ARG;
    if (a->frozen) return COS_LORA_ERR_STATE;
    if (!(lr > 0.0f)) return COS_LORA_ERR_ARG;

    int R = a->rank, IN = a->in_dim, OUT = a->out_dim;
    float scale = a->alpha / (float)R;

    /* --- Forward (pre-update) --- */
    float z_pre[COS_LORA_MAX_RANK];
    float y_pre[COS_LORA_MAX_OUT_DIM];
    for (int r = 0; r < R; r++) {
        float s = 0.0f;
        const float *Ar = a->A + (size_t)r * IN;
        for (int i = 0; i < IN; i++) s += Ar[i] * x[i];
        z_pre[r] = s;
    }
    for (int o = 0; o < OUT; o++) {
        float s = 0.0f;
        const float *Bo = a->B + (size_t)o * R;
        for (int r = 0; r < R; r++) s += Bo[r] * z_pre[r];
        y_pre[o] = (base ? base[o] : 0.0f) + scale * s;
    }
    if (a->training_steps == 0) {
        a->sigma_before = cos_lora_sigma(y_pre, target, OUT);
    }

    /* err = y - t */
    float err[COS_LORA_MAX_OUT_DIM];
    for (int o = 0; o < OUT; o++) err[o] = y_pre[o] - target[o];

    /* g_B = scale · (err ⊗ z_pre)   → out_dim × rank
     * g_A = scale · (Bᵀ · err) ⊗ x → rank × in_dim */
    float Bt_err[COS_LORA_MAX_RANK];
    for (int r = 0; r < R; r++) {
        float s = 0.0f;
        for (int o = 0; o < OUT; o++) s += a->B[(size_t)o * R + r] * err[o];
        Bt_err[r] = s;
    }

    /* Update B. */
    for (int o = 0; o < OUT; o++) {
        float *Bo = a->B + (size_t)o * R;
        float e  = err[o];
        for (int r = 0; r < R; r++) Bo[r] -= lr * scale * e * z_pre[r];
    }
    /* Update A. */
    for (int r = 0; r < R; r++) {
        float *Ar = a->A + (size_t)r * IN;
        float g  = Bt_err[r];
        for (int i = 0; i < IN; i++) Ar[i] -= lr * scale * g * x[i];
    }

    a->training_steps++;

    /* --- Post-update forward for σ_after and out --- */
    float z_post[COS_LORA_MAX_RANK];
    float y_post[COS_LORA_MAX_OUT_DIM];
    for (int r = 0; r < R; r++) {
        float s = 0.0f;
        const float *Ar = a->A + (size_t)r * IN;
        for (int i = 0; i < IN; i++) s += Ar[i] * x[i];
        z_post[r] = s;
    }
    for (int o = 0; o < OUT; o++) {
        float s = 0.0f;
        const float *Bo = a->B + (size_t)o * R;
        for (int r = 0; r < R; r++) s += Bo[r] * z_post[r];
        y_post[o] = (base ? base[o] : 0.0f) + scale * s;
    }
    a->sigma_after = cos_lora_sigma(y_post, target, OUT);
    if (out) memcpy(out, y_post, (size_t)OUT * sizeof *out);
    return COS_LORA_OK;
}

float cos_lora_sigma(const float *y, const float *t, int n) {
    if (!y || !t || n <= 0) return 1.0f;
    double s = 0.0;
    for (int i = 0; i < n; i++) {
        double d = (double)(y[i] - t[i]);
        if (d < 0.0) d = -d;
        if (d > 1.0) d = 1.0;
        s += d;
    }
    return (float)(s / (double)n);
}

/* ==================================================================
 * Introspection
 * ================================================================== */
void cos_lora_stats(const cos_lora_adapter_t *a, cos_lora_stats_t *s) {
    if (!a || !s) return;
    memset(s, 0, sizeof *s);
    s->bytes_weights = ((size_t)a->rank * (size_t)a->in_dim +
                        (size_t)a->out_dim * (size_t)a->rank) * sizeof(float);
    s->sigma_before = a->sigma_before;
    s->sigma_after  = a->sigma_after;
    s->sigma_improvement = a->sigma_before - a->sigma_after;
    s->steps = a->training_steps;
    s->frozen = a->frozen;
}

/* ==================================================================
 * Registry
 * ================================================================== */
void cos_lora_registry_init(cos_lora_registry_t *reg) {
    if (!reg) return;
    memset(reg, 0, sizeof *reg);
}

int cos_lora_registry_bind(cos_lora_registry_t *reg,
                           const char *role,
                           const cos_lora_adapter_t *adapter) {
    if (!reg || !role || !adapter) return COS_LORA_ERR_ARG;
    /* Overwrite any existing binding for `role`. */
    for (int i = 0; i < reg->n; i++) {
        if (strcmp(reg->roles[i], role) == 0) {
            reg->adapters[i] = adapter;
            return COS_LORA_OK;
        }
    }
    if (reg->n >= COS_LORA_REGISTRY_CAP) return COS_LORA_ERR_STATE;
    lncpy(reg->roles[reg->n], COS_LORA_MAX_NAME, role);
    reg->adapters[reg->n] = adapter;
    reg->n++;
    return COS_LORA_OK;
}

const cos_lora_adapter_t *cos_lora_route(const cos_lora_registry_t *reg,
                                         const char *role) {
    if (!reg || !role) return NULL;
    for (int i = 0; i < reg->n; i++) {
        if (strcmp(reg->roles[i], role) == 0) return reg->adapters[i];
    }
    return NULL;
}

/* ==================================================================
 * Self-test
 *
 * Train a tiny adapter to memorise a fixed (x, target) mapping and
 * verify that σ_after ≪ σ_before.
 * ================================================================== */
int cos_lora_self_test(void) {
    cos_lora_adapter_t a;
    if (cos_lora_init(&a, "unit", 8, 4, 4, 16.0f, 0xC05C05C0C05C05C0ULL) != COS_LORA_OK)
        return -1;

    float x[8]    = { 0.1f,-0.2f, 0.3f,-0.4f, 0.5f,-0.6f, 0.7f,-0.8f };
    float tgt[4]  = { 0.5f, 0.25f, 0.0f, -0.5f };
    float base[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    /* (a) initial σ is finite */
    float y0[4];
    cos_lora_forward(&a, x, base, y0);
    float s0 = cos_lora_sigma(y0, tgt, 4);
    if (!(s0 >= 0.0f && s0 <= 1.0f)) return -2;

    /* (b) SGD drives σ down */
    for (int step = 0; step < 200; step++) {
        cos_lora_train_step(&a, x, tgt, base, 0.10f, NULL);
    }
    if (!(a.sigma_after + 1e-3f < a.sigma_before)) return -3;

    /* (c) determinism: a second identically-seeded adapter
     * after the same training trajectory matches bit-for-bit. */
    cos_lora_adapter_t b;
    if (cos_lora_init(&b, "unit", 8, 4, 4, 16.0f, 0xC05C05C0C05C05C0ULL) != COS_LORA_OK)
        return -4;
    for (int step = 0; step < 200; step++) {
        cos_lora_train_step(&b, x, tgt, base, 0.10f, NULL);
    }
    size_t na = 4u * 8u, nb = 4u * 4u;
    for (size_t i = 0; i < na; i++) if (a.A[i] != b.A[i]) return -5;
    for (size_t i = 0; i < nb; i++) if (a.B[i] != b.B[i]) return -6;

    /* (d) routing */
    cos_lora_registry_t reg;
    cos_lora_registry_init(&reg);
    if (cos_lora_registry_bind(&reg, "factual", &a) != COS_LORA_OK) return -7;
    if (cos_lora_registry_bind(&reg, "code",    &b) != COS_LORA_OK) return -8;
    if (cos_lora_route(&reg, "factual") != &a) return -9;
    if (cos_lora_route(&reg, "missing") != NULL) return -10;

    cos_lora_free(&a);
    cos_lora_free(&b);
    return 0;
}
