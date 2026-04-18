/*
 * v214 σ-Swarm-Evolve — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "swarm_evolve.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v214_init(cos_v214_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed          = seed ? seed : 0x214E4011ULL;
    s->eps           = 1e-3f;
    s->min_niche_pop = 2;
    s->n_gens        = COS_V214_N_GENS;
}

/* Founder σ_means engineered so every niche has at
 * least 2 agents at gen 0, and the fleet gets better. */
static const float FOUNDER_SIGMA[COS_V214_POP_SIZE] = {
    0.30f, 0.35f, 0.45f,           /* niche 0 */
    0.32f, 0.38f, 0.48f,           /* niche 1 */
    0.34f, 0.40f, 0.50f,           /* niche 2 */
    0.36f, 0.42f, 0.55f            /* niche 3 */
};

void cos_v214_build(cos_v214_state_t *s) {
    s->n_agents_total = 0;
    for (int i = 0; i < COS_V214_POP_SIZE; ++i) {
        cos_v214_agent_t *a = &s->agents[s->n_agents_total++];
        memset(a, 0, sizeof(*a));
        a->agent_id    = i;
        a->parent_a_id = -1;
        a->parent_b_id = -1;
        a->species_id  = i / 3;      /* 3 per niche */
        a->born_gen    = 0;
        a->retired_gen = -1;
        a->sigma_mean  = FOUNDER_SIGMA[i];
        a->fitness     = 1.0f / (a->sigma_mean + s->eps);
    }
}

static int alive_by_niche(const cos_v214_state_t *s, int niche,
                          int *out_ids, float *out_sigma) {
    int n = 0;
    for (int i = 0; i < s->n_agents_total; ++i) {
        const cos_v214_agent_t *a = &s->agents[i];
        if (a->retired_gen != -1)   continue;
        if (a->species_id != niche) continue;
        out_ids[n]   = i;
        out_sigma[n] = a->sigma_mean;
        n++;
    }
    return n;
}

static void sort_two_best(const int *ids, const float *sigma, int n,
                          int *out_a, int *out_b) {
    *out_a = ids[0]; *out_b = (n > 1) ? ids[1] : ids[0];
    float sa = sigma[0];
    float sb = (n > 1) ? sigma[1] : sigma[0] + 1.0f;
    if (sb < sa) { int ti = *out_a; float ts = sa;
                   *out_a = *out_b; sa = sb;
                   *out_b = ti;    sb = ts; }
    for (int i = 2; i < n; ++i) {
        if (sigma[i] < sa) {
            *out_b = *out_a; sb = sa;
            *out_a = ids[i]; sa = sigma[i];
        } else if (sigma[i] < sb) {
            *out_b = ids[i]; sb = sigma[i];
        }
    }
}

/* Deterministic "niche-local worst" retirement. */
static int pick_worst_in_niche(const cos_v214_state_t *s, int niche) {
    int   worst = -1;
    float ws = -1.0f;
    for (int i = 0; i < s->n_agents_total; ++i) {
        const cos_v214_agent_t *a = &s->agents[i];
        if (a->retired_gen != -1)   continue;
        if (a->species_id != niche) continue;
        if (a->sigma_mean > ws) { ws = a->sigma_mean; worst = i; }
    }
    return worst;
}

void cos_v214_run(cos_v214_state_t *s) {
    uint64_t prev = 0x214D11A1ULL;
    s->total_retired = 0;

    for (int g = 0; g < s->n_gens; ++g) {
        /* Score: every alive agent's σ_mean decays slightly
         * each generation (simulating re-benchmark learning). */
        for (int i = 0; i < s->n_agents_total; ++i) {
            cos_v214_agent_t *a = &s->agents[i];
            if (a->retired_gen != -1) continue;
            float decay = 0.015f;        /* deterministic improvement */
            a->sigma_mean = fmaxf(0.05f, a->sigma_mean - decay);
            a->fitness    = 1.0f / (a->sigma_mean + s->eps);
        }

        int births = 0, deaths = 0;

        /* Per-niche lifecycle: retire the niche-local worst
         * and breed the niche-local top-2 → population is
         * conserved within every ecological niche.  This is
         * the stigmergy-style carrying-capacity rule that
         * preserves diversity (a math specialist does not
         * compete with a code specialist). */
        for (int niche = 0; niche < COS_V214_N_NICHES; ++niche) {
            int   ids[COS_V214_POP_SIZE];
            float sig[COS_V214_POP_SIZE];
            int   n = alive_by_niche(s, niche, ids, sig);
            if (n < 2) continue;
            /* Retire niche-local worst. */
            int dead = pick_worst_in_niche(s, niche);
            if (dead >= 0) {
                s->agents[dead].retired_gen = g;
                deaths++; s->total_retired++;
            }
            /* Breed niche-local top-2 (by σ̄) over survivors. */
            n = alive_by_niche(s, niche, ids, sig);
            if (n < 2) continue;
            int best_a = -1, best_b = -1;
            sort_two_best(ids, sig, n, &best_a, &best_b);
            if (s->n_agents_total >= COS_V214_MAX_AGENTS) break;
            cos_v214_agent_t *child = &s->agents[s->n_agents_total];
            memset(child, 0, sizeof(*child));
            child->agent_id    = s->n_agents_total;
            child->parent_a_id = s->agents[best_a].agent_id;
            child->parent_b_id = s->agents[best_b].agent_id;
            child->species_id  = niche;
            child->born_gen    = g;
            child->retired_gen = -1;
            float psm = 0.5f * (s->agents[best_a].sigma_mean +
                                 s->agents[best_b].sigma_mean);
            child->sigma_mean  = fmaxf(0.05f, 0.97f * psm);
            child->fitness     = 1.0f / (child->sigma_mean + s->eps);
            s->n_agents_total++;
            births++;
        }

        /* Record generation. */
        cos_v214_gen_record_t *gr = &s->gens[g];
        memset(gr, 0, sizeof(*gr));
        gr->gen = g;
        float sum = 0.0f; int cnt = 0;
        for (int n = 0; n < COS_V214_N_NICHES; ++n) gr->per_niche_count[n] = 0;
        for (int i = 0; i < s->n_agents_total; ++i) {
            const cos_v214_agent_t *a = &s->agents[i];
            if (a->retired_gen != -1) continue;
            sum += a->sigma_mean; cnt++;
            gr->per_niche_count[a->species_id]++;
        }
        gr->sigma_emergent = cnt ? sum / (float)cnt : 0.0f;
        gr->alive_count     = cnt;
        gr->births_this_gen = births;
        gr->deaths_this_gen = deaths;
        int est = 0;
        for (int n = 0; n < COS_V214_N_NICHES; ++n)
            if (gr->per_niche_count[n] >= s->min_niche_pop) est++;
        gr->established_species = est;

        /* Hash */
        gr->hash_prev = prev;
        struct { int gen, alive, births, deaths, est;
                 int pn[COS_V214_N_NICHES];
                 float se; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.gen    = gr->gen;
        rec.alive  = gr->alive_count;
        rec.births = gr->births_this_gen;
        rec.deaths = gr->deaths_this_gen;
        rec.est    = gr->established_species;
        for (int n = 0; n < COS_V214_N_NICHES; ++n)
            rec.pn[n] = gr->per_niche_count[n];
        rec.se     = gr->sigma_emergent;
        rec.prev   = prev;
        gr->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = gr->hash_curr;
    }
    s->terminal_hash = prev;

    /* Final aggregates. */
    const cos_v214_gen_record_t *last = &s->gens[s->n_gens - 1];
    s->final_established_species = last->established_species;
    s->final_sigma_emergent       = last->sigma_emergent;

    int max_span = 0;
    for (int i = 0; i < s->n_agents_total; ++i) {
        const cos_v214_agent_t *a = &s->agents[i];
        int end = (a->retired_gen == -1) ? s->n_gens - 1 : a->retired_gen;
        int span = end - a->born_gen + 1;
        if (span > max_span) max_span = span;
    }
    s->max_lineage_span = max_span;

    /* Replay for determinism. */
    uint64_t v = 0x214D11A1ULL;
    s->chain_valid = true;
    for (int g = 0; g < s->n_gens; ++g) {
        const cos_v214_gen_record_t *gr = &s->gens[g];
        if (gr->hash_prev != v) { s->chain_valid = false; break; }
        struct { int gen, alive, births, deaths, est;
                 int pn[COS_V214_N_NICHES];
                 float se; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.gen    = gr->gen;
        rec.alive  = gr->alive_count;
        rec.births = gr->births_this_gen;
        rec.deaths = gr->deaths_this_gen;
        rec.est    = gr->established_species;
        for (int n = 0; n < COS_V214_N_NICHES; ++n)
            rec.pn[n] = gr->per_niche_count[n];
        rec.se     = gr->sigma_emergent;
        rec.prev   = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != gr->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v214_to_json(const cos_v214_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v214\","
        "\"n_gens\":%d,\"n_agents_total\":%d,\"pop_size\":%d,"
        "\"final_established_species\":%d,"
        "\"final_sigma_emergent\":%.4f,"
        "\"max_lineage_span\":%d,\"total_retired\":%d,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"gens\":[",
        s->n_gens, s->n_agents_total, COS_V214_POP_SIZE,
        s->final_established_species, s->final_sigma_emergent,
        s->max_lineage_span, s->total_retired,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int g = 0; g < s->n_gens; ++g) {
        const cos_v214_gen_record_t *gr = &s->gens[g];
        int k = snprintf(buf + off, cap - off,
            "%s{\"gen\":%d,\"alive\":%d,\"births\":%d,"
            "\"deaths\":%d,\"est\":%d,\"se\":%.4f,"
            "\"pn\":[%d,%d,%d,%d]}",
            g == 0 ? "" : ",", gr->gen, gr->alive_count,
            gr->births_this_gen, gr->deaths_this_gen,
            gr->established_species, gr->sigma_emergent,
            gr->per_niche_count[0], gr->per_niche_count[1],
            gr->per_niche_count[2], gr->per_niche_count[3]);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v214_self_test(void) {
    cos_v214_state_t s;
    cos_v214_init(&s, 0x214E4011ULL);
    cos_v214_build(&s);
    cos_v214_run(&s);

    if (s.n_gens != COS_V214_N_GENS)          return 1;
    if (!s.chain_valid)                        return 2;
    if (s.final_established_species < 3)       return 3;
    if (s.max_lineage_span < 5)                return 4;
    if (s.total_retired < 8)                   return 5;

    /* Monotone non-increasing σ_emergent. */
    for (int g = 1; g < s.n_gens; ++g)
        if (s.gens[g].sigma_emergent >
            s.gens[g - 1].sigma_emergent + 1e-6f) return 6;

    /* Constant population. */
    for (int g = 0; g < s.n_gens; ++g)
        if (s.gens[g].alive_count != COS_V214_POP_SIZE) return 7;

    return 0;
}
