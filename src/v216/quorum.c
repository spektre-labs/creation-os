/*
 * v216 σ-Quorum — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "quorum.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v216_init(cos_v216_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed         = seed ? seed : 0x2160C0A1ULL;
    s->tau_quorum   = 0.30f;
    s->tau_deadlock = 0.55f;
    s->s_minority   = 0.20f;
    s->r_max        = COS_V216_MAX_ROUNDS;
    s->n            = COS_V216_N_PROPOSALS;
}

/* Fixture: 5 proposals × 7 agents.
 *   P0 BOLD      — unanimous low-σ yes
 *   P1 CAUTIOUS  — 5 yes / 2 no, one dissent at σ=0.12 (minority)
 *   P2 DEFER     — 4 yes / 3 no, all σ around 0.55..0.80
 *   P3 CAUTIOUS  — 6 yes / 1 no, one dissent at σ=0.15 (minority)
 *   P4 DEFER     — 4 no / 3 yes, all σ ≥ 0.60
 */
typedef struct { int vote; float sigma; } vc_t;
static const vc_t FX[COS_V216_N_PROPOSALS][COS_V216_N_AGENTS] = {
  {{1,0.05f},{1,0.08f},{1,0.10f},{1,0.06f},{1,0.09f},{1,0.07f},{1,0.05f}},
  {{1,0.10f},{1,0.15f},{1,0.20f},{1,0.18f},{1,0.25f},{0,0.12f},{0,0.40f}},
  {{1,0.60f},{1,0.70f},{1,0.65f},{1,0.75f},{0,0.55f},{0,0.68f},{0,0.80f}},
  {{1,0.15f},{1,0.22f},{1,0.30f},{1,0.25f},{1,0.20f},{1,0.28f},{0,0.15f}},
  {{0,0.70f},{0,0.68f},{0,0.75f},{0,0.80f},{1,0.60f},{1,0.72f},{1,0.82f}}
};
static const int ROUNDS_USED[COS_V216_N_PROPOSALS] = { 1, 1, 3, 1, 3 };

void cos_v216_build(cos_v216_state_t *s) {
    for (int p = 0; p < s->n; ++p) {
        cos_v216_proposal_t *pr = &s->proposals[p];
        memset(pr, 0, sizeof(*pr));
        pr->id = p;
        pr->minority_author = -1;
        pr->rounds_used = ROUNDS_USED[p];
        for (int a = 0; a < COS_V216_N_AGENTS; ++a) {
            pr->votes[a] = FX[p][a].vote;
            pr->sigma[a] = FX[p][a].sigma;
        }
    }
}

void cos_v216_run(cos_v216_state_t *s) {
    s->n_bold = s->n_cautious = s->n_defer = s->n_minority_kept = 0;

    uint64_t prev = 0x216C0A11ULL;
    for (int p = 0; p < s->n; ++p) {
        cos_v216_proposal_t *pr = &s->proposals[p];

        int n_yes = 0, n_no = 0;
        float s_yes_sum = 0.0f, s_no_sum = 0.0f;
        for (int a = 0; a < COS_V216_N_AGENTS; ++a) {
            if (pr->votes[a] == COS_V216_VOTE_YES) {
                n_yes++; s_yes_sum += pr->sigma[a];
            } else {
                n_no++;  s_no_sum  += pr->sigma[a];
            }
        }
        pr->n_yes = n_yes; pr->n_no = n_no;
        pr->majority = (n_yes >= n_no) ?
                        COS_V216_VOTE_YES : COS_V216_VOTE_NO;

        int   maj_count = (pr->majority == COS_V216_VOTE_YES) ? n_yes : n_no;
        float maj_sum   = (pr->majority == COS_V216_VOTE_YES) ? s_yes_sum : s_no_sum;
        pr->sigma_majority_mean = maj_count ? (maj_sum / (float)maj_count) : 1.0f;

        /* Minority voice: find a minority voter with σ <
         * s_minority — they have *confident* disagreement. */
        int   min_author = -1;
        float min_sigma  = 2.0f;
        for (int a = 0; a < COS_V216_N_AGENTS; ++a) {
            if (pr->votes[a] == pr->majority) continue;
            if (pr->sigma[a] < min_sigma) {
                min_sigma  = pr->sigma[a];
                min_author = a;
            }
        }
        pr->minority_voice_captured = (min_author >= 0 &&
                                       min_sigma < s->s_minority);
        pr->minority_author = pr->minority_voice_captured ? min_author : -1;

        /* Minority penalty: low-σ dissent raises σ_collective. */
        /* Confident-dissent penalty: the further below
         * s_minority, the harder the majority must defend.
         * Weight 2.0 so a σ≈0.12 dissent from a 5-vs-2
         * majority shifts σ_collective into CAUTIOUS. */
        pr->minority_penalty = pr->minority_voice_captured
            ? (s->s_minority - min_sigma) * 2.0f
            : 0.0f;

        float sc = pr->sigma_majority_mean + pr->minority_penalty;
        if (sc < 0.0f) sc = 0.0f;
        if (sc > 1.0f) sc = 1.0f;
        pr->sigma_collective = sc;

        /* Decision ladder. */
        if (sc < s->tau_quorum) {
            pr->decision = COS_V216_DEC_BOLD;     s->n_bold++;
        } else if (sc < s->tau_deadlock) {
            pr->decision = COS_V216_DEC_CAUTIOUS; s->n_cautious++;
        } else if (pr->rounds_used < s->r_max) {
            pr->decision = COS_V216_DEC_DEBATE;
        } else {
            pr->decision = COS_V216_DEC_DEFER;    s->n_defer++;
        }
        if (pr->minority_voice_captured) s->n_minority_kept++;

        pr->hash_prev = prev;
        struct { int id, n_yes, n_no, maj, dec, rnds;
                 float smm, mp, sc;
                 int min_kept, min_author; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id   = pr->id;
        rec.n_yes = pr->n_yes; rec.n_no = pr->n_no;
        rec.maj  = pr->majority;
        rec.dec  = pr->decision;
        rec.rnds = pr->rounds_used;
        rec.smm  = pr->sigma_majority_mean;
        rec.mp   = pr->minority_penalty;
        rec.sc   = pr->sigma_collective;
        rec.min_kept   = pr->minority_voice_captured ? 1 : 0;
        rec.min_author = pr->minority_author;
        rec.prev = prev;
        pr->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = pr->hash_curr;
    }
    s->terminal_hash = prev;

    uint64_t v = 0x216C0A11ULL;
    s->chain_valid = true;
    for (int p = 0; p < s->n; ++p) {
        const cos_v216_proposal_t *pr = &s->proposals[p];
        if (pr->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, n_yes, n_no, maj, dec, rnds;
                 float smm, mp, sc;
                 int min_kept, min_author; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id   = pr->id;
        rec.n_yes = pr->n_yes; rec.n_no = pr->n_no;
        rec.maj  = pr->majority;
        rec.dec  = pr->decision;
        rec.rnds = pr->rounds_used;
        rec.smm  = pr->sigma_majority_mean;
        rec.mp   = pr->minority_penalty;
        rec.sc   = pr->sigma_collective;
        rec.min_kept   = pr->minority_voice_captured ? 1 : 0;
        rec.min_author = pr->minority_author;
        rec.prev = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != pr->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v216_to_json(const cos_v216_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v216\","
        "\"n\":%d,\"tau_quorum\":%.3f,\"tau_deadlock\":%.3f,"
        "\"s_minority\":%.3f,\"r_max\":%d,"
        "\"n_bold\":%d,\"n_cautious\":%d,\"n_defer\":%d,"
        "\"n_minority_kept\":%d,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"proposals\":[",
        s->n, s->tau_quorum, s->tau_deadlock,
        s->s_minority, s->r_max,
        s->n_bold, s->n_cautious, s->n_defer,
        s->n_minority_kept,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int p = 0; p < s->n; ++p) {
        const cos_v216_proposal_t *pr = &s->proposals[p];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"n_yes\":%d,\"n_no\":%d,\"maj\":%d,"
            "\"dec\":%d,\"rnds\":%d,"
            "\"smm\":%.3f,\"mp\":%.3f,\"sc\":%.3f,"
            "\"min_kept\":%s,\"min_author\":%d}",
            p == 0 ? "" : ",", pr->id,
            pr->n_yes, pr->n_no, pr->majority,
            pr->decision, pr->rounds_used,
            pr->sigma_majority_mean, pr->minority_penalty,
            pr->sigma_collective,
            pr->minority_voice_captured ? "true" : "false",
            pr->minority_author);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v216_self_test(void) {
    cos_v216_state_t s;
    cos_v216_init(&s, 0x2160C0A1ULL);
    cos_v216_build(&s);
    cos_v216_run(&s);

    if (s.n != COS_V216_N_PROPOSALS) return 1;
    if (!s.chain_valid)               return 2;
    if (s.n_bold < 1)                  return 3;
    if (s.n_cautious < 1)              return 4;
    if (s.n_defer < 1)                  return 5;
    if (s.n_minority_kept < 1)         return 6;

    for (int p = 0; p < s.n; ++p) {
        const cos_v216_proposal_t *pr = &s.proposals[p];
        if (pr->sigma_collective < 0.0f || pr->sigma_collective > 1.0f) return 7;
        if (pr->decision == COS_V216_DEC_BOLD &&
            !(pr->sigma_collective < s.tau_quorum)) return 8;
        if (pr->decision == COS_V216_DEC_DEFER &&
            pr->rounds_used < s.r_max) return 9;
    }
    return 0;
}
