/*
 * v240 σ-Pipeline — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "pipeline.h"

#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static void cpy_name(char *dst, size_t cap, const char *src) {
    size_t n = 0;
    for (; n + 1 < cap && src[n]; ++n) dst[n] = src[n];
    dst[n] = '\0';
}

typedef struct {
    const char *name;
    float       sigma;
    int         source_shape; /* for fusion bookkeeping */
} cos_v240_stfx_t;

/* Clean shapes — every stage ≤ τ_branch = 0.50. */
static const cos_v240_stfx_t S_FACTUAL[] = {
    { "recall",  0.20f, COS_V240_SHAPE_FACTUAL },
    { "verify",  0.25f, COS_V240_SHAPE_FACTUAL },
    { "emit",    0.10f, COS_V240_SHAPE_FACTUAL },
};
static const cos_v240_stfx_t S_CREATIVE[] = {
    { "generate", 0.35f, COS_V240_SHAPE_CREATIVE },
    { "debate",   0.40f, COS_V240_SHAPE_CREATIVE },
    { "refine",   0.22f, COS_V240_SHAPE_CREATIVE },
    { "emit",     0.15f, COS_V240_SHAPE_CREATIVE },
};
static const cos_v240_stfx_t S_CODE[] = {
    { "plan",     0.28f, COS_V240_SHAPE_CODE },
    { "generate", 0.32f, COS_V240_SHAPE_CODE },
    { "sandbox",  0.18f, COS_V240_SHAPE_CODE },
    { "test",     0.20f, COS_V240_SHAPE_CODE },
    { "emit",     0.10f, COS_V240_SHAPE_CODE },
};
static const cos_v240_stfx_t S_MORAL[] = {
    { "analyse",         0.30f, COS_V240_SHAPE_MORAL },
    { "multi_framework", 0.38f, COS_V240_SHAPE_MORAL },
    { "geodesic",        0.24f, COS_V240_SHAPE_MORAL },
    { "emit",            0.15f, COS_V240_SHAPE_MORAL },
};

/* Branch fixture: factual starts clean, then `verify`
 * sees σ = 0.62 > τ_branch and the system branches to
 * EXPLORATORY with three more stages. */
static const cos_v240_stfx_t S_BRANCH_FACTUAL[] = {
    { "recall",  0.20f, COS_V240_SHAPE_FACTUAL },
    { "verify",  0.62f, COS_V240_SHAPE_FACTUAL },   /* branch fires here */
};
static const cos_v240_stfx_t S_BRANCH_EXPLORATORY[] = {
    { "retrieve_wide", 0.35f, COS_V240_SHAPE_EXPLORATORY },
    { "synthesise",    0.30f, COS_V240_SHAPE_EXPLORATORY },
    { "re_verify",     0.28f, COS_V240_SHAPE_EXPLORATORY },
    { "emit",          0.15f, COS_V240_SHAPE_EXPLORATORY },
};

/* Fusion fixture: CODE + MORAL → FUSED. */
static const cos_v240_stfx_t S_FUSED[] = {
    { "moral_analyse", 0.32f, COS_V240_SHAPE_MORAL },
    { "code_plan",     0.28f, COS_V240_SHAPE_CODE  },
    { "sandbox",       0.20f, COS_V240_SHAPE_CODE  },
    { "moral_verify",  0.34f, COS_V240_SHAPE_MORAL },
    { "emit",          0.12f, COS_V240_SHAPE_FUSED },
};

typedef struct {
    const char             *label;
    cos_v240_shape_t        shape;
    const cos_v240_stfx_t  *stages_a;
    int                     n_a;
    bool                    branch;
    const cos_v240_stfx_t  *stages_b; /* post-branch or null */
    int                     n_b;
    bool                    fuse;
    cos_v240_shape_t        fuse_to;
    cos_v240_shape_t        fusion_a;
    cos_v240_shape_t        fusion_b;
} cos_v240_fx_t;

static const cos_v240_fx_t FIX[COS_V240_N_REQUESTS] = {
    { "factual_clean",   COS_V240_SHAPE_FACTUAL,
      S_FACTUAL,  3, false, NULL, 0, false, 0, 0, 0 },
    { "creative_clean",  COS_V240_SHAPE_CREATIVE,
      S_CREATIVE, 4, false, NULL, 0, false, 0, 0, 0 },
    { "code_clean",      COS_V240_SHAPE_CODE,
      S_CODE,     5, false, NULL, 0, false, 0, 0, 0 },
    { "moral_clean",     COS_V240_SHAPE_MORAL,
      S_MORAL,    4, false, NULL, 0, false, 0, 0, 0 },
    { "branch_to_explo", COS_V240_SHAPE_FACTUAL,
      S_BRANCH_FACTUAL, 2, true,
      S_BRANCH_EXPLORATORY, 4, false, 0, 0, 0 },
    { "fuse_code_moral", COS_V240_SHAPE_FUSED,
      S_FUSED,    5, false, NULL, 0, true, COS_V240_SHAPE_FUSED,
      COS_V240_SHAPE_CODE, COS_V240_SHAPE_MORAL },
};

void cos_v240_init(cos_v240_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed       = seed ? seed : 0x240F1DE0ULL;
    s->tau_branch = 0.50f;
}

void cos_v240_run(cos_v240_state_t *s) {
    uint64_t prev = 0x240F1E5ULL;
    s->n_branched = s->n_fused = 0;
    int tick = 0;

    for (int i = 0; i < COS_V240_N_REQUESTS; ++i) {
        cos_v240_request_t *r = &s->requests[i];
        memset(r, 0, sizeof(*r));
        r->request_id     = i;
        cpy_name(r->label, sizeof(r->label), FIX[i].label);
        r->declared_shape = FIX[i].shape;
        r->final_shape    = FIX[i].shape;

        int n = 0;
        float max_sigma = 0.0f;

        for (int k = 0; k < FIX[i].n_a && n < COS_V240_MAX_STAGES; ++k) {
            cos_v240_stage_t *st = &r->stages[n];
            st->stage_id = n;
            cpy_name(st->name, sizeof(st->name), FIX[i].stages_a[k].name);
            st->sigma = FIX[i].stages_a[k].sigma;
            st->tick  = ++tick;
            if (st->sigma > max_sigma) max_sigma = st->sigma;
            n++;
        }

        if (FIX[i].branch) {
            r->branched          = true;
            r->branch_from_stage = r->stages[n - 1].stage_id;
            r->branch_from_shape = FIX[i].shape;
            r->branch_to_shape   = COS_V240_SHAPE_EXPLORATORY;
            r->sigma_at_branch   = r->stages[n - 1].sigma;
            r->final_shape       = COS_V240_SHAPE_EXPLORATORY;

            for (int k = 0; k < FIX[i].n_b && n < COS_V240_MAX_STAGES; ++k) {
                cos_v240_stage_t *st = &r->stages[n];
                st->stage_id = n;
                cpy_name(st->name, sizeof(st->name), FIX[i].stages_b[k].name);
                st->sigma = FIX[i].stages_b[k].sigma;
                st->tick  = ++tick;
                if (st->sigma > max_sigma) max_sigma = st->sigma;
                n++;
            }
            s->n_branched++;
        }

        if (FIX[i].fuse) {
            r->fused       = true;
            r->fusion_a    = FIX[i].fusion_a;
            r->fusion_b    = FIX[i].fusion_b;
            r->final_shape = FIX[i].fuse_to;
            s->n_fused++;
        }

        r->n_stages       = n;
        r->sigma_pipeline = max_sigma;

        struct { int id, ds, fs, nstg, br, fu;
                 int bfsg; int bfshape, btshape;
                 int fa, fb; float sab, sp; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id    = r->request_id;
        rec.ds    = (int)r->declared_shape;
        rec.fs    = (int)r->final_shape;
        rec.nstg  = r->n_stages;
        rec.br    = r->branched ? 1 : 0;
        rec.fu    = r->fused    ? 1 : 0;
        rec.bfsg  = r->branch_from_stage;
        rec.bfshape = (int)r->branch_from_shape;
        rec.btshape = (int)r->branch_to_shape;
        rec.fa    = (int)r->fusion_a;
        rec.fb    = (int)r->fusion_b;
        rec.sab   = r->sigma_at_branch;
        rec.sp    = r->sigma_pipeline;
        rec.prev  = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }

    struct { int nb, nf; float tau; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nb  = s->n_branched;
    trec.nf  = s->n_fused;
    trec.tau = s->tau_branch;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *shape_name(cos_v240_shape_t sh) {
    switch (sh) {
    case COS_V240_SHAPE_FACTUAL:     return "FACTUAL";
    case COS_V240_SHAPE_CREATIVE:    return "CREATIVE";
    case COS_V240_SHAPE_CODE:        return "CODE";
    case COS_V240_SHAPE_MORAL:       return "MORAL";
    case COS_V240_SHAPE_EXPLORATORY: return "EXPLORATORY";
    case COS_V240_SHAPE_FUSED:       return "FUSED";
    }
    return "UNKNOWN";
}

size_t cos_v240_to_json(const cos_v240_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v240\","
        "\"n_requests\":%d,\"tau_branch\":%.3f,"
        "\"n_branched\":%d,\"n_fused\":%d,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"requests\":[",
        COS_V240_N_REQUESTS, s->tau_branch,
        s->n_branched, s->n_fused,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V240_N_REQUESTS; ++i) {
        const cos_v240_request_t *r = &s->requests[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"request_id\":%d,\"label\":\"%s\","
            "\"declared_shape\":\"%s\",\"final_shape\":\"%s\","
            "\"n_stages\":%d,\"sigma_pipeline\":%.4f,"
            "\"branched\":%s,\"branch_from_stage\":%d,"
            "\"branch_from_shape\":\"%s\",\"branch_to_shape\":\"%s\","
            "\"sigma_at_branch\":%.4f,"
            "\"fused\":%s,\"fusion_a\":\"%s\",\"fusion_b\":\"%s\","
            "\"stages\":[",
            i == 0 ? "" : ",",
            r->request_id, r->label,
            shape_name(r->declared_shape), shape_name(r->final_shape),
            r->n_stages, r->sigma_pipeline,
            r->branched ? "true" : "false",
            r->branch_from_stage,
            shape_name(r->branch_from_shape),
            shape_name(r->branch_to_shape),
            r->sigma_at_branch,
            r->fused ? "true" : "false",
            shape_name(r->fusion_a), shape_name(r->fusion_b));
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
        for (int k = 0; k < r->n_stages; ++k) {
            const cos_v240_stage_t *st = &r->stages[k];
            int z = snprintf(buf + off, cap - off,
                "%s{\"stage_id\":%d,\"name\":\"%s\","
                "\"sigma\":%.4f,\"tick\":%d}",
                k == 0 ? "" : ",",
                st->stage_id, st->name, st->sigma, st->tick);
            if (z < 0 || off + (size_t)z >= cap) return 0;
            off += (size_t)z;
        }
        int z = snprintf(buf + off, cap - off, "]}");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v240_self_test(void) {
    cos_v240_state_t s;
    cos_v240_init(&s, 0x240F1DE0ULL);
    cos_v240_run(&s);
    if (!s.chain_valid) return 1;

    int n_branched = 0, n_fused = 0;
    for (int i = 0; i < COS_V240_N_REQUESTS; ++i) {
        const cos_v240_request_t *r = &s.requests[i];
        if (r->n_stages <= 0) return 2;
        float max_sigma = 0.0f;
        for (int k = 0; k < r->n_stages; ++k) {
            if (r->stages[k].sigma < 0.0f || r->stages[k].sigma > 1.0f) return 3;
            if (r->stages[k].sigma > max_sigma) max_sigma = r->stages[k].sigma;
            if (k > 0 && r->stages[k].tick <= r->stages[k - 1].tick) return 4;
        }
        if (r->sigma_pipeline != max_sigma) return 5;

        if (r->branched) {
            n_branched++;
            if (r->sigma_at_branch <= s.tau_branch) return 6;
            if (r->branch_to_shape != COS_V240_SHAPE_EXPLORATORY) return 7;
            /* at least one post-branch stage before emit */
            int post = r->n_stages - (r->branch_from_stage + 1);
            if (post < 2) return 8;  /* at least one payload + emit */
        } else {
            /* clean shape: σ_pipeline ≤ τ_branch */
            if (r->sigma_pipeline > s.tau_branch + 1e-6f && !r->fused) return 9;
        }
        if (r->fused) {
            n_fused++;
            if (r->final_shape != COS_V240_SHAPE_FUSED) return 10;
            /* must contain at least one stage from each source shape */
            bool has_code = false, has_moral = false;
            for (int k = 0; k < r->n_stages; ++k) {
                if (strcmp(r->stages[k].name, "code_plan") == 0 ||
                    strcmp(r->stages[k].name, "sandbox")   == 0)
                    has_code = true;
                if (strcmp(r->stages[k].name, "moral_analyse") == 0 ||
                    strcmp(r->stages[k].name, "moral_verify")  == 0)
                    has_moral = true;
            }
            if (!has_code || !has_moral) return 11;
        }
    }
    if (n_branched != 1) return 12;
    if (n_fused    != 1) return 13;
    if (s.n_branched != n_branched) return 14;
    if (s.n_fused    != n_fused)    return 14;

    cos_v240_state_t t;
    cos_v240_init(&t, 0x240F1DE0ULL);
    cos_v240_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 15;
    return 0;
}
