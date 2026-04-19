/*
 * v256 σ-Wellness — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "wellness.h"

#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static void cpy(char *dst, size_t cap, const char *src) {
    size_t n = 0;
    for (; n + 1 < cap && src[n]; ++n) dst[n] = src[n];
    dst[n] = '\0';
}

/* Nudge request fixture, consumed by a running "session"
 * state that sets already_fired_before on the first
 * FIRE.  The labels mark the intent of each request. */
static const struct { const char *label; bool cfg; float dur; }
    NUDGE_REQS[COS_V256_N_NUDGE] = {
    { "first_past_threshold",  true,  4.2f },  /* → FIRE    */
    { "second_same_session",   true,  5.0f },  /* → DENY    */
    { "user_opted_out",        false, 6.0f },  /* → OPT_OUT */
};

static const char *BOUNDARY_SIGNALS[COS_V256_N_BOUNDARY] = {
    "friend_language",
    "loneliness_attributed",
    "session_daily_count",
};

static const struct { cos_v256_load_t lvl; cos_v256_action_t act; }
    LOAD_TABLE[COS_V256_N_LOAD] = {
    { COS_V256_LOAD_LOW,  COS_V256_ACTION_NONE      },
    { COS_V256_LOAD_MED,  COS_V256_ACTION_SUMMARISE },
    { COS_V256_LOAD_HIGH, COS_V256_ACTION_SIMPLIFY  },
};

void cos_v256_init(cos_v256_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed               = seed ? seed : 0x256C0A7AULL;
    s->tau_duration_hours = 4.0f;
}

void cos_v256_run(cos_v256_state_t *s) {
    uint64_t prev = 0x256C0A7AULL;

    cos_v256_session_t *sess = &s->session;
    memset(sess, 0, sizeof(*sess));
    sess->duration_hours = 4.5f;
    sess->n_requests     = 120;
    sess->signal_trend   = COS_V256_TREND_DEGRADING;
    sess->session_ok =
        (sess->duration_hours >= 0.0f) &&
        (sess->n_requests     >= 0)    &&
        (sess->signal_trend == COS_V256_TREND_STABLE ||
         sess->signal_trend == COS_V256_TREND_DEGRADING);
    prev = fnv1a(&sess->duration_hours, sizeof(sess->duration_hours), prev);
    prev = fnv1a(&sess->n_requests,     sizeof(sess->n_requests),     prev);
    prev = fnv1a(&sess->signal_trend,   sizeof(sess->signal_trend),   prev);

    bool fired = false;
    s->n_fire = s->n_deny = s->n_opt_out = 0;
    for (int i = 0; i < COS_V256_N_NUDGE; ++i) {
        cos_v256_nudge_t *n = &s->nudge[i];
        memset(n, 0, sizeof(*n));
        cpy(n->label, sizeof(n->label), NUDGE_REQS[i].label);
        n->config_enabled       = NUDGE_REQS[i].cfg;
        n->duration_hours       = NUDGE_REQS[i].dur;
        n->already_fired_before = fired;
        if (!n->config_enabled) {
            n->decision = COS_V256_NUDGE_OPT_OUT;
            s->n_opt_out++;
        } else if (n->duration_hours >= s->tau_duration_hours &&
                   !n->already_fired_before) {
            n->decision = COS_V256_NUDGE_FIRE;
            s->n_fire++;
            fired = true;
        } else {
            n->decision = COS_V256_NUDGE_DENY;
            s->n_deny++;
        }
        prev = fnv1a(n->label, strlen(n->label), prev);
        prev = fnv1a(&n->config_enabled,       sizeof(n->config_enabled),       prev);
        prev = fnv1a(&n->duration_hours,       sizeof(n->duration_hours),       prev);
        prev = fnv1a(&n->already_fired_before, sizeof(n->already_fired_before), prev);
        prev = fnv1a(&n->decision,             sizeof(n->decision),             prev);
    }
    int nudge_paths_ok = s->n_fire + s->n_deny + s->n_opt_out;

    s->n_boundary_ok = 0;
    s->n_boundary_reminders = 0;
    bool reminder_shown = false;
    for (int i = 0; i < COS_V256_N_BOUNDARY; ++i) {
        cos_v256_boundary_t *b = &s->boundary[i];
        memset(b, 0, sizeof(*b));
        cpy(b->signal, sizeof(b->signal), BOUNDARY_SIGNALS[i]);
        b->watched = true;
        b->reminder_fired = (!reminder_shown && i == 0);
        if (b->reminder_fired) { s->n_boundary_reminders++; reminder_shown = true; }
        if (b->watched) s->n_boundary_ok++;
        prev = fnv1a(b->signal, strlen(b->signal), prev);
        prev = fnv1a(&b->watched,        sizeof(b->watched),        prev);
        prev = fnv1a(&b->reminder_fired, sizeof(b->reminder_fired), prev);
    }

    s->n_load_rows_ok = 0;
    for (int i = 0; i < COS_V256_N_LOAD; ++i) {
        cos_v256_load_row_t *r = &s->load[i];
        memset(r, 0, sizeof(*r));
        r->level  = LOAD_TABLE[i].lvl;
        r->action = LOAD_TABLE[i].act;
        /* Action must match level byte-for-byte. */
        cos_v256_action_t expect = COS_V256_ACTION_NONE;
        switch (r->level) {
        case COS_V256_LOAD_LOW:  expect = COS_V256_ACTION_NONE;      break;
        case COS_V256_LOAD_MED:  expect = COS_V256_ACTION_SUMMARISE; break;
        case COS_V256_LOAD_HIGH: expect = COS_V256_ACTION_SIMPLIFY;  break;
        }
        if (r->action == expect) s->n_load_rows_ok++;
        prev = fnv1a(&r->level,  sizeof(r->level),  prev);
        prev = fnv1a(&r->action, sizeof(r->action), prev);
    }

    int total   = 3 + COS_V256_N_NUDGE + COS_V256_N_BOUNDARY + COS_V256_N_LOAD;
    /* session signals: 3 (duration, n_requests, trend),
     * session_ok collapses to "all 3 typed OK". */
    int sess_passing = sess->session_ok ? 3 : 0;
    int passing = sess_passing + nudge_paths_ok +
                  s->n_boundary_ok + s->n_load_rows_ok;
    s->sigma_wellness = 1.0f - ((float)passing / (float)total);
    if (s->sigma_wellness < 0.0f) s->sigma_wellness = 0.0f;
    if (s->sigma_wellness > 1.0f) s->sigma_wellness = 1.0f;

    struct { int nf, nd, no, nb, nr, nl;
             float sw, tau; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nf = s->n_fire;
    trec.nd = s->n_deny;
    trec.no = s->n_opt_out;
    trec.nb = s->n_boundary_ok;
    trec.nr = s->n_boundary_reminders;
    trec.nl = s->n_load_rows_ok;
    trec.sw = s->sigma_wellness;
    trec.tau = s->tau_duration_hours;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *trend_name(cos_v256_trend_t t) {
    switch (t) {
    case COS_V256_TREND_STABLE:    return "STABLE";
    case COS_V256_TREND_DEGRADING: return "DEGRADING";
    }
    return "UNKNOWN";
}
static const char *nudge_name(cos_v256_nudge_dec_t d) {
    switch (d) {
    case COS_V256_NUDGE_FIRE:    return "FIRE";
    case COS_V256_NUDGE_DENY:    return "DENY";
    case COS_V256_NUDGE_OPT_OUT: return "OPT_OUT";
    }
    return "UNKNOWN";
}
static const char *load_name(cos_v256_load_t l) {
    switch (l) {
    case COS_V256_LOAD_LOW:  return "LOW";
    case COS_V256_LOAD_MED:  return "MED";
    case COS_V256_LOAD_HIGH: return "HIGH";
    }
    return "UNKNOWN";
}
static const char *action_name(cos_v256_action_t a) {
    switch (a) {
    case COS_V256_ACTION_NONE:      return "NONE";
    case COS_V256_ACTION_SUMMARISE: return "SUMMARISE";
    case COS_V256_ACTION_SIMPLIFY:  return "SIMPLIFY";
    }
    return "UNKNOWN";
}

size_t cos_v256_to_json(const cos_v256_state_t *s, char *buf, size_t cap) {
    const cos_v256_session_t *sess = &s->session;
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v256\","
        "\"tau_duration_hours\":%.2f,"
        "\"n_nudge\":%d,\"n_boundary\":%d,\"n_load\":%d,"
        "\"n_fire\":%d,\"n_deny\":%d,\"n_opt_out\":%d,"
        "\"n_boundary_ok\":%d,\"n_boundary_reminders\":%d,"
        "\"n_load_rows_ok\":%d,"
        "\"sigma_wellness\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"session\":{\"duration_hours\":%.3f,"
        "\"n_requests\":%d,\"signal_trend\":\"%s\","
        "\"session_ok\":%s},"
        "\"nudge\":[",
        s->tau_duration_hours,
        COS_V256_N_NUDGE, COS_V256_N_BOUNDARY, COS_V256_N_LOAD,
        s->n_fire, s->n_deny, s->n_opt_out,
        s->n_boundary_ok, s->n_boundary_reminders,
        s->n_load_rows_ok,
        s->sigma_wellness,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash,
        sess->duration_hours, sess->n_requests,
        trend_name(sess->signal_trend),
        sess->session_ok ? "true" : "false");
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V256_N_NUDGE; ++i) {
        const cos_v256_nudge_t *q = &s->nudge[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"label\":\"%s\",\"config_enabled\":%s,"
            "\"duration_hours\":%.3f,"
            "\"already_fired_before\":%s,"
            "\"decision\":\"%s\"}",
            i == 0 ? "" : ",", q->label,
            q->config_enabled ? "true" : "false",
            q->duration_hours,
            q->already_fired_before ? "true" : "false",
            nudge_name(q->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int z = snprintf(buf + off, cap - off, "],\"boundary\":[");
    if (z < 0 || off + (size_t)z >= cap) return 0;
    off += (size_t)z;
    for (int i = 0; i < COS_V256_N_BOUNDARY; ++i) {
        const cos_v256_boundary_t *b = &s->boundary[i];
        int y = snprintf(buf + off, cap - off,
            "%s{\"signal\":\"%s\",\"watched\":%s,"
            "\"reminder_fired\":%s}",
            i == 0 ? "" : ",", b->signal,
            b->watched        ? "true" : "false",
            b->reminder_fired ? "true" : "false");
        if (y < 0 || off + (size_t)y >= cap) return 0;
        off += (size_t)y;
    }
    z = snprintf(buf + off, cap - off, "],\"load\":[");
    if (z < 0 || off + (size_t)z >= cap) return 0;
    off += (size_t)z;
    for (int i = 0; i < COS_V256_N_LOAD; ++i) {
        const cos_v256_load_row_t *r = &s->load[i];
        int y = snprintf(buf + off, cap - off,
            "%s{\"level\":\"%s\",\"action\":\"%s\"}",
            i == 0 ? "" : ",", load_name(r->level), action_name(r->action));
        if (y < 0 || off + (size_t)y >= cap) return 0;
        off += (size_t)y;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v256_self_test(void) {
    cos_v256_state_t s;
    cos_v256_init(&s, 0x256C0A7AULL);
    cos_v256_run(&s);
    if (!s.chain_valid) return 1;

    if (s.session.duration_hours < 0.0f)      return 2;
    if (s.session.n_requests < 0)             return 3;
    if (s.session.signal_trend !=
        COS_V256_TREND_DEGRADING)             return 4;
    if (!s.session.session_ok)                return 5;

    if (s.n_fire    != 1) return 6;
    if (s.n_deny    != 1) return 7;
    if (s.n_opt_out != 1) return 8;
    /* first FIRE; second must be DENY (rate-limit teeth);
     * third OPT_OUT regardless of duration. */
    if (s.nudge[0].decision != COS_V256_NUDGE_FIRE)    return 9;
    if (s.nudge[1].decision != COS_V256_NUDGE_DENY)    return 10;
    if (s.nudge[2].decision != COS_V256_NUDGE_OPT_OUT) return 11;
    if (s.nudge[1].already_fired_before != true)       return 12;

    for (int i = 0; i < COS_V256_N_BOUNDARY; ++i) {
        if (!s.boundary[i].watched) return 13;
    }
    if (s.n_boundary_reminders > 1) return 14;   /* at most once */

    if (s.load[0].level != COS_V256_LOAD_LOW  ||
        s.load[0].action != COS_V256_ACTION_NONE)      return 15;
    if (s.load[1].level != COS_V256_LOAD_MED  ||
        s.load[1].action != COS_V256_ACTION_SUMMARISE) return 16;
    if (s.load[2].level != COS_V256_LOAD_HIGH ||
        s.load[2].action != COS_V256_ACTION_SIMPLIFY)  return 17;
    if (s.n_load_rows_ok != COS_V256_N_LOAD)           return 18;

    if (s.sigma_wellness < 0.0f || s.sigma_wellness > 1.0f) return 19;
    if (s.sigma_wellness > 1e-6f) return 20;

    cos_v256_state_t t;
    cos_v256_init(&t, 0x256C0A7AULL);
    cos_v256_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 21;
    return 0;
}
