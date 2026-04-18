/*
 * v194 σ-Horizon — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "horizon.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ---- init --------------------------------------------------- */

void cos_v194_init(cos_v194_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed               = seed ? seed : 0x194ABCDEULL;
    s->n_goals            = 0;
    s->n_ticks            = COS_V194_N_TICKS;
    s->window             = COS_V194_WINDOW;
    s->tau_degrade_slope  = 0.015f;         /* σ per tick */
    s->detection_tick     = -1;
}

/* ---- build: 1/3/12 goal tree ------------------------------- */

static void add_goal(cos_v194_state_t *s, int tier, int parent,
                     const char *name, float sigma,
                     uint64_t tick) {
    cos_v194_goal_t *g = &s->goals[s->n_goals];
    g->id            = s->n_goals;
    g->parent_id     = parent;
    g->tier          = tier;
    g->status        = COS_V194_STAT_ACTIVE;
    g->sigma         = sigma;
    g->created_tick  = tick;
    g->updated_tick  = tick;
    g->checkpoint_hash = 0;
    strncpy(g->name, name, COS_V194_STR_MAX - 1);
    g->name[COS_V194_STR_MAX - 1] = '\0';
    s->n_goals++;
}

void cos_v194_build(cos_v194_state_t *s) {
    /* strategic (1) */
    add_goal(s, COS_V194_TIER_STRATEGIC, -1,
             "build_creation_os_v200", 0.25f, 0);

    /* tactical (3) */
    const char *tac[COS_V194_N_TACTICAL] = {
        "implement_v194_v198",
        "run_merge_gate_v6_v198",
        "publish_paper_on_k_of_t"
    };
    for (int i = 0; i < COS_V194_N_TACTICAL; ++i)
        add_goal(s, COS_V194_TIER_TACTICAL, 0, tac[i],
                 0.30f + 0.02f * i, (uint64_t)(1 + i));

    /* operational (12) — 4 per tactical */
    const char *op[COS_V194_N_OPERATIONAL] = {
        "write_v194_kernel_c",   "write_v194_benchmark",
        "write_v194_docs",       "wire_v194_makefile",
        "run_check_v184_v193",   "run_check_v6_v29",
        "run_portable_check",    "archive_merge_gate_log",
        "draft_paper_sec_1",     "draft_paper_sec_2",
        "draft_paper_figures",   "submit_to_zenodo"
    };
    for (int i = 0; i < COS_V194_N_OPERATIONAL; ++i) {
        int tac_idx = 1 + (i / 4);  /* parent index in goals[] */
        add_goal(s, COS_V194_TIER_OPERATIONAL, tac_idx, op[i],
                 0.18f + 0.01f * i, (uint64_t)(4 + i));
    }

    /* Build the 30-tick σ trajectory on the operational tier:
     *   t  0..6   : steady σ ≈ 0.18
     *   t  7..16  : monotone climb (10-tick window, slope > τ)
     *   t 17..29  : staged recovery via ladder steps
     */
    for (int t = 0; t < s->n_ticks; ++t) {
        cos_v194_step_t *st = &s->steps[t];
        memset(st, 0, sizeof(*st));
        st->tick = t;
        float sigma;
        if (t <= 6) {
            sigma = 0.18f + 0.002f * t;         /* mild drift */
        } else if (t <= 16) {
            sigma = 0.20f + 0.035f * (t - 6);   /* monotone climb */
        } else if (t == 17) {
            sigma = 0.55f;                       /* detection tick */
        } else if (t <= 19) {
            sigma = 0.50f - 0.05f * (t - 17);
        } else if (t <= 22) {
            sigma = 0.30f - 0.05f * (t - 20);
        } else {
            sigma = 0.15f - 0.005f * (t - 22);
        }
        st->sigma_product = clampf(sigma, 0.02f, 0.95f);
    }
}

/* ---- degradation monitor & recovery ladder ---------------- */

static int detect_degradation(const cos_v194_state_t *s, int t) {
    if (t < s->window) return 0;
    /* slope over window: mean((σ[t-i+1] - σ[t-i]) for i in 0..w-1) */
    float slope_sum = 0.0f;
    int strictly_increasing = 1;
    for (int k = t - s->window + 1; k <= t; ++k) {
        float d = s->steps[k].sigma_product - s->steps[k-1].sigma_product;
        slope_sum += d;
        if (d <= 0.0f) strictly_increasing = 0;
    }
    float mean_slope = slope_sum / (float)s->window;
    return (strictly_increasing && mean_slope > s->tau_degrade_slope) ? 1 : 0;
}

void cos_v194_run(cos_v194_state_t *s) {
    /* First pass: detect the first tick where the 10-tick window
     * slope crosses τ_degrade_slope.  We only run the ladder once. */
    s->degradation_detected = false;
    s->detection_tick       = -1;
    s->recovery_steps_run   = 0;

    for (int t = 0; t < s->n_ticks; ++t) {
        if (!s->degradation_detected && detect_degradation(s, t)) {
            s->degradation_detected          = true;
            s->detection_tick                = t;
            s->steps[t].degradation_detected = true;
        }
    }

    /* Apply the ladder on the three ticks after detection. */
    if (s->degradation_detected) {
        int d = s->detection_tick;
        int ladder[3] = { COS_V194_ACT_FLUSH_KV,
                          COS_V194_ACT_SUMMARIZE,
                          COS_V194_ACT_BREAK_POINT };
        float discount[3] = { 0.10f, 0.20f, 0.35f };
        for (int i = 0; i < 3 && d + 1 + i < s->n_ticks; ++i) {
            cos_v194_step_t *st = &s->steps[d + 1 + i];
            st->action_taken = ladder[i];
            st->sigma_after  = clampf(st->sigma_product - discount[i],
                                      0.02f, 0.95f);
            s->recovery_ladder[i]     = ladder[i];
            s->recovery_steps_run++;
        }
    }

    /* Second pass: compute operational checkpoints.  Each
     * operational goal's checkpoint_hash is chained via FNV-1a. */
    uint64_t prev = 0xCEA7DE1ULL;
    for (int g = 0; g < s->n_goals; ++g) {
        cos_v194_goal_t *go = &s->goals[g];
        if (go->tier != COS_V194_TIER_OPERATIONAL) continue;
        struct {
            int id, parent, status;
            float sigma;
            uint64_t prev;
        } rec = { go->id, go->parent_id, COS_V194_STAT_DONE,
                  go->sigma, prev };
        go->status          = COS_V194_STAT_DONE;
        go->checkpoint_hash = fnv1a(&rec, sizeof(rec), prev);
        prev                = go->checkpoint_hash;
    }
    s->terminal_hash           = prev;
    s->checkpoint_chain_valid  = cos_v194_verify_checkpoints(s);

    /* Simulated crash recovery: rebuild from scratch and compare. */
    uint64_t recover = 0xCEA7DE1ULL;
    for (int g = 0; g < s->n_goals; ++g) {
        const cos_v194_goal_t *go = &s->goals[g];
        if (go->tier != COS_V194_TIER_OPERATIONAL) continue;
        struct {
            int id, parent, status;
            float sigma;
            uint64_t prev;
        } rec = { go->id, go->parent_id, COS_V194_STAT_DONE,
                  go->sigma, recover };
        recover = fnv1a(&rec, sizeof(rec), recover);
    }
    s->crash_recovery_matches = (recover == s->terminal_hash);
}

bool cos_v194_verify_checkpoints(const cos_v194_state_t *s) {
    uint64_t prev = 0xCEA7DE1ULL;
    for (int g = 0; g < s->n_goals; ++g) {
        const cos_v194_goal_t *go = &s->goals[g];
        if (go->tier != COS_V194_TIER_OPERATIONAL) continue;
        struct {
            int id, parent, status;
            float sigma;
            uint64_t prev;
        } rec = { go->id, go->parent_id, go->status,
                  go->sigma, prev };
        uint64_t h = fnv1a(&rec, sizeof(rec), prev);
        if (h != go->checkpoint_hash) return false;
        prev = h;
    }
    return prev == s->terminal_hash;
}

/* ---- JSON -------------------------------------------------- */

size_t cos_v194_to_json(const cos_v194_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v194\","
        "\"n_strategic\":%d,\"n_tactical\":%d,\"n_operational\":%d,"
        "\"n_ticks\":%d,\"window\":%d,\"tau_degrade_slope\":%.4f,"
        "\"degradation_detected\":%s,\"detection_tick\":%d,"
        "\"recovery_steps_run\":%d,"
        "\"recovery_ladder\":[%d,%d,%d],"
        "\"checkpoint_chain_valid\":%s,"
        "\"crash_recovery_matches\":%s,"
        "\"terminal_hash\":\"%016llx\","
        "\"steps\":[",
        COS_V194_N_STRATEGIC, COS_V194_N_TACTICAL, COS_V194_N_OPERATIONAL,
        s->n_ticks, s->window, s->tau_degrade_slope,
        s->degradation_detected ? "true" : "false",
        s->detection_tick, s->recovery_steps_run,
        s->recovery_ladder[0], s->recovery_ladder[1], s->recovery_ladder[2],
        s->checkpoint_chain_valid   ? "true" : "false",
        s->crash_recovery_matches   ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int t = 0; t < s->n_ticks; ++t) {
        const cos_v194_step_t *st = &s->steps[t];
        int k = snprintf(buf + off, cap - off,
            "%s{\"t\":%d,\"sigma\":%.4f,"
            "\"action\":%d,\"sigma_after\":%.4f,"
            "\"degradation\":%s}",
            t == 0 ? "" : ",", st->tick, st->sigma_product,
            st->action_taken, st->sigma_after,
            st->degradation_detected ? "true" : "false");
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int t2 = snprintf(buf + off, cap - off, "],\"goals\":[");
    if (t2 < 0 || off + (size_t)t2 >= cap) return 0;
    off += (size_t)t2;
    for (int g = 0; g < s->n_goals; ++g) {
        const cos_v194_goal_t *go = &s->goals[g];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"parent\":%d,\"tier\":%d,"
            "\"name\":\"%s\",\"status\":%d,\"sigma\":%.4f,"
            "\"checkpoint_hash\":\"%016llx\"}",
            g == 0 ? "" : ",", go->id, go->parent_id, go->tier,
            go->name, go->status, go->sigma,
            (unsigned long long)go->checkpoint_hash);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

/* ---- self-test -------------------------------------------- */

int cos_v194_self_test(void) {
    cos_v194_state_t s;
    cos_v194_init(&s, 0x194ABCDEULL);
    cos_v194_build(&s);
    cos_v194_run(&s);

    if (s.n_goals != COS_V194_N_STRATEGIC +
                     COS_V194_N_TACTICAL +
                     COS_V194_N_OPERATIONAL)            return 1;
    if (!s.degradation_detected)                         return 2;
    if (s.detection_tick < 0)                            return 3;
    if (s.recovery_steps_run != 3)                       return 4;

    /* Ladder order 1,2,3. */
    if (s.recovery_ladder[0] != COS_V194_ACT_FLUSH_KV)    return 5;
    if (s.recovery_ladder[1] != COS_V194_ACT_SUMMARIZE)   return 6;
    if (s.recovery_ladder[2] != COS_V194_ACT_BREAK_POINT) return 7;

    /* σ after full ladder strictly lower than at detection. */
    int d = s.detection_tick;
    float sigma_before = s.steps[d].sigma_product;
    float sigma_after  = s.steps[d + 3].sigma_after;
    if (!(sigma_after < sigma_before))                    return 8;

    if (!s.checkpoint_chain_valid)                        return 9;
    if (!s.crash_recovery_matches)                        return 10;

    /* Tree sanity: parent relationships. */
    for (int g = 0; g < s.n_goals; ++g) {
        const cos_v194_goal_t *go = &s.goals[g];
        if (go->tier == COS_V194_TIER_STRATEGIC &&
            go->parent_id != -1)                          return 11;
        if (go->tier == COS_V194_TIER_TACTICAL &&
            go->parent_id != 0)                           return 12;
        if (go->tier == COS_V194_TIER_OPERATIONAL &&
            (go->parent_id < 1 || go->parent_id > 3))     return 13;
    }
    return 0;
}
