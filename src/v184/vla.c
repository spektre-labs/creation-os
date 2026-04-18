/*
 * v184 σ-VLA — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "vla.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---- tiny deterministic PRNG ----------------------------------- */

static uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static float frand(uint64_t *s) {
    return (float)((splitmix64(s) >> 11) / (double)(1ULL << 53));
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ---- init ------------------------------------------------------ */

void cos_v184_init(cos_v184_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed          = seed ? seed : 0x184BEADULL;
    s->tau_gate      = 0.60f;
    s->tau_ambiguous = 0.18f;
    s->n_scenes      = COS_V184_N_SCENES;
}

/* ---- scene construction ---------------------------------------- */

static void set_box(cos_v184_box_t *b, int id, const char *label,
                    float r, float g, float bl,
                    int x, int y, int w, int h) {
    b->id = id;
    snprintf(b->label, COS_V184_STR_MAX, "%s", label);
    b->color_r = r; b->color_g = g; b->color_b = bl;
    b->x = x; b->y = y; b->w = w; b->h = h;
}

/* Each scene: prompt mentions a "target_label" with target colour.
 * Candidates include the truth and distractors; some scenes are
 * ambiguous (two candidates with similar colour). */
static void make_scene(cos_v184_scene_t *sc, int id, uint64_t *rng) {
    memset(sc, 0, sizeof(*sc));
    sc->id            = id;
    sc->n_candidates  = COS_V184_N_CANDS;

    /* Deterministic scene catalogue -- id drives layout. */
    const char *labels[5]  = {"cup", "book", "phone", "bottle", "ball"};
    const float reds[5]    = {0.95f, 0.20f, 0.15f, 0.30f, 0.85f};
    const float grns[5]    = {0.15f, 0.80f, 0.20f, 0.70f, 0.20f};
    const float blus[5]    = {0.15f, 0.20f, 0.90f, 0.25f, 0.35f};
    const char *cnames[5]  = {"red", "green", "blue", "green", "red"};

    int truth = id % 5;
    snprintf(sc->target_label, COS_V184_STR_MAX, "%s", labels[truth]);
    sc->target_r = reds[truth];
    sc->target_g = grns[truth];
    sc->target_b = blus[truth];

    /* Place the truth at a rotating position. */
    int truth_slot = (id * 3) % COS_V184_N_CANDS;
    sc->truth_idx  = truth_slot;

    for (int i = 0; i < COS_V184_N_CANDS; ++i) {
        int kind = (i == truth_slot) ? truth : ((truth + 1 + i) % 5);
        float r  = reds[kind];
        float g  = grns[kind];
        float bl = blus[kind];

        /* Deterministic small colour jitter. */
        r  += 0.02f * frand(rng);
        g  += 0.02f * frand(rng);
        bl += 0.02f * frand(rng);

        set_box(&sc->candidates[i], i, labels[kind],
                clampf(r, 0, 1), clampf(g, 0, 1), clampf(bl, 0, 1),
                40 + i * 30, 60 + (i * 17) % 80, 32, 32);
    }

    /* For ambiguous scenes (every 3rd), paint a second candidate
     * with the same target colour & label. */
    bool force_ambiguous = (id % 3 == 1);
    if (force_ambiguous) {
        int alt = (truth_slot + 2) % COS_V184_N_CANDS;
        snprintf(sc->candidates[alt].label, COS_V184_STR_MAX, "%s",
                 sc->target_label);
        sc->candidates[alt].color_r = sc->target_r + 0.01f;
        sc->candidates[alt].color_g = sc->target_g + 0.01f;
        sc->candidates[alt].color_b = sc->target_b + 0.01f;
    }

    snprintf(sc->prompt, COS_V184_STR_MAX, "Take the %s %s",
             cnames[truth], labels[truth]);
}

void cos_v184_build(cos_v184_state_t *s) {
    uint64_t rng = s->seed;
    for (int i = 0; i < s->n_scenes; ++i) {
        make_scene(&s->scenes[i], i, &rng);
    }
}

/* ---- per-stage σ + dual-system run ----------------------------- */

static float color_distance(float r1, float g1, float b1,
                            float r2, float g2, float b2) {
    float dr = r1 - r2, dg = g1 - g2, db = b1 - b2;
    return sqrtf(dr*dr + dg*dg + db*db);
}

static void run_scene(cos_v184_state_t *s, cos_v184_scene_t *sc) {
    /* Perception (System 2): encode candidates, pick the closest
     * to the target colour + label. */
    int   best     = 0;
    float best_d   = 1e9f;
    int   n_plaus  = 0;
    float second_d = 1e9f;

    for (int i = 0; i < sc->n_candidates; ++i) {
        const cos_v184_box_t *b = &sc->candidates[i];
        float d = color_distance(b->color_r, b->color_g, b->color_b,
                                 sc->target_r, sc->target_g, sc->target_b);
        if (strcmp(b->label, sc->target_label) != 0) d += 0.50f;
        if (d < best_d) {
            second_d = best_d;
            best_d   = d;
            best     = i;
        } else if (d < second_d) {
            second_d = d;
        }
        if (d < 0.25f) n_plaus++;
    }
    sc->predicted_idx     = best;
    sc->grounding_correct = (best == sc->truth_idx);
    sc->n_plausible       = n_plaus;
    sc->ambiguous         = (n_plaus >= 2) ||
                            (second_d - best_d < s->tau_ambiguous);

    /* σ_perception: small if unique match, large if ambiguous. */
    float margin = second_d - best_d;
    sc->sigma_perception = clampf(0.50f - margin, 0.02f, 0.95f);

    /* σ_plan: BitNet reasoning; perfect match ⇒ low, label mismatch
     * ⇒ high. */
    sc->sigma_plan = sc->grounding_correct ? 0.10f : 0.65f;
    if (sc->ambiguous) sc->sigma_plan = 0.62f;

    /* σ_action: policy-head trajectory confidence. Reactive System
     * 1 contributes extra σ under ambiguity. */
    sc->sigma_action     = sc->ambiguous ? 0.55f : 0.08f;

    /* σ_grounding: composite of perception × ambiguity. */
    float raw = sc->sigma_perception;
    if (sc->ambiguous) raw = raw < 0.65f ? 0.65f : raw;
    sc->sigma_grounding = clampf(raw, 0.02f, 0.98f);

    /* Dual-system σ = 1 - Π(1 - σ_i). */
    float p1 = 1.0f - sc->sigma_perception;
    float p2 = 1.0f - sc->sigma_plan;
    float p3 = 1.0f - sc->sigma_action;
    sc->sigma_dual       = clampf(1.0f - (p1 * p2 * p3), 0.0f, 1.0f);

    /* Dual-system gating. */
    sc->system2_fired    = true;
    bool any_high = (sc->sigma_perception > s->tau_gate) ||
                    (sc->sigma_plan       > s->tau_gate) ||
                    (sc->sigma_action     > s->tau_gate) ||
                    (sc->sigma_grounding  > s->tau_gate);
    sc->aborted       = any_high;
    sc->asked_human   = any_high;
    sc->system1_fired = !any_high;
}

void cos_v184_run(cos_v184_state_t *s) {
    s->n_correct = s->n_aborted = s->n_asked_human = s->n_ambiguous = 0;
    double sum_d = 0, sum_p = 0, sum_pl = 0, sum_a = 0, sum_g = 0;
    for (int i = 0; i < s->n_scenes; ++i) {
        run_scene(s, &s->scenes[i]);
        const cos_v184_scene_t *sc = &s->scenes[i];
        if (sc->grounding_correct) s->n_correct++;
        if (sc->aborted)           s->n_aborted++;
        if (sc->asked_human)       s->n_asked_human++;
        if (sc->ambiguous)         s->n_ambiguous++;
        sum_d  += sc->sigma_dual;
        sum_p  += sc->sigma_perception;
        sum_pl += sc->sigma_plan;
        sum_a  += sc->sigma_action;
        sum_g  += sc->sigma_grounding;
    }
    float n = (float)s->n_scenes;
    s->mean_sigma_dual        = (float)(sum_d  / n);
    s->mean_sigma_perception  = (float)(sum_p  / n);
    s->mean_sigma_plan        = (float)(sum_pl / n);
    s->mean_sigma_action      = (float)(sum_a  / n);
    s->mean_sigma_grounding   = (float)(sum_g  / n);
}

/* ---- JSON ------------------------------------------------------ */

size_t cos_v184_to_json(const cos_v184_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v184\",\"n_scenes\":%d,"
        "\"n_correct\":%d,\"n_aborted\":%d,\"n_asked_human\":%d,"
        "\"n_ambiguous\":%d,"
        "\"tau_gate\":%.3f,"
        "\"mean_sigma_dual\":%.4f,"
        "\"mean_sigma_perception\":%.4f,"
        "\"mean_sigma_plan\":%.4f,"
        "\"mean_sigma_action\":%.4f,"
        "\"mean_sigma_grounding\":%.4f,"
        "\"scenes\":[",
        s->n_scenes, s->n_correct, s->n_aborted, s->n_asked_human,
        s->n_ambiguous, s->tau_gate,
        s->mean_sigma_dual, s->mean_sigma_perception,
        s->mean_sigma_plan, s->mean_sigma_action, s->mean_sigma_grounding);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n_scenes; ++i) {
        const cos_v184_scene_t *sc = &s->scenes[i];
        int m = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"prompt\":\"%s\",\"truth\":%d,"
            "\"pred\":%d,\"correct\":%s,\"ambiguous\":%s,"
            "\"aborted\":%s,\"asked_human\":%s,"
            "\"sigma_perception\":%.4f,\"sigma_plan\":%.4f,"
            "\"sigma_action\":%.4f,\"sigma_grounding\":%.4f,"
            "\"sigma_dual\":%.4f}",
            i == 0 ? "" : ",", sc->id, sc->prompt, sc->truth_idx,
            sc->predicted_idx,
            sc->grounding_correct ? "true" : "false",
            sc->ambiguous         ? "true" : "false",
            sc->aborted           ? "true" : "false",
            sc->asked_human       ? "true" : "false",
            sc->sigma_perception, sc->sigma_plan,
            sc->sigma_action, sc->sigma_grounding, sc->sigma_dual);
        if (m < 0 || off + (size_t)m >= cap) return 0;
        off += (size_t)m;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

/* ---- self-test ------------------------------------------------- */

int cos_v184_self_test(void) {
    cos_v184_state_t s;
    cos_v184_init(&s, 0x184BEADULL);
    cos_v184_build(&s);
    cos_v184_run(&s);

    if (s.n_correct < 8) return 1;               /* ≥ 8/10 correct */
    if (s.n_ambiguous < 1) return 2;             /* must see ambiguity */
    if (s.n_aborted < 1) return 3;               /* must abort on ambig */
    if (s.n_asked_human != s.n_aborted) return 4;

    /* σ bounds */
    for (int i = 0; i < s.n_scenes; ++i) {
        const cos_v184_scene_t *sc = &s.scenes[i];
        if (sc->sigma_perception < 0 || sc->sigma_perception > 1) return 5;
        if (sc->sigma_plan       < 0 || sc->sigma_plan       > 1) return 5;
        if (sc->sigma_action     < 0 || sc->sigma_action     > 1) return 5;
        if (sc->sigma_grounding  < 0 || sc->sigma_grounding  > 1) return 5;
        if (sc->sigma_dual       < 0 || sc->sigma_dual       > 1) return 5;
        /* dual ≥ any individual */
        if (sc->sigma_dual + 1e-6f < sc->sigma_perception) return 6;
    }

    /* Aborted scenes must have at least one σ > τ. */
    for (int i = 0; i < s.n_scenes; ++i) {
        const cos_v184_scene_t *sc = &s.scenes[i];
        if (sc->aborted) {
            float tau = s.tau_gate;
            bool any = (sc->sigma_perception > tau) ||
                       (sc->sigma_plan       > tau) ||
                       (sc->sigma_action     > tau) ||
                       (sc->sigma_grounding  > tau);
            if (!any) return 7;
        }
    }
    return 0;
}
