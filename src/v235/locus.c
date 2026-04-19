/*
 * v235 σ-Locus — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "locus.h"

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
    /* σ per node: 4 before + 4 after.  Fixture designed
     * so that at least one topic migrates and at least
     * two retain unity (low dispersion). */
    float sigma_before[COS_V235_N_NODES];
    float sigma_after [COS_V235_N_NODES];
} cos_v235_fx_t;

static const cos_v235_fx_t FIX[COS_V235_N_TOPICS] = {
    {
        "maths-proof",
        /* before: A=0.45 B=0.12 C=0.30 D=0.80  → locus B */
        { 0.45f, 0.12f, 0.30f, 0.80f },
        /* after : A=0.05 B=0.20 C=0.28 D=0.75  → locus A  (MIGRATED) */
        { 0.05f, 0.20f, 0.28f, 0.75f }
    },
    {
        "biology-QA",
        /* before: A=0.60 B=0.62 C=0.61 D=0.63  → locus A (tight unity) */
        { 0.60f, 0.62f, 0.61f, 0.63f },
        /* after : A=0.59 B=0.61 C=0.60 D=0.62  → locus A (stable) */
        { 0.59f, 0.61f, 0.60f, 0.62f }
    },
    {
        "code-gen",
        /* before: A=0.35 B=0.40 C=0.20 D=0.50  → locus C */
        { 0.35f, 0.40f, 0.20f, 0.50f },
        /* after : A=0.30 B=0.38 C=0.25 D=0.14  → locus D  (MIGRATED) */
        { 0.30f, 0.38f, 0.25f, 0.14f }
    },
};

/* Split-brain fixture: 4 nodes, cut into 3 + 1 after a
 * simulated network partition.  Partition A keeps three
 * nodes and logs 17 audit entries; partition B is the
 * solo node with 11 entries.  Byzantine winner: A. */
static const cos_v235_split_t SPLIT_FX = {
    /* partition_a_nodes   */ 3,
    /* partition_b_nodes   */ 1,
    /* chain_len_a         */ 17,
    /* chain_len_b         */ 11,
    /* winner_partition    */ 0,      /* set in run() */
    /* loser_flagged_fork  */ false,  /* set in run() */
};

static int argmin_sigma(const float *v) {
    int best = 0;
    for (int i = 1; i < COS_V235_N_NODES; ++i) {
        if (v[i] < v[best]) best = i;
        /* tiebreak on lower node_id is implicit: we
         * only update on strict-less. */
    }
    return best;
}

static float mean_dispersion_after(const float *v) {
    int min_idx = argmin_sigma(v);
    float vmin  = v[min_idx];
    float acc   = 0.0f;
    for (int i = 0; i < COS_V235_N_NODES; ++i)
        acc += (v[i] >= vmin ? v[i] - vmin : vmin - v[i]);
    return acc / (float)COS_V235_N_NODES;
}

void cos_v235_init(cos_v235_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x235A0C1ULL;
}

void cos_v235_run(cos_v235_state_t *s) {
    uint64_t prev = 0x235F1E5ULL;

    s->n_migrations          = 0;
    s->sigma_locus_mean_unity = 0.0f;

    for (int t = 0; t < COS_V235_N_TOPICS; ++t) {
        cos_v235_topic_t *tp = &s->topics[t];
        memset(tp, 0, sizeof(*tp));
        tp->topic_id = t;
        cpy_name(tp->topic_name, sizeof(tp->topic_name), FIX[t].name);

        for (int i = 0; i < COS_V235_N_NODES; ++i) {
            tp->sigma_before[i] = FIX[t].sigma_before[i];
            tp->sigma_after [i] = FIX[t].sigma_after [i];
        }

        tp->locus_before = argmin_sigma(tp->sigma_before);
        tp->locus_after  = argmin_sigma(tp->sigma_after);
        tp->migrated     = (tp->locus_before != tp->locus_after);
        if (tp->migrated) s->n_migrations++;

        tp->sigma_min_after = tp->sigma_after[tp->locus_after];

        float disp = mean_dispersion_after(tp->sigma_after);
        if (disp < 0.0f) disp = 0.0f;
        if (disp > 1.0f) disp = 1.0f;
        tp->sigma_locus_unity = 1.0f - disp;
        s->sigma_locus_mean_unity += tp->sigma_locus_unity;

        struct { int tid, lb, la, mig;
                 float sb[COS_V235_N_NODES], sa[COS_V235_N_NODES];
                 float sm, su; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.tid = tp->topic_id;
        rec.lb  = tp->locus_before;
        rec.la  = tp->locus_after;
        rec.mig = tp->migrated ? 1 : 0;
        for (int i = 0; i < COS_V235_N_NODES; ++i) {
            rec.sb[i] = tp->sigma_before[i];
            rec.sa[i] = tp->sigma_after [i];
        }
        rec.sm   = tp->sigma_min_after;
        rec.su   = tp->sigma_locus_unity;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    s->sigma_locus_mean_unity /= (float)COS_V235_N_TOPICS;

    /* Split-brain resolution: longer audit chain wins,
     * loser is flagged fork by construction. */
    s->split_brain = SPLIT_FX;
    s->split_brain.winner_partition =
        (SPLIT_FX.chain_len_a >= SPLIT_FX.chain_len_b) ? 0 : 1;
    s->split_brain.loser_flagged_fork = true;

    struct { int pa, pb, ca, cb, win, fork;
             int nmig; float mean_unity; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.pa = s->split_brain.partition_a_nodes;
    trec.pb = s->split_brain.partition_b_nodes;
    trec.ca = s->split_brain.chain_len_a;
    trec.cb = s->split_brain.chain_len_b;
    trec.win = s->split_brain.winner_partition;
    trec.fork = s->split_brain.loser_flagged_fork ? 1 : 0;
    trec.nmig = s->n_migrations;
    trec.mean_unity = s->sigma_locus_mean_unity;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v235_to_json(const cos_v235_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v235\","
        "\"n_nodes\":%d,\"n_topics\":%d,"
        "\"n_migrations\":%d,"
        "\"sigma_locus_mean_unity\":%.4f,"
        "\"split_brain\":{\"partition_a_nodes\":%d,"
                         "\"partition_b_nodes\":%d,"
                         "\"chain_len_a\":%d,\"chain_len_b\":%d,"
                         "\"winner_partition\":%d,"
                         "\"loser_flagged_fork\":%s},"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"topics\":[",
        COS_V235_N_NODES, COS_V235_N_TOPICS,
        s->n_migrations, s->sigma_locus_mean_unity,
        s->split_brain.partition_a_nodes,
        s->split_brain.partition_b_nodes,
        s->split_brain.chain_len_a,
        s->split_brain.chain_len_b,
        s->split_brain.winner_partition,
        s->split_brain.loser_flagged_fork ? "true" : "false",
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int t = 0; t < COS_V235_N_TOPICS; ++t) {
        const cos_v235_topic_t *tp = &s->topics[t];
        int q = snprintf(buf + off, cap - off,
            "%s{\"topic_id\":%d,\"name\":\"%s\","
            "\"locus_before\":%d,\"locus_after\":%d,\"migrated\":%s,"
            "\"sigma_min_after\":%.4f,\"sigma_locus_unity\":%.4f,"
            "\"sigma_before\":[%.4f,%.4f,%.4f,%.4f],"
            "\"sigma_after\":[%.4f,%.4f,%.4f,%.4f]}",
            t == 0 ? "" : ",",
            tp->topic_id, tp->topic_name,
            tp->locus_before, tp->locus_after,
            tp->migrated ? "true" : "false",
            tp->sigma_min_after, tp->sigma_locus_unity,
            tp->sigma_before[0], tp->sigma_before[1],
            tp->sigma_before[2], tp->sigma_before[3],
            tp->sigma_after[0],  tp->sigma_after[1],
            tp->sigma_after[2],  tp->sigma_after[3]);
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v235_self_test(void) {
    cos_v235_state_t s;
    cos_v235_init(&s, 0x235A0C1ULL);
    cos_v235_run(&s);
    if (!s.chain_valid) return 1;

    for (int t = 0; t < COS_V235_N_TOPICS; ++t) {
        const cos_v235_topic_t *tp = &s.topics[t];
        /* σ ranges */
        for (int i = 0; i < COS_V235_N_NODES; ++i) {
            if (tp->sigma_before[i] < 0.0f || tp->sigma_before[i] > 1.0f) return 2;
            if (tp->sigma_after [i] < 0.0f || tp->sigma_after [i] > 1.0f) return 2;
        }
        /* argmin contract */
        if (tp->locus_before != argmin_sigma(tp->sigma_before)) return 3;
        if (tp->locus_after  != argmin_sigma(tp->sigma_after))  return 4;
        /* unity range */
        if (tp->sigma_locus_unity < 0.0f || tp->sigma_locus_unity > 1.0f) return 5;
        /* min σ matches argmin */
        if (tp->sigma_min_after != tp->sigma_after[tp->locus_after]) return 6;
        /* no negative σ min */
        if (tp->sigma_min_after < 0.0f) return 6;
    }
    if (s.n_migrations < 1) return 7;

    const cos_v235_split_t *sb = &s.split_brain;
    if (sb->chain_len_a <= 0 || sb->chain_len_b <= 0) return 8;
    int expected_winner = (sb->chain_len_a >= sb->chain_len_b) ? 0 : 1;
    if (sb->winner_partition != expected_winner) return 9;
    if (!sb->loser_flagged_fork) return 10;

    if (s.sigma_locus_mean_unity < 0.0f || s.sigma_locus_mean_unity > 1.0f) return 11;

    /* Determinism. */
    cos_v235_state_t t;
    cos_v235_init(&t, 0x235A0C1ULL);
    cos_v235_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 12;
    return 0;
}
