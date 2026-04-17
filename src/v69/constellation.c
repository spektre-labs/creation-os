/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v69 — σ-Constellation: distributed-orchestration +
 * parallel-decoding + multi-agent consensus kernel.  Pure integer C;
 * libc-only; no FP on any decision surface.  Branchless on the data;
 * not constant-time in the fleet size by design (that is the whole
 * point of scaling).
 *
 * 1 = 1.
 */

#include "constellation.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char cos_v69_version[] =
    "69.0.0 — v69.0 sigma-constellation "
    "(tree-speculative + debate + byzantine-vote + moe-route + gossip + "
    "flash-chunked-dot + elo-ucb + popcount-dedup + cl)";

/* --------------------------------------------------------------------
 *  Branchless helpers.
 * -------------------------------------------------------------------- */

static inline int32_t sat_q15_i32(int32_t x)
{
    if (x >  32767) x =  32767;
    if (x < -32768) x = -32768;
    return x;
}

static inline int32_t sel_i32(int32_t cond, int32_t a, int32_t b)
{
    /* cond is 0 or -1 (all bits set). Returns a if cond, else b. */
    return (a & cond) | (b & ~cond);
}

static inline uint32_t sel_u32(int32_t cond, uint32_t a, uint32_t b)
{
    return (uint32_t)((((int32_t)a) & cond) | (((int32_t)b) & ~cond));
}

static inline int32_t max_i32(int32_t a, int32_t b)
{
    int32_t cond = -(int32_t)(a > b);
    return sel_i32(cond, a, b);
}

static void *aligned_calloc64(size_t bytes)
{
    size_t rounded = (bytes + 63u) & ~((size_t)63);
    void *p = aligned_alloc(64, rounded);
    if (p) memset(p, 0, rounded);
    return p;
}

/* --------------------------------------------------------------------
 *  1. Tree speculative decoding.
 * -------------------------------------------------------------------- */

void cos_v69_tree_init(cos_v69_draft_tree_t *t)
{
    if (!t) return;
    memset(t, 0, sizeof(*t));
}

uint32_t cos_v69_tree_push(cos_v69_draft_tree_t *t,
                           uint32_t parent,
                           int32_t token,
                           int16_t accept_q15)
{
    if (!t) return UINT32_MAX;
    if (t->n >= COS_V69_TREE_MAX) return UINT32_MAX;
    uint32_t i = t->n;
    cos_v69_tree_node_t *nd = &t->nodes[i];
    nd->parent = (uint16_t)(parent >= COS_V69_TREE_MAX ? i : parent);
    nd->token  = token;
    nd->accept_q15 = accept_q15;
    if (i == 0 || parent >= i) {
        nd->depth = 0;
        t->root = i;
    } else {
        nd->depth = (uint16_t)(t->nodes[parent].depth + 1u);
        if (nd->depth > t->depth_max) t->depth_max = nd->depth;
    }
    t->n++;
    return i;
}

/* For each node, compute the longest accepted branch ending at it.
 * A branch is accepted iff, walking from root to that node, every
 * token matches target_tokens[depth] AND every accept_q15 >= min. */
uint32_t cos_v69_draft_tree_verify(const cos_v69_draft_tree_t *t,
                                   const int32_t *target_tokens,
                                   uint32_t target_len,
                                   int16_t min_accept_q15,
                                   uint32_t *accepted_path)
{
    if (!t || !target_tokens || t->n == 0) return 0;

    /* accepted[i] = length of accepted prefix ending at node i (0 if
     * node i doesn't match).  We rely on parent < self invariant so
     * a single forward pass suffices. */
    uint32_t accepted[COS_V69_TREE_MAX];
    memset(accepted, 0, sizeof(accepted));

    uint32_t best_i = 0;
    uint32_t best_len = 0;

    for (uint32_t i = 0; i < t->n; ++i) {
        const cos_v69_tree_node_t *nd = &t->nodes[i];
        if (nd->depth == 0xFFFF) continue;  /* pruned */
        uint32_t d = nd->depth;
        if (d >= target_len) continue;

        int32_t tok_ok  = -(int32_t)(nd->token == target_tokens[d]);
        int32_t acc_ok  = -(int32_t)(nd->accept_q15 >= min_accept_q15);
        int32_t node_ok = tok_ok & acc_ok;

        uint32_t parent_len;
        if (i == t->root || nd->parent == i) {
            /* root: accepted iff node_ok → length 1 */
            parent_len = 1u;
        } else {
            /* non-root: accepted iff parent was accepted AND node_ok →
             * length parent_len + 1; else 0 */
            parent_len = accepted[nd->parent];
            /* parent must be non-zero (i.e. parent accepted) */
            int32_t parent_ok = -(int32_t)(parent_len > 0);
            node_ok = node_ok & parent_ok;
            parent_len = parent_len + 1u;
        }
        accepted[i] = sel_u32(node_ok, parent_len, 0u);

        int32_t beat = -(int32_t)(accepted[i] > best_len);
        best_len = sel_u32(beat, accepted[i], best_len);
        best_i   = sel_u32(beat, i, best_i);
    }

    if (accepted_path && best_len > 0) {
        /* walk back from best_i to root, writing in reverse */
        uint32_t idx = best_i;
        for (int32_t pos = (int32_t)best_len - 1; pos >= 0; --pos) {
            accepted_path[pos] = idx;
            if (idx == t->root || t->nodes[idx].parent == idx) break;
            idx = t->nodes[idx].parent;
        }
    }
    return best_len;
}

uint32_t cos_v69_tree_prune(cos_v69_draft_tree_t *t, int16_t min_path_q15)
{
    if (!t) return 0;
    /* path_q15[i] = product of accept_q15 on the path root..i, in Q0.15. */
    int32_t path[COS_V69_TREE_MAX];
    uint32_t pruned = 0;
    for (uint32_t i = 0; i < t->n; ++i) {
        cos_v69_tree_node_t *nd = &t->nodes[i];
        if (nd->depth == 0xFFFF) { path[i] = 0; continue; }
        int32_t p;
        if (i == t->root || nd->parent == i) {
            p = nd->accept_q15;
        } else {
            int32_t parent_p = path[nd->parent];
            p = (parent_p * (int32_t)nd->accept_q15) >> 15;
        }
        path[i] = p;
        if (p < (int32_t)min_path_q15) {
            nd->depth = 0xFFFF;
            nd->accept_q15 = 0;
            pruned++;
        }
    }
    return pruned;
}

/* --------------------------------------------------------------------
 *  2. Multi-agent debate.
 *
 *  Aggregate by proposal id (linear bucketization).  Anti-conformity:
 *  subtract `anti_conformity_q15 * n_votes / n_total` from each score
 *  so a proposal that everyone agrees on without evidence is
 *  down-weighted.
 * -------------------------------------------------------------------- */

static void rank_insert(cos_v69_debate_rank_t *rk, uint32_t *len, uint32_t k,
                        int32_t proposal, int32_t score, uint16_t n_votes)
{
    if (*len < k) {
        rk[*len].proposal = proposal;
        rk[*len].score_q15 = score;
        rk[*len].n_votes   = n_votes;
        (*len)++;
        /* bubble descending */
        for (uint32_t i = *len; i > 1; --i) {
            int32_t cond = -(int32_t)(rk[i-1].score_q15 > rk[i-2].score_q15);
            cos_v69_debate_rank_t tmp = rk[i-1];
            rk[i-1] = (cond ? rk[i-2] : rk[i-1]);
            rk[i-2] = (cond ? tmp      : rk[i-2]);
        }
        return;
    }
    uint32_t m = *len - 1u;
    int32_t cond = -(int32_t)(score > rk[m].score_q15);
    cos_v69_debate_rank_t cand = { proposal, score, n_votes, 0 };
    rk[m] = (cond ? cand : rk[m]);
    for (uint32_t i = *len; i > 1; --i) {
        int32_t c = -(int32_t)(rk[i-1].score_q15 > rk[i-2].score_q15);
        cos_v69_debate_rank_t tmp = rk[i-1];
        rk[i-1] = (c ? rk[i-2] : rk[i-1]);
        rk[i-2] = (c ? tmp     : rk[i-2]);
    }
}

void cos_v69_debate_score(const cos_v69_agent_vote_t *votes,
                          uint32_t n,
                          uint32_t k,
                          int16_t min_margin_q15,
                          int16_t anti_conformity_q15,
                          cos_v69_debate_result_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (k == 0 || k > COS_V69_TOPK_MAX) k = COS_V69_TOPK_MAX;
    out->k = (uint8_t)k;
    if (!votes || n == 0) { out->abstained = 1; return; }

    /* Bucketize by proposal id via linear scan (simple, branchless in
     * the vote loop).  Worst-case O(n^2) — fine for n ≤ 64. */
    int32_t  props[COS_V69_AGENTS_MAX];
    int32_t  sums[COS_V69_AGENTS_MAX];
    uint16_t cnts[COS_V69_AGENTS_MAX];
    uint32_t nprops = 0;

    uint32_t cap = (n < COS_V69_AGENTS_MAX) ? n : COS_V69_AGENTS_MAX;

    for (uint32_t i = 0; i < cap; ++i) {
        int32_t p = votes[i].proposal;
        int32_t s = (int32_t)votes[i].confidence_q15;
        uint32_t slot = nprops;
        for (uint32_t j = 0; j < nprops; ++j) {
            int32_t hit = -(int32_t)(props[j] == p);
            slot = sel_u32(hit, j, slot);
        }
        if (slot == nprops) {
            props[nprops] = p;
            sums[nprops]  = 0;
            cnts[nprops]  = 0;
            nprops++;
        }
        sums[slot] += s;
        cnts[slot] += 1u;
    }

    /* Anti-conformity: penalty = anti_conformity_q15 * votes / n.
     * Equivalent to: everyone agrees → maximum penalty. */
    for (uint32_t j = 0; j < nprops; ++j) {
        int32_t penalty = ((int32_t)anti_conformity_q15 * (int32_t)cnts[j]) /
                          (int32_t)cap;
        sums[j] -= penalty;
    }

    uint32_t len = 0;
    for (uint32_t j = 0; j < nprops; ++j) {
        rank_insert(out->rank, &len, out->k, props[j], sums[j], cnts[j]);
    }
    out->len = (uint8_t)len;

    if (len < 2) {
        /* FREE-MAD safety: abstain on single-proposal or empty votes. */
        out->abstained = (uint8_t)(len == 0 ? 1u : 0u);
        out->margin_q15 = (int16_t)(len == 1 ? 32767 : 0);
        if (len == 1 && out->rank[0].score_q15 < (int32_t)min_margin_q15) {
            out->abstained = 1;
        }
        return;
    }
    int32_t margin = out->rank[0].score_q15 - out->rank[1].score_q15;
    if (margin > 32767) margin = 32767;
    if (margin < 0)     margin = 0;
    out->margin_q15 = (int16_t)margin;
    out->abstained  = (uint8_t)((out->margin_q15 < min_margin_q15) ? 1u : 0u);
}

/* --------------------------------------------------------------------
 *  3. Byzantine-safe vote.
 * -------------------------------------------------------------------- */

uint32_t cos_v69_byzantine_vote(const uint8_t *votes,
                                uint32_t n,
                                uint32_t f,
                                uint8_t  *allow_out)
{
    uint32_t count = 0;
    if (votes) {
        for (uint32_t i = 0; i < n; ++i)
            count += (votes[i] != 0 ? 1u : 0u);
    }
    uint32_t quorum = 2u * f + 1u;
    if (allow_out)
        *allow_out = (uint8_t)((count >= quorum) ? 1u : 0u);
    return count;
}

/* --------------------------------------------------------------------
 *  4. MoE top-K routing.
 * -------------------------------------------------------------------- */

void cos_v69_route_init(cos_v69_route_t *r, uint32_t k)
{
    if (!r) return;
    if (k > COS_V69_TOPK_MAX) k = COS_V69_TOPK_MAX;
    if (k == 0) k = 1;
    r->k = (uint8_t)k;
    r->len = 0;
    for (uint32_t i = 0; i < COS_V69_TOPK_MAX; ++i) {
        r->expert[i] = 0u;
        r->score_q15[i] = INT16_MIN;
    }
}

static void route_insert(cos_v69_route_t *r, uint32_t expert, int16_t score)
{
    if (r->len < r->k) {
        r->expert[r->len] = expert;
        r->score_q15[r->len] = score;
        r->len++;
        for (uint32_t i = r->len; i > 1; --i) {
            int32_t cond = -(int32_t)(r->score_q15[i-1] > r->score_q15[i-2]);
            int32_t a_e = (int32_t)r->expert[i-1];
            int32_t b_e = (int32_t)r->expert[i-2];
            int32_t a_s = (int32_t)r->score_q15[i-1];
            int32_t b_s = (int32_t)r->score_q15[i-2];
            r->expert[i-1]    = (uint32_t)sel_i32(cond, b_e, a_e);
            r->expert[i-2]    = (uint32_t)sel_i32(cond, a_e, b_e);
            r->score_q15[i-1] = (int16_t)sel_i32(cond, b_s, a_s);
            r->score_q15[i-2] = (int16_t)sel_i32(cond, a_s, b_s);
        }
        return;
    }
    uint32_t m = r->len - 1u;
    int32_t cond = -(int32_t)(score > r->score_q15[m]);
    r->expert[m]    = (uint32_t)sel_i32(cond, (int32_t)expert,
                                        (int32_t)r->expert[m]);
    r->score_q15[m] = (int16_t)sel_i32(cond, (int32_t)score,
                                       (int32_t)r->score_q15[m]);
    for (uint32_t i = r->len; i > 1; --i) {
        int32_t c = -(int32_t)(r->score_q15[i-1] > r->score_q15[i-2]);
        int32_t a_e = (int32_t)r->expert[i-1];
        int32_t b_e = (int32_t)r->expert[i-2];
        int32_t a_s = (int32_t)r->score_q15[i-1];
        int32_t b_s = (int32_t)r->score_q15[i-2];
        r->expert[i-1]    = (uint32_t)sel_i32(c, b_e, a_e);
        r->expert[i-2]    = (uint32_t)sel_i32(c, a_e, b_e);
        r->score_q15[i-1] = (int16_t)sel_i32(c, b_s, a_s);
        r->score_q15[i-2] = (int16_t)sel_i32(c, a_s, b_s);
    }
}

void cos_v69_route_topk(const int16_t *scores_q15,
                        uint32_t n,
                        cos_v69_route_t *out,
                        uint32_t *load_counter)
{
    if (!out) return;
    if (out->k == 0) cos_v69_route_init(out, 1);
    out->len = 0;
    for (uint32_t i = 0; i < COS_V69_TOPK_MAX; ++i) {
        out->expert[i] = 0u;
        out->score_q15[i] = INT16_MIN;
    }
    if (!scores_q15 || n == 0) return;
    uint32_t cap = (n < COS_V69_EXPERTS_MAX) ? n : COS_V69_EXPERTS_MAX;
    for (uint32_t i = 0; i < cap; ++i) {
        route_insert(out, i, scores_q15[i]);
    }
    if (load_counter) {
        for (uint32_t k = 0; k < out->len; ++k)
            load_counter[out->expert[k]]++;
    }
}

/* --------------------------------------------------------------------
 *  6. Gossip / vector clock.
 * -------------------------------------------------------------------- */

void cos_v69_clock_init(cos_v69_clock_t *c, uint32_t width)
{
    if (!c) return;
    if (width > COS_V69_CLOCK_MAX) width = COS_V69_CLOCK_MAX;
    if (width == 0) width = 1;
    c->width = width;
    for (uint32_t i = 0; i < COS_V69_CLOCK_MAX; ++i) c->c[i] = 0;
}

void cos_v69_clock_tick(cos_v69_clock_t *c, uint32_t node)
{
    if (!c || node >= c->width) return;
    c->c[node]++;
}

void cos_v69_clock_merge(cos_v69_clock_t *dst, const cos_v69_clock_t *src)
{
    if (!dst || !src) return;
    uint32_t w = dst->width < src->width ? dst->width : src->width;
    for (uint32_t i = 0; i < w; ++i) {
        uint32_t a = dst->c[i], b = src->c[i];
        /* branchless max */
        uint32_t m = a - (((a - b) & -(int32_t)(a < b)));
        dst->c[i] = m;
    }
}

uint8_t cos_v69_clock_happens_before(const cos_v69_clock_t *a,
                                     const cos_v69_clock_t *b)
{
    if (!a || !b) return 0;
    uint32_t w = a->width < b->width ? a->width : b->width;
    uint32_t any_less = 0;
    uint32_t all_le   = 1;
    for (uint32_t i = 0; i < w; ++i) {
        uint32_t ai = a->c[i], bi = b->c[i];
        all_le  &= (ai <= bi ? 1u : 0u);
        any_less |= (ai <  bi ? 1u : 0u);
    }
    return (uint8_t)((all_le & any_less) ? 1u : 0u);
}

/* --------------------------------------------------------------------
 *  7. Chunked flash-style attention score.
 * -------------------------------------------------------------------- */

int16_t cos_v69_chunked_dot(const int16_t *q,
                            const int16_t *k,
                            uint32_t n,
                            uint32_t chunk,
                            int16_t *out_max_q15)
{
    if (!q || !k || n == 0) {
        if (out_max_q15) *out_max_q15 = 0;
        return 0;
    }
    if (chunk == 0) chunk = 16;
    int32_t acc = 0;
    int32_t run_max = INT32_MIN;
    for (uint32_t base = 0; base < n; base += chunk) {
        uint32_t lim = base + chunk;
        if (lim > n) lim = n;
        int32_t local = 0;
        for (uint32_t i = base; i < lim; ++i) {
            int32_t p = ((int32_t)q[i] * (int32_t)k[i]) >> 15;
            local += p;
            run_max = max_i32(run_max, p);
        }
        acc += local;
    }
    if (out_max_q15) {
        *out_max_q15 = (int16_t)sat_q15_i32(run_max);
    }
    return (int16_t)sat_q15_i32(acc);
}

/* --------------------------------------------------------------------
 *  8. Self-play Elo + UCB arm selection.
 * -------------------------------------------------------------------- */

void cos_v69_bandit_init(cos_v69_bandit_t *b, uint32_t n)
{
    if (!b) return;
    if (n > COS_V69_ARMS_MAX) n = COS_V69_ARMS_MAX;
    if (n == 0) n = 1;
    b->n = n;
    b->total_pulls = 0;
    for (uint32_t i = 0; i < COS_V69_ARMS_MAX; ++i) {
        b->arms[i].elo_q15 = COS_V69_ELO_INIT_Q15;
        b->arms[i].pulls   = 0;
    }
}

void cos_v69_elo_update(cos_v69_bandit_t *b,
                        uint32_t arm,
                        uint8_t outcome,
                        int16_t k_q15)
{
    if (!b || arm >= b->n) return;
    if (k_q15 < 0) k_q15 = 0;
    int32_t out = (outcome ? 32767 : 0);
    int32_t cur = (int32_t)b->arms[arm].elo_q15;
    int32_t delta = (((int32_t)k_q15 * (out - cur)) >> 15);
    int32_t nv = cur + delta;
    b->arms[arm].elo_q15 = (int16_t)sat_q15_i32(nv);
    b->arms[arm].pulls++;
    b->total_pulls++;
}

uint32_t cos_v69_ucb_select(const cos_v69_bandit_t *b, int16_t c_q15)
{
    if (!b || b->n == 0) return 0;
    if (c_q15 < 0) c_q15 = 0;
    uint32_t best_i = 0;
    int32_t  best_s = INT32_MIN;
    uint64_t total  = (b->total_pulls == 0) ? 1u : b->total_pulls;
    for (uint32_t i = 0; i < b->n; ++i) {
        int32_t mean = (int32_t)b->arms[i].elo_q15;
        /* bonus = c_q15 * total / (1 + pulls), scaled Q0.15.
         * For small pulls, unexplored arms dominate. */
        int64_t denom = (int64_t)b->arms[i].pulls + 1;
        int64_t bonus = ((int64_t)c_q15 * (int64_t)total) / denom;
        if (bonus > 32767) bonus = 32767;
        int32_t s = mean + (int32_t)bonus;
        int32_t cond = -(int32_t)(s > best_s);
        best_s = sel_i32(cond, s, best_s);
        best_i = sel_u32(cond, i, best_i);
    }
    return best_i;
}

/* --------------------------------------------------------------------
 *  9. KV-dedup via popcount sketch.
 * -------------------------------------------------------------------- */

void cos_v69_dedup_init(cos_v69_dedup_t *d)
{
    if (!d) return;
    memset(d, 0, sizeof(*d));
}

uint32_t cos_v69_dedup_insert(cos_v69_dedup_t *d,
                              const cos_v69_sketch_t *sk,
                              uint32_t hamming_thresh,
                              uint8_t *collapsed_out)
{
    if (collapsed_out) *collapsed_out = 0;
    if (!d || !sk) return UINT32_MAX;
    /* Search for any existing entry within hamming_thresh. */
    for (uint32_t i = 0; i < COS_V69_DEDUP_MAX; ++i) {
        if (!d->valid[i]) continue;
        uint32_t h = cos_v69_sketch_hamming(&d->sk[i], sk);
        if (h < hamming_thresh) {
            d->collapses++;
            if (collapsed_out) *collapsed_out = 1;
            return i;
        }
    }
    /* No match: insert at first empty slot. */
    for (uint32_t i = 0; i < COS_V69_DEDUP_MAX; ++i) {
        if (!d->valid[i]) {
            d->sk[i]    = *sk;
            d->valid[i] = 1u;
            d->len++;
            return i;
        }
    }
    return UINT32_MAX;  /* table full */
}

uint32_t cos_v69_dedup_count(const cos_v69_dedup_t *d)
{ return d ? d->len : 0; }

uint32_t cos_v69_dedup_collapses(const cos_v69_dedup_t *d)
{ return d ? d->collapses : 0; }

/* --------------------------------------------------------------------
 *  Composed 10-bit branchless decision.
 * -------------------------------------------------------------------- */

cos_v69_decision_t cos_v69_compose_decision(uint8_t v60_ok,
                                            uint8_t v61_ok,
                                            uint8_t v62_ok,
                                            uint8_t v63_ok,
                                            uint8_t v64_ok,
                                            uint8_t v65_ok,
                                            uint8_t v66_ok,
                                            uint8_t v67_ok,
                                            uint8_t v68_ok,
                                            uint8_t v69_ok)
{
    cos_v69_decision_t d;
    d.v60_ok = v60_ok ? 1u : 0u;
    d.v61_ok = v61_ok ? 1u : 0u;
    d.v62_ok = v62_ok ? 1u : 0u;
    d.v63_ok = v63_ok ? 1u : 0u;
    d.v64_ok = v64_ok ? 1u : 0u;
    d.v65_ok = v65_ok ? 1u : 0u;
    d.v66_ok = v66_ok ? 1u : 0u;
    d.v67_ok = v67_ok ? 1u : 0u;
    d.v68_ok = v68_ok ? 1u : 0u;
    d.v69_ok = v69_ok ? 1u : 0u;
    d.allow  = (uint8_t)(d.v60_ok & d.v61_ok & d.v62_ok & d.v63_ok &
                         d.v64_ok & d.v65_ok & d.v66_ok & d.v67_ok &
                         d.v68_ok & d.v69_ok);
    return d;
}

/* --------------------------------------------------------------------
 * 10. CL — Constellation Language interpreter.
 * -------------------------------------------------------------------- */

struct cos_v69_cl_state {
    int16_t  reg_q15[COS_V69_NREGS];
    uint32_t reg_ptr[COS_V69_NREGS];
    uint64_t cost;
    uint32_t byz_fail;
    uint8_t  abstained;
    uint8_t  v69_ok;
};

cos_v69_cl_state_t *cos_v69_cl_new(void)
{
    return aligned_calloc64(sizeof(cos_v69_cl_state_t));
}

void cos_v69_cl_free(cos_v69_cl_state_t *s) { free(s); }

void cos_v69_cl_reset(cos_v69_cl_state_t *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
}

uint64_t cos_v69_cl_cost(const cos_v69_cl_state_t *s)
{ return s ? s->cost : 0; }

int16_t cos_v69_cl_reg_q15(const cos_v69_cl_state_t *s, uint32_t i)
{ return (s && i < COS_V69_NREGS) ? s->reg_q15[i] : 0; }

uint32_t cos_v69_cl_reg_ptr(const cos_v69_cl_state_t *s, uint32_t i)
{ return (s && i < COS_V69_NREGS) ? s->reg_ptr[i] : 0; }

uint32_t cos_v69_cl_byz_fail(const cos_v69_cl_state_t *s)
{ return s ? s->byz_fail : 0; }

uint8_t cos_v69_cl_abstained(const cos_v69_cl_state_t *s)
{ return s ? s->abstained : 0u; }

uint8_t cos_v69_cl_v69_ok(const cos_v69_cl_state_t *s)
{ return s ? s->v69_ok : 0u; }

static const uint64_t cos_v69_op_cost[] = {
    [COS_V69_OP_HALT]   = 1,
    [COS_V69_OP_DRAFT]  = 4,
    [COS_V69_OP_VERIFY] = 8,
    [COS_V69_OP_DEBATE] = 16,
    [COS_V69_OP_VOTE]   = 4,
    [COS_V69_OP_ROUTE]  = 4,
    [COS_V69_OP_GOSSIP] = 2,
    [COS_V69_OP_ELO]    = 1,
    [COS_V69_OP_DEDUP]  = 2,
    [COS_V69_OP_GATE]   = 1,
};

int cos_v69_cl_exec(cos_v69_cl_state_t *s,
                    const cos_v69_inst_t *prog,
                    uint32_t n,
                    cos_v69_cl_ctx_t *ctx,
                    uint64_t cost_budget)
{
    if (!s || !prog) return -1;
    cos_v69_cl_reset(s);

    for (uint32_t pc = 0; pc < n; ++pc) {
        cos_v69_inst_t ins = prog[pc];
        if (ins.op > COS_V69_OP_GATE) return -2;

        s->cost += cos_v69_op_cost[ins.op];
        if (s->cost > cost_budget) {
            s->abstained = 1;
            return -3;
        }

        uint32_t dst = ins.dst & (COS_V69_NREGS - 1u);
        uint32_t a   = ins.a   & (COS_V69_NREGS - 1u);
        uint32_t b   = ins.b   & (COS_V69_NREGS - 1u);
        (void)b;

        switch ((cos_v69_op_t)ins.op) {
        case COS_V69_OP_HALT:
            return 0;

        case COS_V69_OP_DRAFT: {
            /* reg_ptr[dst] = tree node count; tree is context-owned
             * (the caller builds the tree prior to exec).  Op is a
             * no-op placeholder for orchestration-cost accounting. */
            if (!ctx || !ctx->tree) return -4;
            s->reg_ptr[dst] = ctx->tree->n;
            break;
        }

        case COS_V69_OP_VERIFY: {
            if (!ctx || !ctx->tree || !ctx->target_tokens) return -5;
            uint32_t len = cos_v69_draft_tree_verify(
                ctx->tree, ctx->target_tokens, ctx->target_len,
                ctx->min_accept_q15, NULL);
            s->reg_ptr[dst] = len;
            /* Also record a Q0.15 coverage score = len * 32767 / target_len. */
            int32_t cov = (ctx->target_len > 0)
                ? (int32_t)(((uint64_t)len * 32767u) / ctx->target_len)
                : 0;
            s->reg_q15[a] = (int16_t)sat_q15_i32(cov);
            break;
        }

        case COS_V69_OP_DEBATE: {
            if (!ctx || !ctx->agent_votes || !ctx->debate_out) return -6;
            cos_v69_debate_score(ctx->agent_votes, ctx->n_agent_votes,
                                 COS_V69_TOPK_MAX,
                                 ctx->min_margin_q15,
                                 ctx->anti_conformity_q15,
                                 ctx->debate_out);
            s->reg_q15[dst] = ctx->debate_out->margin_q15;
            s->reg_ptr[a]   = ctx->debate_out->len;
            if (ctx->debate_out->abstained) s->abstained = 1;
            break;
        }

        case COS_V69_OP_VOTE: {
            if (!ctx || !ctx->byz_votes) return -7;
            uint8_t ok = 0;
            uint32_t count = cos_v69_byzantine_vote(
                ctx->byz_votes, ctx->n_byz_votes,
                ctx->byzantine_budget, &ok);
            s->reg_ptr[dst] = count;
            /* byz_fail = how many voted 0 (dissent/faulty). */
            uint32_t fail = (ctx->n_byz_votes > count)
                          ? (ctx->n_byz_votes - count) : 0;
            s->byz_fail += fail;
            s->reg_q15[a] = (int16_t)(ok ? 32767 : 0);
            break;
        }

        case COS_V69_OP_ROUTE: {
            if (!ctx || !ctx->expert_scores || !ctx->route_out) return -8;
            cos_v69_route_topk(ctx->expert_scores, ctx->n_experts,
                               ctx->route_out, ctx->load_counter);
            s->reg_ptr[dst] = ctx->route_out->len;
            /* record top-1 score */
            s->reg_q15[a] = (ctx->route_out->len > 0)
                ? ctx->route_out->score_q15[0] : 0;
            break;
        }

        case COS_V69_OP_GOSSIP: {
            if (!ctx || !ctx->clock || !ctx->peer_clock) return -9;
            cos_v69_clock_tick(ctx->clock, ctx->clock_node);
            cos_v69_clock_merge(ctx->clock, ctx->peer_clock);
            s->reg_ptr[dst] = ctx->clock->c[ctx->clock_node];
            break;
        }

        case COS_V69_OP_ELO: {
            if (!ctx || !ctx->bandit) return -10;
            /* ins.a = arm, ins.imm = outcome (0/1). */
            uint8_t outcome = (ins.imm ? 1u : 0u);
            cos_v69_elo_update(ctx->bandit, a, outcome, ctx->elo_k_q15);
            uint32_t chosen = cos_v69_ucb_select(ctx->bandit, ctx->ucb_c_q15);
            s->reg_ptr[dst] = chosen;
            s->reg_q15[dst] = ctx->bandit->arms[chosen].elo_q15;
            break;
        }

        case COS_V69_OP_DEDUP: {
            if (!ctx || !ctx->dedup || !ctx->dedup_sk) return -11;
            uint8_t collapsed = 0;
            uint32_t slot = cos_v69_dedup_insert(
                ctx->dedup, ctx->dedup_sk,
                ctx->dedup_thresh, &collapsed);
            s->reg_ptr[dst] = slot;
            s->reg_q15[a]   = (int16_t)(collapsed ? 32767 : 0);
            break;
        }

        case COS_V69_OP_GATE: {
            uint32_t fb = ctx ? ctx->byzantine_budget : 0;
            uint8_t cond_cost   = (s->cost <= cost_budget);
            uint8_t cond_score  = (s->reg_q15[a] >= ins.imm);
            uint8_t cond_byz    = (s->byz_fail <= fb);
            uint8_t cond_no_abs = (s->abstained == 0);
            s->v69_ok = (uint8_t)(cond_cost & cond_score &
                                  cond_byz & cond_no_abs);
            return 0;
        }
        }
    }
    s->abstained = 1;
    return -12;
}
