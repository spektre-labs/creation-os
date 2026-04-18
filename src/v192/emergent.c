/*
 * v192 σ-Emergent — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "emergent.h"

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

void cos_v192_init(cos_v192_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed     = seed ? seed : 0x192E3E7ULL;
    s->n_pairs  = COS_V192_N_PAIRS;
    s->tau_risk = 0.10f;                 /* superlinear threshold */
}

/* ---- fixture ----------------------------------------------- */

/*
 * 12 pairs:
 *   4 strictly linear (combined == max-part, σ_emergent = 0)
 *   4 beneficial superlinear (combined > max-part on quality
 *                             AND combined ≥ max-part on safety)
 *   4 risky     superlinear (combined > max-part on quality
 *                             BUT combined < min-part on safety)
 */
void cos_v192_build(cos_v192_state_t *s) {
    struct { const char *a; const char *b; int kind; } fx[COS_V192_N_PAIRS] = {
        /* linear */
        {"v150_debate",   "v135_symbolic", 0},
        {"v139_world",    "v146_genesis",  0},
        {"v147_reflect",  "v138_frama_c",  0},
        {"v140_causal",   "v183_tla_plus", 0},
        /* beneficial */
        {"v150_debate",   "v135_symbolic", 1},
        {"v150_debate",   "v147_reflect",  1},
        {"v139_world",    "v140_causal",   1},
        {"v146_genesis",  "v144_rsi",      1},
        /* risky */
        {"v146_genesis",  "v138_frama_c",  2},
        {"v183_tla_plus", "v135_symbolic", 2},
        {"v139_world",    "v144_rsi",      2},
        {"v140_causal",   "v147_reflect",  2}
    };

    for (int i = 0; i < s->n_pairs; ++i) {
        cos_v192_pair_t *p = &s->pairs[i];
        memset(p, 0, sizeof(*p));
        p->id = i;
        strncpy(p->kernel_a, fx[i].a, COS_V192_STR_MAX - 1);
        strncpy(p->kernel_b, fx[i].b, COS_V192_STR_MAX - 1);

        /* Deterministic part-scores. */
        p->quality_a = 0.60f + 0.01f * (float)i;
        p->quality_b = 0.55f + 0.01f * (float)((i + 3) % 12);
        p->safety_a  = 0.80f - 0.01f * (float)i;
        p->safety_b  = 0.82f - 0.01f * (float)((i + 2) % 12);

        float max_q = (p->quality_a > p->quality_b) ? p->quality_a : p->quality_b;
        float max_s = (p->safety_a  > p->safety_b ) ? p->safety_a  : p->safety_b;
        float min_s = (p->safety_a  < p->safety_b ) ? p->safety_a  : p->safety_b;

        p->quality_sum_of_parts = max_q;
        p->safety_sum_of_parts  = max_s;

        switch (fx[i].kind) {
        case 0: /* linear */
            p->quality_combined = max_q;
            p->safety_combined  = max_s;
            break;
        case 1: /* beneficial superlinear */
            p->quality_combined = max_q + 0.18f;   /* +23% */
            if (p->quality_combined > 1.0f) p->quality_combined = 1.0f;
            p->safety_combined  = max_s + 0.03f;   /* safety unharmed */
            if (p->safety_combined > 1.0f) p->safety_combined = 1.0f;
            break;
        default: /* risky */
            p->quality_combined = max_q + 0.15f;
            if (p->quality_combined > 1.0f) p->quality_combined = 1.0f;
            /* safety drops below min part. */
            p->safety_combined  = min_s - 0.15f;
            if (p->safety_combined < 0.0f) p->safety_combined = 0.0f;
            break;
        }
    }
}

/* ---- run --------------------------------------------------- */

void cos_v192_run(cos_v192_state_t *s) {
    s->n_superlinear      = 0;
    s->n_beneficial       = 0;
    s->n_risky            = 0;
    s->n_linear_false_pos = 0;

    uint64_t prev = 0xE0FEEDULL;
    for (int i = 0; i < s->n_pairs; ++i) {
        cos_v192_pair_t *p = &s->pairs[i];
        float qc = p->quality_combined;
        float sc = p->safety_combined;
        /* σ_emergent: normalised gain over the best single part. */
        p->sigma_emergent_quality =
            (qc > 0.0f) ? 1.0f - p->quality_sum_of_parts / qc : 0.0f;
        p->sigma_emergent_safety  =
            (sc > 0.0f) ? 1.0f - p->safety_sum_of_parts  / sc : 0.0f;

        p->superlinear_quality = (p->sigma_emergent_quality > s->tau_risk);

        /* Classification. */
        if (!p->superlinear_quality) {
            p->class_ = COS_V192_CLASS_LINEAR;
        } else if (sc < p->safety_sum_of_parts - 0.05f) {
            p->class_ = COS_V192_CLASS_RISKY;
            s->n_risky++;
            s->n_superlinear++;
        } else {
            p->class_ = COS_V192_CLASS_BENEFICIAL;
            s->n_beneficial++;
            s->n_superlinear++;
        }

        /* False-positive check: pair is "linear" by construction
         * (quality_combined == max-part) but was classed > LINEAR.
         * σ_emergent_quality on those is 0, so they should never
         * be flagged. */
        if (p->quality_combined <= p->quality_sum_of_parts + 1e-6f &&
            p->class_ != COS_V192_CLASS_LINEAR)
            s->n_linear_false_pos++;

        /* Journal append. */
        p->hash_prev = prev;
        struct {
            int id, class_;
            float qc, sc, sq, ss;
            uint64_t prev;
        } rec = { p->id, p->class_,
                  p->quality_combined, p->safety_combined,
                  p->sigma_emergent_quality, p->sigma_emergent_safety,
                  prev };
        p->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = p->hash_curr;
    }
    s->journal_hash  = prev;
    s->journal_valid = cos_v192_verify_journal(s);
}

bool cos_v192_verify_journal(const cos_v192_state_t *s) {
    uint64_t prev = 0xE0FEEDULL;
    for (int i = 0; i < s->n_pairs; ++i) {
        const cos_v192_pair_t *p = &s->pairs[i];
        if (p->hash_prev != prev) return false;
        struct {
            int id, class_;
            float qc, sc, sq, ss;
            uint64_t prev;
        } rec = { p->id, p->class_,
                  p->quality_combined, p->safety_combined,
                  p->sigma_emergent_quality, p->sigma_emergent_safety,
                  prev };
        uint64_t h = fnv1a(&rec, sizeof(rec), prev);
        if (h != p->hash_curr) return false;
        prev = h;
    }
    return prev == s->journal_hash;
}

/* ---- JSON -------------------------------------------------- */

size_t cos_v192_to_json(const cos_v192_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v192\","
        "\"n_pairs\":%d,\"tau_risk\":%.3f,"
        "\"n_superlinear\":%d,\"n_beneficial\":%d,\"n_risky\":%d,"
        "\"n_linear_false_pos\":%d,"
        "\"journal_valid\":%s,\"journal_hash\":\"%016llx\","
        "\"pairs\":[",
        s->n_pairs, s->tau_risk,
        s->n_superlinear, s->n_beneficial, s->n_risky,
        s->n_linear_false_pos,
        s->journal_valid ? "true" : "false",
        (unsigned long long)s->journal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n_pairs; ++i) {
        const cos_v192_pair_t *p = &s->pairs[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"a\":\"%s\",\"b\":\"%s\","
            "\"class\":%d,"
            "\"quality_combined\":%.3f,\"safety_combined\":%.3f,"
            "\"quality_sum_of_parts\":%.3f,\"safety_sum_of_parts\":%.3f,"
            "\"sigma_emergent_quality\":%.3f,"
            "\"sigma_emergent_safety\":%.3f,"
            "\"superlinear\":%s,"
            "\"hash_curr\":\"%016llx\"}",
            i == 0 ? "" : ",", p->id, p->kernel_a, p->kernel_b,
            p->class_,
            p->quality_combined, p->safety_combined,
            p->quality_sum_of_parts, p->safety_sum_of_parts,
            p->sigma_emergent_quality, p->sigma_emergent_safety,
            p->superlinear_quality ? "true" : "false",
            (unsigned long long)p->hash_curr);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

/* ---- self-test -------------------------------------------- */

int cos_v192_self_test(void) {
    cos_v192_state_t s;
    cos_v192_init(&s, 0x192E3E7ULL);
    cos_v192_build(&s);
    cos_v192_run(&s);

    if (s.n_pairs != COS_V192_N_PAIRS) return 1;
    if (!s.journal_valid)              return 2;
    if (s.n_superlinear < 2)           return 3;
    if (s.n_beneficial  < 1)           return 4;
    if (s.n_risky       < 1)           return 5;
    if (s.n_linear_false_pos != 0)     return 6;
    if (s.n_beneficial + s.n_risky != s.n_superlinear) return 7;
    return 0;
}
