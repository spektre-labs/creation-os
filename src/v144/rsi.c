/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v144 σ-RSI — implementation.
 */
#include "rsi.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void history_push(cos_v144_rsi_t *r, float score) {
    r->history[r->history_head] = score;
    r->history_head = (r->history_head + 1) % COS_V144_HISTORY;
    if (r->history_len < COS_V144_HISTORY) r->history_len++;
}

void cos_v144_init(cos_v144_rsi_t *r, float baseline_score) {
    if (!r) return;
    memset(r, 0, sizeof *r);
    if (baseline_score < 0.0f) baseline_score = 0.0f;
    if (baseline_score > 1.0f) baseline_score = 1.0f;
    r->current_score = baseline_score;
    history_push(r, baseline_score);
}

void cos_v144_resume(cos_v144_rsi_t *r) {
    if (!r) return;
    r->paused = 0;
    r->consecutive_regressions = 0;
}

float cos_v144_sigma_rsi(const cos_v144_rsi_t *r) {
    if (!r || r->history_len < 2) return 0.0f;
    double sum = 0.0;
    for (int i = 0; i < r->history_len; ++i) sum += r->history[i];
    double mean = sum / (double)r->history_len;
    double var  = 0.0;
    for (int i = 0; i < r->history_len; ++i) {
        double d = r->history[i] - mean;
        var += d * d;
    }
    var /= (double)r->history_len;           /* population variance */
    if (mean < 1e-6) mean = 1e-6;
    return (float)(var / mean);
}

int cos_v144_submit(cos_v144_rsi_t *r, float candidate_score,
                    cos_v144_cycle_report_t *out) {
    if (!r || !out) return -1;
    if (candidate_score < 0.0f) candidate_score = 0.0f;
    if (candidate_score > 1.0f) candidate_score = 1.0f;

    memset(out, 0, sizeof *out);
    out->cycle_index = r->cycle_count;
    out->score_before = r->current_score;

    /* Contract C2 fast-path: if paused, short-circuit. */
    if (r->paused) {
        r->skipped_while_paused++;
        out->score_after              = r->current_score;
        out->delta                    = 0.0f;
        out->accepted                 = 0;
        out->rolled_back              = 0;
        out->skipped_paused           = 1;
        out->consecutive_regressions  = r->consecutive_regressions;
        out->sigma_rsi                = cos_v144_sigma_rsi(r);
        /* cycle_count intentionally NOT bumped: a skipped submit is
         * not a cycle attempt, it is a user-observable "system is
         * waiting for review" event.  The v148 sovereign loop uses
         * this distinction to pace its own polling. */
        return 0;
    }

    r->cycle_count++;
    float delta = candidate_score - r->current_score;

    if (candidate_score >= r->current_score) {
        /* C1 (negated) + C3: accept. */
        r->current_score           = candidate_score;
        r->consecutive_regressions = 0;
        r->accepted_cycles++;
        history_push(r, candidate_score);
        out->accepted     = 1;
        out->rolled_back  = 0;
        out->score_after  = candidate_score;
        out->delta        = delta;
    } else {
        /* C1: regression ⇒ rollback, current_score unchanged. */
        r->consecutive_regressions++;
        r->rolled_back_cycles++;
        if (r->consecutive_regressions >= COS_V144_PAUSE_AFTER) {
            r->paused         = 1;
            out->paused_now   = 1;
        }
        out->accepted    = 0;
        out->rolled_back = 1;
        out->score_after = r->current_score;
        out->delta       = delta;   /* negative, as emitted */
    }

    out->consecutive_regressions = r->consecutive_regressions;
    out->sigma_rsi               = cos_v144_sigma_rsi(r);
    return 0;
}

int cos_v144_cycle_to_json(const cos_v144_cycle_report_t *c,
                           const cos_v144_rsi_t *r,
                           char *buf, size_t cap) {
    if (!c || !r || !buf || cap == 0) return -1;
    int rc = snprintf(buf, cap,
        "{\"cycle\":%d,\"score_before\":%.6f,\"score_after\":%.6f,"
        "\"delta\":%.6f,\"accepted\":%s,\"rolled_back\":%s,"
        "\"skipped_paused\":%s,\"paused_now\":%s,"
        "\"consecutive_regressions\":%d,\"sigma_rsi\":%.6f,"
        "\"paused\":%s,\"history_len\":%d,"
        "\"accepted_total\":%d,\"rolled_back_total\":%d,"
        "\"skipped_total\":%d}",
        c->cycle_index,
        (double)c->score_before, (double)c->score_after,
        (double)c->delta,
        c->accepted      ? "true" : "false",
        c->rolled_back   ? "true" : "false",
        c->skipped_paused ? "true" : "false",
        c->paused_now    ? "true" : "false",
        c->consecutive_regressions,
        (double)c->sigma_rsi,
        r->paused ? "true" : "false",
        r->history_len,
        r->accepted_cycles, r->rolled_back_cycles,
        r->skipped_while_paused);
    if (rc < 0 || (size_t)rc >= cap) return -1;
    return rc;
}

int cos_v144_state_to_json(const cos_v144_rsi_t *r,
                           char *buf, size_t cap) {
    if (!r || !buf || cap == 0) return -1;
    size_t off = 0;
    int rc = snprintf(buf + off, cap - off,
        "{\"v144_state\":true,\"current_score\":%.6f,"
        "\"cycle_count\":%d,\"accepted\":%d,\"rolled_back\":%d,"
        "\"skipped_while_paused\":%d,"
        "\"consecutive_regressions\":%d,\"paused\":%s,"
        "\"sigma_rsi\":%.6f,\"history\":[",
        (double)r->current_score,
        r->cycle_count, r->accepted_cycles,
        r->rolled_back_cycles, r->skipped_while_paused,
        r->consecutive_regressions,
        r->paused ? "true" : "false",
        (double)cos_v144_sigma_rsi(r));
    if (rc < 0 || (size_t)rc >= cap - off) return -1;
    off += (size_t)rc;
    /* Emit history in chronological order (oldest first). */
    for (int i = 0; i < r->history_len; ++i) {
        int idx = (r->history_head - r->history_len + i + COS_V144_HISTORY)
                  % COS_V144_HISTORY;
        rc = snprintf(buf + off, cap - off,
            "%s%.6f", i ? "," : "", (double)r->history[idx]);
        if (rc < 0 || (size_t)rc >= cap - off) return -1;
        off += (size_t)rc;
    }
    rc = snprintf(buf + off, cap - off, "]}");
    if (rc < 0 || (size_t)rc >= cap - off) return -1;
    off += (size_t)rc;
    return (int)off;
}

/* ================================================================
 * Self-test
 * ============================================================= */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v144 self-test FAIL: %s (line %d)\n", \
                (msg), __LINE__); return 1; \
    } \
} while (0)

int cos_v144_self_test(void) {
    cos_v144_rsi_t r;
    cos_v144_cycle_report_t rep;

    /* --- A. Baseline + accept path ----------------------------- */
    cos_v144_init(&r, 0.50f);
    _CHECK(r.current_score == 0.50f,   "baseline stored");
    _CHECK(r.history_len   == 1,       "baseline pushed to history");
    _CHECK(r.paused        == 0,       "not paused at start");

    _CHECK(cos_v144_submit(&r, 0.55f, &rep) == 0, "accept 0.55");
    _CHECK(rep.accepted     == 1,      "0.55 > 0.50 → accept");
    _CHECK(rep.rolled_back  == 0,      "not rolled back");
    _CHECK(fabsf(rep.delta - 0.05f) < 1e-6f, "delta = 0.05");
    _CHECK(r.current_score  == 0.55f,  "current bumped to 0.55");
    _CHECK(r.accepted_cycles == 1,     "accepted_cycles");
    _CHECK(r.consecutive_regressions == 0, "regressions reset");

    /* --- B. Rollback path --------------------------------------- */
    _CHECK(cos_v144_submit(&r, 0.52f, &rep) == 0, "submit regression");
    _CHECK(rep.accepted     == 0,      "0.52 < 0.55 → reject");
    _CHECK(rep.rolled_back  == 1,      "rolled back");
    _CHECK(r.current_score  == 0.55f,  "current preserved on rollback");
    _CHECK(r.consecutive_regressions == 1, "regressions = 1");

    /* A good cycle must reset the regression counter (C3). */
    _CHECK(cos_v144_submit(&r, 0.60f, &rep) == 0, "accept 0.60");
    _CHECK(rep.accepted == 1 && r.consecutive_regressions == 0,
           "good cycle resets regressions");

    /* --- C. 3-strike pause (C2) -------------------------------- */
    _CHECK(cos_v144_submit(&r, 0.58f, &rep) == 0, "reg 1/3");
    _CHECK(rep.rolled_back == 1, "reg 1 counted");
    _CHECK(r.paused == 0,        "not paused yet");
    _CHECK(cos_v144_submit(&r, 0.57f, &rep) == 0, "reg 2/3");
    _CHECK(r.paused == 0,        "still not paused");
    _CHECK(cos_v144_submit(&r, 0.56f, &rep) == 0, "reg 3/3");
    _CHECK(rep.paused_now == 1,  "pause triggered on 3rd regression");
    _CHECK(r.paused == 1,        "paused latched");
    _CHECK(r.consecutive_regressions == 3, "counter = 3");

    /* Submits while paused must short-circuit and NOT count as
     * cycle attempts (C2). */
    int cycles_before = r.cycle_count;
    _CHECK(cos_v144_submit(&r, 0.99f, &rep) == 0, "submit while paused");
    _CHECK(rep.skipped_paused == 1, "skipped_paused flag");
    _CHECK(r.cycle_count == cycles_before, "cycle_count unchanged");
    _CHECK(r.current_score == 0.60f, "current frozen at 0.60 during pause");

    /* --- D. Manual resume -------------------------------------- */
    cos_v144_resume(&r);
    _CHECK(r.paused == 0,                  "resume clears pause");
    _CHECK(r.consecutive_regressions == 0, "resume clears reg counter");
    _CHECK(cos_v144_submit(&r, 0.65f, &rep) == 0, "submit after resume");
    _CHECK(rep.accepted && r.current_score == 0.65f,
           "learning proceeds after resume");

    /* --- E. σ_rsi sanity --------------------------------------- */
    /* Feed a very stable sequence — σ_rsi should be tiny. */
    cos_v144_rsi_t s;
    cos_v144_init(&s, 0.70f);
    for (int i = 0; i < 9; ++i) {
        float v = 0.70f + (i & 1 ? 0.001f : -0.001f);
        cos_v144_submit(&s, v, &rep);
    }
    float sig_stable = cos_v144_sigma_rsi(&s);
    fprintf(stderr, "check-v144: σ_rsi(stable) = %.6f\n",
            (double)sig_stable);
    _CHECK(sig_stable < 1e-4f,
           "σ_rsi tiny on near-constant history");

    /* Feed a volatile sequence — σ_rsi should be meaningfully larger. */
    cos_v144_rsi_t v;
    cos_v144_init(&v, 0.50f);
    float steps[] = {0.80f, 0.50f, 0.90f, 0.55f, 0.85f,
                     0.60f, 0.95f, 0.65f, 0.95f};
    for (size_t i = 0; i < sizeof(steps)/sizeof(*steps); ++i)
        cos_v144_submit(&v, steps[i], &rep);
    float sig_vol = cos_v144_sigma_rsi(&v);
    fprintf(stderr, "check-v144: σ_rsi(volatile) = %.6f\n",
            (double)sig_vol);
    _CHECK(sig_vol > sig_stable * 50.0f,
           "σ_rsi clearly larger on volatile trajectory");

    /* --- F. JSON shape ---------------------------------------- */
    char buf[1024];
    int n = cos_v144_state_to_json(&r, buf, sizeof buf);
    _CHECK(n > 0, "state json emit");
    _CHECK(strstr(buf, "\"v144_state\":true") != NULL, "json tag");
    _CHECK(strstr(buf, "\"paused\":false")   != NULL,
           "json paused=false after resume+good cycle");

    fprintf(stderr,
        "check-v144: OK (accept + rollback + 3-strike pause + resume + σ_rsi)\n");
    return 0;
}
