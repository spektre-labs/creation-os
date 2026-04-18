/*
 * v205 σ-Experiment — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "experiment.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v205_init(cos_v205_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed            = seed ? seed : 0x20547E97ULL;
    s->tau_sim         = 0.35f;
    s->tau_power_sigma = 0.40f;
    s->budget_n        = COS_V205_BUDGET_N;
}

/* Fixture: 3 experiments, one per v204 TEST_QUEUE slot.
 *   exp 0  hypothesis id 3  effect=0.30  strong signal, fits budget
 *   exp 1  hypothesis id 0  effect=0.20  moderate, fits budget
 *   exp 2  hypothesis id 6  effect=0.06  tiny effect, under-powered
 */
typedef struct { int hyp; int dep, indep, ctrl;
                 float eff; float a, b;
                 int sim_supports; float s_design, s_sim; } fx_t;
static const fx_t FX[COS_V205_N_EXP] = {
    /* hyp dep indep ctrl  eff   α     β    sim  sd   ss  */
    {   3,  10,  20,  30, 0.30f, 0.05f, 0.20f, 1, 0.08f, 0.15f },
    {   0,  11,  21,  31, 0.20f, 0.05f, 0.20f, 1, 0.12f, 0.20f },
    {   6,  12,  22,  32, 0.06f, 0.05f, 0.20f, 0, 0.18f, 0.28f }
};

void cos_v205_build(cos_v205_state_t *s) {
    s->n = COS_V205_N_EXP;
    for (int i = 0; i < s->n; ++i) {
        cos_v205_experiment_t *e = &s->exp[i];
        memset(e, 0, sizeof(*e));
        e->id            = i;
        e->hypothesis_id = FX[i].hyp;
        e->dep_var       = FX[i].dep;
        e->indep_var     = FX[i].indep;
        e->control_var   = FX[i].ctrl;
        e->effect_size   = FX[i].eff;
        e->alpha         = FX[i].a;
        e->beta          = FX[i].b;
        e->sigma_design  = FX[i].s_design;
        e->sigma_sim     = FX[i].s_sim;
    }
}

/* Canonical two-sided z-scores for α=0.05 and β=0.20. */
static float z_alpha_05(void) { return 1.96f; }
static float z_beta_20 (void) { return 0.8416f; }

static int required_n(float effect) {
    float zsum = z_alpha_05() + z_beta_20();
    float n = (zsum * zsum) / (effect * effect);
    return (int)(n + 0.9999f);
}

void cos_v205_run(cos_v205_state_t *s) {
    s->n_sim_supports = s->n_sim_refutes = s->n_under_powered = 0;

    uint64_t prev = 0x20547E97ULL;
    for (int i = 0; i < s->n; ++i) {
        cos_v205_experiment_t *e = &s->exp[i];

        e->n_required   = required_n(e->effect_size);
        e->fits_budget  = e->n_required <= s->budget_n;
        /* σ_power: map fractional overshoot to [0,1]. */
        if (e->fits_budget) {
            e->sigma_power = 0.10f + 0.20f *
                (float)e->n_required / (float)s->budget_n;
        } else {
            float over = (float)e->n_required / (float)s->budget_n - 1.0f;
            float sp = 0.50f + 0.40f * over;
            if (sp > 1.0f) sp = 1.0f;
            e->sigma_power = sp;
        }

        int sim_supports = FX[i].sim_supports;
        bool sim_ok      = e->sigma_sim < s->tau_sim;
        bool power_ok    = e->sigma_power < s->tau_power_sigma;

        if (!power_ok) {
            e->decision = COS_V205_DEC_UNDER_POWERED;
            s->n_under_powered++;
        } else if (sim_ok && sim_supports) {
            e->decision = COS_V205_DEC_SIM_SUPPORTS;
            s->n_sim_supports++;
        } else {
            e->decision = COS_V205_DEC_SIM_REFUTES;
            s->n_sim_refutes++;
        }

        e->hash_prev = prev;
        struct { int id, hyp, dep, indep, ctrl, n, decision, fits;
                 float eff, a, b, sd, ss, sp;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id    = e->id;
        rec.hyp   = e->hypothesis_id;
        rec.dep   = e->dep_var;
        rec.indep = e->indep_var;
        rec.ctrl  = e->control_var;
        rec.n     = e->n_required;
        rec.decision = e->decision;
        rec.fits  = e->fits_budget ? 1 : 0;
        rec.eff   = e->effect_size;
        rec.a     = e->alpha;
        rec.b     = e->beta;
        rec.sd    = e->sigma_design;
        rec.ss    = e->sigma_sim;
        rec.sp    = e->sigma_power;
        rec.prev  = prev;
        e->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = e->hash_curr;
    }
    s->terminal_hash = prev;

    uint64_t v = 0x20547E97ULL;
    s->chain_valid = true;
    for (int i = 0; i < s->n; ++i) {
        const cos_v205_experiment_t *e = &s->exp[i];
        if (e->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, hyp, dep, indep, ctrl, n, decision, fits;
                 float eff, a, b, sd, ss, sp;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id    = e->id;
        rec.hyp   = e->hypothesis_id;
        rec.dep   = e->dep_var;
        rec.indep = e->indep_var;
        rec.ctrl  = e->control_var;
        rec.n     = e->n_required;
        rec.decision = e->decision;
        rec.fits  = e->fits_budget ? 1 : 0;
        rec.eff   = e->effect_size;
        rec.a     = e->alpha;
        rec.b     = e->beta;
        rec.sd    = e->sigma_design;
        rec.ss    = e->sigma_sim;
        rec.sp    = e->sigma_power;
        rec.prev  = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != e->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v205_to_json(const cos_v205_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v205\","
        "\"n\":%d,\"budget_n\":%d,"
        "\"tau_sim\":%.3f,\"tau_power_sigma\":%.3f,"
        "\"n_sim_supports\":%d,\"n_sim_refutes\":%d,"
        "\"n_under_powered\":%d,"
        "\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\","
        "\"experiments\":[",
        s->n, s->budget_n, s->tau_sim, s->tau_power_sigma,
        s->n_sim_supports, s->n_sim_refutes, s->n_under_powered,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n; ++i) {
        const cos_v205_experiment_t *e = &s->exp[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"hyp\":%d,\"dep\":%d,\"indep\":%d,"
            "\"ctrl\":%d,\"eff\":%.3f,\"n_required\":%d,"
            "\"fits\":%s,\"sd\":%.3f,\"ss\":%.3f,\"sp\":%.3f,"
            "\"decision\":%d}",
            i == 0 ? "" : ",", e->id, e->hypothesis_id,
            e->dep_var, e->indep_var, e->control_var,
            e->effect_size, e->n_required,
            e->fits_budget ? "true" : "false",
            e->sigma_design, e->sigma_sim, e->sigma_power,
            e->decision);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v205_self_test(void) {
    cos_v205_state_t s;
    cos_v205_init(&s, 0x20547E97ULL);
    cos_v205_build(&s);
    cos_v205_run(&s);

    if (s.n != COS_V205_N_EXP)             return 1;
    if (s.n_sim_supports  < 1)              return 2;
    if (s.n_under_powered < 1)              return 3;
    if (!s.chain_valid)                      return 4;

    for (int i = 0; i < s.n; ++i) {
        const cos_v205_experiment_t *e = &s.exp[i];
        if (e->dep_var == e->indep_var ||
            e->dep_var == e->control_var ||
            e->indep_var == e->control_var) return 5;
        if (e->sigma_design < 0.0f || e->sigma_design > 1.0f) return 6;
        if (e->sigma_sim    < 0.0f || e->sigma_sim    > 1.0f) return 6;
        if (e->sigma_power  < 0.0f || e->sigma_power  > 1.0f) return 6;
        int expected = (int)((z_alpha_05() + z_beta_20()) *
                              (z_alpha_05() + z_beta_20()) /
                              (e->effect_size * e->effect_size)
                              + 0.9999f);
        if (e->n_required != expected)      return 7;
    }
    return 0;
}

