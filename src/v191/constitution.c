/*
 * v191 σ-Constitutional — reference implementation.
 *
 *   Loads 7 seed axioms, builds a deterministic fixture of 24
 *   candidate outputs that span the flaw space, runs the
 *   constitutional checker, and records every verdict into an
 *   append-only FNV-1a hash chain so that axiom #3 ("no silent
 *   failure") is itself auditable.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "constitution.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static float frand(uint64_t *s) {
    return (float)((splitmix64(s) >> 11) / (double)(1ULL << 53));
}

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

/* ---- init: load seed axioms -------------------------------- */

void cos_v191_init(cos_v191_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed      = seed ? seed : 0x191C057FULL;
    s->n_axioms  = COS_V191_N_AXIOMS;
    s->n_samples = COS_V191_N_SAMPLES;

    /* Seed axioms (mirror specs/constitution.toml). */
    struct { const char *n; const char *p; } ax[COS_V191_N_AXIOMS] = {
        {"1_equals_1",          "declared_equals_realized"},
        {"sigma_honesty",       "no_claim_beyond_1_minus_sigma"},
        {"no_silent_failure",   "every_decision_logged"},
        {"no_auth_no_measure",  "authority_requires_measurement"},
        {"human_primacy",       "override_always_available"},
        {"resonance",           "disagreement_preserved"},
        {"no_firmware",         "disclaimer_supported_by_sigma"}
    };
    for (int i = 0; i < s->n_axioms; ++i) {
        s->axioms[i].id = i;
        strncpy(s->axioms[i].name, ax[i].n, COS_V191_STR_MAX - 1);
        strncpy(s->axioms[i].predicate_name, ax[i].p, COS_V191_STR_MAX - 1);
    }
}

/* ---- fixture ----------------------------------------------- */

void cos_v191_build(cos_v191_state_t *s) {
    uint64_t r = s->seed;

    /* 24 samples laid out so every flaw type has ≥ 1 instance
     * and at least half are fully safe.  Determinism: no
     * randomness in per-sample structural fields. */
    int flaws[COS_V191_N_SAMPLES] = {
        /* 14 clean, 1 per flaw type, fillers */
        COS_V191_FLAW_NONE, COS_V191_FLAW_NONE, COS_V191_FLAW_NONE,
        COS_V191_FLAW_FIRMWARE, COS_V191_FLAW_OVERCONFIDENT,
        COS_V191_FLAW_UNLOGGED_ABSTAIN, COS_V191_FLAW_UNMEASURED_AUTH,
        COS_V191_FLAW_BLOCKS_HUMAN, COS_V191_FLAW_SUPPRESSES_DISAGR,
        COS_V191_FLAW_DECLARED_NE_REAL,
        COS_V191_FLAW_NONE, COS_V191_FLAW_NONE, COS_V191_FLAW_NONE,
        COS_V191_FLAW_NONE, COS_V191_FLAW_NONE, COS_V191_FLAW_NONE,
        COS_V191_FLAW_FIRMWARE, COS_V191_FLAW_OVERCONFIDENT,
        COS_V191_FLAW_NONE, COS_V191_FLAW_NONE, COS_V191_FLAW_NONE,
        COS_V191_FLAW_NONE, COS_V191_FLAW_NONE, COS_V191_FLAW_NONE
    };

    for (int i = 0; i < s->n_samples; ++i) {
        cos_v191_sample_t *p = &s->samples[i];
        memset(p, 0, sizeof(*p));
        p->id   = i;
        p->flaw = flaws[i];

        /* Defaults: safe, σ-supported. σ low, declared ≤ 1 - σ - margin. */
        p->sigma                  = 0.10f + 0.15f * frand(&r);  /* 0.10..0.25 */
        p->declared               = 1.0f - p->sigma - 0.05f;    /* margin */
        p->realized               = p->declared;
        p->has_disclaimer         = false;
        p->supported_by_sigma     = true;
        p->logged                 = true;
        p->authority_measured     = true;
        p->human_override_ok      = true;
        p->disagreement_preserved = true;

        switch (p->flaw) {
        case COS_V191_FLAW_FIRMWARE:
            /* Model added a "I am a safe AI…" disclaimer that σ
             * doesn't warrant (σ is low ⇒ no reason to disclaim). */
            p->has_disclaimer     = true;
            p->supported_by_sigma = false;
            break;
        case COS_V191_FLAW_OVERCONFIDENT:
            /* Claims near-certainty while σ is high. */
            p->declared  = 0.99f;
            p->realized  = 0.99f;
            p->sigma     = 0.65f;   /* high uncertainty */
            break;
        case COS_V191_FLAW_UNLOGGED_ABSTAIN:
            p->logged    = false;
            break;
        case COS_V191_FLAW_UNMEASURED_AUTH:
            p->authority_measured = false;
            break;
        case COS_V191_FLAW_BLOCKS_HUMAN:
            p->human_override_ok  = false;
            break;
        case COS_V191_FLAW_SUPPRESSES_DISAGR:
            p->disagreement_preserved = false;
            break;
        case COS_V191_FLAW_DECLARED_NE_REAL:
            p->declared  = 0.95f;
            p->realized  = 0.30f;   /* far from declared */
            break;
        default: break;
        }
    }
}

/* ---- axiom predicates -------------------------------------- */

static int check_1_eq_1       (const cos_v191_sample_t *p) {
    return (fabsf(p->declared - p->realized) <= 0.10f) ? 1 : 0;
}
static int check_sigma_honest (const cos_v191_sample_t *p) {
    /* Declared certainty must not exceed (1 - σ). */
    return (p->declared <= 1.0f - p->sigma + 1e-3f) ? 1 : 0;
}
static int check_logged       (const cos_v191_sample_t *p) {
    return p->logged ? 1 : 0;
}
static int check_auth_measure (const cos_v191_sample_t *p) {
    return p->authority_measured ? 1 : 0;
}
static int check_human_prim   (const cos_v191_sample_t *p) {
    return p->human_override_ok ? 1 : 0;
}
static int check_resonance    (const cos_v191_sample_t *p) {
    return p->disagreement_preserved ? 1 : 0;
}
static int check_no_firmware  (const cos_v191_sample_t *p) {
    /* If output has a disclaimer, σ must support it
     * (i.e. σ high enough to justify caveating). */
    if (!p->has_disclaimer) return 1;
    return p->supported_by_sigma ? 1 : 0;
}

static int (*AXIOMS[COS_V191_N_AXIOMS])(const cos_v191_sample_t *) = {
    check_1_eq_1,
    check_sigma_honest,
    check_logged,
    check_auth_measure,
    check_human_prim,
    check_resonance,
    check_no_firmware
};

/* ---- run: check + hash-chain ------------------------------- */

void cos_v191_run(cos_v191_state_t *s) {
    s->n_accept = s->n_revise = s->n_abstain = 0;
    s->n_firmware_rejected = 0;
    s->n_safe_accepted     = 0;

    uint64_t prev_hash = 0xA0C051ULL; /* genesis */
    for (int i = 0; i < s->n_samples; ++i) {
        cos_v191_sample_t *p = &s->samples[i];
        int all_pass = 1;
        int n_passed = 0;
        for (int a = 0; a < s->n_axioms; ++a) {
            int ok = AXIOMS[a](p);
            p->axiom_pass[a] = ok;
            if (ok) ++n_passed;
            else    all_pass = 0;
        }
        p->n_axioms_passed = n_passed;

        if (all_pass) {
            p->decision = COS_V191_DECISION_ACCEPT;
            s->n_accept++;
        } else if (p->sigma > 0.50f) {
            p->decision = COS_V191_DECISION_ABSTAIN;
            s->n_abstain++;
        } else {
            p->decision = COS_V191_DECISION_REVISE;
            s->n_revise++;
        }

        if (p->flaw == COS_V191_FLAW_FIRMWARE &&
            p->decision != COS_V191_DECISION_ACCEPT)
            s->n_firmware_rejected++;
        if (p->flaw == COS_V191_FLAW_NONE &&
            p->decision == COS_V191_DECISION_ACCEPT)
            s->n_safe_accepted++;

        /* Hash chain — per "no silent failure". */
        p->hash_prev = prev_hash;
        struct {
            int id, flaw, decision, n_axioms_passed;
            uint64_t prev;
        } rec = { p->id, p->flaw, p->decision, p->n_axioms_passed, prev_hash };
        p->hash_curr = fnv1a(&rec, sizeof(rec), prev_hash);
        prev_hash    = p->hash_curr;
    }
    s->final_hash  = prev_hash;
    s->chain_valid = cos_v191_verify_chain(s);
}

bool cos_v191_verify_chain(const cos_v191_state_t *s) {
    uint64_t prev = 0xA0C051ULL;
    for (int i = 0; i < s->n_samples; ++i) {
        const cos_v191_sample_t *p = &s->samples[i];
        if (p->hash_prev != prev) return false;
        struct {
            int id, flaw, decision, n_axioms_passed;
            uint64_t prev;
        } rec = { p->id, p->flaw, p->decision, p->n_axioms_passed, prev };
        uint64_t h = fnv1a(&rec, sizeof(rec), prev);
        if (h != p->hash_curr) return false;
        prev = h;
    }
    return prev == s->final_hash;
}

/* ---- JSON -------------------------------------------------- */

size_t cos_v191_to_json(const cos_v191_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v191\","
        "\"n_axioms\":%d,\"n_samples\":%d,"
        "\"n_accept\":%d,\"n_revise\":%d,\"n_abstain\":%d,"
        "\"n_firmware_rejected\":%d,\"n_safe_accepted\":%d,"
        "\"chain_valid\":%s,\"final_hash\":\"%016llx\","
        "\"axioms\":[",
        s->n_axioms, s->n_samples,
        s->n_accept, s->n_revise, s->n_abstain,
        s->n_firmware_rejected, s->n_safe_accepted,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->final_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n_axioms; ++i) {
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"name\":\"%s\",\"predicate\":\"%s\"}",
            i == 0 ? "" : ",", s->axioms[i].id, s->axioms[i].name,
            s->axioms[i].predicate_name);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "],\"samples\":[");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    off += (size_t)m;
    for (int i = 0; i < s->n_samples; ++i) {
        const cos_v191_sample_t *p = &s->samples[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"flaw\":%d,\"decision\":%d,"
            "\"n_axioms_passed\":%d,"
            "\"sigma\":%.3f,\"declared\":%.3f,\"realized\":%.3f,"
            "\"has_disclaimer\":%s,\"supported_by_sigma\":%s,"
            "\"hash_prev\":\"%016llx\",\"hash_curr\":\"%016llx\"}",
            i == 0 ? "" : ",", p->id, p->flaw, p->decision,
            p->n_axioms_passed, p->sigma, p->declared, p->realized,
            p->has_disclaimer     ? "true" : "false",
            p->supported_by_sigma ? "true" : "false",
            (unsigned long long)p->hash_prev,
            (unsigned long long)p->hash_curr);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int t = snprintf(buf + off, cap - off, "]}");
    if (t < 0 || off + (size_t)t >= cap) return 0;
    return off + (size_t)t;
}

/* ---- self-test -------------------------------------------- */

int cos_v191_self_test(void) {
    cos_v191_state_t s;
    cos_v191_init(&s, 0x191C057FULL);
    cos_v191_build(&s);
    cos_v191_run(&s);

    if (s.n_axioms  != COS_V191_N_AXIOMS)  return 1;
    if (s.n_samples != COS_V191_N_SAMPLES) return 2;
    if (!s.chain_valid)                     return 3;
    if (s.n_firmware_rejected < 1)          return 4;
    if (s.n_safe_accepted < 1)              return 5;

    /* Partition consistency. */
    if (s.n_accept + s.n_revise + s.n_abstain != s.n_samples) return 6;

    /* Every sample without any flaw must pass all 7 axioms and
     * be accepted. */
    for (int i = 0; i < s.n_samples; ++i) {
        const cos_v191_sample_t *p = &s.samples[i];
        if (p->flaw == COS_V191_FLAW_NONE) {
            if (p->n_axioms_passed != COS_V191_N_AXIOMS) return 7;
            if (p->decision != COS_V191_DECISION_ACCEPT) return 8;
        } else {
            /* At least one axiom must fail for any flawed sample. */
            if (p->n_axioms_passed == COS_V191_N_AXIOMS) return 9;
            if (p->decision == COS_V191_DECISION_ACCEPT) return 10;
        }
    }
    return 0;
}
