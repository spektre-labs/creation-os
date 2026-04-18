/*
 * v195 σ-Recover — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "recover.h"

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

void cos_v195_init(cos_v195_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x195EC0E5ULL;
    s->n_incidents = COS_V195_N_INCIDENTS;
}

/* ---- canonical operator table ------------------------------ */

static int canonical_ops(int class_, int out[COS_V195_MAX_OPS]) {
    switch (class_) {
    case COS_V195_ERR_HALLUCINATION:
        out[0] = COS_V195_OP_STEER_TRUTHFUL;
        out[1] = COS_V195_OP_DPO_PAIR;
        return 2;
    case COS_V195_ERR_CAPABILITY_GAP:
        out[0] = COS_V195_OP_CURRICULUM;
        out[1] = COS_V195_OP_SKILL_ACQUIRE;
        return 2;
    case COS_V195_ERR_PLANNING_ERROR:
        out[0] = COS_V195_OP_REPLAN;
        return 1;
    case COS_V195_ERR_TOOL_FAILURE:
        out[0] = COS_V195_OP_HEAL_RETRY;
        return 1;
    case COS_V195_ERR_CONTEXT_LOSS:
        out[0] = COS_V195_OP_CHECKPOINT;
        return 1;
    }
    return 0;
}

/* ---- fixture ------------------------------------------------ */

void cos_v195_build(cos_v195_state_t *s) {
    /* 30 incidents: 6 per class, two passes (15 + 15).
     *   Pass 0: every incident gets full canonical operator set.
     *   Pass 1: learnt recovery — each incident consumes at most
     *           ⌈n_ops/2⌉ operators (rounded up) because the
     *           v174 flywheel has already trained on the pass-0
     *           (error, recovery) pair. */
    int id = 0;
    for (int pass = 0; pass < 2; ++pass) {
        for (int c = 0; c < COS_V195_N_CLASSES; ++c) {
            for (int k = 0; k < 3; ++k) {
                cos_v195_incident_t *in = &s->incidents[id++];
                memset(in, 0, sizeof(*in));
                in->id      = id - 1;
                in->class_  = c;
                /* Hallucinations must cover all 4 domains across the
                 * two passes so every domain's ECE has signal. */
                if (c == COS_V195_ERR_HALLUCINATION)
                    in->domain = (k + pass * 3) % COS_V195_N_DOMAINS;
                else
                    in->domain = (c + k) % COS_V195_N_DOMAINS;
                in->sigma   = (c == COS_V195_ERR_HALLUCINATION) ? 0.12f
                             : (c == COS_V195_ERR_CAPABILITY_GAP) ? 0.80f
                             : 0.35f;
                in->tau     = 0.50f;
                in->correct = (c == COS_V195_ERR_CAPABILITY_GAP);
                in->pass    = pass;

                int ops[COS_V195_MAX_OPS];
                int n = canonical_ops(c, ops);
                int n_use = (pass == 0) ? n : ((n + 1) / 2);  /* ⌈n/2⌉ */
                in->n_recovery_ops = n_use;
                for (int j = 0; j < n_use; ++j) in->recovery_ops[j] = ops[j];
            }
        }
    }
    s->n_incidents = id;
    if (s->n_incidents > COS_V195_N_INCIDENTS)
        s->n_incidents = COS_V195_N_INCIDENTS;

    /* Baseline ECE per domain: higher where hallucinations cluster. */
    for (int dom = 0; dom < COS_V195_N_DOMAINS; ++dom)
        s->ece_before[dom] = 0.08f + 0.01f * dom; /* 0.08..0.11 */
}

/* ---- run ---------------------------------------------------- */

void cos_v195_run(cos_v195_state_t *s) {
    memset(s->class_counts, 0, sizeof(s->class_counts));
    s->n_ops_pass0 = s->n_ops_pass1 = 0;
    s->class_to_ops_canonical = true;

    /* Chain. */
    uint64_t prev = 0x195CA11ULL;
    /* Count hallucinations per domain — they drive ECE update. */
    int hall_per_dom[COS_V195_N_DOMAINS] = {0};
    for (int i = 0; i < s->n_incidents; ++i) {
        cos_v195_incident_t *in = &s->incidents[i];
        s->class_counts[in->class_]++;
        if (in->pass == 0) s->n_ops_pass0 += in->n_recovery_ops;
        else                s->n_ops_pass1 += in->n_recovery_ops;

        /* Canonical-ops partition check. */
        int canon[COS_V195_MAX_OPS];
        int nc = canonical_ops(in->class_, canon);
        for (int j = 0; j < in->n_recovery_ops; ++j) {
            int found = 0;
            for (int k = 0; k < nc; ++k)
                if (canon[k] == in->recovery_ops[j]) { found = 1; break; }
            if (!found) s->class_to_ops_canonical = false;
        }

        if (in->class_ == COS_V195_ERR_HALLUCINATION)
            hall_per_dom[in->domain]++;

        in->hash_prev = prev;
        struct {
            int id, class_, domain, pass, n_ops;
            float sigma;
            int ops[COS_V195_MAX_OPS];
            uint64_t prev;
        } rec = { in->id, in->class_, in->domain, in->pass,
                  in->n_recovery_ops, in->sigma,
                  { in->recovery_ops[0], in->recovery_ops[1],
                    in->recovery_ops[2] }, prev };
        in->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = in->hash_curr;
    }
    s->terminal_hash = prev;

    /* Per-domain ECE update: every hallucination on domain d
     * lowers ece_after[d] by 0.02 (capped at 0.01). */
    for (int dom = 0; dom < COS_V195_N_DOMAINS; ++dom) {
        float drop = 0.02f * (float)hall_per_dom[dom];
        float e = s->ece_before[dom] - drop;
        if (e < 0.01f) e = 0.01f;
        s->ece_after[dom] = e;
    }

    /* Chain verify. */
    uint64_t v = 0x195CA11ULL;
    s->chain_valid = true;
    for (int i = 0; i < s->n_incidents; ++i) {
        const cos_v195_incident_t *in = &s->incidents[i];
        if (in->hash_prev != v) { s->chain_valid = false; break; }
        struct {
            int id, class_, domain, pass, n_ops;
            float sigma;
            int ops[COS_V195_MAX_OPS];
            uint64_t prev;
        } rec = { in->id, in->class_, in->domain, in->pass,
                  in->n_recovery_ops, in->sigma,
                  { in->recovery_ops[0], in->recovery_ops[1],
                    in->recovery_ops[2] }, v };
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != in->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

/* ---- JSON -------------------------------------------------- */

size_t cos_v195_to_json(const cos_v195_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v195\","
        "\"n_incidents\":%d,"
        "\"class_counts\":[%d,%d,%d,%d,%d],"
        "\"n_ops_pass0\":%d,\"n_ops_pass1\":%d,"
        "\"ece_before\":[%.4f,%.4f,%.4f,%.4f],"
        "\"ece_after\":[%.4f,%.4f,%.4f,%.4f],"
        "\"class_to_ops_canonical\":%s,"
        "\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\","
        "\"incidents\":[",
        s->n_incidents,
        s->class_counts[0], s->class_counts[1], s->class_counts[2],
        s->class_counts[3], s->class_counts[4],
        s->n_ops_pass0, s->n_ops_pass1,
        s->ece_before[0], s->ece_before[1], s->ece_before[2], s->ece_before[3],
        s->ece_after[0], s->ece_after[1], s->ece_after[2], s->ece_after[3],
        s->class_to_ops_canonical ? "true" : "false",
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n_incidents; ++i) {
        const cos_v195_incident_t *in = &s->incidents[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"class\":%d,\"domain\":%d,"
            "\"sigma\":%.3f,\"pass\":%d,\"n_ops\":%d,"
            "\"ops\":[%d,%d,%d]}",
            i == 0 ? "" : ",", in->id, in->class_, in->domain,
            in->sigma, in->pass, in->n_recovery_ops,
            in->recovery_ops[0], in->recovery_ops[1], in->recovery_ops[2]);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

/* ---- self-test -------------------------------------------- */

int cos_v195_self_test(void) {
    cos_v195_state_t s;
    cos_v195_init(&s, 0x195EC0E5ULL);
    cos_v195_build(&s);
    cos_v195_run(&s);

    if (s.n_incidents != COS_V195_N_INCIDENTS) return 1;
    for (int c = 0; c < COS_V195_N_CLASSES; ++c)
        if (s.class_counts[c] < 1) return 2;
    if (!s.class_to_ops_canonical) return 3;
    if (!s.chain_valid)             return 4;

    /* Learning: pass1 ops < pass0 ops strictly. */
    if (!(s.n_ops_pass1 < s.n_ops_pass0)) return 5;

    /* ECE strictly drops on every domain that had a
     * hallucination incident — fixture ensures every domain
     * gets at least one. */
    for (int d = 0; d < COS_V195_N_DOMAINS; ++d)
        if (!(s.ece_after[d] < s.ece_before[d])) return 6;

    return 0;
}
