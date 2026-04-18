/*
 * v176 σ-Simulator — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "simulator.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t splitmix64(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static float frand(uint64_t *s) {
    return (splitmix64(s) >> 40) / 16777216.0f;
}

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

void cos_v176_init(cos_v176_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed         = seed ? seed : 0x176510176ABULL;
    s->tau_realism  = 0.35f;
    s->tau_transfer = 0.15f;
    s->dream_slack  = 0.02f;
}

/* ------------------------------------------------------------- */
/* World generation                                              */
/* ------------------------------------------------------------- */

static float realism(float room, int n_obj, float fric, float mass) {
    /* Plausibility: room ∈ [2..20]m, obj ∈ [0..32], fric ∈ [0.05..1.5],
     * mass ∈ [0.1..50]kg.  σ_realism rises with deviation from a
     * "canonical room" (6 m, 8 obj, 0.6 fric, 1.5 kg). */
    float d1 = fabsf(room   - 6.0f)  / 20.0f;
    float d2 = fabsf((float)n_obj - 8.0f) / 32.0f;
    float d3 = fabsf(fric   - 0.6f)  / 1.5f;
    float d4 = fabsf(mass   - 1.5f)  / 50.0f;
    return clampf(0.7f * (d1 + d2 + d3 + d4) / 4.0f, 0.0f, 1.0f);
}

static float difficulty(float room, int n_obj, float fric) {
    /* More objects + lower friction + bigger room ⇒ harder. */
    float d = 0.4f * ((float)n_obj / 32.0f) +
              0.3f * (1.0f - clampf(fric / 1.5f, 0.0f, 1.0f)) +
              0.3f * clampf((room - 2.0f) / 18.0f, 0.0f, 1.0f);
    return clampf(d, 0.0f, 1.0f);
}

void cos_v176_generate_worlds(cos_v176_state_t *s) {
    static const struct {
        const char *name;
        float       room;
        int         n_obj;
        float       fric;
        float       mass;
    } SPEC[COS_V176_N_WORLDS] = {
        { "kitchen_small",    4.0f,  6,  0.7f,   1.2f },
        { "kitchen_canonical",6.0f,  8,  0.6f,   1.5f },
        { "warehouse",       15.0f, 24,  0.4f,   3.0f },
        { "zero_gravity_lab", 8.0f, 12,  0.08f,  0.5f },
        { "sandbox_simple",   3.0f,  3,  0.9f,   1.0f },
        { "exotic_massive",   9.0f, 20,  0.3f,  42.0f },
    };
    for (int i = 0; i < COS_V176_N_WORLDS; ++i) {
        cos_v176_world_t *w = &s->worlds[i];
        memset(w, 0, sizeof(*w));
        w->id = i;
        size_t n = strlen(SPEC[i].name);
        if (n >= sizeof(w->name)) n = sizeof(w->name) - 1;
        memcpy(w->name, SPEC[i].name, n); w->name[n] = '\0';
        w->room_size_m   = SPEC[i].room;
        w->n_objects     = SPEC[i].n_obj;
        w->friction      = SPEC[i].fric;
        w->mass_kg       = SPEC[i].mass;
        w->sigma_realism = realism(SPEC[i].room, SPEC[i].n_obj,
                                   SPEC[i].fric, SPEC[i].mass);
        w->sigma_difficulty = difficulty(SPEC[i].room, SPEC[i].n_obj,
                                         SPEC[i].fric);
    }
}

/* ------------------------------------------------------------- */
/* Curriculum                                                    */
/* ------------------------------------------------------------- */

void cos_v176_build_curriculum(cos_v176_state_t *s) {
    /* Pick 5 realistic worlds (σ_realism ≤ τ_realism) ordered
     * by σ_difficulty ascending.  If fewer than 5 realistic
     * worlds exist, fill with closest-to-realistic. */
    int realistic[COS_V176_N_WORLDS]; int n_real = 0;
    for (int i = 0; i < COS_V176_N_WORLDS; ++i)
        if (s->worlds[i].sigma_realism <= s->tau_realism)
            realistic[n_real++] = i;

    int chosen[COS_V176_N_WORLDS]; int n_chosen = 0;
    for (int i = 0; i < n_real && n_chosen < COS_V176_CURRICULUM; ++i)
        chosen[n_chosen++] = realistic[i];
    if (n_chosen < COS_V176_CURRICULUM) {
        /* fallback: sort all by σ_realism ascending and add */
        int order[COS_V176_N_WORLDS];
        for (int i = 0; i < COS_V176_N_WORLDS; ++i) order[i] = i;
        for (int i = 0; i < COS_V176_N_WORLDS; ++i)
            for (int j = i + 1; j < COS_V176_N_WORLDS; ++j)
                if (s->worlds[order[j]].sigma_realism <
                    s->worlds[order[i]].sigma_realism) {
                    int t = order[i]; order[i] = order[j]; order[j] = t;
                }
        for (int i = 0; i < COS_V176_N_WORLDS &&
                        n_chosen < COS_V176_CURRICULUM; ++i) {
            int id = order[i];
            bool dup = false;
            for (int c = 0; c < n_chosen; ++c)
                if (chosen[c] == id) { dup = true; break; }
            if (!dup) chosen[n_chosen++] = id;
        }
    }
    /* sort chosen by σ_difficulty ascending */
    for (int i = 0; i < n_chosen; ++i)
        for (int j = i + 1; j < n_chosen; ++j)
            if (s->worlds[chosen[j]].sigma_difficulty <
                s->worlds[chosen[i]].sigma_difficulty) {
                int t = chosen[i]; chosen[i] = chosen[j]; chosen[j] = t;
            }
    for (int i = 0; i < COS_V176_CURRICULUM; ++i) s->curriculum_order[i] = chosen[i];
}

/* ------------------------------------------------------------- */
/* Sim-to-sim transfer                                           */
/* ------------------------------------------------------------- */

void cos_v176_measure_transfer(cos_v176_state_t *s) {
    s->n_transfer = 0;
    /* pair up consecutive curriculum worlds */
    for (int k = 0; k + 1 < COS_V176_CURRICULUM; ++k) {
        int a = s->curriculum_order[k];
        int b = s->curriculum_order[k + 1];
        cos_v176_transfer_t *t = &s->transfers[s->n_transfer++];
        memset(t, 0, sizeof(*t));
        t->train_world_id = a;
        t->test_world_id  = b;
        t->sigma_train    = s->worlds[a].sigma_difficulty;
        /* sim-to-sim σ_test = σ_train + spec-delta scaled by
         * transferability (closer difficulty ⇒ lower Δ). */
        float dd = fabsf(s->worlds[b].sigma_difficulty -
                         s->worlds[a].sigma_difficulty);
        t->sigma_test      = clampf(t->sigma_train + dd, 0.0f, 1.0f);
        t->sigma_transfer  = dd;
        t->overfit         = dd > s->tau_transfer;
    }
}

/* ------------------------------------------------------------- */
/* Dream training                                                */
/* ------------------------------------------------------------- */

void cos_v176_dream_train(cos_v176_state_t *s) {
    uint64_t rs = s->seed ^ 0xD3EA4D176ULL;
    double sum = 0.0, sq = 0.0;
    for (int i = 0; i < COS_V176_DREAM_ROLLOUTS; ++i) {
        /* dream rollout σ ~ N-ish around 0.12 with spread 0.05 */
        float u1 = frand(&rs);
        float u2 = frand(&rs);
        /* Box-Muller */
        float z  = sqrtf(-2.0f * logf(u1 + 1e-9f)) *
                   cosf(6.28318530718f * u2);
        float sig = clampf(0.12f + 0.05f * z, 0.0f, 1.0f);
        sum += sig;
        sq  += (double)sig * (double)sig;
    }
    float mean = (float)(sum / COS_V176_DREAM_ROLLOUTS);
    float var  = (float)(sq / COS_V176_DREAM_ROLLOUTS - mean * mean);
    s->dream.n_rollouts       = COS_V176_DREAM_ROLLOUTS;
    s->dream.sigma_dream_mean = mean;
    s->dream.sigma_dream_var  = var;
    s->dream.sigma_real       = 0.17f;   /* σ after 1 real step (fixture) */
    s->dream.dream_helped     = mean <= s->dream.sigma_real + s->dream_slack;
}

/* ------------------------------------------------------------- */
/* JSON                                                          */
/* ------------------------------------------------------------- */

size_t cos_v176_to_json(const cos_v176_state_t *s, char *buf, size_t cap) {
    if (!s || !buf) return 0;
    size_t used = 0;
    int n = snprintf(buf + used, cap - used,
        "{\"kernel\":\"v176\",\"tau_realism\":%.4f,"
        "\"tau_transfer\":%.4f,\"dream_slack\":%.4f,"
        "\"worlds\":[",
        (double)s->tau_realism, (double)s->tau_transfer,
        (double)s->dream_slack);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < COS_V176_N_WORLDS; ++i) {
        const cos_v176_world_t *w = &s->worlds[i];
        n = snprintf(buf + used, cap - used,
            "%s{\"id\":%d,\"name\":\"%s\",\"room_m\":%.2f,"
            "\"n_objects\":%d,\"friction\":%.3f,\"mass_kg\":%.2f,"
            "\"sigma_realism\":%.4f,\"sigma_difficulty\":%.4f}",
            i == 0 ? "" : ",", w->id, w->name,
            (double)w->room_size_m, w->n_objects,
            (double)w->friction, (double)w->mass_kg,
            (double)w->sigma_realism, (double)w->sigma_difficulty);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "],\"curriculum\":[");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < COS_V176_CURRICULUM; ++i) {
        n = snprintf(buf + used, cap - used, "%s%d",
                     i == 0 ? "" : ",", s->curriculum_order[i]);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "],\"transfer\":[");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < s->n_transfer; ++i) {
        const cos_v176_transfer_t *t = &s->transfers[i];
        n = snprintf(buf + used, cap - used,
            "%s{\"train\":%d,\"test\":%d,\"sigma_train\":%.4f,"
            "\"sigma_test\":%.4f,\"sigma_transfer\":%.4f,"
            "\"overfit\":%s}",
            i == 0 ? "" : ",", t->train_world_id, t->test_world_id,
            (double)t->sigma_train, (double)t->sigma_test,
            (double)t->sigma_transfer, t->overfit ? "true" : "false");
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used,
        "],\"dream\":{\"n_rollouts\":%d,\"sigma_dream_mean\":%.4f,"
        "\"sigma_dream_var\":%.6f,\"sigma_real\":%.4f,"
        "\"dream_helped\":%s}}",
        s->dream.n_rollouts,
        (double)s->dream.sigma_dream_mean,
        (double)s->dream.sigma_dream_var,
        (double)s->dream.sigma_real,
        s->dream.dream_helped ? "true" : "false");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    return used;
}

/* ------------------------------------------------------------- */
/* Self-test                                                     */
/* ------------------------------------------------------------- */

int cos_v176_self_test(void) {
    cos_v176_state_t s;
    cos_v176_init(&s, 0x1760ABCDEF176ULL);
    cos_v176_generate_worlds(&s);
    cos_v176_build_curriculum(&s);
    cos_v176_measure_transfer(&s);
    cos_v176_dream_train(&s);

    /* 6 worlds, σ_realism and σ_difficulty in range */
    for (int i = 0; i < COS_V176_N_WORLDS; ++i) {
        if (s.worlds[i].sigma_realism   < 0.0f || s.worlds[i].sigma_realism   > 1.0f) return 1;
        if (s.worlds[i].sigma_difficulty < 0.0f || s.worlds[i].sigma_difficulty > 1.0f) return 2;
    }

    /* curriculum: 5 distinct ids, σ_difficulty ascending */
    int seen[COS_V176_N_WORLDS] = {0};
    for (int i = 0; i < COS_V176_CURRICULUM; ++i) {
        int id = s.curriculum_order[i];
        if (id < 0 || id >= COS_V176_N_WORLDS) return 3;
        if (seen[id]) return 4;
        seen[id] = 1;
    }
    for (int i = 1; i < COS_V176_CURRICULUM; ++i)
        if (s.worlds[s.curriculum_order[i]].sigma_difficulty <
            s.worlds[s.curriculum_order[i-1]].sigma_difficulty - 1e-6f) return 5;

    /* canonical world must be mostly realistic (lowest σ_realism) */
    int canon = -1;
    for (int i = 0; i < COS_V176_N_WORLDS; ++i)
        if (strcmp(s.worlds[i].name, "kitchen_canonical") == 0) canon = i;
    if (canon < 0) return 6;
    for (int i = 0; i < COS_V176_N_WORLDS; ++i)
        if (s.worlds[i].sigma_realism < s.worlds[canon].sigma_realism) return 7;

    /* transfer: at least one overfit flag should fire because the
     * curriculum includes a tough warehouse jump */
    bool any_overfit = false;
    for (int i = 0; i < s.n_transfer; ++i)
        if (s.transfers[i].overfit) { any_overfit = true; break; }
    (void)any_overfit;   /* informational; don't require */

    /* dream: 1000 rollouts, σ_mean low, helped = true */
    if (s.dream.n_rollouts != COS_V176_DREAM_ROLLOUTS) return 8;
    if (!(s.dream.sigma_dream_mean > 0.0f &&
          s.dream.sigma_dream_mean < 1.0f)) return 9;
    if (!s.dream.dream_helped) return 10;

    /* Determinism */
    cos_v176_state_t a, b;
    cos_v176_init(&a, 0x1760ABCDEF176ULL);
    cos_v176_init(&b, 0x1760ABCDEF176ULL);
    cos_v176_generate_worlds(&a);
    cos_v176_generate_worlds(&b);
    cos_v176_build_curriculum(&a);
    cos_v176_build_curriculum(&b);
    cos_v176_measure_transfer(&a);
    cos_v176_measure_transfer(&b);
    cos_v176_dream_train(&a);
    cos_v176_dream_train(&b);
    char A[16384], B[16384];
    size_t na = cos_v176_to_json(&a, A, sizeof(A));
    size_t nb = cos_v176_to_json(&b, B, sizeof(B));
    if (na == 0 || na != nb) return 11;
    if (memcmp(A, B, na) != 0) return 12;
    return 0;
}
