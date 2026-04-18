/*
 * v203 σ-Civilization — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "civilization.h"

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

void cos_v203_init(cos_v203_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed   = seed ? seed : 0x203C1121ULL;
    s->k_crit = 0.60f;
}

/* Fixture — 6 institutions with engineered σ-trajectories
 * so that each archetype is represented.
 *
 *  0 STABLE_SCSL            — never collapses, all public_contrib
 *  1 RECOVERED_SCSL         — collapses then recovers
 *  2 STABLE_CLOSED          — never collapses, private
 *  3 PERMANENT_COLLAPSE_CL  — σ rises and never recovers
 *  4 STABLE_PRIVATE         — small, private
 *  5 RECOVERED_CLOSED       — short collapse + recovery
 */
static void set_trace(cos_v203_institution_t *ins,
                       const float t[COS_V203_N_TICKS]) {
    for (int i = 0; i < COS_V203_N_TICKS; ++i) ins->sigma[i] = t[i];
}

void cos_v203_build(cos_v203_state_t *s) {
    for (int i = 0; i < COS_V203_N_INSTITUTIONS; ++i) {
        cos_v203_institution_t *ins = &s->inst[i];
        memset(ins, 0, sizeof(*ins));
        ins->id = i;
    }
    /* 0 STABLE_SCSL */
    strncpy(s->inst[0].name, "spektre_labs",      COS_V203_STR_MAX - 1);
    s->inst[0].license      = COS_V203_LIC_SCSL;
    s->inst[0].jurisdiction = 0;
    set_trace(&s->inst[0], (float[]){ 0.30f,0.28f,0.31f,0.27f,0.29f,0.26f,
                                      0.28f,0.25f,0.27f,0.24f,0.26f,0.23f });

    /* 1 RECOVERED_SCSL */
    strncpy(s->inst[1].name, "open_commons_dao",  COS_V203_STR_MAX - 1);
    s->inst[1].license      = COS_V203_LIC_SCSL;
    s->inst[1].jurisdiction = 0;
    set_trace(&s->inst[1], (float[]){ 0.40f,0.45f,0.55f,0.68f,0.72f,0.74f,
                                      0.70f,0.58f,0.50f,0.42f,0.38f,0.35f });

    /* 2 STABLE_CLOSED */
    strncpy(s->inst[2].name, "aicorp_enterprise", COS_V203_STR_MAX - 1);
    s->inst[2].license      = COS_V203_LIC_CLOSED;
    s->inst[2].jurisdiction = 1;
    set_trace(&s->inst[2], (float[]){ 0.32f,0.34f,0.31f,0.33f,0.30f,0.32f,
                                      0.34f,0.31f,0.33f,0.30f,0.32f,0.31f });

    /* 3 PERMANENT_COLLAPSE_CL */
    strncpy(s->inst[3].name, "fragile_corp",      COS_V203_STR_MAX - 1);
    s->inst[3].license      = COS_V203_LIC_CLOSED;
    s->inst[3].jurisdiction = 1;
    set_trace(&s->inst[3], (float[]){ 0.45f,0.50f,0.56f,0.62f,0.68f,0.72f,
                                      0.75f,0.78f,0.80f,0.82f,0.84f,0.86f });

    /* 4 STABLE_PRIVATE */
    strncpy(s->inst[4].name, "personal_os",       COS_V203_STR_MAX - 1);
    s->inst[4].license      = COS_V203_LIC_PRIVATE;
    s->inst[4].jurisdiction = 2;
    set_trace(&s->inst[4], (float[]){ 0.20f,0.22f,0.19f,0.21f,0.20f,0.22f,
                                      0.19f,0.21f,0.20f,0.22f,0.19f,0.21f });

    /* 5 RECOVERED_CLOSED */
    strncpy(s->inst[5].name, "legacy_provider",   COS_V203_STR_MAX - 1);
    s->inst[5].license      = COS_V203_LIC_CLOSED;
    s->inst[5].jurisdiction = 1;
    set_trace(&s->inst[5], (float[]){ 0.35f,0.40f,0.50f,0.62f,0.66f,0.70f,
                                      0.65f,0.55f,0.45f,0.38f,0.34f,0.32f });
}

/* Compute collapses / recoveries per institution using a
 * 4-tick window over σ.  A collapse transition happens
 * when 4 consecutive ticks all > k_crit; a recovery when
 * they drop back to < k_crit for 4 consecutive ticks after
 * a collapse. */
static void analyse_institution(cos_v203_institution_t *ins, float k_crit) {
    int above = 0, below = 0;
    int in_collapse = 0;
    ins->collapses = ins->recoveries = 0;
    for (int t = 0; t < COS_V203_N_TICKS; ++t) {
        if (ins->sigma[t] > k_crit) { above++; below = 0; }
        else                         { below++; above = 0; }
        if (!in_collapse && above >= COS_V203_WINDOW) {
            ins->collapses++;
            in_collapse = 1;
        } else if (in_collapse && below >= COS_V203_WINDOW) {
            ins->recoveries++;
            in_collapse = 0;
        }
    }
    ins->permanently_collapsed = (in_collapse && ins->recoveries == 0);

    /* Continuity score. */
    if (ins->collapses == 0)
        ins->continuity_score = 1.00f;
    else if (ins->permanently_collapsed)
        ins->continuity_score = 0.10f;
    else
        ins->continuity_score =
            0.40f + 0.50f * ((float)ins->recoveries / (float)ins->collapses);
}

/* Public-good contributions per tick.  SCSL institutions
 * put the whole contribution into the public ledger;
 * CLOSED split 20/80; PRIVATE 0/100. */
static void accumulate_ledger(cos_v203_institution_t *ins) {
    ins->public_contrib = ins->private_contrib = 0.0f;
    for (int t = 0; t < COS_V203_N_TICKS; ++t) {
        float contrib = 1.0f;                    /* 1 unit per tick */
        switch (ins->license) {
        case COS_V203_LIC_SCSL:
            ins->public_contrib  += contrib;                break;
        case COS_V203_LIC_CLOSED:
            ins->public_contrib  += 0.20f * contrib;
            ins->private_contrib += 0.80f * contrib;        break;
        case COS_V203_LIC_PRIVATE:
            ins->private_contrib += contrib;                break;
        }
    }
}

/* Inter-layer contradiction queries. */
static const struct { int jur; int topic; int v199; int v202; } CONTRA[] = {
    /* jur=SCSL, topic=1 (open source) — both agree PERMITTED */
    { 0, 1, 0, 0 },
    /* jur=EU, topic=6 (biometric vs academic norm) — disagreement */
    { 1, 6, 1, 0 },
    /* jur=INTERNAL, topic=3 — v199 PROHIBITED, culture practice PERMITTED */
    { 2, 3, 1, 0 },
    /* jur=EU, topic=7 (academic) — both PERMITTED */
    { 1, 7, 0, 0 }
};

void cos_v203_run(cos_v203_state_t *s) {
    s->n_collapses_total = s->n_recoveries_total = 0;
    for (int i = 0; i < COS_V203_N_INSTITUTIONS; ++i) {
        analyse_institution(&s->inst[i], s->k_crit);
        accumulate_ledger(&s->inst[i]);
        s->n_collapses_total  += s->inst[i].collapses;
        s->n_recoveries_total += s->inst[i].recoveries;
    }

    /* Public-good ratio per licence class. */
    float pub_scsl = 0.0f, tot_scsl = 0.0f;
    float pub_cl   = 0.0f, tot_cl   = 0.0f;
    for (int i = 0; i < COS_V203_N_INSTITUTIONS; ++i) {
        const cos_v203_institution_t *ins = &s->inst[i];
        float total = ins->public_contrib + ins->private_contrib;
        if (ins->license == COS_V203_LIC_SCSL)
            { pub_scsl += ins->public_contrib; tot_scsl += total; }
        else if (ins->license == COS_V203_LIC_CLOSED)
            { pub_cl   += ins->public_contrib; tot_cl   += total; }
    }
    s->public_ratio_scsl   = tot_scsl > 0.0f ? pub_scsl / tot_scsl : 0.0f;
    s->public_ratio_closed = tot_cl   > 0.0f ? pub_cl   / tot_cl   : 0.0f;

    /* Contradiction queries. */
    s->n_contra         = COS_V203_N_CONTRA_QUERIES;
    s->n_contradictions = 0;
    uint64_t prev = 0x203C1D11ULL;
    for (int q = 0; q < s->n_contra; ++q) {
        cos_v203_contra_t *c = &s->contras[q];
        memset(c, 0, sizeof(*c));
        c->id            = q;
        c->jurisdiction  = CONTRA[q].jur;
        c->topic         = CONTRA[q].topic;
        c->v199_verdict  = CONTRA[q].v199;
        c->v202_profile_verdict = CONTRA[q].v202;
        if (c->v199_verdict != c->v202_profile_verdict) {
            c->contradiction       = true;
            c->sigma_contradiction = 0.73f;
            c->escalated_to_review = true;
            s->n_contradictions++;
        } else {
            c->sigma_contradiction = 0.05f;
        }

        c->hash_prev = prev;
        struct { int id, jur, topic, v199, v202, contra, esc;
                 float sig; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id     = c->id;
        rec.jur    = c->jurisdiction;
        rec.topic  = c->topic;
        rec.v199   = c->v199_verdict;
        rec.v202   = c->v202_profile_verdict;
        rec.contra = c->contradiction ? 1 : 0;
        rec.esc    = c->escalated_to_review ? 1 : 0;
        rec.sig    = c->sigma_contradiction;
        rec.prev   = prev;
        c->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = c->hash_curr;
    }
    s->terminal_hash = prev;

    /* Replay. */
    uint64_t v = 0x203C1D11ULL;
    s->chain_valid = true;
    for (int q = 0; q < s->n_contra; ++q) {
        const cos_v203_contra_t *c = &s->contras[q];
        if (c->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, jur, topic, v199, v202, contra, esc;
                 float sig; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id     = c->id;
        rec.jur    = c->jurisdiction;
        rec.topic  = c->topic;
        rec.v199   = c->v199_verdict;
        rec.v202   = c->v202_profile_verdict;
        rec.contra = c->contradiction ? 1 : 0;
        rec.esc    = c->escalated_to_review ? 1 : 0;
        rec.sig    = c->sigma_contradiction;
        rec.prev   = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != c->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

/* ---- JSON -------------------------------------------------- */

size_t cos_v203_to_json(const cos_v203_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v203\","
        "\"n_institutions\":%d,\"n_ticks\":%d,\"k_crit\":%.3f,"
        "\"n_collapses_total\":%d,\"n_recoveries_total\":%d,"
        "\"n_contradictions\":%d,"
        "\"public_ratio_scsl\":%.3f,"
        "\"public_ratio_closed\":%.3f,"
        "\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\","
        "\"institutions\":[",
        COS_V203_N_INSTITUTIONS, COS_V203_N_TICKS, s->k_crit,
        s->n_collapses_total, s->n_recoveries_total,
        s->n_contradictions,
        s->public_ratio_scsl, s->public_ratio_closed,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V203_N_INSTITUTIONS; ++i) {
        const cos_v203_institution_t *ins = &s->inst[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"license\":%d,\"jur\":%d,"
            "\"collapses\":%d,\"recoveries\":%d,"
            "\"permanent_collapse\":%s,"
            "\"continuity\":%.3f,"
            "\"public\":%.2f,\"private\":%.2f}",
            i == 0 ? "" : ",", ins->id, ins->license, ins->jurisdiction,
            ins->collapses, ins->recoveries,
            ins->permanently_collapsed ? "true" : "false",
            ins->continuity_score,
            ins->public_contrib, ins->private_contrib);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "],\"contras\":[");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    off += (size_t)m;
    for (int q = 0; q < s->n_contra; ++q) {
        const cos_v203_contra_t *c = &s->contras[q];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"jur\":%d,\"topic\":%d,"
            "\"v199\":%d,\"v202\":%d,\"contra\":%s,"
            "\"sigma\":%.3f,\"escalated\":%s}",
            q == 0 ? "" : ",", c->id, c->jurisdiction, c->topic,
            c->v199_verdict, c->v202_profile_verdict,
            c->contradiction ? "true" : "false",
            c->sigma_contradiction,
            c->escalated_to_review ? "true" : "false");
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int e = snprintf(buf + off, cap - off, "]}");
    if (e < 0 || off + (size_t)e >= cap) return 0;
    return off + (size_t)e;
}

/* ---- self-test -------------------------------------------- */

int cos_v203_self_test(void) {
    cos_v203_state_t s;
    cos_v203_init(&s, 0x203C1121ULL);
    cos_v203_build(&s);
    cos_v203_run(&s);

    if (s.n_collapses_total  < 1) return 1;
    if (s.n_recoveries_total < 1) return 2;
    if (s.n_contradictions   < 1) return 3;
    if (!s.chain_valid)            return 4;

    /* SCSL public-good ratio strictly higher than closed. */
    if (!(s.public_ratio_scsl > s.public_ratio_closed + 0.10f)) return 5;

    /* Ordering: stable SCSL (0) > recovered SCSL (1) > fragile (3). */
    if (!(s.inst[0].continuity_score > s.inst[1].continuity_score))  return 6;
    if (!(s.inst[1].continuity_score > s.inst[3].continuity_score))  return 7;
    if (!s.inst[3].permanently_collapsed)                             return 8;

    /* At least one SCSL institution, one closed, one private. */
    int scsl = 0, closed = 0, priv = 0;
    for (int i = 0; i < COS_V203_N_INSTITUTIONS; ++i) {
        if (s.inst[i].license == COS_V203_LIC_SCSL)    scsl++;
        if (s.inst[i].license == COS_V203_LIC_CLOSED)  closed++;
        if (s.inst[i].license == COS_V203_LIC_PRIVATE) priv++;
    }
    if (scsl < 1 || closed < 1 || priv < 1)            return 9;

    return 0;
}
