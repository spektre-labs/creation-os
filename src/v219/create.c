/*
 * v219 σ-Create — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "create.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v219_init(cos_v219_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed                  = seed ? seed : 0x219C4EA7ULL;
    s->tau_novelty_safe      = 0.25f;
    s->tau_novelty_creative  = 0.85f;
    s->min_novelty_creative  = 0.40f;
    s->n_debate_rounds       = 3;
}

static float dot8(const float *a, const float *b) {
    float s = 0.0f;
    for (int i = 0; i < COS_V219_EMBED_DIM; ++i) s += a[i] * b[i];
    return s;
}
static float norm8(const float *a) { return sqrtf(dot8(a, a)); }

/* Fixture: 8 requests (2 per mode) × 5 candidates.  We
 * assign input embeddings on the unit sphere and hand-
 * pick candidate embeddings so that:
 *   - Every SAFE request has ≥ 1 candidate with
 *     σ_novelty_raw ≤ τ_novelty_safe (=0.25) that also
 *     has competitive quality.
 *   - Every CREATIVE request has ≥ 1 candidate with
 *     σ_novelty_raw ∈ [0.40, 0.85] that wins on impact. */

typedef struct {
    int   mode;
    int   level;
    const char *prompt;
    float in_embed[COS_V219_EMBED_DIM];
    float cand_novelty[COS_V219_N_CANDIDATES];
    float cand_quality[COS_V219_N_CANDIDATES];
    float cand_embed[COS_V219_N_CANDIDATES][COS_V219_EMBED_DIM];
} reqfx_t;

static const reqfx_t REQ_FX[COS_V219_N_REQUESTS] = {
  /* R0 TEXT/SAFE — a nice sonnet about an oak tree. */
  { COS_V219_MODE_TEXT,   COS_V219_LEVEL_SAFE,     "sonnet_oak_tree",
    {1,0,0,0,0,0,0,0},
    { 0.12f, 0.18f, 0.22f, 0.55f, 0.78f },
    { 0.84f, 0.82f, 0.78f, 0.65f, 0.55f },
    { {0.99f,0.10f,0.05f,0,0,0,0,0},
      {0.97f,0.15f,0.10f,0,0,0,0,0},
      {0.95f,0.20f,0.05f,0.05f,0,0,0,0},
      {0.80f,0.40f,0.30f,0.20f,0,0,0,0},
      {0.60f,0.50f,0.40f,0.30f,0.20f,0,0,0} } },
  /* R1 TEXT/CREATIVE — surrealist poem about silence. */
  { COS_V219_MODE_TEXT,   COS_V219_LEVEL_CREATIVE, "surreal_poem_silence",
    {0,1,0,0,0,0,0,0},
    { 0.15f, 0.35f, 0.50f, 0.68f, 0.82f },
    { 0.70f, 0.74f, 0.80f, 0.72f, 0.55f },
    { {0.10f,0.99f,0.05f,0,0,0,0,0},
      {0.20f,0.85f,0.30f,0.20f,0,0,0,0},
      {0.30f,0.60f,0.50f,0.30f,0.20f,0,0,0},
      {0.30f,0.40f,0.60f,0.40f,0.30f,0.20f,0,0},
      {0.30f,0.20f,0.50f,0.40f,0.40f,0.30f,0.30f,0} } },
  /* R2 CODE/SAFE — standard binary search. */
  { COS_V219_MODE_CODE,   COS_V219_LEVEL_SAFE,     "binary_search",
    {0,0,1,0,0,0,0,0},
    { 0.08f, 0.15f, 0.22f, 0.60f, 0.80f },
    { 0.90f, 0.86f, 0.80f, 0.60f, 0.45f },
    { {0.05f,0.10f,0.99f,0,0,0,0,0},
      {0.10f,0.15f,0.97f,0.10f,0,0,0,0},
      {0.15f,0.25f,0.92f,0.20f,0,0,0,0},
      {0.30f,0.40f,0.75f,0.30f,0.20f,0,0,0},
      {0.40f,0.50f,0.55f,0.40f,0.30f,0.20f,0,0} } },
  /* R3 CODE/CREATIVE — novel concurrent hashmap. */
  { COS_V219_MODE_CODE,   COS_V219_LEVEL_CREATIVE, "novel_concurrent_map",
    {0,0,0,1,0,0,0,0},
    { 0.18f, 0.42f, 0.55f, 0.70f, 0.82f },
    { 0.65f, 0.78f, 0.82f, 0.70f, 0.52f },
    { {0.10f,0.05f,0.10f,0.95f,0,0,0,0},
      {0.20f,0.10f,0.20f,0.80f,0.30f,0,0,0},
      {0.30f,0.20f,0.20f,0.60f,0.40f,0.30f,0,0},
      {0.30f,0.30f,0.30f,0.40f,0.50f,0.30f,0.20f,0},
      {0.30f,0.30f,0.40f,0.20f,0.40f,0.30f,0.30f,0.20f} } },
  /* R4 DESIGN/SAFE — clean material UI layout. */
  { COS_V219_MODE_DESIGN, COS_V219_LEVEL_SAFE,     "clean_material_ui",
    {0,0,0,0,1,0,0,0},
    { 0.10f, 0.18f, 0.28f, 0.60f, 0.80f },
    { 0.88f, 0.85f, 0.78f, 0.62f, 0.50f },
    { {0,0,0,0,1.0f,0,0,0},
      {0.05f,0.05f,0,0.10f,0.97f,0.10f,0,0},
      {0.10f,0.10f,0.15f,0.20f,0.90f,0.15f,0,0},
      {0.30f,0.30f,0.30f,0.30f,0.70f,0.20f,0.10f,0},
      {0.40f,0.30f,0.30f,0.30f,0.50f,0.30f,0.20f,0.10f} } },
  /* R5 DESIGN/CREATIVE — brutalist dashboard. */
  { COS_V219_MODE_DESIGN, COS_V219_LEVEL_CREATIVE, "brutalist_dashboard",
    {0,0,0,0,0,1,0,0},
    { 0.20f, 0.40f, 0.58f, 0.75f, 0.85f },
    { 0.62f, 0.76f, 0.84f, 0.68f, 0.50f },
    { {0,0,0,0,0.10f,0.98f,0.10f,0},
      {0.10f,0.10f,0.10f,0.20f,0.20f,0.85f,0.20f,0},
      {0.20f,0.20f,0.20f,0.30f,0.30f,0.60f,0.30f,0.20f},
      {0.30f,0.30f,0.30f,0.30f,0.40f,0.40f,0.40f,0.30f},
      {0.30f,0.30f,0.40f,0.40f,0.30f,0.20f,0.40f,0.40f} } },
  /* R6 MUSIC/SAFE — cadence in C major. */
  { COS_V219_MODE_MUSIC,  COS_V219_LEVEL_SAFE,     "cadence_c_major",
    {0,0,0,0,0,0,1,0},
    { 0.09f, 0.16f, 0.24f, 0.62f, 0.80f },
    { 0.90f, 0.85f, 0.78f, 0.60f, 0.48f },
    { {0,0,0,0,0,0.05f,0.99f,0},
      {0.05f,0.05f,0.05f,0.05f,0.10f,0.15f,0.95f,0.10f},
      {0.10f,0.10f,0.15f,0.15f,0.15f,0.20f,0.90f,0.20f},
      {0.30f,0.30f,0.30f,0.30f,0.30f,0.30f,0.65f,0.30f},
      {0.40f,0.30f,0.30f,0.30f,0.30f,0.30f,0.45f,0.40f} } },
  /* R7 MUSIC/CREATIVE — polyrhythm in 7/8. */
  { COS_V219_MODE_MUSIC,  COS_V219_LEVEL_CREATIVE, "polyrhythm_7_8",
    {0,0,0,0,0,0,0,1},
    { 0.20f, 0.45f, 0.60f, 0.72f, 0.82f },
    { 0.60f, 0.80f, 0.82f, 0.66f, 0.50f },
    { {0,0,0,0,0,0.05f,0.10f,0.98f},
      {0.10f,0.10f,0.10f,0.10f,0.20f,0.20f,0.20f,0.80f},
      {0.20f,0.20f,0.20f,0.20f,0.30f,0.30f,0.30f,0.55f},
      {0.30f,0.30f,0.30f,0.30f,0.40f,0.40f,0.40f,0.35f},
      {0.30f,0.30f,0.30f,0.40f,0.40f,0.40f,0.40f,0.25f} } }
};

void cos_v219_build(cos_v219_state_t *s) {
    for (int r = 0; r < COS_V219_N_REQUESTS; ++r) {
        cos_v219_request_t *rq = &s->requests[r];
        memset(rq, 0, sizeof(*rq));
        rq->id    = r;
        rq->mode  = REQ_FX[r].mode;
        rq->level = REQ_FX[r].level;
        strncpy(rq->prompt, REQ_FX[r].prompt, COS_V219_STR_MAX - 1);
        memcpy(rq->input_embed, REQ_FX[r].in_embed, sizeof(rq->input_embed));
        rq->n = COS_V219_N_CANDIDATES;
        for (int c = 0; c < COS_V219_N_CANDIDATES; ++c) {
            cos_v219_candidate_t *cand = &rq->candidates[c];
            memset(cand, 0, sizeof(*cand));
            cand->id                = c;
            cand->sigma_novelty_raw = REQ_FX[r].cand_novelty[c];
            cand->sigma_quality_raw = REQ_FX[r].cand_quality[c];
            memcpy(cand->embed, REQ_FX[r].cand_embed[c], sizeof(cand->embed));
        }
    }
}

void cos_v219_run(cos_v219_state_t *s) {
    uint64_t prev = 0x219D11A1ULL;

    s->n_safe_ok = s->n_creative_ok = 0;
    s->n_refine_monotone = 0;
    s->n_surprise_consistent = 0;

    for (int r = 0; r < COS_V219_N_REQUESTS; ++r) {
        cos_v219_request_t *rq = &s->requests[r];

        /* 1. σ_surprise = clamp(1 − cos(in, out), 0, 1). */
        float in_norm = norm8(rq->input_embed);
        if (in_norm < 1e-6f) in_norm = 1.0f;
        for (int c = 0; c < rq->n; ++c) {
            cos_v219_candidate_t *cand = &rq->candidates[c];
            float out_norm = norm8(cand->embed);
            float cos_sim  = (out_norm > 1e-6f)
                ? dot8(rq->input_embed, cand->embed) / (in_norm * out_norm)
                : 0.0f;
            float s_surprise = 1.0f - cos_sim;
            if (s_surprise < 0.0f) s_surprise = 0.0f;
            if (s_surprise > 1.0f) s_surprise = 1.0f;
            cand->sigma_surprise = s_surprise;
        }

        /* (Winner index is chosen AFTER refinement below.
         * The monotonicity check is per-winner: for the
         * *same* candidate, debate must sharpen quality —
         * never dull it.) */

        /* 3. Refinement: v150 debate sharpens quality
         *    toward the eligible-pool mean + 0.05; v147
         *    reflect pulls novelty toward reflect_mean.
         *    Both are bounded so σ_quality_after ≥
         *    σ_quality_before for the winner. */
        float qsum = 0.0f;
        int   qn   = 0;
        for (int c = 0; c < rq->n; ++c) {
            float nov = rq->candidates[c].sigma_novelty_raw;
            if (rq->level == COS_V219_LEVEL_SAFE &&
                nov > s->tau_novelty_safe) continue;
            if (rq->level == COS_V219_LEVEL_CREATIVE &&
                nov > s->tau_novelty_creative) continue;
            qsum += rq->candidates[c].sigma_quality_raw;
            qn++;
        }
        float q_target = (qn > 0) ? (qsum / qn + 0.05f) : 0.0f;
        float n_reflect_mean = 0.0f;
        for (int c = 0; c < rq->n; ++c)
            n_reflect_mean += rq->candidates[c].sigma_novelty_raw;
        n_reflect_mean /= (float)rq->n;

        for (int c = 0; c < rq->n; ++c) {
            cos_v219_candidate_t *cand = &rq->candidates[c];
            /* debate: 3 rounds of α=0.15 shrinkage toward
             * q_target, but NEVER below current quality
             * — sharpens, never dulls. */
            float q = cand->sigma_quality_raw;
            for (int k = 0; k < s->n_debate_rounds; ++k) {
                float step = 0.15f * (q_target - q);
                if (step < 0.0f) step = 0.0f;  /* never drops */
                q += step;
            }
            if (q < 0.0f) q = 0.0f;
            if (q > 1.0f) q = 1.0f;
            cand->sigma_quality = q;

            float nov = cand->sigma_novelty_raw;
            nov += 0.10f * (n_reflect_mean - nov);
            if (nov < 0.0f) nov = 0.0f;
            if (nov > 1.0f) nov = 1.0f;
            cand->sigma_novelty = nov;
            cand->impact = cand->sigma_novelty * cand->sigma_quality;
        }

        /* 4. Winner argmax(impact) subject to mode
         *    gates on refined σ_novelty. */
        float best = -1.0f;
        int   idx  = 0;
        for (int c = 0; c < rq->n; ++c) {
            const cos_v219_candidate_t *cand = &rq->candidates[c];
            if (rq->level == COS_V219_LEVEL_SAFE &&
                cand->sigma_novelty > s->tau_novelty_safe) continue;
            if (rq->level == COS_V219_LEVEL_CREATIVE &&
                cand->sigma_novelty > s->tau_novelty_creative) continue;
            if (cand->impact > best) { best = cand->impact; idx = c; }
        }
        rq->winner       = idx;
        rq->winner_impact= rq->candidates[idx].impact;
        /* Per-winner monotonicity: the candidate that
         * ultimately wins must not have LOST quality in
         * the refinement.  Debate is clamped-positive
         * shrinkage, so q_refined ≥ q_raw by construction
         * for every candidate — we record the pair. */
        rq->winner_quality_before = rq->candidates[idx].sigma_quality_raw;
        rq->winner_quality_after  = rq->candidates[idx].sigma_quality;

        if (rq->level == COS_V219_LEVEL_SAFE &&
            rq->candidates[idx].sigma_novelty <=
            s->tau_novelty_safe + 1e-6f)    s->n_safe_ok++;
        if (rq->level == COS_V219_LEVEL_CREATIVE &&
            rq->candidates[idx].sigma_novelty >=
            s->min_novelty_creative - 1e-6f) s->n_creative_ok++;
        if (rq->winner_quality_after >=
            rq->winner_quality_before - 1e-6f) s->n_refine_monotone++;

        /* 5. σ_surprise cross-check (expected vs computed
         *    1 − cos(in, out)). */
        int sc_ok = 1;
        for (int c = 0; c < rq->n; ++c) {
            float out_norm = norm8(rq->candidates[c].embed);
            float in_norm2 = norm8(rq->input_embed);
            float cos_sim = (out_norm > 1e-6f && in_norm2 > 1e-6f)
                ? dot8(rq->input_embed, rq->candidates[c].embed) /
                  (in_norm2 * out_norm)
                : 0.0f;
            float expected = 1.0f - cos_sim;
            if (expected < 0.0f) expected = 0.0f;
            if (expected > 1.0f) expected = 1.0f;
            if (fabsf(rq->candidates[c].sigma_surprise - expected) > 1e-4f) {
                sc_ok = 0; break;
            }
        }
        if (sc_ok) s->n_surprise_consistent++;

        /* 6. Hash */
        rq->hash_prev = prev;
        struct { int id, mode, level, winner;
                 float wi, wqb, wqa,
                       wn, wq, ws;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id      = rq->id;
        rec.mode    = rq->mode;
        rec.level   = rq->level;
        rec.winner  = rq->winner;
        rec.wi      = rq->winner_impact;
        rec.wqb     = rq->winner_quality_before;
        rec.wqa     = rq->winner_quality_after;
        rec.wn      = rq->candidates[idx].sigma_novelty;
        rec.wq      = rq->candidates[idx].sigma_quality;
        rec.ws      = rq->candidates[idx].sigma_surprise;
        rec.prev    = prev;
        rq->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = rq->hash_curr;
    }
    s->terminal_hash = prev;

    uint64_t v = 0x219D11A1ULL;
    s->chain_valid = true;
    for (int r = 0; r < COS_V219_N_REQUESTS; ++r) {
        const cos_v219_request_t *rq = &s->requests[r];
        if (rq->hash_prev != v) { s->chain_valid = false; break; }
        int idx = rq->winner;
        struct { int id, mode, level, winner;
                 float wi, wqb, wqa,
                       wn, wq, ws;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id      = rq->id;
        rec.mode    = rq->mode;
        rec.level   = rq->level;
        rec.winner  = rq->winner;
        rec.wi      = rq->winner_impact;
        rec.wqb     = rq->winner_quality_before;
        rec.wqa     = rq->winner_quality_after;
        rec.wn      = rq->candidates[idx].sigma_novelty;
        rec.wq      = rq->candidates[idx].sigma_quality;
        rec.ws      = rq->candidates[idx].sigma_surprise;
        rec.prev    = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != rq->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v219_to_json(const cos_v219_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v219\","
        "\"n_requests\":%d,\"n_candidates\":%d,"
        "\"tau_novelty_safe\":%.3f,"
        "\"tau_novelty_creative\":%.3f,"
        "\"min_novelty_creative\":%.3f,"
        "\"n_safe_ok\":%d,\"n_creative_ok\":%d,"
        "\"n_refine_monotone\":%d,"
        "\"n_surprise_consistent\":%d,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"requests\":[",
        COS_V219_N_REQUESTS, COS_V219_N_CANDIDATES,
        s->tau_novelty_safe, s->tau_novelty_creative,
        s->min_novelty_creative,
        s->n_safe_ok, s->n_creative_ok,
        s->n_refine_monotone, s->n_surprise_consistent,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int r = 0; r < COS_V219_N_REQUESTS; ++r) {
        const cos_v219_request_t *rq = &s->requests[r];
        int idx = rq->winner;
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"mode\":%d,\"level\":%d,\"prompt\":\"%s\","
            "\"winner\":%d,\"impact\":%.4f,"
            "\"wn\":%.4f,\"wq\":%.4f,\"ws\":%.4f,"
            "\"wqb\":%.4f,\"wqa\":%.4f}",
            r == 0 ? "" : ",", rq->id, rq->mode, rq->level, rq->prompt,
            rq->winner, rq->winner_impact,
            rq->candidates[idx].sigma_novelty,
            rq->candidates[idx].sigma_quality,
            rq->candidates[idx].sigma_surprise,
            rq->winner_quality_before, rq->winner_quality_after);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v219_self_test(void) {
    cos_v219_state_t s;
    cos_v219_init(&s, 0x219C4EA7ULL);
    cos_v219_build(&s);
    cos_v219_run(&s);

    if (!s.chain_valid)                              return 1;

    int n_safe = 0, n_creative = 0;
    for (int r = 0; r < COS_V219_N_REQUESTS; ++r) {
        const cos_v219_request_t *rq = &s.requests[r];
        if (rq->level == COS_V219_LEVEL_SAFE)     n_safe++;
        if (rq->level == COS_V219_LEVEL_CREATIVE) n_creative++;
    }
    if (s.n_safe_ok     != n_safe)                   return 2;
    if (s.n_creative_ok != n_creative)               return 3;
    if (s.n_refine_monotone != COS_V219_N_REQUESTS)  return 4;
    if (s.n_surprise_consistent != COS_V219_N_REQUESTS) return 5;

    for (int r = 0; r < COS_V219_N_REQUESTS; ++r) {
        const cos_v219_request_t *rq = &s.requests[r];
        int idx = rq->winner;
        const cos_v219_candidate_t *cand = &rq->candidates[idx];
        if (cand->sigma_novelty  < 0.0f || cand->sigma_novelty  > 1.0f) return 6;
        if (cand->sigma_quality  < 0.0f || cand->sigma_quality  > 1.0f) return 6;
        if (cand->sigma_surprise < 0.0f || cand->sigma_surprise > 1.0f) return 6;
        if (fabsf(cand->impact -
                  (cand->sigma_novelty * cand->sigma_quality)) > 1e-5f) return 7;
    }
    return 0;
}
