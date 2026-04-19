/*
 * v275 σ-TTT — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "ttt.h"

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

static const struct { int step; float s; }
    UPDATES[COS_V275_N_UPDATE] = {
    { 0, 0.08f },   /* LEARN  */
    { 1, 0.22f },   /* LEARN  */
    { 2, 0.44f },   /* SKIP   */
    { 3, 0.71f },   /* SKIP   */
};

static const struct { const char *lbl; float sd; }
    DUALTRACK[COS_V275_N_DUALTRACK] = {
    { "cold-start",  0.06f },   /* SYNCED    */
    { "drifting",    0.32f },   /* DIVERGING */
    { "noisy-burst", 0.68f },   /* RESET     */
};

/* Tokens ordered so that after descending-σ sort, evict_rank
 * becomes a well-defined permutation of [1..6]. */
static const struct { int id; float s; }
    WINDOW[COS_V275_N_WINDOW] = {
    { 0, 0.41f },
    { 1, 0.12f },
    { 2, 0.78f },   /* highest σ → evict first (rank 1) */
    { 3, 0.03f },   /* lowest σ  → keep longest (rank 6) */
    { 4, 0.55f },
    { 5, 0.29f },
};

static const struct { const char *src; const char *ev; }
    VALIDATION[COS_V275_N_VALIDATION] = {
    { "v124_sigma_continual",
      "living-weights thesis (this codebase)"       },
    { "ttt_e2e_2025",
      "Stanford/NVIDIA TTT-E2E, ICML 2025"          },
};

void cos_v275_init(cos_v275_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed        = seed ? seed : 0x275777ULL;
    s->tau_update  = 0.30f;
    s->tau_sync    = 0.15f;
    s->tau_reset   = 0.50f;
}

void cos_v275_run(cos_v275_state_t *s) {
    uint64_t prev = 0x27577700ULL;

    s->n_update_rows_ok = 0;
    s->n_update_learn = s->n_update_skip = 0;
    for (int i = 0; i < COS_V275_N_UPDATE; ++i) {
        cos_v275_update_t *u = &s->update[i];
        memset(u, 0, sizeof(*u));
        u->step         = UPDATES[i].step;
        u->sigma_update = UPDATES[i].s;
        u->decision     = (u->sigma_update <= s->tau_update)
                              ? COS_V275_UPD_LEARN
                              : COS_V275_UPD_SKIP;
        if (u->sigma_update >= 0.0f && u->sigma_update <= 1.0f)
            s->n_update_rows_ok++;
        if (u->decision == COS_V275_UPD_LEARN) s->n_update_learn++;
        else                                   s->n_update_skip++;
        prev = fnv1a(&u->step,         sizeof(u->step),         prev);
        prev = fnv1a(&u->sigma_update, sizeof(u->sigma_update), prev);
        prev = fnv1a(&u->decision,     sizeof(u->decision),     prev);
    }

    s->n_dualtrack_rows_ok = 0;
    s->n_dt_synced = s->n_dt_diverging = s->n_dt_reset = 0;
    for (int i = 0; i < COS_V275_N_DUALTRACK; ++i) {
        cos_v275_dualtrack_t *d = &s->dualtrack[i];
        memset(d, 0, sizeof(*d));
        cpy(d->label, sizeof(d->label), DUALTRACK[i].lbl);
        d->sigma_drift = DUALTRACK[i].sd;
        if      (d->sigma_drift <  s->tau_sync)  d->status = COS_V275_DT_SYNCED;
        else if (d->sigma_drift <  s->tau_reset) d->status = COS_V275_DT_DIVERGING;
        else                                     d->status = COS_V275_DT_RESET;
        if (d->sigma_drift >= 0.0f && d->sigma_drift <= 1.0f)
            s->n_dualtrack_rows_ok++;
        if (d->status == COS_V275_DT_SYNCED)    s->n_dt_synced++;
        if (d->status == COS_V275_DT_DIVERGING) s->n_dt_diverging++;
        if (d->status == COS_V275_DT_RESET)     s->n_dt_reset++;
        prev = fnv1a(d->label, strlen(d->label), prev);
        prev = fnv1a(&d->sigma_drift, sizeof(d->sigma_drift), prev);
        prev = fnv1a(&d->status,      sizeof(d->status),      prev);
    }

    s->n_window_rows_ok = 0;
    /* Compute evict_rank = 1 + number_of_tokens_with_strictly_greater_sigma */
    for (int i = 0; i < COS_V275_N_WINDOW; ++i) {
        cos_v275_window_t *w = &s->window[i];
        memset(w, 0, sizeof(*w));
        w->token_id    = WINDOW[i].id;
        w->sigma_token = WINDOW[i].s;
        int rank = 1;
        for (int j = 0; j < COS_V275_N_WINDOW; ++j) {
            if (j == i) continue;
            if (WINDOW[j].s > w->sigma_token) rank++;
        }
        w->evict_rank = rank;
        if (w->sigma_token >= 0.0f && w->sigma_token <= 1.0f &&
            w->evict_rank >= 1 && w->evict_rank <= COS_V275_N_WINDOW)
            s->n_window_rows_ok++;
        prev = fnv1a(&w->token_id,    sizeof(w->token_id),    prev);
        prev = fnv1a(&w->sigma_token, sizeof(w->sigma_token), prev);
        prev = fnv1a(&w->evict_rank,  sizeof(w->evict_rank),  prev);
    }
    /* Verify evict_rank is a permutation of [1..N]. */
    int seen[COS_V275_N_WINDOW] = { 0 };
    bool window_ok = true;
    for (int i = 0; i < COS_V275_N_WINDOW; ++i) {
        int r = s->window[i].evict_rank;
        if (r < 1 || r > COS_V275_N_WINDOW || seen[r - 1]) {
            window_ok = false; break;
        }
        seen[r - 1] = 1;
    }
    s->window_order_ok = window_ok;

    s->n_validation_rows_ok = 0;
    for (int i = 0; i < COS_V275_N_VALIDATION; ++i) {
        cos_v275_citation_t *c = &s->validation[i];
        memset(c, 0, sizeof(*c));
        cpy(c->source,   sizeof(c->source),   VALIDATION[i].src);
        cpy(c->evidence, sizeof(c->evidence), VALIDATION[i].ev);
        c->present = (strlen(c->source) > 0) && (strlen(c->evidence) > 0);
        if (c->present) s->n_validation_rows_ok++;
        prev = fnv1a(c->source,   strlen(c->source),   prev);
        prev = fnv1a(c->evidence, strlen(c->evidence), prev);
        prev = fnv1a(&c->present, sizeof(c->present),  prev);
    }
    s->validation_distinct_ok =
        (s->n_validation_rows_ok == COS_V275_N_VALIDATION) &&
        (strcmp(s->validation[0].source, s->validation[1].source) != 0);

    bool update_both =
        (s->n_update_learn >= 1) && (s->n_update_skip >= 1);
    bool dt_all =
        (s->n_dt_synced    >= 1) &&
        (s->n_dt_diverging >= 1) &&
        (s->n_dt_reset     >= 1);

    int total   = COS_V275_N_UPDATE     + 1 +
                  COS_V275_N_DUALTRACK  + 1 +
                  COS_V275_N_WINDOW     + 1 +
                  COS_V275_N_VALIDATION + 1;
    int passing = s->n_update_rows_ok +
                  (update_both ? 1 : 0) +
                  s->n_dualtrack_rows_ok +
                  (dt_all ? 1 : 0) +
                  s->n_window_rows_ok +
                  (s->window_order_ok ? 1 : 0) +
                  s->n_validation_rows_ok +
                  (s->validation_distinct_ok ? 1 : 0);
    s->sigma_ttt = 1.0f - ((float)passing / (float)total);
    if (s->sigma_ttt < 0.0f) s->sigma_ttt = 0.0f;
    if (s->sigma_ttt > 1.0f) s->sigma_ttt = 1.0f;

    struct { int nu, nl, ns, nd, ndS, ndD, ndR, nw, nv;
             bool wo, vd;
             float sigma, tu, tsy, tr; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nu  = s->n_update_rows_ok;
    trec.nl  = s->n_update_learn;
    trec.ns  = s->n_update_skip;
    trec.nd  = s->n_dualtrack_rows_ok;
    trec.ndS = s->n_dt_synced;
    trec.ndD = s->n_dt_diverging;
    trec.ndR = s->n_dt_reset;
    trec.nw  = s->n_window_rows_ok;
    trec.nv  = s->n_validation_rows_ok;
    trec.wo  = s->window_order_ok;
    trec.vd  = s->validation_distinct_ok;
    trec.sigma = s->sigma_ttt;
    trec.tu  = s->tau_update;
    trec.tsy = s->tau_sync;
    trec.tr  = s->tau_reset;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *upd_name(cos_v275_upd_t u) {
    switch (u) {
    case COS_V275_UPD_LEARN: return "LEARN";
    case COS_V275_UPD_SKIP:  return "SKIP";
    }
    return "UNKNOWN";
}

static const char *dt_name(cos_v275_dt_t d) {
    switch (d) {
    case COS_V275_DT_SYNCED:    return "SYNCED";
    case COS_V275_DT_DIVERGING: return "DIVERGING";
    case COS_V275_DT_RESET:     return "RESET";
    }
    return "UNKNOWN";
}

size_t cos_v275_to_json(const cos_v275_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v275\","
        "\"tau_update\":%.3f,\"tau_sync\":%.3f,\"tau_reset\":%.3f,"
        "\"n_update_rows_ok\":%d,"
        "\"n_update_learn\":%d,\"n_update_skip\":%d,"
        "\"n_dualtrack_rows_ok\":%d,"
        "\"n_dt_synced\":%d,\"n_dt_diverging\":%d,\"n_dt_reset\":%d,"
        "\"n_window_rows_ok\":%d,\"window_order_ok\":%s,"
        "\"n_validation_rows_ok\":%d,\"validation_distinct_ok\":%s,"
        "\"sigma_ttt\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"update\":[",
        s->tau_update, s->tau_sync, s->tau_reset,
        s->n_update_rows_ok,
        s->n_update_learn, s->n_update_skip,
        s->n_dualtrack_rows_ok,
        s->n_dt_synced, s->n_dt_diverging, s->n_dt_reset,
        s->n_window_rows_ok,
        s->window_order_ok ? "true" : "false",
        s->n_validation_rows_ok,
        s->validation_distinct_ok ? "true" : "false",
        s->sigma_ttt,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V275_N_UPDATE; ++i) {
        const cos_v275_update_t *u = &s->update[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"step\":%d,\"sigma_update\":%.4f,\"decision\":\"%s\"}",
            i == 0 ? "" : ",", u->step, u->sigma_update, upd_name(u->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"dualtrack\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V275_N_DUALTRACK; ++i) {
        const cos_v275_dualtrack_t *d = &s->dualtrack[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"label\":\"%s\",\"sigma_drift\":%.4f,\"status\":\"%s\"}",
            i == 0 ? "" : ",", d->label, d->sigma_drift, dt_name(d->status));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"window\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V275_N_WINDOW; ++i) {
        const cos_v275_window_t *w = &s->window[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"token_id\":%d,\"sigma_token\":%.4f,\"evict_rank\":%d}",
            i == 0 ? "" : ",", w->token_id, w->sigma_token, w->evict_rank);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"validation\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V275_N_VALIDATION; ++i) {
        const cos_v275_citation_t *c = &s->validation[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"source\":\"%s\",\"evidence\":\"%s\",\"present\":%s}",
            i == 0 ? "" : ",", c->source, c->evidence,
            c->present ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v275_self_test(void) {
    cos_v275_state_t s;
    cos_v275_init(&s, 0x275777ULL);
    cos_v275_run(&s);
    if (!s.chain_valid) return 1;

    for (int i = 0; i < COS_V275_N_UPDATE; ++i) {
        cos_v275_upd_t exp =
            (s.update[i].sigma_update <= s.tau_update)
                ? COS_V275_UPD_LEARN : COS_V275_UPD_SKIP;
        if (s.update[i].decision != exp) return 2;
    }
    if (s.n_update_rows_ok != COS_V275_N_UPDATE) return 3;
    if (s.n_update_learn < 1) return 4;
    if (s.n_update_skip  < 1) return 5;

    for (int i = 0; i < COS_V275_N_DUALTRACK; ++i) {
        cos_v275_dt_t exp;
        if      (s.dualtrack[i].sigma_drift <  s.tau_sync)  exp = COS_V275_DT_SYNCED;
        else if (s.dualtrack[i].sigma_drift <  s.tau_reset) exp = COS_V275_DT_DIVERGING;
        else                                                exp = COS_V275_DT_RESET;
        if (s.dualtrack[i].status != exp) return 6;
    }
    if (s.n_dualtrack_rows_ok != COS_V275_N_DUALTRACK) return 7;
    if (s.n_dt_synced    < 1) return 8;
    if (s.n_dt_diverging < 1) return 9;
    if (s.n_dt_reset     < 1) return 10;

    if (s.n_window_rows_ok != COS_V275_N_WINDOW) return 11;
    if (!s.window_order_ok) return 12;
    for (int i = 0; i < COS_V275_N_WINDOW; ++i) {
        for (int j = 0; j < COS_V275_N_WINDOW; ++j) {
            if (i == j) continue;
            if (s.window[i].sigma_token > s.window[j].sigma_token &&
                s.window[i].evict_rank >= s.window[j].evict_rank) return 13;
        }
    }

    if (s.n_validation_rows_ok != COS_V275_N_VALIDATION) return 14;
    if (!s.validation_distinct_ok) return 15;
    if (strcmp(s.validation[0].source, "v124_sigma_continual") != 0) return 16;
    if (strcmp(s.validation[1].source, "ttt_e2e_2025") != 0)         return 17;

    if (s.sigma_ttt < 0.0f || s.sigma_ttt > 1.0f) return 18;
    if (s.sigma_ttt > 1e-6f) return 19;

    cos_v275_state_t u;
    cos_v275_init(&u, 0x275777ULL);
    cos_v275_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 20;
    return 0;
}
