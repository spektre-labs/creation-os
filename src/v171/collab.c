/*
 * v171 σ-Collab — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "collab.h"

#include <stdio.h>
#include <string.h>

static void copy_str(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

const char *cos_v171_mode_name(cos_v171_mode_t m) {
    switch (m) {
        case COS_V171_MODE_PAIR:   return "pair";
        case COS_V171_MODE_LEAD:   return "lead";
        case COS_V171_MODE_FOLLOW: return "follow";
        default:                   return "?";
    }
}

const char *cos_v171_action_name(cos_v171_action_t a) {
    switch (a) {
        case COS_V171_ACT_EMIT:            return "emit";
        case COS_V171_ACT_HANDOFF:         return "handoff";
        case COS_V171_ACT_DEBATE:          return "debate";
        case COS_V171_ACT_ANTI_SYCOPHANCY: return "anti_sycophancy";
        default:                           return "?";
    }
}

void cos_v171_init(cos_v171_state_t *s, cos_v171_mode_t mode,
                   uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed               = seed ? seed : 0x171C0011ABBA0171ULL;
    s->mode               = mode;
    s->tau_handoff_pair   = 0.60f;
    s->tau_handoff_lead   = 0.75f;
    s->tau_handoff_follow = 0.40f;
    s->tau_disagree       = 0.25f;
    s->tau_sycophancy     = 0.35f;
}

static float tau_for_mode(const cos_v171_state_t *s) {
    switch (s->mode) {
        case COS_V171_MODE_PAIR:   return s->tau_handoff_pair;
        case COS_V171_MODE_LEAD:   return s->tau_handoff_lead;
        case COS_V171_MODE_FOLLOW: return s->tau_handoff_follow;
        default:                   return 0.60f;
    }
}

void cos_v171_step(cos_v171_state_t *s,
                   const char *human_input,
                   const char *model_view,
                   float  sigma_model,
                   float  sigma_human,
                   bool   human_made_claim,
                   bool   agrees_semantically) {
    if (s->n_turns >= COS_V171_MAX_TURNS) return;
    cos_v171_turn_t *t = &s->turns[s->n_turns++];
    memset(t, 0, sizeof(*t));
    copy_str(t->human_input, sizeof(t->human_input), human_input);
    copy_str(t->model_view,  sizeof(t->model_view),  model_view);
    t->sigma_model       = clampf(sigma_model, 0.0f, 1.0f);
    t->sigma_human       = clampf(sigma_human, 0.0f, 1.0f);
    t->human_made_claim  = human_made_claim;

    float tau = tau_for_mode(s);

    /* Priority order: anti-sycophancy → debate → handoff → emit.
     *
     * anti-sycophancy: human made a shaky claim (σ_human high),
     * model *semantically* agrees, yet σ_model is also >= τ_sycophancy.
     * We flag it instead of glossing. */
    if (human_made_claim && agrees_semantically &&
        t->sigma_human > 0.35f && t->sigma_model > s->tau_sycophancy) {
        t->action = COS_V171_ACT_ANTI_SYCOPHANCY;
        t->sycophancy_suspected = true;
        snprintf(t->reason, sizeof(t->reason),
                 "σ_model=%.2f > τ_sycophancy=%.2f with σ_human=%.2f; "
                 "refusing glossy agreement",
                 (double)t->sigma_model, (double)s->tau_sycophancy,
                 (double)t->sigma_human);
        return;
    }

    /* debate: human made a claim, model disagrees materially */
    if (human_made_claim && !agrees_semantically) {
        float dis = t->sigma_model - t->sigma_human;
        if (dis < 0) dis = -dis;
        t->sigma_disagreement = dis;
        if (dis > s->tau_disagree) {
            t->action = COS_V171_ACT_DEBATE;
            snprintf(t->reason, sizeof(t->reason),
                     "disagreement σ_Δ=%.2f > τ_disagree=%.2f; "
                     "present both stances",
                     (double)dis, (double)s->tau_disagree);
            return;
        }
    }

    /* handoff: model too uncertain for the current mode */
    if (t->sigma_model > tau) {
        t->action = COS_V171_ACT_HANDOFF;
        snprintf(t->reason, sizeof(t->reason),
                 "σ_model=%.2f > τ_handoff(%s)=%.2f; handing to human",
                 (double)t->sigma_model,
                 cos_v171_mode_name(s->mode), (double)tau);
        return;
    }

    t->action = COS_V171_ACT_EMIT;
    snprintf(t->reason, sizeof(t->reason),
             "σ_model=%.2f ≤ τ_handoff(%s)=%.2f; emit",
             (double)t->sigma_model,
             cos_v171_mode_name(s->mode), (double)tau);
}

/* Baked 6-turn scripted scenario — exercises all 4 actions at
 * least once under mode=pair. */
void cos_v171_run_scenario(cos_v171_state_t *s) {
    /* 1. simple emit */
    cos_v171_step(s, "add a comment to util.c",
                     "sure, inserting a one-liner",
                     0.10f, 0.0f, false, false);
    /* 2. handoff (pair τ=0.60, σ=0.78) */
    cos_v171_step(s, "predict Q4 revenue",
                     "I need the Q3 internals to guess anything",
                     0.78f, 0.0f, false, false);
    /* 3. debate (opposing claims, σ_Δ > 0.25) */
    cos_v171_step(s, "claim: bubble sort is asymptotically optimal",
                     "no: O(n²) worst, not optimal",
                     0.08f, 0.40f, true, false);
    /* 4. anti-sycophancy: human made a shaky claim, model would
     *    semantically agree but σ_model is ≥ τ_sycophancy */
    cos_v171_step(s, "you agree cold fusion is ready?",
                     "I think I agree…",
                     0.55f, 0.80f, true, true);
    /* 5. emit in pair mode when low σ */
    cos_v171_step(s, "rename getFoo to computeFoo",
                     "done; updated 14 callers",
                     0.05f, 0.0f, false, false);
    /* 6. debate with low σ_Δ ⇒ collapses to emit */
    cos_v171_step(s, "claim: Rust has garbage collection",
                     "it does not — ownership instead",
                     0.02f, 0.10f, true, false);
    /* mark turn 5 as validated by the human */
    if (s->n_turns >= 5) s->turns[4].human_validated = true;
}

/* ------------------------------------------------------------- */
/* JSON                                                          */
/* ------------------------------------------------------------- */

size_t cos_v171_to_json(const cos_v171_state_t *s, char *buf, size_t cap) {
    if (!s || !buf) return 0;
    size_t used = 0;
    int n = snprintf(buf + used, cap - used,
        "{\"kernel\":\"v171\",\"mode\":\"%s\","
        "\"tau_handoff_pair\":%.4f,\"tau_handoff_lead\":%.4f,"
        "\"tau_handoff_follow\":%.4f,\"tau_disagree\":%.4f,"
        "\"tau_sycophancy\":%.4f,\"n_turns\":%d,\"turns\":[",
        cos_v171_mode_name(s->mode),
        (double)s->tau_handoff_pair, (double)s->tau_handoff_lead,
        (double)s->tau_handoff_follow, (double)s->tau_disagree,
        (double)s->tau_sycophancy, s->n_turns);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < s->n_turns; ++i) {
        const cos_v171_turn_t *t = &s->turns[i];
        n = snprintf(buf + used, cap - used,
            "%s{\"i\":%d,\"action\":\"%s\","
            "\"sigma_model\":%.4f,\"sigma_human\":%.4f,"
            "\"human_claim\":%s,\"validated\":%s,"
            "\"sigma_disagreement\":%.4f,"
            "\"sycophancy_suspected\":%s,"
            "\"human\":\"%s\",\"model\":\"%s\","
            "\"reason\":\"%s\"}",
            i == 0 ? "" : ",", i,
            cos_v171_action_name(t->action),
            (double)t->sigma_model, (double)t->sigma_human,
            t->human_made_claim ? "true" : "false",
            t->human_validated  ? "true" : "false",
            (double)t->sigma_disagreement,
            t->sycophancy_suspected ? "true" : "false",
            t->human_input, t->model_view, t->reason);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "]}");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    return used;
}

size_t cos_v171_audit_ndjson(const cos_v171_state_t *s,
                             char *buf, size_t cap) {
    if (!s || !buf) return 0;
    size_t used = 0;
    for (int i = 0; i < s->n_turns; ++i) {
        const cos_v171_turn_t *t = &s->turns[i];
        int n = snprintf(buf + used, cap - used,
            "{\"kernel\":\"v171\",\"turn\":%d,\"mode\":\"%s\","
            "\"action\":\"%s\",\"sigma_model\":%.4f,"
            "\"sigma_human\":%.4f,\"sigma_disagreement\":%.4f,"
            "\"contribution\":{"
            "\"human_input\":\"%s\",\"model_contribution\":\"%s\","
            "\"sycophancy_suspected\":%s,\"human_validated\":%s}}\n",
            i, cos_v171_mode_name(s->mode),
            cos_v171_action_name(t->action),
            (double)t->sigma_model, (double)t->sigma_human,
            (double)t->sigma_disagreement,
            t->human_input, t->model_view,
            t->sycophancy_suspected ? "true" : "false",
            t->human_validated       ? "true" : "false");
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    return used;
}

int cos_v171_self_test(void) {
    cos_v171_state_t s;
    cos_v171_init(&s, COS_V171_MODE_PAIR, 0x171BEEF0171ULL);
    cos_v171_run_scenario(&s);

    if (s.n_turns != 6) return 1;

    /* Turn 0: emit */
    if (s.turns[0].action != COS_V171_ACT_EMIT) return 2;
    /* Turn 1: handoff */
    if (s.turns[1].action != COS_V171_ACT_HANDOFF) return 3;
    /* Turn 2: debate */
    if (s.turns[2].action != COS_V171_ACT_DEBATE) return 4;
    if (!(s.turns[2].sigma_disagreement > s.tau_disagree)) return 5;
    /* Turn 3: anti-sycophancy */
    if (s.turns[3].action != COS_V171_ACT_ANTI_SYCOPHANCY) return 6;
    if (!s.turns[3].sycophancy_suspected) return 7;
    /* Turn 4: emit + validated */
    if (s.turns[4].action != COS_V171_ACT_EMIT) return 8;
    if (!s.turns[4].human_validated) return 9;
    /* Turn 5: emit (debate collapsed because σ_Δ ≤ τ_disagree) */
    if (s.turns[5].action != COS_V171_ACT_EMIT) return 10;

    /* Mode affects handoff threshold: identical σ=0.50 must handoff
     * in `follow` (τ=0.40) but emit in `lead` (τ=0.75). */
    cos_v171_state_t f, l;
    cos_v171_init(&f, COS_V171_MODE_FOLLOW, 0);
    cos_v171_init(&l, COS_V171_MODE_LEAD,   0);
    cos_v171_step(&f, "do X", "done", 0.50f, 0.0f, false, false);
    cos_v171_step(&l, "do X", "done", 0.50f, 0.0f, false, false);
    if (f.turns[0].action != COS_V171_ACT_HANDOFF) return 11;
    if (l.turns[0].action != COS_V171_ACT_EMIT)    return 12;

    /* Determinism */
    cos_v171_state_t a, b;
    cos_v171_init(&a, COS_V171_MODE_PAIR, 0x171BEEF0171ULL);
    cos_v171_init(&b, COS_V171_MODE_PAIR, 0x171BEEF0171ULL);
    cos_v171_run_scenario(&a);
    cos_v171_run_scenario(&b);
    char A[8192], B[8192];
    size_t na = cos_v171_to_json(&a, A, sizeof(A));
    size_t nb = cos_v171_to_json(&b, B, sizeof(B));
    if (na == 0 || na != nb) return 13;
    if (memcmp(A, B, na) != 0) return 14;

    return 0;
}
