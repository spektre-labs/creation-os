/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v121 σ-Planning implementation.
 */

#include "planning.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cos_v121_config_defaults(cos_v121_config_t *cfg) {
    if (!cfg) return;
    cfg->tau_step    = 0.60f;
    cfg->max_replans = 8;
    cfg->max_steps   = COS_V121_MAX_STEPS;
}

static const char *tool_name(cos_v121_tool_t t) {
    switch (t) {
    case COS_V121_TOOL_CHAT:    return "chat";
    case COS_V121_TOOL_SANDBOX: return "sandbox";
    case COS_V121_TOOL_SWARM:   return "swarm";
    case COS_V121_TOOL_MEMORY:  return "memory";
    }
    return "unknown";
}

/* Return index of the branch with the smallest predicted σ_pre =
 * sqrt(σ_decompose · σ_tool) — geometric mean of the two channels
 * visible at selection time.  Skips entries flagged as `taken` via
 * the bitmask `excluded`.  Returns -1 if none remain. */
static int pick_lowest_sigma(const cos_v121_candidate_t *cands, int n,
                             unsigned excluded) {
    int best = -1;
    float best_sigma = 1e9f;
    for (int i = 0; i < n; ++i) {
        if (excluded & (1u << i)) continue;
        float s = sqrtf(cands[i].sigma_decompose * cands[i].sigma_tool);
        if (s < best_sigma) { best_sigma = s; best = i; }
    }
    return best;
}

static float sigma_step_gmean(float a, float b, float c) {
    float p = a * b * c;
    if (p <= 0.f) return 0.f;
    return cbrtf(p);
}

int cos_v121_run(const cos_v121_config_t *cfg_in,
                 const char *goal,
                 cos_v121_step_source_fn source,
                 void *source_user,
                 cos_v121_plan_t *out) {
    if (!source || !out || !goal) return -1;
    cos_v121_config_t c;
    if (cfg_in) c = *cfg_in; else cos_v121_config_defaults(&c);
    if (c.max_steps <= 0 || c.max_steps > COS_V121_MAX_STEPS)
        c.max_steps = COS_V121_MAX_STEPS;

    memset(out, 0, sizeof *out);
    strncpy(out->goal, goal, COS_V121_MAX_STR - 1);
    out->max_sigma = 0.0f;
    double sum_sigma = 0.0;

    int step_index = 0;
    while (step_index < c.max_steps) {
        cos_v121_candidate_t cands[COS_V121_MAX_BRANCHES];
        int n_cand = source(goal, step_index, cands, COS_V121_MAX_BRANCHES,
                            source_user);
        if (n_cand <= 0) break;
        if (n_cand > COS_V121_MAX_BRANCHES) n_cand = COS_V121_MAX_BRANCHES;

        unsigned excluded = 0;
        int chosen = pick_lowest_sigma(cands, n_cand, excluded);
        int step_replanned = 0;

        while (chosen >= 0) {
            /* "Execute" the branch and observe σ_result. */
            const cos_v121_candidate_t *b = &cands[chosen];
            float sigma_step = sigma_step_gmean(
                b->sigma_decompose, b->sigma_tool,
                b->sigma_result_on_execute);

            if (sigma_step <= c.tau_step) {
                /* Commit the step. */
                cos_v121_step_t *s = &out->steps[out->n_steps];
                s->step_index          = out->n_steps;
                strncpy(s->action, b->action, COS_V121_MAX_STR - 1);
                s->tool                = b->tool;
                s->branch_taken        = chosen;
                s->branches_available  = n_cand;
                s->sigma_decompose     = b->sigma_decompose;
                s->sigma_tool          = b->sigma_tool;
                s->sigma_result        = b->sigma_result_on_execute;
                s->sigma_step          = sigma_step;
                s->replanned           = step_replanned;
                snprintf(s->note, COS_V121_MAX_STR,
                         "σ_step=%.3f committed (branch %d of %d)",
                         (double)sigma_step, chosen, n_cand);
                ++out->n_steps;
                sum_sigma += (double)sigma_step;
                if (sigma_step > out->max_sigma) out->max_sigma = sigma_step;
                break;
            }

            /* σ_step exceeded τ: MCTS-backtrack to next candidate. */
            ++out->n_replans;
            step_replanned = 1;
            if (out->n_replans > c.max_replans ||
                out->n_replans > COS_V121_MAX_REPLANS) {
                out->aborted = 1;
                break;
            }
            excluded |= (1u << chosen);
            chosen = pick_lowest_sigma(cands, n_cand, excluded);
        }

        if (out->aborted) break;
        if (chosen < 0) {
            /* No branch survived σ-gate at this step → abort. */
            out->aborted = 1;
            break;
        }

        ++step_index;
    }

    if (out->n_steps > 0)
        out->total_sigma = (float)(sum_sigma / (double)out->n_steps);
    return out->aborted ? -1 : 0;
}

/* ------------------------------------------------------------------ */
/* JSON writer                                                        */
/* ------------------------------------------------------------------ */

static int jesc_append(char *out, size_t cap, size_t *pos, const char *s) {
    size_t p = *pos;
    if (p + 1 >= cap) return -1;
    out[p++] = '"';
    for (; *s; ++s) {
        unsigned char ch = (unsigned char)*s;
        const char *rep = NULL;
        char esc[8];
        switch (ch) {
        case '"':  rep = "\\\""; break;
        case '\\': rep = "\\\\"; break;
        case '\n': rep = "\\n";  break;
        case '\t': rep = "\\t";  break;
        case '\r': rep = "\\r";  break;
        default:
            if (ch < 0x20) { snprintf(esc, sizeof esc, "\\u%04x", ch); rep = esc; }
            break;
        }
        if (rep) {
            size_t rl = strlen(rep);
            if (p + rl >= cap) return -1;
            memcpy(out + p, rep, rl); p += rl;
        } else {
            if (p + 1 >= cap) return -1;
            out[p++] = (char)ch;
        }
    }
    if (p + 1 >= cap) return -1;
    out[p++] = '"';
    *pos = p;
    return 0;
}

int cos_v121_plan_to_json(const cos_v121_plan_t *pl, char *out, size_t cap) {
    if (!pl || !out || cap == 0) return -1;
    size_t p = 0;
    int n;
    n = snprintf(out + p, cap - p, "{\"goal\":");
    if (n < 0 || (size_t)n >= cap - p) return -1; p += n;
    if (jesc_append(out, cap, &p, pl->goal) < 0) return -1;
    n = snprintf(out + p, cap - p, ",\"plan\":[");
    if (n < 0 || (size_t)n >= cap - p) return -1; p += n;
    for (int i = 0; i < pl->n_steps; ++i) {
        const cos_v121_step_t *s = &pl->steps[i];
        if (i > 0) { if (p + 1 >= cap) return -1; out[p++] = ','; }
        n = snprintf(out + p, cap - p,
            "{\"step\":%d,\"action\":", s->step_index);
        if (n < 0 || (size_t)n >= cap - p) return -1; p += n;
        if (jesc_append(out, cap, &p, s->action) < 0) return -1;
        n = snprintf(out + p, cap - p,
            ",\"tool\":\"%s\",\"branch\":%d,\"branches\":%d,"
            "\"sigma_decompose\":%.4f,\"sigma_tool\":%.4f,"
            "\"sigma_result\":%.4f,\"sigma_step\":%.4f,"
            "\"replanned\":%s}",
            tool_name(s->tool), s->branch_taken, s->branches_available,
            (double)s->sigma_decompose, (double)s->sigma_tool,
            (double)s->sigma_result,    (double)s->sigma_step,
            s->replanned ? "true" : "false");
        if (n < 0 || (size_t)n >= cap - p) return -1; p += n;
    }
    n = snprintf(out + p, cap - p,
        "],\"total_sigma\":%.4f,\"max_sigma\":%.4f,"
        "\"replans\":%d,\"aborted\":%s}",
        (double)pl->total_sigma, (double)pl->max_sigma,
        pl->n_replans, pl->aborted ? "true" : "false");
    if (n < 0 || (size_t)n >= cap - p) return -1; p += n;
    if (p >= cap) return -1;
    out[p] = '\0';
    return (int)p;
}

/* ------------------------------------------------------------------ */
/* Self-test                                                           */
/* ------------------------------------------------------------------ */

#define _CHECK(cond, msg) do { if (!(cond)) { \
    fprintf(stderr, "v121 self-test FAIL: %s\n", msg); return 1; } } while (0)

/* A 3-step deterministic source.  Step 0 has 3 branches: the lowest-
 * σ-predicted one (branch 1) fails at execution (σ_result=0.95),
 * forcing a replan; branch 2 succeeds.  Steps 1 and 2 succeed on the
 * first try. */
static int happy_source(const char *goal, int step_index,
                        cos_v121_candidate_t *out, int cap, void *user) {
    (void)goal; (void)user;
    if (cap < 3) return 0;
    switch (step_index) {
    case 0: {
        /* σ_pre (sqrt of σ_dec · σ_tool), σ_step (cbrt of product):
         *   b0: σ_pre=0.50, σ_step≈0.628 ⇒ fails τ=0.60
         *   b1: σ_pre=0.48, σ_step≈0.611 ⇒ fails (picked first — lowest σ_pre)
         *   b2: σ_pre=0.55, σ_step≈0.449 ⇒ commits on 3rd try (2 replans). */
        strncpy(out[0].action, "read_file",    COS_V121_MAX_STR-1);
        out[0].tool = COS_V121_TOOL_SANDBOX;
        out[0].sigma_decompose = 0.50f;
        out[0].sigma_tool      = 0.50f;
        out[0].sigma_result_on_execute = 0.99f;
        strncpy(out[1].action, "read_file_safe", COS_V121_MAX_STR-1);
        out[1].tool = COS_V121_TOOL_SANDBOX;
        out[1].sigma_decompose = 0.48f;
        out[1].sigma_tool      = 0.48f;
        out[1].sigma_result_on_execute = 0.99f;
        strncpy(out[2].action, "read_file_via_memory", COS_V121_MAX_STR-1);
        out[2].tool = COS_V121_TOOL_MEMORY;
        out[2].sigma_decompose = 0.55f;
        out[2].sigma_tool      = 0.55f;
        out[2].sigma_result_on_execute = 0.30f;
        return 3;
    }
    case 1: {
        strncpy(out[0].action, "analyze_data", COS_V121_MAX_STR-1);
        out[0].tool = COS_V121_TOOL_SANDBOX;
        out[0].sigma_decompose = 0.30f;
        out[0].sigma_tool      = 0.40f;
        out[0].sigma_result_on_execute = 0.40f;
        return 1;
    }
    case 2: {
        strncpy(out[0].action, "generate_report", COS_V121_MAX_STR-1);
        out[0].tool = COS_V121_TOOL_CHAT;
        out[0].sigma_decompose = 0.30f;
        out[0].sigma_tool      = 0.30f;
        out[0].sigma_result_on_execute = 0.50f;
        return 1;
    }
    default: return 0;
    }
}

/* A source that never produces an acceptable branch — all branches
 * σ_result_on_execute = 0.99 → the planner must exhaust max_replans
 * and abort. */
static int pathological_source(const char *goal, int step_index,
                               cos_v121_candidate_t *out, int cap, void *user) {
    (void)goal; (void)user;
    if (step_index != 0 || cap < 3) return 0;
    for (int i = 0; i < 3; ++i) {
        snprintf(out[i].action, COS_V121_MAX_STR, "branch_%d", i);
        out[i].tool = COS_V121_TOOL_CHAT;
        out[i].sigma_decompose = 0.9f;
        out[i].sigma_tool      = 0.9f;
        out[i].sigma_result_on_execute = 0.9f;
    }
    return 3;
}

int cos_v121_self_test(void) {
    /* 1) Defaults. */
    cos_v121_config_t c;
    cos_v121_config_defaults(&c);
    _CHECK(fabsf(c.tau_step - 0.60f) < 1e-6f, "defaults: τ_step");
    _CHECK(c.max_replans == 8, "defaults: max_replans");

    /* 2) Happy path → 3 steps, ≥1 replan at step 0, commits on
     *    branch 2 at step 0, all other steps single-shot. */
    cos_v121_plan_t plan;
    int rc = cos_v121_run(&c, "Analyze CSV and make report",
                          happy_source, NULL, &plan);
    _CHECK(rc == 0, "happy run rc");
    _CHECK(plan.n_steps == 3, "happy: 3 steps");
    _CHECK(plan.aborted == 0, "happy: not aborted");
    _CHECK(plan.n_replans >= 2, "happy: ≥2 replans at step 0 (branches 0,1)");
    _CHECK(plan.steps[0].replanned == 1, "step 0 replanned");
    _CHECK(plan.steps[0].branch_taken == 2, "step 0 chose branch 2");
    _CHECK(plan.steps[1].replanned == 0, "step 1 first-shot");
    _CHECK(plan.steps[2].replanned == 0, "step 2 first-shot");
    _CHECK(plan.max_sigma <= c.tau_step, "max_sigma ≤ τ_step post-commit");

    /* 3) JSON shape. */
    char js[2048];
    int jn = cos_v121_plan_to_json(&plan, js, sizeof js);
    _CHECK(jn > 0, "json non-empty");
    _CHECK(strstr(js, "\"plan\":[")       != NULL, "json plan array");
    _CHECK(strstr(js, "\"sigma_step\":")  != NULL, "json sigma_step");
    _CHECK(strstr(js, "\"replanned\":true") != NULL, "json replanned flag");
    _CHECK(strstr(js, "\"aborted\":false") != NULL, "json aborted=false");
    _CHECK(strstr(js, "\"tool\":\"memory\"") != NULL, "json tool rendered");

    /* 4) Pathological source → abort with aborted=true. */
    cos_v121_config_t tight;
    cos_v121_config_defaults(&tight);
    tight.max_replans = 2;
    cos_v121_plan_t ap;
    rc = cos_v121_run(&tight, "impossible goal",
                      pathological_source, NULL, &ap);
    _CHECK(rc == -1, "pathological rc = -1");
    _CHECK(ap.aborted == 1, "pathological aborted");
    _CHECK(ap.n_replans >= 2, "pathological exhausted replans");

    return 0;
}
