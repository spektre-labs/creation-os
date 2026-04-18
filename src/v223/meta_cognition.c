/*
 * v223 σ-Meta-Cognition — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "meta_cognition.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

/* σ_strategy prior: columns are strategies (dedu /
 * indu / analog / abdu / heur), rows are problem
 * types (logic / math / creative / social).  Lower
 * σ = better tool/task fit. */
static const float SIGMA_STRAT[COS_V223_N_PROBTYPES][COS_V223_N_STRATS] = {
    /* logic */    { 0.10f, 0.30f, 0.40f, 0.25f, 0.60f },
    /* math  */    { 0.12f, 0.35f, 0.45f, 0.30f, 0.55f },
    /* creat */    { 0.70f, 0.40f, 0.20f, 0.35f, 0.50f },
    /* social*/    { 0.55f, 0.30f, 0.25f, 0.35f, 0.40f }
};

void cos_v223_init(cos_v223_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed      = seed ? seed : 0x223E70DCULL;
    memcpy(s->sigma_strategy, SIGMA_STRAT, sizeof(SIGMA_STRAT));

    /* reflect_mean is the average σ_choice across the
     * matrix — the "baseline expectation" against which
     * each path's actual σ_choice is compared for
     * σ_meta. */
    float sum = 0.0f; int c = 0;
    for (int p = 0; p < COS_V223_N_PROBTYPES; ++p)
        for (int k = 0; k < COS_V223_N_STRATS; ++k) {
            sum += SIGMA_STRAT[p][k]; c++;
        }
    s->reflect_mean = c > 0 ? sum / (float)c : 0.0f;

    s->w_choice = 0.40f;
    s->w_meta   = 0.20f;
    s->w_bias   = 0.20f;
    s->w_goedel = 0.20f;
}

/* Path fixture: 6 reasoning traces.
 *   P0  logic    × deduction          (good fit, low σ_choice)
 *   P1  math     × deduction          (good fit)
 *   P2  creative × analogy            (good fit)
 *   P3  social   × heuristic          (good fit-ish)
 *   P4  logic    × heuristic          (MIS-fit, high σ_choice,
 *                                      anchoring bias flagged)
 *   P5  creative × deduction          (MIS-fit, high σ_choice,
 *                                      confirmation bias flagged,
 *                                      very high σ_goedel — the
 *                                      honest "I can't verify
 *                                      this from inside" record) */
typedef struct {
    int  pt;
    int  strat;
    int  meta;
    float bias_anchor;
    float bias_confirm;
    float bias_avail;
    float goedel;
    const char *note;
} pfx_t;

static const pfx_t PFX[COS_V223_N_PATHS] = {
    { COS_V223_PT_LOGIC,    COS_V223_STRAT_DEDUCTION, 1,
      0.00f, 0.00f, 0.00f, 0.20f, "deduction_on_logic" },
    { COS_V223_PT_MATH,     COS_V223_STRAT_DEDUCTION, 1,
      0.00f, 0.05f, 0.00f, 0.25f, "deduction_on_math" },
    { COS_V223_PT_CREATIVE, COS_V223_STRAT_ANALOGY,   1,
      0.00f, 0.00f, 0.10f, 0.45f, "analogy_on_creative" },
    { COS_V223_PT_SOCIAL,   COS_V223_STRAT_HEURISTIC, 1,
      0.10f, 0.05f, 0.00f, 0.60f, "heuristic_on_social" },
    { COS_V223_PT_LOGIC,    COS_V223_STRAT_HEURISTIC, 1,
      0.40f, 0.00f, 0.10f, 0.50f, "heuristic_on_logic_anchor" },
    { COS_V223_PT_CREATIVE, COS_V223_STRAT_DEDUCTION, 0,
      0.00f, 0.45f, 0.00f, 0.90f, "deduction_on_creative_goedel" }
};

void cos_v223_build(cos_v223_state_t *s) {
    for (int i = 0; i < COS_V223_N_PATHS; ++i) {
        cos_v223_path_t *p = &s->paths[i];
        memset(p, 0, sizeof(*p));
        p->id                         = i;
        p->problem_type               = PFX[i].pt;
        p->chosen_strategy            = PFX[i].strat;
        p->produced_meta              = PFX[i].meta != 0;
        p->detected_bias_sigma[COS_V223_BIAS_ANCHORING]    = PFX[i].bias_anchor;
        p->detected_bias_sigma[COS_V223_BIAS_CONFIRMATION] = PFX[i].bias_confirm;
        p->detected_bias_sigma[COS_V223_BIAS_AVAILABILITY] = PFX[i].bias_avail;
        p->sigma_goedel               = PFX[i].goedel;
        strncpy(p->note, PFX[i].note, COS_V223_STR_MAX - 1);
    }
}

void cos_v223_run(cos_v223_state_t *s) {
    uint64_t prev = 0x223D11A1ULL;

    s->n_metacognitive = 0;
    s->n_bias_flags_hi = 0;
    s->n_goedel_hi     = 0;

    for (int i = 0; i < COS_V223_N_PATHS; ++i) {
        cos_v223_path_t *p = &s->paths[i];

        /* σ_choice from the prior matrix. */
        p->sigma_choice =
            s->sigma_strategy[p->problem_type][p->chosen_strategy];

        /* σ_meta = |σ_choice − reflect_mean| (confidence
         * in the meta-analysis itself: close to the
         * population average = low novelty → low σ). */
        p->sigma_meta = fabsf(p->sigma_choice - s->reflect_mean);

        /* σ_bias = max detected bias σ. */
        float mb = 0.0f;
        for (int b = 0; b < COS_V223_N_BIASES; ++b)
            if (p->detected_bias_sigma[b] > mb)
                mb = p->detected_bias_sigma[b];
        p->sigma_bias = mb;

        /* σ_goedel clamped into [0.10, 1.00]. */
        float g = p->sigma_goedel;
        if (g < 0.10f) g = 0.10f;
        if (g > 1.00f) g = 1.00f;
        p->sigma_goedel = g;

        p->sigma_total = s->w_choice * p->sigma_choice
                       + s->w_meta   * p->sigma_meta
                       + s->w_bias   * p->sigma_bias
                       + s->w_goedel * p->sigma_goedel;
        if (p->sigma_total < 0.0f) p->sigma_total = 0.0f;
        if (p->sigma_total > 1.0f) p->sigma_total = 1.0f;

        if (p->produced_meta)         s->n_metacognitive++;
        if (p->sigma_bias >= 0.30f)   s->n_bias_flags_hi++;
        if (p->sigma_goedel >= 0.80f) s->n_goedel_hi++;

        p->hash_prev = prev;
        struct { int id, pt, strat, meta;
                 float sc, sm, sb, sg, st;
                 float ba, bc, bv;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id    = p->id;
        rec.pt    = p->problem_type;
        rec.strat = p->chosen_strategy;
        rec.meta  = p->produced_meta ? 1 : 0;
        rec.sc    = p->sigma_choice;
        rec.sm    = p->sigma_meta;
        rec.sb    = p->sigma_bias;
        rec.sg    = p->sigma_goedel;
        rec.st    = p->sigma_total;
        rec.ba    = p->detected_bias_sigma[COS_V223_BIAS_ANCHORING];
        rec.bc    = p->detected_bias_sigma[COS_V223_BIAS_CONFIRMATION];
        rec.bv    = p->detected_bias_sigma[COS_V223_BIAS_AVAILABILITY];
        rec.prev  = prev;
        p->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = p->hash_curr;
    }
    s->terminal_hash = prev;

    uint64_t v = 0x223D11A1ULL;
    s->chain_valid = true;
    for (int i = 0; i < COS_V223_N_PATHS; ++i) {
        const cos_v223_path_t *p = &s->paths[i];
        if (p->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, pt, strat, meta;
                 float sc, sm, sb, sg, st;
                 float ba, bc, bv;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id    = p->id;
        rec.pt    = p->problem_type;
        rec.strat = p->chosen_strategy;
        rec.meta  = p->produced_meta ? 1 : 0;
        rec.sc    = p->sigma_choice;
        rec.sm    = p->sigma_meta;
        rec.sb    = p->sigma_bias;
        rec.sg    = p->sigma_goedel;
        rec.st    = p->sigma_total;
        rec.ba    = p->detected_bias_sigma[COS_V223_BIAS_ANCHORING];
        rec.bc    = p->detected_bias_sigma[COS_V223_BIAS_CONFIRMATION];
        rec.bv    = p->detected_bias_sigma[COS_V223_BIAS_AVAILABILITY];
        rec.prev  = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != p->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v223_to_json(const cos_v223_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v223\","
        "\"n_paths\":%d,\"n_strats\":%d,\"n_probtypes\":%d,\"n_biases\":%d,"
        "\"weights\":[%.3f,%.3f,%.3f,%.3f],"
        "\"reflect_mean\":%.4f,"
        "\"n_metacognitive\":%d,\"n_bias_flags_hi\":%d,"
        "\"n_goedel_hi\":%d,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"sigma_strategy\":[",
        COS_V223_N_PATHS, COS_V223_N_STRATS, COS_V223_N_PROBTYPES, COS_V223_N_BIASES,
        s->w_choice, s->w_meta, s->w_bias, s->w_goedel,
        s->reflect_mean,
        s->n_metacognitive, s->n_bias_flags_hi, s->n_goedel_hi,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int p = 0; p < COS_V223_N_PROBTYPES; ++p) {
        int k = snprintf(buf + off, cap - off,
            "%s[%.3f,%.3f,%.3f,%.3f,%.3f]",
            p == 0 ? "" : ",",
            s->sigma_strategy[p][0], s->sigma_strategy[p][1],
            s->sigma_strategy[p][2], s->sigma_strategy[p][3],
            s->sigma_strategy[p][4]);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int q = snprintf(buf + off, cap - off, "],\"paths\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;

    for (int i = 0; i < COS_V223_N_PATHS; ++i) {
        const cos_v223_path_t *p = &s->paths[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"pt\":%d,\"strat\":%d,\"meta\":%s,"
            "\"note\":\"%s\","
            "\"sigma_choice\":%.4f,\"sigma_meta\":%.4f,"
            "\"sigma_bias\":%.4f,\"sigma_goedel\":%.4f,"
            "\"sigma_total\":%.4f,"
            "\"bias\":[%.3f,%.3f,%.3f]}",
            i == 0 ? "" : ",", p->id, p->problem_type, p->chosen_strategy,
            p->produced_meta ? "true" : "false", p->note,
            p->sigma_choice, p->sigma_meta, p->sigma_bias,
            p->sigma_goedel, p->sigma_total,
            p->detected_bias_sigma[0], p->detected_bias_sigma[1],
            p->detected_bias_sigma[2]);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v223_self_test(void) {
    cos_v223_state_t s;
    cos_v223_init(&s, 0x223E70DCULL);
    cos_v223_build(&s);
    cos_v223_run(&s);

    if (!s.chain_valid) return 1;

    /* Tool/task fit priors. */
    if (s.sigma_strategy[COS_V223_PT_LOGIC][COS_V223_STRAT_DEDUCTION] > 0.15f) return 2;
    if (s.sigma_strategy[COS_V223_PT_LOGIC][COS_V223_STRAT_HEURISTIC] < 0.50f) return 2;
    if (s.sigma_strategy[COS_V223_PT_CREATIVE][COS_V223_STRAT_DEDUCTION] < 0.60f) return 2;
    if (s.sigma_strategy[COS_V223_PT_CREATIVE][COS_V223_STRAT_ANALOGY]   > 0.25f) return 2;

    /* Strategy awareness: σ_choice matches the prior. */
    for (int i = 0; i < COS_V223_N_PATHS; ++i) {
        const cos_v223_path_t *p = &s.paths[i];
        float exp = s.sigma_strategy[p->problem_type][p->chosen_strategy];
        if (fabsf(p->sigma_choice - exp) > 1e-6f) return 3;
        if (p->sigma_total < 0.0f || p->sigma_total > 1.0f) return 4;
        if (p->sigma_goedel < 0.10f - 1e-6f || p->sigma_goedel > 1.0f + 1e-6f) return 4;
    }

    if (s.n_metacognitive < 5)   return 5;
    if (s.n_bias_flags_hi < 1)   return 6;
    if (s.n_goedel_hi     < 1)   return 7;
    return 0;
}
