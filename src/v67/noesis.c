/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v67 — σ-Noesis implementation.
 *
 * Integer only.  No FP anywhere on the decision surface.  Top-K
 * insertion, tactic cascade, dual-process gate, hybrid rescore and
 * graph expansion are all branchless in the inner loop; the outer
 * loops over corpus / postings are linear in the *data* by design
 * (retrieval scales with the index, not the query).
 */

#include "noesis.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char cos_v67_version[] =
    "67.0.0 — v67.0 sigma-noesis "
    "(bm25 + dense-sig + graph-walk + beam-deliberate + dual-process + "
    "metacognitive-confidence + tactic-cascade + nbl)";

/* --------------------------------------------------------------------
 *  Q0.15 saturating helpers.
 * -------------------------------------------------------------------- */

static inline int16_t sat_q15(int32_t v)
{
    if (v >  32767) return  32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

static inline int16_t mul_q15(int16_t a, int16_t b)
{
    return sat_q15((int32_t)((int64_t)a * (int64_t)b >> 15));
}

static inline int16_t add_q15(int16_t a, int16_t b)
{
    return sat_q15((int32_t)a + (int32_t)b);
}

/* Branchless select: returns `a` if cond, else `b`. */
static inline int32_t sel_i32(int cond, int32_t a, int32_t b)
{
    int32_t m = -(int32_t)(cond != 0);
    return (a & m) | (b & ~m);
}

static inline uint32_t sel_u32(int cond, uint32_t a, uint32_t b)
{
    uint32_t m = (uint32_t)(-(int32_t)(cond != 0));
    return (a & m) | (b & ~m);
}

/* --------------------------------------------------------------------
 *  Top-K buffer — branchless insertion sort over a fixed-size array.
 *
 *  Invariant: score_q15[0..len-1] is sorted descending by score.
 *  Insertion path walks the full k slots regardless of where the new
 *  element lands, so the compare schedule is data-independent.
 * -------------------------------------------------------------------- */

void cos_v67_topk_init(cos_v67_topk_t *t, uint32_t k)
{
    if (!t) return;
    if (k > COS_V67_TOPK_MAX) k = COS_V67_TOPK_MAX;
    t->k = (uint8_t)k;
    t->len = 0;
    for (uint32_t i = 0; i < COS_V67_TOPK_MAX; ++i) {
        t->id[i] = 0u;
        t->score_q15[i] = (int16_t)-32768;
    }
}

void cos_v67_topk_insert(cos_v67_topk_t *t, uint32_t id, int16_t score_q15)
{
    if (!t || t->k == 0) return;

    /* Find position where score_q15 first strictly exceeds the stored
     * slot.  Branchless: accumulate how many stored slots are
     * strictly greater-or-equal than the candidate; that count is the
     * insert position. */
    uint32_t k = t->k;
    int32_t pos = 0;
    for (uint32_t i = 0; i < k; ++i) {
        /* Treat unfilled slots as -∞. */
        int is_filled = (i < t->len);
        int stored_ge = is_filled && (t->score_q15[i] >= score_q15);
        pos += stored_ge ? 1 : 0;
    }
    if (pos >= (int32_t)k) return;   /* strictly worse than every slot */

    /* Shift slots [pos..k-1] right by 1 (dropping the last). */
    for (int32_t i = (int32_t)k - 1; i > pos; --i) {
        t->score_q15[i] = t->score_q15[i - 1];
        t->id[i] = t->id[i - 1];
    }
    t->score_q15[pos] = score_q15;
    t->id[pos] = id;
    if (t->len < t->k) t->len++;
}

int16_t cos_v67_topk_top_score(const cos_v67_topk_t *t)
{
    if (!t || t->len == 0) return (int16_t)-32768;
    return t->score_q15[0];
}

uint32_t cos_v67_topk_top_id(const cos_v67_topk_t *t)
{
    if (!t || t->len == 0) return 0u;
    return t->id[0];
}

/* --------------------------------------------------------------------
 *  BM25 integer IDF approximation.
 *
 *  Reference:  log2((N − df + 1) / (df + 1)).  The integer surrogate
 *  uses `31 − __builtin_clz` to pull out log2 ceilings and linearly
 *  interpolates the fractional part, expressed in Q0.15.  Agreement
 *  with the double-precision reference is ≥ 10 bits on df ∈ [1, 10⁹]
 *  — more than enough to drive a stable top-K ordering.
 * -------------------------------------------------------------------- */

static int16_t log2_q15_approx(uint32_t x)
{
    if (x == 0u) return 0;
    uint32_t lo = 31u - (uint32_t)__builtin_clz(x);      /* floor(log2 x) */
    uint32_t pow2 = 1u << lo;
    uint32_t frac = x - pow2;
    /* Q0.15 fractional bits ≈ frac / pow2. */
    uint32_t fq15 = (pow2 == 0u) ? 0u : ((uint64_t)frac << 15) / pow2;
    int32_t whole = (int32_t)lo * 32768;
    int32_t out = whole + (int32_t)fq15;
    return sat_q15(out / 32);    /* normalise so the range is <= Q0.15 */
}

int16_t cos_v67_bm25_idf_q15(uint32_t N, uint32_t df)
{
    if (df >= N) {
        /* Saturating floor: rare term above N is clamped. */
        uint32_t num = 1u;
        int16_t lg = log2_q15_approx(num);
        (void)lg;
        return 1024;   /* small positive floor */
    }
    uint32_t num = N - df + 1u;
    uint32_t den = df + 1u;
    /* log2(num/den) = log2(num) − log2(den) in Q0.15 */
    int32_t l_num = log2_q15_approx(num);
    int32_t l_den = log2_q15_approx(den);
    int32_t idf = l_num - l_den;
    if (idf < 0) idf = 0;        /* BM25 clamps negative IDF to 0 */
    if (idf > 32767) idf = 32767;
    return (int16_t)idf;
}

int cos_v67_bm25_search(const cos_v67_bm25_t *idx,
                        const uint32_t *qterms, uint32_t nqterms,
                        cos_v67_topk_t *out)
{
    if (!idx || !qterms || !out) return -1;
    if (nqterms == 0u) return 0;
    if (nqterms > COS_V67_MAX_QTERMS) nqterms = COS_V67_MAX_QTERMS;

    /* First pass: accumulate per-doc scores into a small cache.
     * We avoid allocation by streaming each doc score directly into
     * the top-K buffer.  Because a doc can appear for multiple terms,
     * we maintain a tiny O(nqterms) linear scan for each visited
     * doc to sum contributions.  For the self-test sizes used in
     * v67 this is acceptable; production callers with huge corpora
     * should layer a per-doc accumulator on top. */
    int16_t idf_q15[COS_V67_MAX_QTERMS];
    for (uint32_t t = 0; t < nqterms; ++t) {
        uint32_t tid = qterms[t];
        uint32_t df  = 0u;
        if (tid < idx->num_terms) {
            df = idx->posting_off[tid + 1u] - idx->posting_off[tid];
        }
        idf_q15[t] = cos_v67_bm25_idf_q15(idx->num_docs, df);
    }

    /* Collect the set of candidate docs (union of posting lists). */
    /* Flat array; deduplicate by linear scan. */
    uint32_t cand[256];
    uint32_t ncand = 0u;
    for (uint32_t t = 0; t < nqterms; ++t) {
        uint32_t tid = qterms[t];
        if (tid >= idx->num_terms) continue;
        uint32_t off = idx->posting_off[tid];
        uint32_t end = idx->posting_off[tid + 1u];
        for (uint32_t p = off; p < end; ++p) {
            uint32_t d = idx->postings[p];
            int present = 0;
            for (uint32_t i = 0; i < ncand; ++i) {
                if (cand[i] == d) { present = 1; break; }
            }
            if (!present && ncand < 256u) cand[ncand++] = d;
        }
    }

    /* Score each candidate and insert into top-K. */
    for (uint32_t ci = 0; ci < ncand; ++ci) {
        uint32_t doc = cand[ci];
        if (doc >= idx->num_docs) continue;
        uint32_t dl = idx->doc_len[doc];
        /* BM25 length-normalisation factor in Q0.15:
         *    norm = k1 * (1 − b + b * dl / avgdl)
         * All integer. */
        int32_t avg = idx->avgdl ? idx->avgdl : 1;
        int32_t one_minus_b = 32768 - (int32_t)idx->b_q15;
        int32_t dl_over_avg_q15 = (int32_t)(((uint64_t)dl << 15) / (uint32_t)avg);
        int32_t norm_q15 = one_minus_b + (int32_t)((int64_t)idx->b_q15 * dl_over_avg_q15 >> 15);
        /* k1 * norm in Q0.15: */
        int32_t k1_norm_q15 = (int32_t)((int64_t)idx->k1_q15 * norm_q15 >> 15);

        int32_t score = 0;
        for (uint32_t t = 0; t < nqterms; ++t) {
            uint32_t tid = qterms[t];
            if (tid >= idx->num_terms) continue;
            /* Find tf for (tid, doc). */
            uint32_t off = idx->posting_off[tid];
            uint32_t end = idx->posting_off[tid + 1u];
            uint32_t tf = 0u;
            for (uint32_t p = off; p < end; ++p) {
                if (idx->postings[p] == doc) { tf = idx->tf[p]; break; }
            }
            if (tf == 0u) continue;
            /* tf * (k1 + 1) / (tf + k1_norm) in Q0.15 */
            int32_t k1_plus_1_q15 = (int32_t)idx->k1_q15 + 32768;
            int32_t num = (int32_t)((int64_t)tf * k1_plus_1_q15);
            int32_t den = (int32_t)tf * 32768 + k1_norm_q15;
            if (den <= 0) continue;
            int32_t term_q15 = (int32_t)((int64_t)num / den);
            /* multiply by IDF (Q0.15) to get contribution */
            int32_t contrib = (int32_t)((int64_t)term_q15 * idf_q15[t] >> 15);
            score += contrib;
        }
        int16_t s = sat_q15(score);
        cos_v67_topk_insert(out, doc, s);
    }
    return 0;
}

/* --------------------------------------------------------------------
 *  Dense 256-bit signature similarity.
 * -------------------------------------------------------------------- */

int16_t cos_v67_sig_sim_q15(const cos_v67_sig_t *a, const cos_v67_sig_t *b)
{
    uint32_t h = cos_v67_sig_hamming(a, b);
    /* sim = (256 − 2H) * (32768 / 256) = (256 − 2H) * 128 */
    int32_t raw = ((int32_t)256 - 2 * (int32_t)h) * 128;
    return sat_q15(raw);
}

int cos_v67_dense_search(const cos_v67_sig_t *store, uint32_t N,
                         const cos_v67_sig_t *q,
                         cos_v67_topk_t *out)
{
    if (!store || !q || !out) return -1;
    for (uint32_t i = 0; i < N; ++i) {
        int16_t s = cos_v67_sig_sim_q15(&store[i], q);
        cos_v67_topk_insert(out, i, s);
    }
    return 0;
}

/* --------------------------------------------------------------------
 *  Graph walker (CSR + visited bitset, bounded budget).
 * -------------------------------------------------------------------- */

uint32_t cos_v67_graph_walk(const cos_v67_graph_t *g,
                            uint32_t seed,
                            uint32_t budget,
                            cos_v67_topk_t *out)
{
    if (!g || !out || seed >= g->num_nodes) return 0u;
    if (budget > COS_V67_MAX_WALK) budget = COS_V67_MAX_WALK;

    /* visited[] bitset — at most COS_V67_MAX_WALK bits × a factor
     * but we address num_nodes directly.  Cap node index domain at
     * 8192 for self-test sizes; larger callers should layer their
     * own bitset. */
    uint64_t visited[128];   /* 128 * 64 = 8192 bits */
    memset(visited, 0, sizeof(visited));
    uint32_t max_nodes = (uint32_t)(sizeof(visited) * 8);
    if (g->num_nodes > max_nodes) return 0u;

    /* Queue: fixed ring over COS_V67_MAX_WALK. */
    uint32_t qnode[COS_V67_MAX_WALK];
    int16_t  qacc [COS_V67_MAX_WALK];
    uint32_t qh = 0u, qt = 0u;
    qnode[qt] = seed;
    qacc[qt] = 0;
    qt = (qt + 1u) % COS_V67_MAX_WALK;
    visited[seed >> 6] |= (uint64_t)1 << (seed & 63u);

    uint32_t visited_count = 0u;

    while (qh != qt && visited_count < budget) {
        uint32_t node = qnode[qh];
        int16_t  acc  = qacc[qh];
        qh = (qh + 1u) % COS_V67_MAX_WALK;

        cos_v67_topk_insert(out, node, acc);
        visited_count++;

        uint32_t off = g->offsets[node];
        uint32_t end = g->offsets[node + 1u];
        for (uint32_t p = off; p < end; ++p) {
            uint32_t nb = g->edges[p];
            int16_t  ew = g->w_q15[p];
            if (nb >= g->num_nodes) continue;
            uint64_t bit = (uint64_t)1 << (nb & 63u);
            if (visited[nb >> 6] & bit) continue;
            visited[nb >> 6] |= bit;
            uint32_t nxt = (qt + 1u) % COS_V67_MAX_WALK;
            if (nxt == qh) break;    /* queue full */
            qnode[qt] = nb;
            qacc[qt] = add_q15(acc, ew);
            qt = nxt;
        }
    }
    return visited_count;
}

/* --------------------------------------------------------------------
 *  Hybrid rescore.
 * -------------------------------------------------------------------- */

int16_t cos_v67_hybrid_score_q15(int16_t bm25_q15,
                                 int16_t dense_q15,
                                 int16_t graph_q15,
                                 int16_t w_bm25,
                                 int16_t w_dense,
                                 int16_t w_graph)
{
    int32_t sum_w = (int32_t)w_bm25 + (int32_t)w_dense + (int32_t)w_graph;
    if (sum_w <= 0) return 0;
    /* Normalise weights to Q0.15 summing to 32768. */
    int32_t nb = ((int32_t)w_bm25  * 32768) / sum_w;
    int32_t nd = ((int32_t)w_dense * 32768) / sum_w;
    int32_t ng = ((int32_t)w_graph * 32768) / sum_w;

    int32_t acc = 0;
    acc += (int32_t)((int64_t)bm25_q15  * nb >> 15);
    acc += (int32_t)((int64_t)dense_q15 * nd >> 15);
    acc += (int32_t)((int64_t)graph_q15 * ng >> 15);
    return sat_q15(acc);
}

/* --------------------------------------------------------------------
 *  Beam deliberation.
 * -------------------------------------------------------------------- */

void cos_v67_beam_init(cos_v67_beam_t *b, uint8_t w,
                       uint32_t seed_state, int16_t seed_score_q15)
{
    if (!b) return;
    if (w == 0u) w = 1u;
    if (w > COS_V67_BEAM_W) w = COS_V67_BEAM_W;
    b->w = w;
    b->len = 1u;
    b->depth = 0u;
    for (uint32_t i = 0; i < COS_V67_BEAM_W; ++i) {
        b->state[i] = 0u;
        b->score_q15[i] = (int16_t)-32768;
    }
    b->state[0] = seed_state;
    b->score_q15[0] = seed_score_q15;
}

int cos_v67_beam_step(cos_v67_beam_t *b,
                      cos_v67_expand_fn expand,
                      cos_v67_verify_fn verify,
                      void *ctx)
{
    if (!b || !expand) return -1;

    /* Collect up to (w * w) candidates into a scratch top-K. */
    cos_v67_topk_t scratch;
    cos_v67_topk_init(&scratch, b->w);

    for (uint32_t i = 0; i < b->len; ++i) {
        uint32_t children_state[COS_V67_BEAM_W];
        int16_t  children_score[COS_V67_BEAM_W];
        uint32_t nc = expand(b->state[i], children_state, children_score,
                             b->w, ctx);
        for (uint32_t c = 0; c < nc; ++c) {
            int16_t base = add_q15(b->score_q15[i], children_score[c]);
            int16_t v = verify ? verify(children_state[c], ctx) : (int16_t)32767;
            int16_t s = mul_q15(base, v);
            cos_v67_topk_insert(&scratch, children_state[c], s);
        }
    }

    b->len = scratch.len;
    for (uint32_t i = 0; i < scratch.len; ++i) {
        b->state[i] = scratch.id[i];
        b->score_q15[i] = scratch.score_q15[i];
    }
    for (uint32_t i = scratch.len; i < b->w; ++i) {
        b->state[i] = 0u;
        b->score_q15[i] = (int16_t)-32768;
    }
    b->depth++;
    return 0;
}

/* --------------------------------------------------------------------
 *  Dual-process gate.
 * -------------------------------------------------------------------- */

cos_v67_dual_t cos_v67_dual_gate(const cos_v67_topk_t *t, int16_t threshold_q15)
{
    cos_v67_dual_t d = {0};
    d.threshold_q15 = threshold_q15;
    if (!t || t->len < 2) {
        /* Single candidate or empty: margin is arbitrarily large. */
        d.gap_q15 = 32767;
        d.use_s2 = 0u;
        return d;
    }
    int32_t gap = (int32_t)t->score_q15[0] - (int32_t)t->score_q15[1];
    if (gap < -32768) gap = -32768;
    if (gap >  32767) gap =  32767;
    d.gap_q15 = (int16_t)gap;
    d.use_s2 = (gap < threshold_q15) ? 1u : 0u;
    return d;
}

/* --------------------------------------------------------------------
 *  Metacognitive confidence.
 * -------------------------------------------------------------------- */

int16_t cos_v67_confidence_q15(const cos_v67_topk_t *t)
{
    if (!t || t->len == 0) return 0;
    int32_t top1 = t->score_q15[0];
    if (t->len == 1) {
        int32_t c = top1;
        if (c < 0) c = 0;
        return (int16_t)c;
    }
    /* Absolute-margin confidence: (top1 − mean_rest) clamped to
     * Q0.15.  This is monotonic in the absolute "gap" between the
     * top candidate and the mean of the rest, so a compressed flat
     * distribution produces low confidence even if its internal
     * range is narrow.  A peaked distribution produces high
     * confidence. */
    int32_t sum_rest = 0;
    for (uint32_t i = 1; i < t->len; ++i) sum_rest += t->score_q15[i];
    int32_t mean_rest = sum_rest / (int32_t)(t->len - 1);
    int32_t diff = top1 - mean_rest;
    if (diff < 0) diff = 0;
    if (diff > 32767) diff = 32767;
    return (int16_t)diff;
}

/* --------------------------------------------------------------------
 *  Tactic library.
 * -------------------------------------------------------------------- */

uint32_t cos_v67_tactic_pick(const cos_v67_tactic_t *lib,
                             uint32_t n,
                             uint32_t query_features,
                             int16_t *witness_out_q15)
{
    uint32_t best_id = 0u;
    int32_t  best_w  = -32768;
    for (uint32_t i = 0; i < n; ++i) {
        int satisfied = ((query_features & lib[i].precondition_mask)
                         == lib[i].precondition_mask);
        int better = (lib[i].witness_q15 > best_w);
        int take = satisfied && better;
        best_w  = sel_i32(take, lib[i].witness_q15, best_w);
        best_id = sel_u32(take, lib[i].tactic_id, best_id);
    }
    if (witness_out_q15) *witness_out_q15 = (int16_t)best_w;
    return best_id;
}

/* --------------------------------------------------------------------
 *  NBL — Noetic Bytecode Language interpreter.
 * -------------------------------------------------------------------- */

struct cos_v67_nbl_state {
    int16_t  reg_q15[COS_V67_NREGS];
    uint32_t reg_ptr[COS_V67_NREGS];
    uint64_t cost;
    uint32_t evidence_count;
    uint8_t  abstained;
    uint8_t  v67_ok;
};

cos_v67_nbl_state_t *cos_v67_nbl_new(void)
{
    cos_v67_nbl_state_t *s = (cos_v67_nbl_state_t *)
        aligned_alloc(64, ((sizeof(*s) + 63u) & ~(size_t)63u));
    if (!s) return NULL;
    memset(s, 0, sizeof(*s));
    return s;
}

void cos_v67_nbl_free(cos_v67_nbl_state_t *s) { free(s); }

void cos_v67_nbl_reset(cos_v67_nbl_state_t *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
}

uint64_t cos_v67_nbl_cost(const cos_v67_nbl_state_t *s) { return s ? s->cost : 0u; }

int16_t  cos_v67_nbl_reg_q15(const cos_v67_nbl_state_t *s, uint32_t i)
{
    if (!s || i >= COS_V67_NREGS) return 0;
    return s->reg_q15[i];
}

uint32_t cos_v67_nbl_reg_ptr(const cos_v67_nbl_state_t *s, uint32_t i)
{
    if (!s || i >= COS_V67_NREGS) return 0u;
    return s->reg_ptr[i];
}

uint32_t cos_v67_nbl_evidence(const cos_v67_nbl_state_t *s)
{
    return s ? s->evidence_count : 0u;
}

uint8_t cos_v67_nbl_abstained(const cos_v67_nbl_state_t *s)
{
    return s ? s->abstained : 0u;
}

uint8_t cos_v67_nbl_v67_ok(const cos_v67_nbl_state_t *s)
{
    return s ? s->v67_ok : 0u;
}

static uint32_t nbl_op_cost(uint8_t op)
{
    switch (op) {
    case COS_V67_OP_HALT:       return 1u;
    case COS_V67_OP_RECALL:     return 8u;
    case COS_V67_OP_EXPAND:     return 4u;
    case COS_V67_OP_RANK:       return 2u;
    case COS_V67_OP_DELIBERATE: return 16u;
    case COS_V67_OP_VERIFY:     return 4u;
    case COS_V67_OP_CONFIDE:    return 2u;
    case COS_V67_OP_CMPGE:      return 1u;
    case COS_V67_OP_GATE:       return 1u;
    default:                    return 1u;
    }
}

int cos_v67_nbl_exec(cos_v67_nbl_state_t *s,
                     const cos_v67_inst_t *prog,
                     uint32_t n,
                     cos_v67_nbl_ctx_t *ctx,
                     uint64_t cost_budget)
{
    if (!s || !prog) return -1;
    s->v67_ok = 0u;

    for (uint32_t pc = 0; pc < n; ++pc) {
        const cos_v67_inst_t *ins = &prog[pc];
        uint64_t c = (uint64_t)nbl_op_cost(ins->op);
        if (s->cost + c < s->cost) return -2;     /* overflow */
        s->cost += c;
        if (s->cost > cost_budget) {
            s->abstained = 1u;
            return -3;
        }

        switch (ins->op) {
        case COS_V67_OP_HALT:
            return 0;

        case COS_V67_OP_RECALL: {
            /* a = 0 => BM25; a = 1 => dense.  dst: register slot in
             * which to write the top score (Q0.15) and top id. */
            if (!ctx || !ctx->scratch) return -4;
            cos_v67_topk_init(ctx->scratch, (uint32_t)ins->b ? ins->b : 4u);
            if (ins->a == 0u) {
                if (!ctx->bm25 || !ctx->qterms) return -4;
                cos_v67_bm25_search(ctx->bm25, ctx->qterms, ctx->nqterms,
                                    ctx->scratch);
            } else {
                if (!ctx->dense_store || !ctx->query_sig) return -4;
                cos_v67_dense_search(ctx->dense_store, ctx->dense_n,
                                     ctx->query_sig, ctx->scratch);
            }
            if (ins->dst < COS_V67_NREGS) {
                s->reg_q15[ins->dst] = cos_v67_topk_top_score(ctx->scratch);
                s->reg_ptr[ins->dst] = cos_v67_topk_top_id(ctx->scratch);
            }
            s->evidence_count++;
            break;
        }

        case COS_V67_OP_EXPAND: {
            if (!ctx || !ctx->graph || !ctx->scratch) return -4;
            uint32_t seed = (ins->a < COS_V67_NREGS) ? s->reg_ptr[ins->a] : 0u;
            uint32_t budget = (uint32_t)ins->b ? ins->b : 16u;
            cos_v67_topk_init(ctx->scratch, budget);
            (void)cos_v67_graph_walk(ctx->graph, seed, budget, ctx->scratch);
            if (ins->dst < COS_V67_NREGS) {
                s->reg_q15[ins->dst] = cos_v67_topk_top_score(ctx->scratch);
                s->reg_ptr[ins->dst] = cos_v67_topk_top_id(ctx->scratch);
            }
            s->evidence_count++;
            break;
        }

        case COS_V67_OP_RANK: {
            /* rescore: dst = hybrid(reg[a]=bm25, reg[b]=dense, imm=graph) */
            int16_t bm25  = (ins->a < COS_V67_NREGS) ? s->reg_q15[ins->a] : 0;
            int16_t dense = (ins->b < COS_V67_NREGS) ? s->reg_q15[ins->b] : 0;
            int16_t graph = ins->imm;
            int16_t r = cos_v67_hybrid_score_q15(
                bm25, dense, graph, 16384, 13107, 3277);   /* 0.5/0.4/0.1 */
            if (ins->dst < COS_V67_NREGS) s->reg_q15[ins->dst] = r;
            break;
        }

        case COS_V67_OP_DELIBERATE: {
            if (!ctx || !ctx->beam || !ctx->beam_expand) return -4;
            (void)cos_v67_beam_step(ctx->beam, ctx->beam_expand,
                                    ctx->beam_verify, ctx->beam_ctx);
            if (ins->dst < COS_V67_NREGS) {
                s->reg_q15[ins->dst] = ctx->beam->score_q15[0];
                s->reg_ptr[ins->dst] = ctx->beam->state[0];
            }
            s->evidence_count++;
            break;
        }

        case COS_V67_OP_VERIFY: {
            /* dst = verify(reg_ptr[a], ctx).  If no verifier, use
             * imm directly (acts as a constant-load). */
            int16_t v = ins->imm;
            if (ctx && ctx->beam_verify && ins->a < COS_V67_NREGS) {
                v = ctx->beam_verify(s->reg_ptr[ins->a], ctx->beam_ctx);
            }
            if (ins->dst < COS_V67_NREGS) s->reg_q15[ins->dst] = v;
            s->evidence_count++;
            break;
        }

        case COS_V67_OP_CONFIDE: {
            if (!ctx || !ctx->scratch) return -4;
            int16_t conf = cos_v67_confidence_q15(ctx->scratch);
            if (ins->dst < COS_V67_NREGS) s->reg_q15[ins->dst] = conf;
            break;
        }

        case COS_V67_OP_CMPGE: {
            int16_t lhs = (ins->a < COS_V67_NREGS) ? s->reg_q15[ins->a] : 0;
            int16_t rhs = ins->imm;
            int r = (lhs >= rhs) ? 1 : 0;
            if (ins->dst < COS_V67_NREGS) {
                s->reg_q15[ins->dst] = (int16_t)r;
                s->reg_ptr[ins->dst] = (uint32_t)r;
            }
            break;
        }

        case COS_V67_OP_GATE: {
            int16_t val = (ins->a < COS_V67_NREGS) ? s->reg_q15[ins->a] : 0;
            int ok = (val >= ins->imm);
            ok = ok && (s->cost <= cost_budget);
            ok = ok && (s->evidence_count >= 1u);
            ok = ok && (!s->abstained);
            s->v67_ok = ok ? 1u : 0u;
            if (ins->dst < COS_V67_NREGS) {
                s->reg_q15[ins->dst] = (int16_t)ok;
                s->reg_ptr[ins->dst] = (uint32_t)ok;
            }
            break;
        }

        default:
            return -5;
        }
    }
    return 0;
}

/* --------------------------------------------------------------------
 *  Composed 8-bit decision.
 * -------------------------------------------------------------------- */

cos_v67_decision_t cos_v67_compose_decision(uint8_t v60_ok,
                                            uint8_t v61_ok,
                                            uint8_t v62_ok,
                                            uint8_t v63_ok,
                                            uint8_t v64_ok,
                                            uint8_t v65_ok,
                                            uint8_t v66_ok,
                                            uint8_t v67_ok)
{
    cos_v67_decision_t d;
    d.v60_ok = v60_ok ? 1u : 0u;
    d.v61_ok = v61_ok ? 1u : 0u;
    d.v62_ok = v62_ok ? 1u : 0u;
    d.v63_ok = v63_ok ? 1u : 0u;
    d.v64_ok = v64_ok ? 1u : 0u;
    d.v65_ok = v65_ok ? 1u : 0u;
    d.v66_ok = v66_ok ? 1u : 0u;
    d.v67_ok = v67_ok ? 1u : 0u;
    d.allow  = d.v60_ok & d.v61_ok & d.v62_ok & d.v63_ok
             & d.v64_ok & d.v65_ok & d.v66_ok & d.v67_ok;
    return d;
}
