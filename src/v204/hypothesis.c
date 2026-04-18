/*
 * v204 σ-Hypothesis — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "hypothesis.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v204_init(cos_v204_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed       = seed ? seed : 0x2040471DULL;
    s->tau_spec   = 0.60f;
    s->tau_ground = 0.55f;
    s->observation_topic    = 7;                /* "σ drives K_eff" */
    s->observation_severity = 0.75f;
}

/* Candidate templates.  Each is a closed-form variant of
 * the observation "σ drives K_eff" with engineered scores
 * so the ranking contract is deterministic. */
typedef struct { const char *stmt; float sh; float sg; } tmpl_t;
static const tmpl_t TMPL[COS_V204_N_HYPOTHESES] = {
    /* stmt                                         σ_h   σ_g  */
    { "σ monotonically reduces K_eff",              0.12f, 0.05f }, /* strong */
    { "τ couples σ into K_eff",                     0.18f, 0.08f }, /* strong */
    { "σ is noise; K_eff independent",              0.35f, 0.95f }, /* contradicted (grounded-out) */
    { "K_eff bounds actionable bandwidth",          0.22f, 0.10f }, /* novel */
    { "σ spikes cause habit break-out",             0.28f, 0.12f },
    { "σ is measurable by firmware only",           0.55f, 0.80f }, /* grounded-out */
    { "calibration update mirrors K_eff update",    0.30f, 0.15f },
    { "novelty × (1-σ) predicts impact",            0.65f, 0.18f }, /* speculative */
    { "σ ≥ K_crit implies institutional collapse",  0.68f, 0.20f }, /* speculative */
    { "σ is an irrelevant internal variable",       0.75f, 0.88f }  /* grounded-out + spec */
};

/* Deterministic 16-dim hash embedding. */
static void embed(const char *s, float emb[COS_V204_EMBED_DIM]) {
    uint64_t h = fnv1a(s, strlen(s), 0xEEEEULL);
    for (int k = 0; k < COS_V204_EMBED_DIM; ++k) {
        h = h * 0x100000001b3ULL + (uint64_t)(k + 1);
        uint32_t lo = (uint32_t)(h & 0xFFFFFFFFULL);
        emb[k] = ((float)lo / (float)0xFFFFFFFFu) * 2.0f - 1.0f;
    }
}

/* 5 known-hypothesis embeddings (closed form: embed of the
 * canonical statements). */
static const char *KNOWN[COS_V204_N_KNOWN] = {
    "σ is a confidence signal",
    "σ gates downstream actions",
    "K_eff depends on σ",
    "calibration improves with feedback",
    "unknown hypotheses are new"
};

static float l2(const float *a, const float *b) {
    float s = 0.0f;
    for (int k = 0; k < COS_V204_EMBED_DIM; ++k) {
        float d = a[k] - b[k];
        s += d * d;
    }
    return sqrtf(s);
}

static float novelty_score(const float *cand_emb,
                            float known[COS_V204_N_KNOWN][COS_V204_EMBED_DIM]) {
    float min_d = 1e9f;
    for (int i = 0; i < COS_V204_N_KNOWN; ++i) {
        float d = l2(cand_emb, known[i]);
        if (d < min_d) min_d = d;
    }
    /* Map distance to [0, 1] — embeddings live in [-1, 1]^16, so
     * maximal L2 ≈ sqrt(16 * 4) = 8. */
    float n = min_d / 8.0f;
    if (n < 0.0f) n = 0.0f;
    if (n > 1.0f) n = 1.0f;
    return n;
}

void cos_v204_build(cos_v204_state_t *s) {
    s->n = COS_V204_N_HYPOTHESES;
    for (int i = 0; i < s->n; ++i) {
        cos_v204_hypothesis_t *h = &s->hypotheses[i];
        memset(h, 0, sizeof(*h));
        h->id = i;
        strncpy(h->statement, TMPL[i].stmt, COS_V204_STR_MAX - 1);
        h->sigma_hypothesis = TMPL[i].sh;
        h->sigma_grounding  = TMPL[i].sg;
        embed(h->statement, h->embedding);
    }
}

void cos_v204_run(cos_v204_state_t *s) {
    /* Precompute known embeddings. */
    float known[COS_V204_N_KNOWN][COS_V204_EMBED_DIM];
    for (int i = 0; i < COS_V204_N_KNOWN; ++i) embed(KNOWN[i], known[i]);

    s->n_speculative = s->n_rejected = 0;
    s->n_test_queue  = s->n_to_memory = 0;

    for (int i = 0; i < s->n; ++i) {
        cos_v204_hypothesis_t *h = &s->hypotheses[i];
        h->sigma_novelty = novelty_score(h->embedding, known);

        float raw_impact =
            h->sigma_novelty * (1.0f - h->sigma_hypothesis);
        bool grounded_out = h->sigma_grounding > s->tau_ground;
        h->impact = grounded_out ? 0.0f : raw_impact;

        if (grounded_out) {
            h->status = COS_V204_STATUS_REJECTED;
            s->n_rejected++;
        } else if (h->sigma_hypothesis > s->tau_spec) {
            h->status = COS_V204_STATUS_SPECULATIVE;
            s->n_speculative++;
        } else {
            h->status = COS_V204_STATUS_CONFIDENT;
        }
    }

    /* Rank by impact (stable sort by (impact desc, id asc)). */
    int order[COS_V204_N_HYPOTHESES];
    for (int i = 0; i < s->n; ++i) order[i] = i;
    for (int i = 0; i < s->n - 1; ++i)
        for (int j = i + 1; j < s->n; ++j) {
            const cos_v204_hypothesis_t *a = &s->hypotheses[order[i]];
            const cos_v204_hypothesis_t *b = &s->hypotheses[order[j]];
            if (b->impact > a->impact + 1e-9f ||
                (fabsf(b->impact - a->impact) < 1e-9f &&
                 order[j] < order[i])) {
                int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
            }
        }

    for (int r = 0; r < s->n; ++r) {
        cos_v204_hypothesis_t *h = &s->hypotheses[order[r]];
        h->rank = r + 1;
        if (r < COS_V204_TEST_QUEUE_SIZE) {
            h->dest = COS_V204_DEST_TEST_QUEUE;
            s->n_test_queue++;
        } else {
            h->dest = COS_V204_DEST_MEMORY;
            s->n_to_memory++;
        }
    }

    /* Hash chain over hypotheses in rank order. */
    uint64_t prev = 0x2040411DULL;
    s->chain_valid = true;
    for (int r = 0; r < s->n; ++r) {
        cos_v204_hypothesis_t *h = &s->hypotheses[order[r]];
        h->hash_prev = prev;
        struct { int id, rank, status, dest;
                 float sh, sg, sn, imp;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id     = h->id;
        rec.rank   = h->rank;
        rec.status = h->status;
        rec.dest   = h->dest;
        rec.sh     = h->sigma_hypothesis;
        rec.sg     = h->sigma_grounding;
        rec.sn     = h->sigma_novelty;
        rec.imp    = h->impact;
        rec.prev   = prev;
        h->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = h->hash_curr;
    }
    s->terminal_hash = prev;

    /* Replay in the same rank order. */
    uint64_t v = 0x2040411DULL;
    for (int r = 0; r < s->n; ++r) {
        const cos_v204_hypothesis_t *h = &s->hypotheses[order[r]];
        if (h->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, rank, status, dest;
                 float sh, sg, sn, imp;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id     = h->id;
        rec.rank   = h->rank;
        rec.status = h->status;
        rec.dest   = h->dest;
        rec.sh     = h->sigma_hypothesis;
        rec.sg     = h->sigma_grounding;
        rec.sn     = h->sigma_novelty;
        rec.imp    = h->impact;
        rec.prev   = v;
        uint64_t hh = fnv1a(&rec, sizeof(rec), v);
        if (hh != h->hash_curr) { s->chain_valid = false; break; }
        v = hh;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v204_to_json(const cos_v204_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v204\","
        "\"n\":%d,\"tau_spec\":%.3f,\"tau_ground\":%.3f,"
        "\"n_speculative\":%d,\"n_rejected\":%d,"
        "\"n_test_queue\":%d,\"n_to_memory\":%d,"
        "\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\","
        "\"hypotheses\":[",
        s->n, s->tau_spec, s->tau_ground,
        s->n_speculative, s->n_rejected,
        s->n_test_queue, s->n_to_memory,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n; ++i) {
        const cos_v204_hypothesis_t *h = &s->hypotheses[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"rank\":%d,\"status\":%d,\"dest\":%d,"
            "\"sh\":%.3f,\"sg\":%.3f,\"sn\":%.3f,\"imp\":%.3f}",
            i == 0 ? "" : ",", h->id, h->rank, h->status, h->dest,
            h->sigma_hypothesis, h->sigma_grounding,
            h->sigma_novelty, h->impact);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v204_self_test(void) {
    cos_v204_state_t s;
    cos_v204_init(&s, 0x2040471DULL);
    cos_v204_build(&s);
    cos_v204_run(&s);

    if (s.n             != COS_V204_N_HYPOTHESES)      return 1;
    if (s.n_test_queue  != COS_V204_TEST_QUEUE_SIZE)   return 2;
    if (s.n_to_memory   != s.n - COS_V204_TEST_QUEUE_SIZE) return 3;
    if (s.n_rejected    < 1)                           return 4;
    if (s.n_speculative < 1)                           return 5;
    if (!s.chain_valid)                                 return 6;

    /* σ ranges. */
    for (int i = 0; i < s.n; ++i) {
        const cos_v204_hypothesis_t *h = &s.hypotheses[i];
        if (h->sigma_hypothesis < 0.0f || h->sigma_hypothesis > 1.0f) return 7;
        if (h->sigma_grounding  < 0.0f || h->sigma_grounding  > 1.0f) return 7;
        if (h->sigma_novelty    < 0.0f || h->sigma_novelty    > 1.0f) return 7;
        if (h->impact           < 0.0f || h->impact           > 1.0f) return 7;
    }

    /* Top-3 strictly >= rest — min(top3 impact) >= max(non-top3 impact). */
    float min_top = 2.0f, max_rest = -1.0f;
    for (int i = 0; i < s.n; ++i) {
        const cos_v204_hypothesis_t *h = &s.hypotheses[i];
        if (h->dest == COS_V204_DEST_TEST_QUEUE) {
            if (h->impact < min_top) min_top = h->impact;
        } else {
            if (h->impact > max_rest) max_rest = h->impact;
        }
    }
    if (!(min_top >= max_rest)) return 8;

    /* At least one rejected must have impact == 0. */
    int zeroed = 0;
    for (int i = 0; i < s.n; ++i)
        if (s.hypotheses[i].status == COS_V204_STATUS_REJECTED &&
            s.hypotheses[i].impact == 0.0f) zeroed++;
    if (zeroed < 1) return 9;
    return 0;
}
