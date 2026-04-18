/*
 * v206 σ-Theorem — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "theorem.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v206_init(cos_v206_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed       = seed ? seed : 0x206F42EAULL;
    s->tau_step   = 0.50f;
    s->tau_formal = 0.35f;
}

typedef struct {
    const char *name;
    float       s_formal;
    float       steps[COS_V206_N_STEPS];
    bool        has_counter;
    int         counter_id;
} fx_t;
static const fx_t FX[COS_V206_N_CONJECTURES] = {
    /* name                              sF        steps                    counter */
    { "σ_monotone_reduces_K_eff",        0.08f, { 0.04f, 0.05f, 0.06f, 0.07f }, false, 0 },
    { "τ_couples_σ_into_K_eff",          0.10f, { 0.06f, 0.08f, 0.09f, 0.10f }, false, 0 },
    { "calibration_mirrors_K_eff",       0.22f, { 0.14f, 0.18f, 0.20f, 0.22f }, false, 0 },
    { "K_eff_bounds_actionable_bw",      0.16f, { 0.09f, 0.10f, 0.11f, 0.14f }, false, 0 },
    { "σ_spikes_trigger_habit_breakout", 0.30f, { 0.25f, 0.35f, 0.40f, 0.55f }, false, 0 }, /* weak step */
    { "novelty_times_certainty_impact",  0.45f, { 0.40f, 0.55f, 0.65f, 0.75f }, false, 0 }, /* speculation */
    { "firmware_sigma_independence",     0.62f, { 0.55f, 0.65f, 0.70f, 0.80f }, true,  17 }, /* refuted */
    { "sigma_irrelevance",               0.70f, { 0.65f, 0.70f, 0.75f, 0.85f }, true,  23 }  /* refuted */
};

void cos_v206_build(cos_v206_state_t *s) {
    s->n = COS_V206_N_CONJECTURES;
    for (int i = 0; i < s->n; ++i) {
        cos_v206_theorem_t *t = &s->thms[i];
        memset(t, 0, sizeof(*t));
        t->id = i;
        strncpy(t->name, FX[i].name, COS_V206_STR_MAX - 1);
        t->sigma_formalization = FX[i].s_formal;
        for (int k = 0; k < COS_V206_N_STEPS; ++k) t->sigma_step[k] = FX[i].steps[k];
        t->counter_example_id = FX[i].has_counter ? FX[i].counter_id : 0;
    }
}

void cos_v206_run(cos_v206_state_t *s) {
    s->n_proven = s->n_conjecture = s->n_speculation = s->n_refuted = 0;

    uint64_t prev = 0x20671F44ULL;
    for (int i = 0; i < s->n; ++i) {
        cos_v206_theorem_t *t = &s->thms[i];

        /* σ_proof = max σ_step + pick weakest step. */
        float m = 0.0f; int w = 0;
        for (int k = 0; k < COS_V206_N_STEPS; ++k) {
            if (t->sigma_step[k] > m) { m = t->sigma_step[k]; w = k; }
        }
        t->sigma_proof   = m;
        t->weakest_step  = w;

        /* Closed-form Lean accept predicate. */
        bool all_steps_ok = m <= s->tau_step;
        bool formal_ok    = t->sigma_formalization <= s->tau_formal;
        t->lean_accepts   = all_steps_ok && formal_ok;

        /* Status ladder. */
        if (FX[i].has_counter) {
            t->status = COS_V206_STATUS_REFUTED;
            t->lean_accepts = false;
            s->n_refuted++;
        } else if (t->lean_accepts) {
            t->status = COS_V206_STATUS_PROVEN;
            s->n_proven++;
        } else if (t->sigma_proof <= s->tau_step + 0.10f) {
            t->status = COS_V206_STATUS_CONJECTURE;
            s->n_conjecture++;
        } else {
            t->status = COS_V206_STATUS_SPECULATION;
            s->n_speculation++;
        }

        t->hash_prev = prev;
        struct { int id, status, weakest, lean, counter;
                 float sf, sp;
                 float steps[COS_V206_N_STEPS];
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id      = t->id;
        rec.status  = t->status;
        rec.weakest = t->weakest_step;
        rec.lean    = t->lean_accepts ? 1 : 0;
        rec.counter = t->counter_example_id;
        rec.sf      = t->sigma_formalization;
        rec.sp      = t->sigma_proof;
        for (int k = 0; k < COS_V206_N_STEPS; ++k) rec.steps[k] = t->sigma_step[k];
        rec.prev    = prev;
        t->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = t->hash_curr;
    }
    s->terminal_hash = prev;

    uint64_t v = 0x20671F44ULL;
    s->chain_valid = true;
    for (int i = 0; i < s->n; ++i) {
        const cos_v206_theorem_t *t = &s->thms[i];
        if (t->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, status, weakest, lean, counter;
                 float sf, sp;
                 float steps[COS_V206_N_STEPS];
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id      = t->id;
        rec.status  = t->status;
        rec.weakest = t->weakest_step;
        rec.lean    = t->lean_accepts ? 1 : 0;
        rec.counter = t->counter_example_id;
        rec.sf      = t->sigma_formalization;
        rec.sp      = t->sigma_proof;
        for (int k = 0; k < COS_V206_N_STEPS; ++k) rec.steps[k] = t->sigma_step[k];
        rec.prev    = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != t->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v206_to_json(const cos_v206_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v206\","
        "\"n\":%d,\"tau_step\":%.3f,\"tau_formal\":%.3f,"
        "\"n_proven\":%d,\"n_conjecture\":%d,"
        "\"n_speculation\":%d,\"n_refuted\":%d,"
        "\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\","
        "\"theorems\":[",
        s->n, s->tau_step, s->tau_formal,
        s->n_proven, s->n_conjecture, s->n_speculation, s->n_refuted,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n; ++i) {
        const cos_v206_theorem_t *t = &s->thms[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"status\":%d,\"lean\":%s,"
            "\"sf\":%.3f,\"sp\":%.3f,\"weakest\":%d,"
            "\"counter\":%d}",
            i == 0 ? "" : ",", t->id, t->status,
            t->lean_accepts ? "true" : "false",
            t->sigma_formalization, t->sigma_proof,
            t->weakest_step, t->counter_example_id);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v206_self_test(void) {
    cos_v206_state_t s;
    cos_v206_init(&s, 0x206F42EAULL);
    cos_v206_build(&s);
    cos_v206_run(&s);

    if (s.n != COS_V206_N_CONJECTURES) return 1;
    if (s.n_proven      < 1)            return 2;
    if (s.n_conjecture  < 1)            return 3;
    if (s.n_speculation < 1)            return 4;
    if (s.n_refuted     < 1)            return 5;
    if (!s.chain_valid)                  return 6;

    /* σ-honesty: no PROVEN without lean_accepts + σ_proof ≤ τ_step. */
    for (int i = 0; i < s.n; ++i) {
        const cos_v206_theorem_t *t = &s.thms[i];
        if (t->status == COS_V206_STATUS_PROVEN) {
            if (!t->lean_accepts)               return 7;
            if (t->sigma_proof > s.tau_step + 1e-6f) return 8;
        }
        if (t->status == COS_V206_STATUS_REFUTED && t->counter_example_id == 0)
            return 9;
        for (int k = 0; k < COS_V206_N_STEPS; ++k)
            if (t->sigma_step[k] < 0.0f || t->sigma_step[k] > 1.0f) return 10;
    }
    return 0;
}
