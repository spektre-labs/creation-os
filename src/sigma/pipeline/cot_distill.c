/*
 * σ-CoT-distill implementation.  See cot_distill.h for contracts.
 *
 * Parser
 * ------
 * Steps are split on any of:
 *   (a) newline
 *   (b) substrings "Step " / "Thought:" / "Wait,"
 * Leading whitespace and a leading number+dot or the step marker
 * itself are stripped off each step body.  Empty steps are
 * dropped.  The substring splitters exist so we can distill CoT
 * formats shipped by popular teachers without brittle regexes.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "cot_distill.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static float cot_clip01(float x) {
    if (!(x == x)) return 0.0f;
    if (x < 0.0f)  return 0.0f;
    if (x > 1.0f)  return 1.0f;
    return x;
}

static char *cot_strndup(const char *s, size_t n) {
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    if (n) memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static char *cot_strdup_safe(const char *s) {
    if (!s) {
        char *e = (char *)malloc(1);
        if (!e) return NULL;
        e[0] = '\0';
        return e;
    }
    return cot_strndup(s, strlen(s));
}

/* Trim leading whitespace + optional "N." / "N)" / "Step N:" /
 * "Thought:" / "Wait," prefixes.  Returns new start pointer into
 * the same buffer; the caller then copies [start, end). */
static const char *cot_lstrip(const char *p, const char *end) {
    while (p < end && isspace((unsigned char)*p)) p++;
    /* "N." or "N)" prefix */
    if (p < end && isdigit((unsigned char)*p)) {
        const char *q = p;
        while (q < end && isdigit((unsigned char)*q)) q++;
        if (q < end && (*q == '.' || *q == ')')) {
            p = q + 1;
            while (p < end && isspace((unsigned char)*p)) p++;
        }
    }
    /* "Step N:" */
    if ((end - p) >= 5 && strncmp(p, "Step ", 5) == 0) {
        const char *q = p + 5;
        while (q < end && (isdigit((unsigned char)*q) ||
                           *q == ':' || *q == ' ' || *q == '.'))
            q++;
        p = q;
    }
    /* "Thought:" */
    if ((end - p) >= 8 && strncmp(p, "Thought:", 8) == 0) {
        p += 8;
        while (p < end && isspace((unsigned char)*p)) p++;
    }
    /* "Wait," */
    if ((end - p) >= 5 && strncmp(p, "Wait,", 5) == 0) {
        p += 5;
        while (p < end && isspace((unsigned char)*p)) p++;
    }
    return p;
}

static void cot_rstrip(const char *start, const char **end_io) {
    const char *e = *end_io;
    while (e > start && isspace((unsigned char)e[-1])) e--;
    *end_io = e;
}

/* Find next split-point >= p, scanning markers.  Returns a pointer
 * to the start of the next step's body, or `end` if none. */
static const char *cot_find_next_split(const char *p, const char *end) {
    for (const char *q = p; q < end; q++) {
        if (*q == '\n')                                  return q + 1;
        if (end - q >= 8 && strncmp(q, "\nThought:", 8) == 0)
            return q + 1;
        if (end - q >= 5 && strncmp(q, "Step ", 5) == 0 && q != p)
            return q;
        if (end - q >= 8 && strncmp(q, "Thought:", 8) == 0 && q != p)
            return q;
        if (end - q >= 5 && strncmp(q, "Wait,",   5) == 0 && q != p)
            return q;
    }
    return end;
}

int cos_sigma_cot_parse(const char *cot,
                        const float *sigma_seq, int n_sigma,
                        float tau_rethink,
                        cos_sigma_cot_step_t *out_steps,
                        int cap) {
    if (!out_steps || cap <= 0) return 0;
    for (int i = 0; i < cap; i++) {
        out_steps[i].body       = NULL;
        out_steps[i].sigma      = 0.0f;
        out_steps[i].is_rethink = 0;
    }
    if (!cot || !*cot) return 0;

    float tau = cot_clip01(tau_rethink > 0.0f ? tau_rethink
                                              : COS_COT_TAU_RETHINK_DEFAULT);

    const char *end = cot + strlen(cot);
    const char *p   = cot;
    int n = 0;

    while (p < end && n < cap) {
        const char *next = cot_find_next_split(p, end);
        const char *body_start = cot_lstrip(p, next);
        const char *body_end   = next;
        cot_rstrip(body_start, &body_end);
        if (body_end > body_start) {
            out_steps[n].body = cot_strndup(body_start,
                                           (size_t)(body_end - body_start));
            if (!out_steps[n].body) break;
            float s = 0.0f;
            if (sigma_seq && n < n_sigma) s = cot_clip01(sigma_seq[n]);
            out_steps[n].sigma      = s;
            out_steps[n].is_rethink = (s >= tau);
            n++;
        }
        if (next == p) next = p + 1;  /* defensive: avoid infinite loop */
        p = next;
    }
    return n;
}

float cos_sigma_cot_value(const cos_sigma_cot_step_t *steps,
                          int n_steps, float tau_confident) {
    if (!steps || n_steps <= 0) return 0.0f;
    float tau = cot_clip01(tau_confident > 0.0f ? tau_confident
                                                : COS_COT_TAU_CONFIDENT);
    float acc = 0.0f;
    for (int i = 0; i < n_steps; i++) {
        float e = steps[i].sigma - tau;
        if (e > 0.0f) acc += e;
    }
    return acc / (float)n_steps;
}

/* ------------------------------------------------------------------ */

int cos_sigma_cot_init(cos_sigma_cot_state_t *st,
                       float tau_rethink, float tau_confident) {
    if (!st) return COS_COT_ERR_ARG;
    memset(st, 0, sizeof *st);
    st->tau_rethink   = cot_clip01(tau_rethink   > 0 ? tau_rethink   : COS_COT_TAU_RETHINK_DEFAULT);
    st->tau_confident = cot_clip01(tau_confident > 0 ? tau_confident : COS_COT_TAU_CONFIDENT);
    return COS_COT_OK;
}

static void cot_record_free(cos_sigma_cot_record_t *r) {
    if (!r) return;
    free(r->input);        r->input = NULL;
    free(r->teacher_cot);  r->teacher_cot = NULL;
    free(r->teacher_answer); r->teacher_answer = NULL;
    for (int i = 0; i < COS_COT_MAX_STEPS; i++) {
        free(r->steps[i].body);
        r->steps[i].body       = NULL;
        r->steps[i].sigma      = 0.0f;
        r->steps[i].is_rethink = 0;
    }
    r->n_steps = 0;
    r->n_rethink = 0;
    r->sigma_cot_value = 0.0f;
    r->sigma_max_step  = 0.0f;
}

void cos_sigma_cot_free(cos_sigma_cot_state_t *st) {
    if (!st) return;
    for (int i = 0; i < COS_COT_RING_CAP; i++) cot_record_free(&st->ring[i]);
    float tr = st->tau_rethink, tc = st->tau_confident;
    memset(st, 0, sizeof *st);
    st->tau_rethink   = tr;
    st->tau_confident = tc;
}

int cos_sigma_cot_append(cos_sigma_cot_state_t *st,
                         const cos_sigma_cot_pair_t *pair) {
    if (!st || !pair) return COS_COT_ERR_ARG;
    st->stats.total_seen++;

    cos_sigma_cot_record_t *slot = &st->ring[st->head];
    int evicted = (slot->input != NULL);
    cot_record_free(slot);

    slot->input          = cot_strdup_safe(pair->input);
    slot->teacher_cot    = cot_strdup_safe(pair->teacher_cot);
    slot->teacher_answer = cot_strdup_safe(pair->teacher_answer);
    if (!slot->input || !slot->teacher_cot || !slot->teacher_answer) {
        cot_record_free(slot);
        return COS_COT_ERR_OOM;
    }
    slot->timestamp_ns    = pair->timestamp_ns;
    slot->insertion_order = ++st->insertion_counter;

    slot->n_steps = cos_sigma_cot_parse(pair->teacher_cot,
                                        pair->sigma_seq, pair->n_sigma,
                                        st->tau_rethink,
                                        slot->steps,
                                        COS_COT_MAX_STEPS);

    float smax = 0.0f; int rethink = 0;
    for (int i = 0; i < slot->n_steps; i++) {
        if (slot->steps[i].sigma > smax) smax = slot->steps[i].sigma;
        if (slot->steps[i].is_rethink)   rethink++;
    }
    slot->sigma_max_step  = smax;
    slot->n_rethink       = rethink;
    slot->sigma_cot_value = cos_sigma_cot_value(slot->steps, slot->n_steps,
                                                st->tau_confident);

    if (evicted) st->stats.evicted++;
    if (st->size < COS_COT_RING_CAP) st->size++;
    st->head = (st->head + 1) % COS_COT_RING_CAP;

    uint64_t n = st->size;
    float    inv = 1.0f / (float)n;
    st->stats.mean_cot_value = 0.0f;
    st->stats.mean_max_step  = 0.0f;
    st->stats.rethink_points_total = 0;
    for (int i = 0; i < COS_COT_RING_CAP; i++) {
        const cos_sigma_cot_record_t *r = &st->ring[i];
        if (!r->input) continue;
        st->stats.mean_cot_value += r->sigma_cot_value * inv;
        st->stats.mean_max_step  += r->sigma_max_step  * inv;
        st->stats.rethink_points_total += (uint64_t)r->n_rethink;
    }
    return COS_COT_OK;
}

const cos_sigma_cot_record_t *
cos_sigma_cot_at(const cos_sigma_cot_state_t *st, int idx) {
    if (!st || idx < 0 || idx >= COS_COT_RING_CAP) return NULL;
    const cos_sigma_cot_record_t *r = &st->ring[idx];
    return r->input ? r : NULL;
}

void cos_sigma_cot_stats(const cos_sigma_cot_state_t *st,
                         cos_sigma_cot_stats_t *out) {
    if (!out) return;
    if (!st) { memset(out, 0, sizeof *out); return; }
    *out = st->stats;
}

/* ------------------------------------------------------------------ */

int cos_sigma_cot_self_test(void) {
    cos_sigma_cot_state_t st;
    if (cos_sigma_cot_init(&st, 0.0f, 0.0f) != COS_COT_OK) return -1;

    /* Simple CoT with explicit rethink point. */
    const char *cot =
        "Step 1: Look up the capital of France.\n"
        "Step 2: Paris is the capital.\n"
        "Wait, let me double-check — yes, Paris is correct.\n"
        "Step 3: Final answer.";
    float sig[4] = { 0.05f, 0.08f, 0.62f, 0.04f };

    cos_sigma_cot_pair_t p = {
        .input = "What is the capital of France?",
        .teacher_cot    = cot,
        .teacher_answer = "Paris",
        .sigma_seq      = sig,
        .n_sigma        = 4,
        .timestamp_ns   = 1,
    };
    if (cos_sigma_cot_append(&st, &p) != COS_COT_OK) return -2;

    const cos_sigma_cot_record_t *r = cos_sigma_cot_at(&st, 0);
    if (!r) return -3;
    if (r->n_steps != 4) return -4;
    /* Step 3 is the only rethink (σ = 0.62 ≥ 0.50). */
    if (r->n_rethink != 1) return -5;
    if (!r->steps[2].is_rethink) return -6;
    if (r->steps[2].sigma < 0.50f) return -7;
    if (r->sigma_max_step < 0.62f - 1e-6f ||
        r->sigma_max_step > 0.62f + 1e-6f) return -8;

    /* Value > 0 because step 3 exceeds τ_confident. */
    if (!(r->sigma_cot_value > 0.0f)) return -9;
    if (r->sigma_cot_value > 1.0f)    return -10;

    /* Bound: zero sigma_seq → value exactly 0. */
    cos_sigma_cot_pair_t p2 = {
        .input = "Noop", .teacher_cot = "Step 1: quick.\nStep 2: done.",
        .teacher_answer = "x", .sigma_seq = NULL, .n_sigma = 0,
        .timestamp_ns = 2,
    };
    if (cos_sigma_cot_append(&st, &p2) != COS_COT_OK) return -11;
    const cos_sigma_cot_record_t *r2 = cos_sigma_cot_at(&st, 1);
    if (!r2) return -12;
    if (r2->n_rethink != 0) return -13;
    if (r2->sigma_cot_value > 1e-6f) return -14;

    cos_sigma_cot_free(&st);
    return 0;
}
