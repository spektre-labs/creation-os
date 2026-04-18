/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v147 σ-Reflect — implementation.
 */
#include "reflect.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* RAIN thresholds — repo-wide defaults (contract R3). */
#define TAU_REWIND_LOCAL  0.30f
#define TAU_REWIND_MID    0.60f

static void safe_copy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t i = 0;
    for (; i + 1 < cap && src[i]; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

void cos_v147_trace_init(cos_v147_trace_t *t,
                         const char *final_answer,
                         float final_sigma) {
    if (!t) return;
    memset(t, 0, sizeof *t);
    safe_copy(t->final_answer, sizeof t->final_answer, final_answer);
    if (final_sigma < 0.0f) final_sigma = 0.0f;
    if (final_sigma > 1.0f) final_sigma = 1.0f;
    t->final_sigma = final_sigma;
}

int cos_v147_trace_add(cos_v147_trace_t *t,
                       const char *text,
                       float sigma_product) {
    if (!t || !text) return -1;
    if (t->n_thoughts >= COS_V147_MAX_THOUGHTS) return -2;
    if (sigma_product < 0.0f) sigma_product = 0.0f;
    if (sigma_product > 1.0f) sigma_product = 1.0f;
    cos_v147_thought_t *th = &t->thoughts[t->n_thoughts];
    th->index         = t->n_thoughts;
    safe_copy(th->text, sizeof th->text, text);
    th->sigma_product = sigma_product;
    t->n_thoughts++;
    return 0;
}

int cos_v147_rewind_depth(float weakest_sigma, int n_steps) {
    if (n_steps <= 0) return 0;
    if (weakest_sigma <= TAU_REWIND_LOCAL) return 1;
    if (weakest_sigma <= TAU_REWIND_MID)   return (n_steps + 1) / 2;
    return n_steps;
}

int cos_v147_reflect(const cos_v147_trace_t *t,
                     const char *pure_answer,
                     float pure_sigma,
                     cos_v147_reflection_t *out) {
    if (!t || !pure_answer || !out) return -1;
    (void)pure_sigma;        /* captured for JSON symmetry below */
    memset(out, 0, sizeof *out);

    if (t->n_thoughts == 0) {
        out->weakest_step   = -1;
        out->weakest_sigma  = 0.0f;
        out->rewind_depth   = 0;
        out->consistency_ok = strcmp(t->final_answer, pure_answer) == 0;
        out->divergence_detected = !out->consistency_ok;
        return 0;
    }

    /* R2: argmax σ_product across thoughts. */
    int   weakest = 0;
    float mx      = t->thoughts[0].sigma_product;
    float mn      = mx;
    double sum    = 0.0;
    for (int i = 0; i < t->n_thoughts; ++i) {
        float s = t->thoughts[i].sigma_product;
        sum += s;
        if (s > mx) { mx = s; weakest = i; }
        if (s < mn) { mn = s; }
    }
    out->weakest_step       = weakest;
    out->weakest_sigma      = mx;
    out->max_sigma          = mx;
    out->min_sigma          = mn;
    out->mean_sigma         = (float)(sum / (double)t->n_thoughts);

    /* R3: RAIN rewind depth. */
    out->rewind_depth       = cos_v147_rewind_depth(mx, t->n_thoughts);
    out->rain_should_rewind = mx > TAU_REWIND_LOCAL ? 1 : 0;

    /* R1: consistency between pure and chain answers. */
    out->consistency_ok      =
        strcmp(t->final_answer, pure_answer) == 0 ? 1 : 0;
    out->divergence_detected = !out->consistency_ok;
    return 0;
}

int cos_v147_trace_to_json(const cos_v147_trace_t *t,
                           char *buf, size_t cap) {
    if (!t || !buf || cap == 0) return -1;
    size_t off = 0;
    int rc = snprintf(buf + off, cap - off,
        "{\"v147_trace\":true,\"final_answer\":\"%s\","
        "\"final_sigma\":%.6f,\"n_thoughts\":%d,\"thoughts\":[",
        t->final_answer, (double)t->final_sigma, t->n_thoughts);
    if (rc < 0 || (size_t)rc >= cap - off) return -1;
    off += (size_t)rc;
    for (int i = 0; i < t->n_thoughts; ++i) {
        rc = snprintf(buf + off, cap - off,
            "%s{\"index\":%d,\"text\":\"%s\",\"sigma\":%.6f}",
            i ? "," : "",
            t->thoughts[i].index, t->thoughts[i].text,
            (double)t->thoughts[i].sigma_product);
        if (rc < 0 || (size_t)rc >= cap - off) return -1;
        off += (size_t)rc;
    }
    rc = snprintf(buf + off, cap - off, "]}");
    if (rc < 0 || (size_t)rc >= cap - off) return -1;
    off += (size_t)rc;
    return (int)off;
}

int cos_v147_reflection_to_json(const cos_v147_reflection_t *r,
                                const cos_v147_trace_t *t,
                                char *buf, size_t cap) {
    if (!r || !t || !buf || cap == 0) return -1;
    const char *weakest_text = "";
    float weakest_sigma = 0.0f;
    int   weakest_idx   = r->weakest_step;
    if (weakest_idx >= 0 && weakest_idx < t->n_thoughts) {
        weakest_text  = t->thoughts[weakest_idx].text;
        weakest_sigma = t->thoughts[weakest_idx].sigma_product;
    }
    int rc = snprintf(buf, cap,
        "{\"v147_reflection\":true,"
        "\"weakest_step\":%d,\"weakest_text\":\"%s\","
        "\"weakest_sigma\":%.6f,\"rewind_depth\":%d,"
        "\"rain_should_rewind\":%s,\"consistency_ok\":%s,"
        "\"divergence_detected\":%s,"
        "\"mean_sigma\":%.6f,\"max_sigma\":%.6f,\"min_sigma\":%.6f,"
        "\"final_answer\":\"%s\"}",
        r->weakest_step, weakest_text,
        (double)weakest_sigma, r->rewind_depth,
        r->rain_should_rewind  ? "true" : "false",
        r->consistency_ok      ? "true" : "false",
        r->divergence_detected ? "true" : "false",
        (double)r->mean_sigma, (double)r->max_sigma,
        (double)r->min_sigma, t->final_answer);
    if (rc < 0 || (size_t)rc >= cap) return -1;
    return rc;
}

/* ================================================================
 * Self-test
 * ============================================================= */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v147 self-test FAIL: %s (line %d)\n", \
                (msg), __LINE__); return 1; \
    } \
} while (0)

int cos_v147_self_test(void) {
    /* --- A. Happy path: 4 thoughts, pure-vs-chain agree -------- *
     * σ: 0.15, 0.22, 0.67, 0.18 → weakest index 2.               *
     * σ_weakest=0.67 > 0.60 ⇒ rewind full chain (depth = n=4).   */
    cos_v147_trace_t t;
    cos_v147_trace_init(&t, "42", 0.20f);
    cos_v147_trace_add(&t, "parse the question",   0.15f);
    cos_v147_trace_add(&t, "recall the formula",   0.22f);
    cos_v147_trace_add(&t, "apply with values",    0.67f);
    cos_v147_trace_add(&t, "check arithmetic",     0.18f);

    cos_v147_reflection_t r;
    _CHECK(cos_v147_reflect(&t, "42", 0.22f, &r) == 0, "reflect ok");
    fprintf(stderr,
        "check-v147: weakest step=%d σ=%.2f rewind=%d consistent=%d\n",
        r.weakest_step, (double)r.weakest_sigma,
        r.rewind_depth, r.consistency_ok);
    _CHECK(r.weakest_step   == 2,     "weakest index = 2");
    _CHECK(fabsf(r.weakest_sigma - 0.67f) < 1e-5f,
           "weakest σ = 0.67");
    _CHECK(r.rewind_depth   == 4,     "RAIN rewind = full chain on σ>0.60");
    _CHECK(r.consistency_ok == 1,     "pure_answer matches chain → consistent");
    _CHECK(r.divergence_detected == 0, "no divergence");
    _CHECK(r.rain_should_rewind == 1,  "rewind triggered on σ > 0.30");

    /* --- B. Mid-σ weakest → depth ⌈n/2⌉ ------------------------ */
    cos_v147_trace_t m;
    cos_v147_trace_init(&m, "answer", 0.25f);
    cos_v147_trace_add(&m, "step a", 0.10f);
    cos_v147_trace_add(&m, "step b", 0.20f);
    cos_v147_trace_add(&m, "step c", 0.45f);   /* 0.30 < σ ≤ 0.60 */
    cos_v147_trace_add(&m, "step d", 0.15f);
    cos_v147_trace_add(&m, "step e", 0.28f);
    cos_v147_reflection_t rm;
    _CHECK(cos_v147_reflect(&m, "answer", 0.25f, &rm) == 0, "mid reflect");
    _CHECK(rm.weakest_step == 2,    "weakest = step c");
    _CHECK(rm.rewind_depth == 3,    "⌈5/2⌉ = 3 mid-chain rewind");
    _CHECK(rm.rain_should_rewind == 1, "rewind at σ=0.45 > 0.30");

    /* --- C. Low-σ chain → local rewind ------------------------- */
    cos_v147_trace_t lo;
    cos_v147_trace_init(&lo, "ok", 0.15f);
    for (int i = 0; i < 6; ++i) {
        char txt[16]; snprintf(txt, sizeof txt, "step %d", i);
        cos_v147_trace_add(&lo, txt, 0.10f + 0.03f * (float)i);
    }
    cos_v147_reflection_t rlo;
    _CHECK(cos_v147_reflect(&lo, "ok", 0.14f, &rlo) == 0, "lo reflect");
    _CHECK(rlo.weakest_sigma <= TAU_REWIND_LOCAL + 1e-6f,
           "max σ stays ≤ 0.30");
    _CHECK(rlo.rewind_depth == 1,        "depth = 1 on low-σ chain");
    _CHECK(rlo.rain_should_rewind == 0,  "no rewind on σ ≤ 0.30");

    /* --- D. Divergence: pure ≠ chain -------------------------- */
    cos_v147_reflection_t rdiv;
    _CHECK(cos_v147_reflect(&t, "WRONG", 0.25f, &rdiv) == 0,
           "reflect with divergent pure");
    _CHECK(rdiv.consistency_ok      == 0, "divergence detected");
    _CHECK(rdiv.divergence_detected == 1, "divergence flag");
    _CHECK(rdiv.weakest_step        == 2,
           "weakest step identified regardless of divergence");

    /* --- E. Empty trace still reports consistency on matching -- */
    cos_v147_trace_t empty;
    cos_v147_trace_init(&empty, "x", 0.5f);
    cos_v147_reflection_t re;
    _CHECK(cos_v147_reflect(&empty, "x", 0.5f, &re) == 0, "empty ok");
    _CHECK(re.weakest_step   == -1,      "empty weakest = -1");
    _CHECK(re.rewind_depth   == 0,       "empty depth = 0");
    _CHECK(re.consistency_ok == 1,       "empty agrees with matching pure");

    /* --- F. JSON shape ---------------------------------------- */
    char buf[1024];
    int jw = cos_v147_reflection_to_json(&r, &t, buf, sizeof buf);
    _CHECK(jw > 0, "reflection json emit");
    _CHECK(strstr(buf, "\"v147_reflection\":true") != NULL, "tag");
    _CHECK(strstr(buf, "\"consistency_ok\":true")  != NULL, "consistency");

    fprintf(stderr,
        "check-v147: OK (weakest + RAIN depth + divergence + trace json)\n");
    return 0;
}
