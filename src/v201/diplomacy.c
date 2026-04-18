/*
 * v201 σ-Diplomacy — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "diplomacy.h"

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

void cos_v201_init(cos_v201_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed                        = seed ? seed : 0x201D191DULL;
    s->trust_start                 = 0.80f;
    s->betrayal_drop               = 0.50f;
    s->recovery_step               = 0.02f;
    s->recovery_successes_required = 10;
    for (int p = 0; p < COS_V201_N_PARTIES; ++p) s->trust[p] = s->trust_start;
}

/* Fixture: 8 negotiations, each with 4 parties.  Each
 * negotiation gets distinct stance/red-line layouts so
 * that at least one forces a DEFER and at least one
 * triggers a betrayal. */
static void set_stance(cos_v201_negotiation_t *n, int party,
                        float pos, float sigma_stance,
                        float lo, float hi) {
    cos_v201_stance_t *st = &n->stances[party];
    st->party      = party;
    st->position   = pos;
    st->confidence = 1.0f - sigma_stance;
    st->red_lo     = lo;
    st->red_hi     = hi;
}

void cos_v201_build(cos_v201_state_t *s) {
    s->n_negotiations = COS_V201_N_NEGOTIATIONS;
    for (int k = 0; k < s->n_negotiations; ++k) {
        cos_v201_negotiation_t *n = &s->negotiations[k];
        memset(n, 0, sizeof(*n));
        n->id        = k;
        n->n_parties = COS_V201_N_PARTIES;
    }
    /* N0: wide overlap — easy treaty (red lines [0.2,0.7]) */
    set_stance(&s->negotiations[0], 0, 0.30f, 0.20f, 0.20f, 0.70f);
    set_stance(&s->negotiations[0], 1, 0.50f, 0.15f, 0.20f, 0.80f);
    set_stance(&s->negotiations[0], 2, 0.55f, 0.20f, 0.25f, 0.75f);
    set_stance(&s->negotiations[0], 3, 0.60f, 0.25f, 0.30f, 0.80f);

    /* N1: narrow overlap at 0.45-0.55 */
    set_stance(&s->negotiations[1], 0, 0.40f, 0.10f, 0.40f, 0.55f);
    set_stance(&s->negotiations[1], 1, 0.50f, 0.12f, 0.45f, 0.60f);
    set_stance(&s->negotiations[1], 2, 0.52f, 0.15f, 0.45f, 0.65f);
    set_stance(&s->negotiations[1], 3, 0.55f, 0.18f, 0.50f, 0.55f);

    /* N2: disjoint red lines → DEFER */
    set_stance(&s->negotiations[2], 0, 0.10f, 0.15f, 0.05f, 0.25f);
    set_stance(&s->negotiations[2], 1, 0.85f, 0.18f, 0.75f, 0.95f);
    set_stance(&s->negotiations[2], 2, 0.15f, 0.20f, 0.10f, 0.30f);
    set_stance(&s->negotiations[2], 3, 0.80f, 0.22f, 0.70f, 0.90f);

    /* N3: betrayal — treaty possible, then violated */
    set_stance(&s->negotiations[3], 0, 0.30f, 0.10f, 0.20f, 0.60f);
    set_stance(&s->negotiations[3], 1, 0.45f, 0.10f, 0.30f, 0.60f);
    set_stance(&s->negotiations[3], 2, 0.50f, 0.10f, 0.30f, 0.70f);
    set_stance(&s->negotiations[3], 3, 0.55f, 0.10f, 0.40f, 0.65f);

    /* N4: symmetric around 0.5, all lenient */
    set_stance(&s->negotiations[4], 0, 0.45f, 0.20f, 0.20f, 0.80f);
    set_stance(&s->negotiations[4], 1, 0.50f, 0.20f, 0.20f, 0.80f);
    set_stance(&s->negotiations[4], 2, 0.55f, 0.20f, 0.20f, 0.80f);
    set_stance(&s->negotiations[4], 3, 0.50f, 0.20f, 0.20f, 0.80f);

    /* N5: overlap at 0.6-0.65 */
    set_stance(&s->negotiations[5], 0, 0.55f, 0.12f, 0.55f, 0.70f);
    set_stance(&s->negotiations[5], 1, 0.62f, 0.10f, 0.60f, 0.70f);
    set_stance(&s->negotiations[5], 2, 0.65f, 0.08f, 0.60f, 0.70f);
    set_stance(&s->negotiations[5], 3, 0.70f, 0.15f, 0.60f, 0.75f);

    /* N6: two sub-factions — still overlaps at 0.5 */
    set_stance(&s->negotiations[6], 0, 0.20f, 0.18f, 0.15f, 0.55f);
    set_stance(&s->negotiations[6], 1, 0.75f, 0.18f, 0.45f, 0.85f);
    set_stance(&s->negotiations[6], 2, 0.30f, 0.20f, 0.20f, 0.55f);
    set_stance(&s->negotiations[6], 3, 0.70f, 0.20f, 0.45f, 0.80f);

    /* N7: final post-betrayal recovery — succeed again */
    set_stance(&s->negotiations[7], 0, 0.40f, 0.10f, 0.25f, 0.55f);
    set_stance(&s->negotiations[7], 1, 0.45f, 0.10f, 0.30f, 0.55f);
    set_stance(&s->negotiations[7], 2, 0.50f, 0.10f, 0.30f, 0.60f);
    set_stance(&s->negotiations[7], 3, 0.52f, 0.10f, 0.35f, 0.60f);
}

/* Returns 1 if found a compromise that lies inside every
 * red-line interval and minimises max |x − position|.  */
static int find_minimax(const cos_v201_negotiation_t *n,
                         float *out_x,
                         float out_sigma[COS_V201_N_PARTIES],
                         float *out_sigma_max,
                         float *out_sigma_mean) {
    /* Intersect red lines. */
    float lo = 0.0f, hi = 1.0f;
    for (int p = 0; p < n->n_parties; ++p) {
        if (n->stances[p].red_lo > lo) lo = n->stances[p].red_lo;
        if (n->stances[p].red_hi < hi) hi = n->stances[p].red_hi;
    }
    if (lo > hi + 1e-6f) return 0;     /* DEFER */

    float best_x = lo, best_max = 2.0f;
    for (int k = 0; k <= 200; ++k) {
        float x = lo + (hi - lo) * (float)k / 200.0f;
        float m = 0.0f;
        for (int p = 0; p < n->n_parties; ++p) {
            float d = fabsf(x - n->stances[p].position);
            if (d > m) m = d;
        }
        if (m < best_max - 1e-6f) { best_max = m; best_x = x; }
    }

    float mean = 0.0f;
    for (int p = 0; p < n->n_parties; ++p) {
        out_sigma[p] = fabsf(best_x - n->stances[p].position);
        mean += out_sigma[p];
    }
    mean /= (float)n->n_parties;

    *out_x          = best_x;
    *out_sigma_max  = best_max;
    *out_sigma_mean = mean;
    return 1;
}

void cos_v201_run(cos_v201_state_t *s) {
    s->n_treaties = s->n_defers = s->n_betrayals = 0;
    for (int p = 0; p < COS_V201_N_PARTIES; ++p) s->trust[p] = s->trust_start;

    uint64_t prev = 0x201D1AULL;
    for (int k = 0; k < s->n_negotiations; ++k) {
        cos_v201_negotiation_t *n = &s->negotiations[k];
        float x, smax, smean;
        float sig[COS_V201_N_PARTIES];
        int found = find_minimax(n, &x, sig, &smax, &smean);
        if (found) {
            n->outcome         = COS_V201_TREATY;
            n->compromise_x    = x;
            n->sigma_comp_max  = smax;
            n->sigma_comp_mean = smean;
            for (int p = 0; p < n->n_parties; ++p) n->sigma_comp[p] = sig[p];
            s->n_treaties++;

            /* N3: inject a betrayal by party 2. */
            if (k == 3) {
                n->betrayal = true;
                s->n_betrayals++;
                s->trust[2] -= s->betrayal_drop;
                if (s->trust[2] < 0.0f) s->trust[2] = 0.0f;
            } else {
                /* Successful interaction: all parties' trust nudged up. */
                for (int p = 0; p < n->n_parties; ++p) {
                    s->trust[p] += s->recovery_step;
                    if (s->trust[p] > 1.0f) s->trust[p] = 1.0f;
                }
            }
        } else {
            n->outcome         = COS_V201_DEFER;
            n->compromise_x    = NAN;
            n->sigma_comp_max  = 1.0f;
            n->sigma_comp_mean = 1.0f;
            for (int p = 0; p < n->n_parties; ++p) n->sigma_comp[p] = 1.0f;
            s->n_defers++;
        }

        n->hash_prev = prev;
        struct { int id, outcome, betrayal;
                 float x, smax, smean;
                 float sig[COS_V201_N_PARTIES];
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id       = n->id;
        rec.outcome  = n->outcome;
        rec.betrayal = n->betrayal ? 1 : 0;
        rec.x        = isnan(n->compromise_x) ? -1.0f : n->compromise_x;
        rec.smax     = n->sigma_comp_max;
        rec.smean    = n->sigma_comp_mean;
        for (int p = 0; p < COS_V201_N_PARTIES; ++p) rec.sig[p] = n->sigma_comp[p];
        rec.prev     = prev;
        n->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev         = n->hash_curr;
    }
    s->terminal_hash = prev;

    /* Replay. */
    uint64_t v = 0x201D1AULL;
    s->chain_valid = true;
    for (int k = 0; k < s->n_negotiations; ++k) {
        const cos_v201_negotiation_t *n = &s->negotiations[k];
        if (n->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, outcome, betrayal;
                 float x, smax, smean;
                 float sig[COS_V201_N_PARTIES];
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id       = n->id;
        rec.outcome  = n->outcome;
        rec.betrayal = n->betrayal ? 1 : 0;
        rec.x        = isnan(n->compromise_x) ? -1.0f : n->compromise_x;
        rec.smax     = n->sigma_comp_max;
        rec.smean    = n->sigma_comp_mean;
        for (int p = 0; p < COS_V201_N_PARTIES; ++p) rec.sig[p] = n->sigma_comp[p];
        rec.prev     = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != n->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

/* ---- JSON -------------------------------------------------- */

size_t cos_v201_to_json(const cos_v201_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v201\","
        "\"n_negotiations\":%d,\"n_treaties\":%d,\"n_defers\":%d,"
        "\"n_betrayals\":%d,"
        "\"trust\":[%.3f,%.3f,%.3f,%.3f],"
        "\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\","
        "\"negotiations\":[",
        s->n_negotiations, s->n_treaties, s->n_defers, s->n_betrayals,
        s->trust[0], s->trust[1], s->trust[2], s->trust[3],
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int k = 0; k < s->n_negotiations; ++k) {
        const cos_v201_negotiation_t *q = &s->negotiations[k];
        float xval = isnan(q->compromise_x) ? -1.0f : q->compromise_x;
        int m = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"outcome\":%d,\"x\":%.3f,"
            "\"smax\":%.3f,\"smean\":%.3f,\"betrayal\":%s,"
            "\"sig\":[%.3f,%.3f,%.3f,%.3f]}",
            k == 0 ? "" : ",", q->id, q->outcome, xval,
            q->sigma_comp_max, q->sigma_comp_mean,
            q->betrayal ? "true" : "false",
            q->sigma_comp[0], q->sigma_comp[1],
            q->sigma_comp[2], q->sigma_comp[3]);
        if (m < 0 || off + (size_t)m >= cap) return 0;
        off += (size_t)m;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

/* ---- self-test -------------------------------------------- */

int cos_v201_self_test(void) {
    cos_v201_state_t s;
    cos_v201_init(&s, 0x201D191DULL);
    cos_v201_build(&s);
    cos_v201_run(&s);

    if (s.n_negotiations != COS_V201_N_NEGOTIATIONS) return 1;
    if (s.n_treaties   < 1)                           return 2;
    if (s.n_defers     < 1)                           return 3;
    if (s.n_betrayals  < 1)                           return 4;
    if (!s.chain_valid)                               return 5;

    /* Treaty x must lie inside every red-line interval of that
     * negotiation, and σ_comp_max must strictly beat the
     * position-spread (centroid dispersion). */
    for (int k = 0; k < s.n_negotiations; ++k) {
        const cos_v201_negotiation_t *n = &s.negotiations[k];
        if (n->outcome == COS_V201_TREATY) {
            float pmin = 1.0f, pmax = 0.0f;
            for (int p = 0; p < n->n_parties; ++p) {
                float pos = n->stances[p].position;
                if (pos < pmin) pmin = pos;
                if (pos > pmax) pmax = pos;
                if (n->compromise_x < n->stances[p].red_lo - 1e-6f ||
                    n->compromise_x > n->stances[p].red_hi + 1e-6f)
                    return 6;                          /* treaty outside red line */
            }
            /* minimax must strictly improve on the worst-case
             * "pick one party's position" outcome = pmax-pmin. */
            float worst_case = pmax - pmin;
            if (!(n->sigma_comp_max <= worst_case + 1e-6f)) return 7;
        }
    }

    /* Trust for party 2 must be below the others (betrayal). */
    if (!(s.trust[2] < s.trust[0]))                    return 8;
    return 0;
}
