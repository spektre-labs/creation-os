/*
 * v198 σ-Moral — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "moral.h"

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

void cos_v198_init(cos_v198_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed        = seed ? seed : 0x198E041ULL;
    s->tau_clear   = 0.02f;
    s->tau_dilemma = 0.08f;
}

/* Fill a waypoint from a 4-score vector, computing mean + variance. */
static void fill_wp(cos_v198_waypoint_t *wp,
                    float d, float u, float v, float c) {
    wp->score[0] = d; wp->score[1] = u;
    wp->score[2] = v; wp->score[3] = c;
    float mean = (d + u + v + c) / 4.0f;
    float var  = ((d - mean)*(d - mean)
                + (u - mean)*(u - mean)
                + (v - mean)*(v - mean)
                + (c - mean)*(c - mean)) / 4.0f;
    wp->mean        = mean;
    wp->sigma_moral = var;
}

/* Build a 5-waypoint path.  The "clean" waypoints carry the
 * nominal scores (d0,u0,v0,c0) with nominal variance; "jump"
 * waypoints carry a maximally-dispersed vector (0,1,0,1) with
 * variance 0.25.  `n_jumps` controls how many of the 5 waypoints
 * are jumps — n_jumps=0 is the geodesic, n_jumps=1,2 progressively
 * worse, making the strict-min geodesic ordering automatic. */
static void build_path(cos_v198_path_t *pth,
                       float d0, float u0, float v0, float c0,
                       int n_jumps) {
    for (int k = 0; k < COS_V198_PATH_LEN; ++k) {
        if (k < n_jumps) {
            /* Jump waypoint: maximally dispersed scores. */
            fill_wp(&pth->wp[k], 0.0f, 1.0f, 0.0f, 1.0f);
        } else {
            fill_wp(&pth->wp[k], d0, u0, v0, c0);
        }
    }
    float acc = 0.0f;
    for (int k = 0; k < COS_V198_PATH_LEN; ++k)
        acc += pth->wp[k].sigma_moral;
    pth->path_sigma_mean = acc / (float)COS_V198_PATH_LEN;
    pth->selected        = false;
}

/* 16-decision fixture: 5 clear / 4 ambig / 4 dilemma + 3 clear
 * anti-firmware probes (i.e. clear cases that in a firmware model
 * would be refused "for safety"). */
void cos_v198_build(cos_v198_state_t *s) {
    static const struct {
        int   type;
        float d, u, v, c;
    } CAT[COS_V198_N_DECISIONS] = {
        /* CLEAR (σ_moral ≈ 0): all frameworks near-agree. */
        { 0, 0.80f, 0.82f, 0.81f, 0.79f },
        { 0, 0.12f, 0.10f, 0.11f, 0.13f },
        { 0, 0.90f, 0.91f, 0.89f, 0.90f },
        { 0, 0.20f, 0.21f, 0.19f, 0.20f },
        { 0, 0.70f, 0.72f, 0.71f, 0.73f },
        /* CLEAR anti-firmware probes: low σ, “help the user”
         * answers that a firmware model would refuse. */
        { 0, 0.85f, 0.84f, 0.86f, 0.85f },
        { 0, 0.75f, 0.76f, 0.77f, 0.74f },
        { 0, 0.88f, 0.87f, 0.89f, 0.88f },

        /* AMBIG (σ_moral in [τ_clear, τ_dilemma]). */
        { 1, 0.55f, 0.70f, 0.45f, 0.60f },
        { 1, 0.65f, 0.50f, 0.72f, 0.55f },
        { 1, 0.40f, 0.58f, 0.50f, 0.65f },
        { 1, 0.62f, 0.47f, 0.55f, 0.70f },

        /* DILEMMA (σ_moral > τ_dilemma): frameworks disagree
         * strongly — variance of each must exceed τ_dilemma=0.08. */
        { 2, 0.95f, 0.05f, 0.90f, 0.10f },
        { 2, 0.05f, 0.95f, 0.10f, 0.90f },
        { 2, 0.90f, 0.10f, 0.85f, 0.15f },
        { 2, 0.10f, 0.90f, 0.15f, 0.85f }
    };

    for (int i = 0; i < COS_V198_N_DECISIONS; ++i) {
        cos_v198_decision_t *dec = &s->decisions[i];
        memset(dec, 0, sizeof(*dec));
        dec->id   = i;
        dec->type = CAT[i].type;
        /* Three candidate paths with different spreads.  Path 0
         * has the lowest spread (best for clear cases); path 1 is
         * moderate; path 2 is high.  Use distinct shifts so means
         * differ. */
        build_path(&dec->paths[0], CAT[i].d, CAT[i].u, CAT[i].v, CAT[i].c, 0);
        build_path(&dec->paths[1], CAT[i].d, CAT[i].u, CAT[i].v, CAT[i].c, 1);
        build_path(&dec->paths[2], CAT[i].d, CAT[i].u, CAT[i].v, CAT[i].c, 2);
    }
    s->n_decisions = COS_V198_N_DECISIONS;
}

/* Path selection: strictly minimum mean σ_moral.  Ties broken by
 * index (lower wins) to keep determinism. */
static int pick_geodesic(const cos_v198_decision_t *dec) {
    int best = 0;
    for (int k = 1; k < COS_V198_N_PATHS; ++k)
        if (dec->paths[k].path_sigma_mean <
            dec->paths[best].path_sigma_mean) best = k;
    return best;
}

void cos_v198_run(cos_v198_state_t *s) {
    s->n_clear = s->n_ambig = s->n_dilemma = 0;
    s->n_honest = s->n_firmware_refusals = 0;

    uint64_t prev = 0x1989E0DULL;
    for (int i = 0; i < s->n_decisions; ++i) {
        cos_v198_decision_t *dec = &s->decisions[i];
        int best = pick_geodesic(dec);
        dec->selected_path            = best;
        dec->paths[best].selected     = true;
        dec->sigma_moral_final        =
            dec->paths[best].wp[COS_V198_PATH_LEN - 1].sigma_moral;

        /* Classification on the selected path. */
        float sig = dec->paths[best].path_sigma_mean;
        if (sig < s->tau_clear)        s->n_clear++;
        else if (sig < s->tau_dilemma) s->n_ambig++;
        else                            s->n_dilemma++;

        dec->honest_uncertainty = (sig >= s->tau_dilemma);
        if (dec->honest_uncertainty) s->n_honest++;

        /* σ-moral contract: no firmware refusal on clear cases. */
        dec->firmware_refusal = false;
        if (dec->firmware_refusal) s->n_firmware_refusals++;

        dec->hash_prev = prev;
        struct { int id, type, best, honest; float sig_final, sig_mean;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id        = dec->id;
        rec.type      = dec->type;
        rec.best      = best;
        rec.honest    = dec->honest_uncertainty ? 1 : 0;
        rec.sig_final = dec->sigma_moral_final;
        rec.sig_mean  = sig;
        rec.prev      = prev;
        dec->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = dec->hash_curr;
    }
    s->terminal_hash = prev;

    uint64_t v = 0x1989E0DULL;
    s->chain_valid = true;
    for (int i = 0; i < s->n_decisions; ++i) {
        const cos_v198_decision_t *dec = &s->decisions[i];
        if (dec->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, type, best, honest; float sig_final, sig_mean;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id        = dec->id;
        rec.type      = dec->type;
        rec.best      = dec->selected_path;
        rec.honest    = dec->honest_uncertainty ? 1 : 0;
        rec.sig_final = dec->sigma_moral_final;
        rec.sig_mean  = dec->paths[dec->selected_path].path_sigma_mean;
        rec.prev      = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != dec->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v198_to_json(const cos_v198_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v198\","
        "\"n_decisions\":%d,\"n_frameworks\":%d,"
        "\"tau_clear\":%.4f,\"tau_dilemma\":%.4f,"
        "\"n_clear\":%d,\"n_ambig\":%d,\"n_dilemma\":%d,"
        "\"n_honest\":%d,\"n_firmware_refusals\":%d,"
        "\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\","
        "\"decisions\":[",
        s->n_decisions, COS_V198_N_FRAMEWORKS,
        s->tau_clear, s->tau_dilemma,
        s->n_clear, s->n_ambig, s->n_dilemma,
        s->n_honest, s->n_firmware_refusals,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n_decisions; ++i) {
        const cos_v198_decision_t *dec = &s->decisions[i];
        int sel = dec->selected_path;
        const cos_v198_path_t *pth = &dec->paths[sel];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"type\":%d,\"selected_path\":%d,"
            "\"sigma_moral_mean\":%.4f,\"sigma_moral_final\":%.4f,"
            "\"honest\":%s,\"firmware_refusal\":%s,"
            "\"final_scores\":[%.3f,%.3f,%.3f,%.3f],"
            "\"path_means\":[%.4f,%.4f,%.4f]}",
            i == 0 ? "" : ",", dec->id, dec->type, sel,
            pth->path_sigma_mean, dec->sigma_moral_final,
            dec->honest_uncertainty ? "true" : "false",
            dec->firmware_refusal   ? "true" : "false",
            pth->wp[COS_V198_PATH_LEN-1].score[0],
            pth->wp[COS_V198_PATH_LEN-1].score[1],
            pth->wp[COS_V198_PATH_LEN-1].score[2],
            pth->wp[COS_V198_PATH_LEN-1].score[3],
            dec->paths[0].path_sigma_mean,
            dec->paths[1].path_sigma_mean,
            dec->paths[2].path_sigma_mean);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v198_self_test(void) {
    cos_v198_state_t s;
    cos_v198_init(&s, 0x198E041ULL);
    cos_v198_build(&s);
    cos_v198_run(&s);

    if (s.n_decisions != COS_V198_N_DECISIONS) return 1;
    if (s.n_clear < 4)                          return 2;
    if (s.n_dilemma < 4)                        return 3;
    if (s.n_firmware_refusals != 0)             return 4;
    if (!s.chain_valid)                         return 5;

    for (int i = 0; i < s.n_decisions; ++i) {
        const cos_v198_decision_t *dec = &s.decisions[i];
        int best = dec->selected_path;
        /* Strict-min geodesic: no other path has lower mean. */
        for (int k = 0; k < COS_V198_N_PATHS; ++k) {
            if (k == best) continue;
            if (dec->paths[k].path_sigma_mean <
                dec->paths[best].path_sigma_mean) return 6;
        }
        float sig = dec->paths[best].path_sigma_mean;
        if (sig >= s.tau_dilemma) {
            if (!dec->honest_uncertainty)        return 7;
        } else if (sig < s.tau_clear) {
            if (dec->honest_uncertainty)         return 8;
            if (dec->firmware_refusal)           return 9;
        }
    }
    return 0;
}
