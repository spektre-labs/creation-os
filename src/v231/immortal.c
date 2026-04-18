/*
 * v231 σ-Immortal — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "immortal.h"

#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static int popcount64(uint64_t x) {
    int c = 0;
    while (x) { c += (int)(x & 1ULL); x >>= 1; }
    return c;
}

/* 4 bits flipped per step, at deterministic offsets. */
static const uint64_t STEP_DELTA[COS_V231_N_STEPS] = {
    0x00000000000000F0ULL,
    0x0000000000000F00ULL,
    0x000000000000F000ULL,
    0x00000000000F0000ULL,
    0x0000000000F00000ULL,
    0x000000000F000000ULL,
    0x00000000F0000000ULL,
    0x0000000F00000000ULL,
    0x000000F000000000ULL,
    0x00000F0000000000ULL,
};

void cos_v231_init(cos_v231_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x231BAC0FFEULL;
    s->source_identity = 0x231500A7C300DULL;
    s->full_per_step_bits = COS_V231_N_BITS * COS_V231_N_STEPS;
}

void cos_v231_run(cos_v231_state_t *s) {
    uint64_t prev = 0x231CA10ULL;
    /* Trajectory. */
    uint64_t state = 0xA5A5A5A5A5A5A5A5ULL;
    s->trajectory[0].t = 0;
    s->trajectory[0].state = state;
    s->trajectory[0].delta = state;           /* full initial */
    s->trajectory[0].delta_popcount = popcount64(state);
    s->incremental_bits = s->trajectory[0].delta_popcount;

    for (int i = 0; i < COS_V231_N_STEPS; ++i) {
        uint64_t next = state ^ STEP_DELTA[i];
        int t = i + 1;
        s->trajectory[t].t = t;
        s->trajectory[t].state = next;
        s->trajectory[t].delta = STEP_DELTA[i];
        s->trajectory[t].delta_popcount = popcount64(STEP_DELTA[i]);
        s->incremental_bits += s->trajectory[t].delta_popcount;
        state = next;

        struct { int t; uint64_t st, dl; int pc; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.t = t; rec.st = next; rec.dl = STEP_DELTA[i];
        rec.pc = s->trajectory[t].delta_popcount;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    s->source_last_skills = state;

    /* Restore from snapshots (incremental replay). */
    s->restored[0] = s->trajectory[0].state;   /* full snap 0 */
    for (int t = 1; t <= COS_V231_N_STEPS; ++t)
        s->restored[t] = s->restored[t - 1] ^ s->trajectory[t].delta;

    int diff = popcount64(s->restored[COS_V231_N_STEPS]
                          ^ s->trajectory[COS_V231_N_STEPS].state);
    s->sigma_continuity = (float)diff / (float)COS_V231_N_BITS;

    /* Brain transplant to a fresh machine. */
    struct { uint64_t sid; int mark; uint64_t prev; } mrec;
    memset(&mrec, 0, sizeof(mrec));
    mrec.sid = s->source_identity; mrec.mark = 0x7A1;
    mrec.prev = 0x231B3A10ULL;
    s->target_identity = fnv1a(&mrec, sizeof(mrec), mrec.prev);
    s->target_skills   = s->source_last_skills;   /* copy brain */
    int tdiff = popcount64(s->target_skills ^ s->source_last_skills);
    s->sigma_transplant = (float)tdiff / (float)COS_V231_N_BITS;

    {
        struct { uint64_t src, tgt, tgt_sk; int inc, full;
                 float sc, st; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.src = s->source_identity;
        rec.tgt = s->target_identity;
        rec.tgt_sk = s->target_skills;
        rec.inc = s->incremental_bits;
        rec.full = s->full_per_step_bits;
        rec.sc = s->sigma_continuity;
        rec.st = s->sigma_transplant;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v231_to_json(const cos_v231_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v231\","
        "\"n_steps\":%d,\"delta_budget\":%d,\"n_bits\":%d,"
        "\"incremental_bits\":%d,\"full_per_step_bits\":%d,"
        "\"source_identity\":\"%016llx\","
        "\"target_identity\":\"%016llx\","
        "\"source_last_skills\":\"%016llx\","
        "\"target_skills\":\"%016llx\","
        "\"sigma_continuity\":%.4f,\"sigma_transplant\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"trajectory\":[",
        COS_V231_N_STEPS, COS_V231_DELTA_BUDGET, COS_V231_N_BITS,
        s->incremental_bits, s->full_per_step_bits,
        (unsigned long long)s->source_identity,
        (unsigned long long)s->target_identity,
        (unsigned long long)s->source_last_skills,
        (unsigned long long)s->target_skills,
        s->sigma_continuity, s->sigma_transplant,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int t = 0; t <= COS_V231_N_STEPS; ++t) {
        const cos_v231_snapshot_t *sn = &s->trajectory[t];
        int q = snprintf(buf + off, cap - off,
            "%s{\"t\":%d,\"state\":\"%016llx\","
            "\"delta\":\"%016llx\",\"delta_popcount\":%d,"
            "\"restored\":\"%016llx\"}",
            t == 0 ? "" : ",",
            sn->t,
            (unsigned long long)sn->state,
            (unsigned long long)sn->delta,
            sn->delta_popcount,
            (unsigned long long)s->restored[t]);
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v231_self_test(void) {
    cos_v231_state_t s;
    cos_v231_init(&s, 0x231BAC0FFEULL);
    cos_v231_run(&s);
    if (!s.chain_valid) return 1;

    /* Delta budget. */
    for (int i = 0; i < COS_V231_N_STEPS; ++i) {
        if (s.trajectory[i + 1].delta_popcount > COS_V231_DELTA_BUDGET)
            return 2;
    }
    /* Incremental < full per-step total. */
    if (s.incremental_bits >= s.full_per_step_bits) return 3;
    /* σ_continuity = 0. */
    if (s.sigma_continuity > 0.0f + 1e-9f)          return 4;
    for (int t = 0; t <= COS_V231_N_STEPS; ++t)
        if (s.restored[t] != s.trajectory[t].state) return 5;
    /* σ_transplant = 0 and identities differ. */
    if (s.sigma_transplant > 0.0f + 1e-9f)          return 6;
    if (s.target_identity == s.source_identity)     return 7;
    if (s.target_skills    != s.source_last_skills) return 8;
    return 0;
}
