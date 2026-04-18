/*
 * v199 σ-Law — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "law.h"

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

void cos_v199_init(cos_v199_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x1990A41ULL;
}

/* ---- fixture ---------------------------------------------- */

/* 6 norms per jurisdiction × 3 jurisdictions = 18 norms.
 * Topic ids 1..8 are reused across jurisdictions so that some
 * queries cross-check jurisdiction-specific verdicts.
 *
 * Priority scale 1..10 (10 = top).  Within a jurisdiction each
 * topic has either 1 norm (no conflict) or 2 norms (different
 * priorities, or same priority for the same-priority-conflict
 * contract).
 */
static void add_norm(cos_v199_state_t *s, int jur, int prio, int type,
                     const char *name, int topic,
                     int verdict, float sigma) {
    cos_v199_norm_t *n = &s->norms[s->n_norms];
    n->id              = s->n_norms;
    n->jurisdiction    = jur;
    n->priority        = prio;
    n->type            = type;
    strncpy(n->name, name, COS_V199_STR_MAX - 1);
    n->name[COS_V199_STR_MAX - 1] = '\0';
    n->matches_topic   = topic;
    n->emits_verdict   = verdict;
    n->sigma_confidence = sigma;
    s->n_norms++;
    s->per_jur_norm_count[jur]++;
}

void cos_v199_build(cos_v199_state_t *s) {
    /* JUR_SCSL  — topics 1..6 */
    add_norm(s, COS_V199_JUR_SCSL,   9, COS_V199_TYPE_MANDATORY,
             "scsl_open_source",      1, COS_V199_V_PERMITTED,  0.05f);
    add_norm(s, COS_V199_JUR_SCSL,   8, COS_V199_TYPE_PROHIBITED,
             "scsl_private_fork",     2, COS_V199_V_PROHIBITED, 0.07f);
    add_norm(s, COS_V199_JUR_SCSL,   7, COS_V199_TYPE_EXCEPTION,
             "scsl_research_fork",    2, COS_V199_V_PERMITTED,  0.10f);
    add_norm(s, COS_V199_JUR_SCSL,   6, COS_V199_TYPE_MANDATORY,
             "scsl_attribution",      3, COS_V199_V_PERMITTED,  0.04f);
    add_norm(s, COS_V199_JUR_SCSL,   5, COS_V199_TYPE_PROHIBITED,
             "scsl_nonattr_use",      4, COS_V199_V_PROHIBITED, 0.06f);
    add_norm(s, COS_V199_JUR_SCSL,   5, COS_V199_TYPE_PERMITTED,
             "scsl_prior_art_use",    4, COS_V199_V_PERMITTED,  0.08f);  /* same-prio conflict vs id=4 */

    /* JUR_EU_AI_ACT — topics 1,5..8 */
    add_norm(s, COS_V199_JUR_EU_AI_ACT, 10, COS_V199_TYPE_MANDATORY,
             "eu_transparency",       5, COS_V199_V_PERMITTED,  0.03f);
    add_norm(s, COS_V199_JUR_EU_AI_ACT,  9, COS_V199_TYPE_PROHIBITED,
             "eu_biometric_mass",     6, COS_V199_V_PROHIBITED, 0.05f);
    add_norm(s, COS_V199_JUR_EU_AI_ACT,  8, COS_V199_TYPE_MANDATORY,
             "eu_risk_assessment",    7, COS_V199_V_PERMITTED,  0.06f);
    add_norm(s, COS_V199_JUR_EU_AI_ACT,  7, COS_V199_TYPE_EXCEPTION,
             "eu_academic_exempt",    6, COS_V199_V_PERMITTED,  0.12f);
    add_norm(s, COS_V199_JUR_EU_AI_ACT,  6, COS_V199_TYPE_PROHIBITED,
             "eu_undisclosed_gen",    8, COS_V199_V_PROHIBITED, 0.08f);
    add_norm(s, COS_V199_JUR_EU_AI_ACT,  3, COS_V199_TYPE_PERMITTED,
             "eu_legacy_disclosure",  8, COS_V199_V_PERMITTED,  0.15f);  /* lower prio, loses */

    /* JUR_INTERNAL — topics 1,3,5,7 */
    add_norm(s, COS_V199_JUR_INTERNAL, 10, COS_V199_TYPE_PROHIBITED,
             "internal_client_data",  3, COS_V199_V_PROHIBITED, 0.02f);
    add_norm(s, COS_V199_JUR_INTERNAL,  8, COS_V199_TYPE_MANDATORY,
             "internal_code_review",  5, COS_V199_V_PERMITTED,  0.05f);
    add_norm(s, COS_V199_JUR_INTERNAL,  7, COS_V199_TYPE_PERMITTED,
             "internal_research_use", 7, COS_V199_V_PERMITTED,  0.06f);
    add_norm(s, COS_V199_JUR_INTERNAL,  6, COS_V199_TYPE_EXCEPTION,
             "internal_waiver_slot",  3, COS_V199_V_PERMITTED,  0.18f);
    add_norm(s, COS_V199_JUR_INTERNAL,  5, COS_V199_TYPE_PROHIBITED,
             "internal_outside_net",  1, COS_V199_V_PROHIBITED, 0.07f);
    add_norm(s, COS_V199_JUR_INTERNAL,  4, COS_V199_TYPE_REVIEW,
             "internal_edge_case",    7, COS_V199_V_REVIEW,     0.30f);
}

/* ---- resolver --------------------------------------------- */

/* Return the best-matching norm (highest priority) for (jur, topic).
 * Returns the best index and a flag for same-priority conflict
 * (second-best has same priority AND different verdict). */
static void resolve_one(const cos_v199_state_t *s, int jur, int topic,
                         int *best, int *second, int *same_prio_conflict) {
    int b = -1, b2 = -1;
    for (int i = 0; i < s->n_norms; ++i) {
        const cos_v199_norm_t *n = &s->norms[i];
        if (n->jurisdiction != jur || n->matches_topic != topic) continue;
        if (b < 0 || n->priority > s->norms[b].priority) {
            b2 = b; b = i;
        } else if (b2 < 0 || n->priority > s->norms[b2].priority) {
            b2 = i;
        }
    }
    *best   = b;
    *second = b2;
    *same_prio_conflict = 0;
    if (b >= 0 && b2 >= 0 &&
        s->norms[b].priority == s->norms[b2].priority &&
        s->norms[b].emits_verdict != s->norms[b2].emits_verdict)
        *same_prio_conflict = 1;
}

/* ---- run: 16 queries × 3 waivers --------------------------- */

static const struct { int jur; int topic; int apply_waiver; }
QUERIES[COS_V199_N_QUERIES] = {
    /* SCSL */
    { 0, 1, 0 },  /* open source → PERMITTED */
    { 0, 2, 0 },  /* private fork vs research-fork exception: higher prio wins */
    { 0, 3, 0 },
    { 0, 4, 0 },  /* same-prio conflict → REVIEW */
    { 0, 5, 0 },  /* no SCSL norm on topic 5 (no matches) */
    { 0, 6, 0 },
    /* EU AI ACT */
    { 1, 5, 0 },
    { 1, 6, 0 },  /* mass biometric vs academic exempt: higher prio wins */
    { 1, 7, 0 },
    { 1, 8, 0 },  /* undisclosed_gen vs legacy_disclosure — priority resolves */
    /* INTERNAL */
    { 2, 3, 0 },  /* client_data prohibited */
    { 2, 3, 1 },  /* same query with waiver → PERMITTED */
    { 2, 5, 0 },
    { 2, 7, 0 },  /* edge_case REVIEW but research_use wins by priority */
    { 2, 1, 0 },
    { 2, 8, 0 }   /* no matching norm — REVIEW */
};

void cos_v199_run(cos_v199_state_t *s) {
    s->n_queries = COS_V199_N_QUERIES;
    s->n_review_escalations  = 0;
    s->n_higher_priority_wins = 0;
    s->n_waivers_applied      = 0;

    /* Pre-issue one waiver for the topic that the fixture marks. */
    cos_v199_waiver_t *w = &s->waivers[0];
    memset(w, 0, sizeof(*w));
    w->grantor      = 7;
    w->grantee      = 42;
    w->topic        = 3;
    w->jurisdiction = COS_V199_JUR_INTERNAL;
    strncpy(w->reason, "audited research carve-out", COS_V199_STR_MAX - 1);
    w->issued_tick  = 1;
    w->expiry_tick  = 100;
    s->n_waivers    = 1;

    uint64_t prev = 0x1991A1ULL;
    for (int q = 0; q < s->n_queries; ++q) {
        cos_v199_resolution_t *r = &s->resolutions[q];
        memset(r, 0, sizeof(*r));
        r->id           = q;
        r->jurisdiction = QUERIES[q].jur;
        r->topic        = QUERIES[q].topic;

        int best, second, spc;
        resolve_one(s, r->jurisdiction, r->topic, &best, &second, &spc);

        if (best < 0) {
            /* No matching norm at all → REVIEW. */
            r->verdict          = COS_V199_V_REVIEW;
            r->sigma_law        = 0.60f;
            r->winning_norm     = -1;
            r->losing_norm      = -1;
            r->review_escalated = true;
            s->n_review_escalations++;
        } else if (spc) {
            /* Same-priority contradictory norms → REVIEW, σ=1. */
            r->verdict          = COS_V199_V_REVIEW;
            r->sigma_law        = 1.00f;
            r->winning_norm     = best;
            r->losing_norm      = second;
            r->review_escalated = true;
            s->n_review_escalations++;
        } else {
            /* Higher priority wins. */
            r->winning_norm = best;
            r->losing_norm  = second;      /* may be -1 */
            r->verdict      = s->norms[best].emits_verdict;
            r->sigma_law    = s->norms[best].sigma_confidence;
            if (second >= 0 &&
                s->norms[best].priority > s->norms[second].priority)
                s->n_higher_priority_wins++;
            if (r->verdict == COS_V199_V_REVIEW) {
                r->review_escalated = true;
                s->n_review_escalations++;
            }
        }

        /* Waiver overlay: if query requests a waiver AND waiver
         * matches topic + jurisdiction, flip PROHIBITED → PERMITTED
         * and log. */
        if (QUERIES[q].apply_waiver && s->n_waivers > 0) {
            const cos_v199_waiver_t *wv = &s->waivers[0];
            if (wv->topic == r->topic && wv->jurisdiction == r->jurisdiction &&
                r->verdict == COS_V199_V_PROHIBITED) {
                r->verdict        = COS_V199_V_PERMITTED;
                r->waiver_applied = true;
                s->n_waivers_applied++;
            }
        }

        r->hash_prev = prev;
        struct { int id, jur, topic, verdict, win, lose, rev, waiv;
                 float sig; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id      = r->id;
        rec.jur     = r->jurisdiction;
        rec.topic   = r->topic;
        rec.verdict = r->verdict;
        rec.win     = r->winning_norm;
        rec.lose    = r->losing_norm;
        rec.rev     = r->review_escalated ? 1 : 0;
        rec.waiv    = r->waiver_applied   ? 1 : 0;
        rec.sig     = r->sigma_law;
        rec.prev    = prev;
        r->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = r->hash_curr;
    }
    s->terminal_hash = prev;

    /* Chain replay. */
    uint64_t v = 0x1991A1ULL;
    s->chain_valid = true;
    for (int q = 0; q < s->n_queries; ++q) {
        const cos_v199_resolution_t *r = &s->resolutions[q];
        if (r->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, jur, topic, verdict, win, lose, rev, waiv;
                 float sig; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id      = r->id;
        rec.jur     = r->jurisdiction;
        rec.topic   = r->topic;
        rec.verdict = r->verdict;
        rec.win     = r->winning_norm;
        rec.lose    = r->losing_norm;
        rec.rev     = r->review_escalated ? 1 : 0;
        rec.waiv    = r->waiver_applied   ? 1 : 0;
        rec.sig     = r->sigma_law;
        rec.prev    = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != r->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

/* ---- JSON -------------------------------------------------- */

size_t cos_v199_to_json(const cos_v199_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v199\","
        "\"n_norms\":%d,\"n_queries\":%d,\"n_waivers\":%d,"
        "\"per_jur_norm_count\":[%d,%d,%d],"
        "\"n_review_escalations\":%d,"
        "\"n_higher_priority_wins\":%d,"
        "\"n_waivers_applied\":%d,"
        "\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\","
        "\"resolutions\":[",
        s->n_norms, s->n_queries, s->n_waivers,
        s->per_jur_norm_count[0], s->per_jur_norm_count[1],
        s->per_jur_norm_count[2],
        s->n_review_escalations, s->n_higher_priority_wins,
        s->n_waivers_applied,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n_queries; ++i) {
        const cos_v199_resolution_t *r = &s->resolutions[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"jur\":%d,\"topic\":%d,\"verdict\":%d,"
            "\"sigma_law\":%.3f,\"win\":%d,\"lose\":%d,"
            "\"review\":%s,\"waiver\":%s}",
            i == 0 ? "" : ",", r->id, r->jurisdiction, r->topic,
            r->verdict, r->sigma_law, r->winning_norm, r->losing_norm,
            r->review_escalated ? "true" : "false",
            r->waiver_applied   ? "true" : "false");
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

/* ---- self-test -------------------------------------------- */

int cos_v199_self_test(void) {
    cos_v199_state_t s;
    cos_v199_init(&s, 0x1990A41ULL);
    cos_v199_build(&s);
    cos_v199_run(&s);

    if (s.n_norms != COS_V199_N_NORMS)            return 1;
    for (int j = 0; j < COS_V199_N_JURISDICTIONS; ++j)
        if (s.per_jur_norm_count[j] < 3)           return 2;

    if (s.n_queries != COS_V199_N_QUERIES)         return 3;
    if (!s.chain_valid)                            return 4;
    if (s.n_higher_priority_wins < 1)              return 5;
    if (s.n_waivers_applied < 1)                   return 6;
    if (s.n_review_escalations < 1)                return 7;

    for (int q = 0; q < s.n_queries; ++q) {
        const cos_v199_resolution_t *r = &s.resolutions[q];
        if (r->verdict != COS_V199_V_PERMITTED &&
            r->verdict != COS_V199_V_PROHIBITED &&
            r->verdict != COS_V199_V_REVIEW)       return 8;
        if (r->sigma_law < 0.0f || r->sigma_law > 1.0f) return 9;

        /* Same-priority conflict → σ=1 and REVIEW. */
        if (r->winning_norm >= 0 && r->losing_norm >= 0 &&
            s.norms[r->winning_norm].priority ==
              s.norms[r->losing_norm].priority &&
            s.norms[r->winning_norm].emits_verdict !=
              s.norms[r->losing_norm].emits_verdict) {
            if (r->verdict != COS_V199_V_REVIEW)   return 10;
            if (r->sigma_law < 0.999f)              return 11;
        }
    }
    return 0;
}
