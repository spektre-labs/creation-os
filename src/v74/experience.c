/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v74 — σ-Experience — implementation.
 *
 * Branchless integer primitives; libc-only hot path.  Every HV token
 * is 256 bits = 4 × uint64.  Arenas are 64-byte aligned.
 */

#include "experience.h"

#include <stdlib.h>
#include <string.h>

const char cos_v74_version[] =
    "v74.0 experience · universal-ux · universal-expertise · mobile-gs · frame-gen · second-world";

/* ====================================================================
 *  Internal helpers.
 * ==================================================================== */

static void *cos_v74__aligned_alloc(size_t bytes) {
    size_t rounded = (bytes + 63u) & ~((size_t)63u);
    void  *p       = aligned_alloc(64, rounded);
    if (p) memset(p, 0, rounded);
    return p;
}

static uint64_t cos_v74__splitmix64(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static inline uint32_t cos_v74__popcount64(uint64_t x) {
    return (uint32_t)__builtin_popcountll(x);
}

/* Branchless integer max for int32_t. */
static inline int32_t cos_v74__max_i32(int32_t a, int32_t b) {
    int32_t d = a - b;
    int32_t mask = d >> 31;             /* arithmetic shift */
    return a - (d & mask);
}

/* ====================================================================
 *  HV primitives.
 * ==================================================================== */

void cos_v74_hv_zero(cos_v74_hv_t *dst) {
    for (uint32_t i = 0; i < COS_V74_HV_WORDS; ++i) dst->w[i] = 0;
}

void cos_v74_hv_copy(cos_v74_hv_t *dst, const cos_v74_hv_t *src) {
    for (uint32_t i = 0; i < COS_V74_HV_WORDS; ++i) dst->w[i] = src->w[i];
}

void cos_v74_hv_xor(cos_v74_hv_t *dst, const cos_v74_hv_t *a, const cos_v74_hv_t *b) {
    for (uint32_t i = 0; i < COS_V74_HV_WORDS; ++i) dst->w[i] = a->w[i] ^ b->w[i];
}

void cos_v74_hv_from_seed(cos_v74_hv_t *dst, uint64_t seed) {
    uint64_t s = seed ^ 0xA24BAED4963EE407ULL;
    for (uint32_t i = 0; i < COS_V74_HV_WORDS; ++i) dst->w[i] = cos_v74__splitmix64(&s);
}

static const uint64_t COS_V74_DOM_SALT[8] = {
    0x0000000000000000ULL,   /* UX     */
    0x0F0F0F0F0F0F0F0FULL,   /* CODE   */
    0xF0F0F0F0F0F0F0F0ULL,   /* RENDER */
    0x5A5A5A5A5A5A5A5AULL,   /* AUDIO  */
    0xA5A5A5A5A5A5A5A5ULL,   /* MATH   */
    0xCCCCCCCCCCCCCCCCULL,   /* BIO    */
    0x3333333333333333ULL,   /* SEC    */
    0x9696969696969696ULL,   /* WORLD  */
};

void cos_v74_hv_permute(cos_v74_hv_t     *dst,
                        const cos_v74_hv_t *src,
                        cos_v74_domain_t    d)
{
    uint32_t di   = (uint32_t)d & 7u;
    uint64_t salt = COS_V74_DOM_SALT[di];
    for (uint32_t i = 0; i < COS_V74_HV_WORDS; ++i) {
        uint32_t j = (i + di) & (COS_V74_HV_WORDS - 1u);
        uint64_t x = src->w[j];
        uint32_t r = (11u * di) & 63u;
        uint64_t rot = (r == 0) ? x : ((x << r) | (x >> ((64u - r) & 63u)));
        dst->w[i] = rot ^ salt ^ ((uint64_t)i * 0xC6BC279692B5C323ULL);
    }
}

uint32_t cos_v74_hv_hamming(const cos_v74_hv_t *a, const cos_v74_hv_t *b) {
    uint32_t d = 0;
    for (uint32_t i = 0; i < COS_V74_HV_WORDS; ++i) {
        d += cos_v74__popcount64(a->w[i] ^ b->w[i]);
    }
    return d;
}

/* ====================================================================
 *  1.  Fitts-V2P target heatmap.
 * ==================================================================== */

static inline int32_t cos_v74__fitts_weight(const cos_v74_target_t *t,
                                            int32_t cx, int32_t cy)
{
    int32_t dx  = cx - t->x;
    int32_t dy  = cy - t->y;
    int32_t sz  = t->size;
    int32_t var = sz * sz;
    int32_t d2  = dx * dx + dy * dy;
    int32_t w   = var - d2;
    /* saturate at 0 via branchless max */
    return cos_v74__max_i32(w, 0);
}

uint32_t cos_v74_target_argmax(const cos_v74_target_t *tg,
                               uint32_t                n,
                               int32_t                 cx,
                               int32_t                 cy,
                               int32_t                *out_weight)
{
    uint32_t best_idx = 0;
    int32_t  best_w   = -1;
    for (uint32_t i = 0; i < n; ++i) {
        int32_t w = cos_v74__fitts_weight(&tg[i], cx, cy);
        /* Branchless argmax: if w > best_w, update.  Uses sign of diff. */
        int32_t d    = w - best_w;
        int32_t mask = (int32_t)((uint32_t)(-d) >> 31);  /* 1 if d>0 else 0 */
        best_idx = (best_idx & (uint32_t)(mask - 1)) | (i & (uint32_t)(-mask));
        best_w   = best_w + (d & (-mask));
    }
    if (out_weight) *out_weight = best_w;
    return best_idx;
}

uint8_t cos_v74_target_hit(const cos_v74_target_t *tg,
                           uint32_t                n,
                           int32_t                 cx,
                           int32_t                 cy,
                           int32_t                 threshold)
{
    int32_t w = 0;
    (void)cos_v74_target_argmax(tg, n, cx, cy, &w);
    /* w ≥ threshold  ↔  sign bit of (w - threshold) is 0. */
    int32_t d = w - threshold;
    return (uint8_t)((((uint32_t)d) >> 31) ^ 1u);
}

/* ====================================================================
 *  2.  Adaptive layout verify.
 * ==================================================================== */

static inline uint8_t cos_v74__bit_get(const uint64_t *bits, uint32_t i) {
    return (uint8_t)((bits[i >> 6] >> (i & 63u)) & 1u);
}

uint8_t cos_v74_layout_verify(const cos_v74_layout_spec_t *spec,
                              const cos_v74_target_t      *tg,
                              const uint32_t              *slot_to_target,
                              const int32_t               *slot_cx,
                              const int32_t               *slot_cy,
                              int32_t                     *out_total_effort)
{
    if (!spec || !tg || !slot_to_target || !slot_cx || !slot_cy) return 0;

    uint8_t  reach_ok  = 1;
    uint8_t  frozen_ok = 1;
    uint8_t  bounds_ok = 1;
    uint8_t  unique_ok = 1;
    int32_t  total     = 0;

    /* bounds + reachability + frozen check, accumulate Fitts effort  */
    uint64_t used[((COS_V74_TARGETS_MAX + 63u) / 64u)];
    for (uint32_t w = 0; w < sizeof(used)/sizeof(used[0]); ++w) used[w] = 0;

    for (uint32_t i = 0; i < spec->n_slots; ++i) {
        uint32_t t = slot_to_target[i];
        /* bounds: t < n_targets */
        bounds_ok &= (uint8_t)(t < spec->n_targets);
        if (t >= spec->n_targets) continue; /* keep arrays safe */

        /* reach bit */
        if (spec->reach_bits) {
            reach_ok &= cos_v74__bit_get(spec->reach_bits, t);
        }

        /* frozen slot must map to its assigned target */
        if (spec->frozen_bits && spec->frozen_slot_map) {
            uint8_t fr = cos_v74__bit_get(spec->frozen_bits, i);
            uint8_t eq = (uint8_t)(slot_to_target[i] == spec->frozen_slot_map[i]);
            /* if frozen, must equal; branchless: (!fr) | eq */
            frozen_ok &= (uint8_t)((fr ^ 1u) | eq);
        }

        /* uniqueness: mark bit */
        uint32_t bw = t >> 6;
        uint64_t bm = (uint64_t)1 << (t & 63u);
        unique_ok &= (uint8_t)(((used[bw] & bm) == 0) ? 1u : 0u);
        used[bw] |= bm;

        /* Fitts effort = (size²) - weight, accumulated for budget */
        int32_t sz  = tg[t].size;
        int32_t var = sz * sz;
        int32_t w   = cos_v74__fitts_weight(&tg[t], slot_cx[i], slot_cy[i]);
        total += (var - w);   /* effort ≥ 0, bounded by var */
    }

    uint8_t budget_ok = (uint8_t)(total <= spec->effort_budget);

    if (out_total_effort) *out_total_effort = total;
    return (uint8_t)(reach_ok & frozen_ok & bounds_ok & unique_ok & budget_ok);
}

/* ====================================================================
 *  3.  Designer-basis personalisation.
 * ==================================================================== */

cos_v74_basis_t *cos_v74_basis_new(uint32_t k, uint64_t seed) {
    if (k == 0 || k > COS_V74_BASIS_MAX) return NULL;
    cos_v74_basis_t *b = (cos_v74_basis_t *)cos_v74__aligned_alloc(sizeof(*b));
    if (!b) return NULL;
    b->k = k;
    b->bank = (cos_v74_hv_t *)cos_v74__aligned_alloc((size_t)k * sizeof(cos_v74_hv_t));
    if (!b->bank) { free(b); return NULL; }
    uint64_t s = seed ^ 0xB7E151628AED2A6BULL;
    for (uint32_t i = 0; i < k; ++i) {
        cos_v74_hv_from_seed(&b->bank[i], cos_v74__splitmix64(&s));
    }
    return b;
}

void cos_v74_basis_free(cos_v74_basis_t *b) {
    if (!b) return;
    free(b->bank);
    free(b);
}

uint32_t cos_v74_basis_nearest(const cos_v74_basis_t *b,
                               const cos_v74_hv_t    *user_hv,
                               uint32_t              *out_dist)
{
    uint32_t best_i = 0;
    uint32_t best_d = 257;   /* > max hamming 256 */
    for (uint32_t i = 0; i < b->k; ++i) {
        uint32_t d = cos_v74_hv_hamming(&b->bank[i], user_hv);
        /* branchless min-tracking */
        uint32_t less = (uint32_t)(-(int32_t)(d < best_d));
        best_i = (best_i & ~less) | (i & less);
        best_d = (best_d & ~less) | (d & less);
    }
    if (out_dist) *out_dist = best_d;
    return best_i;
}

uint8_t cos_v74_basis_ok(const cos_v74_basis_t *b,
                         const cos_v74_hv_t    *user_hv,
                         uint32_t               tol,
                         uint32_t               margin_min)
{
    /* Find nearest and second-nearest branchlessly.                   *
     *   if d < d1: d2 = d1; d1 = d                                    *
     *   else if d < d2: d2 = d                                        */
    uint32_t d1 = 257, d2 = 257;
    for (uint32_t i = 0; i < b->k; ++i) {
        uint32_t d      = cos_v74_hv_hamming(&b->bank[i], user_hv);
        uint32_t less1  = (uint32_t)(-(int32_t)(d < d1));
        /* push old d1 → d2, then update d1 with d (both branchless)    */
        d2 = (d2 & ~less1) | (d1 & less1);
        d1 = (d1 & ~less1) | (d  & less1);
        /* Second pass: if NOT less1 AND d < d2 (post-update), set d2=d */
        uint32_t not_less1 = ~less1;
        uint32_t less2 = (uint32_t)(-(int32_t)(d < d2)) & not_less1;
        d2 = (d2 & ~less2) | (d & less2);
    }
    uint8_t near_ok   = (uint8_t)(d1 <= tol);
    uint8_t margin_ok = (uint8_t)((d2 - d1) >= margin_min);
    return (uint8_t)(near_ok & margin_ok);
}

/* ====================================================================
 *  4.  SquireIR slot authoring.
 * ==================================================================== */

uint8_t cos_v74_squire_verify(const uint64_t *mutable_bits,
                              const uint64_t *frozen_bits,
                              const uint64_t *edit_bits,
                              uint32_t        row_words)
{
    if (!mutable_bits || !frozen_bits || !edit_bits) return 0;
    uint64_t bad_frozen = 0;
    uint64_t bad_escape = 0;
    for (uint32_t w = 0; w < row_words; ++w) {
        bad_frozen |= edit_bits[w] & frozen_bits[w];
        bad_escape |= edit_bits[w] & ~mutable_bits[w];
    }
    return (uint8_t)((bad_frozen == 0) & (bad_escape == 0));
}

/* ====================================================================
 *  5.  Universal-expert LoRA-MoE mesh.
 * ==================================================================== */

cos_v74_expert_t *cos_v74_expert_new(uint32_t n, uint64_t seed) {
    if (n == 0 || n > COS_V74_EXPERTS_MAX) return NULL;
    cos_v74_expert_t *e = (cos_v74_expert_t *)cos_v74__aligned_alloc(sizeof(*e));
    if (!e) return NULL;
    e->n = n;
    e->bank = (cos_v74_hv_t *)cos_v74__aligned_alloc((size_t)n * sizeof(cos_v74_hv_t));
    e->rank = (uint8_t *)cos_v74__aligned_alloc((size_t)n);
    if (!e->bank || !e->rank) { free(e->bank); free(e->rank); free(e); return NULL; }
    uint64_t s = seed ^ 0x243F6A8885A308D3ULL;
    for (uint32_t i = 0; i < n; ++i) {
        cos_v74_hv_from_seed(&e->bank[i], cos_v74__splitmix64(&s));
        e->rank[i] = (uint8_t)((cos_v74__splitmix64(&s) & 31u) + 1u);   /* 1..32 */
    }
    return e;
}

void cos_v74_expert_free(cos_v74_expert_t *e) {
    if (!e) return;
    free(e->bank);
    free(e->rank);
    free(e);
}

void cos_v74_expert_topk(const cos_v74_expert_t *e,
                         const cos_v74_hv_t     *q,
                         uint32_t                k,
                         cos_v74_topk_t         *out)
{
    if (k > COS_V74_TOPK_MAX) k = COS_V74_TOPK_MAX;
    /* Initialise to "empty" (dist = 257 > 256 max). */
    for (uint32_t j = 0; j < COS_V74_TOPK_MAX; ++j) {
        out->idx[j] = 0;
        out->dist[j] = 257;
    }
    out->k = k;

    for (uint32_t i = 0; i < e->n; ++i) {
        uint32_t d = cos_v74_hv_hamming(&e->bank[i], q);
        /* Bubble down into sorted (ascending dist) top-k list.          */
        /* This is an O(k) branchless(-ish) insertion for small k≤4.     */
        uint32_t cur_i = i;
        uint32_t cur_d = d;
        for (uint32_t j = 0; j < k; ++j) {
            uint32_t less = (uint32_t)(-(int32_t)(cur_d < out->dist[j]));
            uint32_t t_i  = out->idx[j];
            uint32_t t_d  = out->dist[j];
            out->idx[j]  = (t_i & ~less) | (cur_i & less);
            out->dist[j] = (t_d & ~less) | (cur_d & less);
            cur_i = (cur_i & ~less) | (t_i & less);
            cur_d = (cur_d & ~less) | (t_d & less);
        }
    }
}

uint8_t cos_v74_expert_route_ok(const cos_v74_topk_t *tk,
                                uint32_t              margin_min)
{
    if (tk->k == 0 || tk->k > COS_V74_TOPK_MAX) return 0;
    uint8_t monotone = 1;
    for (uint32_t j = 1; j < tk->k; ++j) {
        monotone &= (uint8_t)(tk->dist[j - 1] <= tk->dist[j]);
    }
    uint32_t margin = tk->dist[tk->k - 1] - tk->dist[0];
    uint8_t  margin_ok = (uint8_t)(margin >= margin_min);
    return (uint8_t)(monotone & margin_ok);
}

/* ====================================================================
 *  6.  Skill composition.
 * ==================================================================== */

void cos_v74_skill_bind(cos_v74_hv_t           *out,
                        const cos_v74_expert_t *e,
                        const cos_v74_topk_t   *tk,
                        const cos_v74_domain_t *doms)
{
    cos_v74_hv_zero(out);
    for (uint32_t j = 0; j < tk->k; ++j) {
        cos_v74_hv_t p;
        cos_v74_hv_permute(&p, &e->bank[tk->idx[j]], doms[j]);
        cos_v74_hv_xor(out, out, &p);
    }
}

uint8_t cos_v74_skill_verify(const cos_v74_expert_t *e,
                             const cos_v74_topk_t   *tk,
                             const cos_v74_domain_t *doms,
                             const cos_v74_hv_t     *claim,
                             uint32_t                tol)
{
    cos_v74_hv_t bound;
    cos_v74_skill_bind(&bound, e, tk, doms);
    uint32_t d = cos_v74_hv_hamming(&bound, claim);
    return (uint8_t)(d <= tol);
}

/* ====================================================================
 *  7.  Mobile-GS order-free render step.
 * ==================================================================== */

cos_v74_render_t *cos_v74_render_new(uint32_t n_tiles,
                                     uint32_t row_words,
                                     uint64_t seed)
{
    if (n_tiles == 0 || n_tiles > COS_V74_TILES_MAX || row_words == 0) return NULL;
    cos_v74_render_t *r = (cos_v74_render_t *)cos_v74__aligned_alloc(sizeof(*r));
    if (!r) return NULL;
    r->n_tiles   = n_tiles;
    r->row_words = row_words;
    r->vis_budget = 64u * row_words * n_tiles; /* permissive default */
    r->view_tol  = 256u;                        /* permissive default */
    r->tile_bits = (uint64_t *)cos_v74__aligned_alloc(
        (size_t)n_tiles * row_words * sizeof(uint64_t));
    r->tile_tag  = (cos_v74_hv_t *)cos_v74__aligned_alloc(
        (size_t)n_tiles * sizeof(cos_v74_hv_t));
    if (!r->tile_bits || !r->tile_tag) {
        free(r->tile_bits); free(r->tile_tag); free(r); return NULL;
    }
    uint64_t s = seed ^ 0xB5026F5AA96619E9ULL;
    for (uint32_t i = 0; i < n_tiles * row_words; ++i) {
        r->tile_bits[i] = cos_v74__splitmix64(&s);
    }
    for (uint32_t i = 0; i < n_tiles; ++i) {
        cos_v74_hv_from_seed(&r->tile_tag[i], cos_v74__splitmix64(&s));
    }
    /* Default bucket masks spread across every 4th bit.                */
    r->bucket_mask[0] = 0x1111111111111111ULL;
    r->bucket_mask[1] = 0x2222222222222222ULL;
    r->bucket_mask[2] = 0x4444444444444444ULL;
    r->bucket_mask[3] = 0x8888888888888888ULL;
    return r;
}

void cos_v74_render_free(cos_v74_render_t *r) {
    if (!r) return;
    free(r->tile_bits);
    free(r->tile_tag);
    free(r);
}

uint8_t cos_v74_render_step(const cos_v74_render_t *r,
                            const cos_v74_hv_t     *claim_tag,
                            uint32_t               *out_score)
{
    if (!r || !claim_tag) return 0;
    uint32_t total_score   = 0;
    uint32_t worst_hamming = 0;
    for (uint32_t t = 0; t < r->n_tiles; ++t) {
        const uint64_t *row = &r->tile_bits[(size_t)t * r->row_words];
        uint32_t tile_score = 0;
        for (uint32_t bw = 0; bw < r->row_words; ++bw) {
            uint64_t x = row[bw];
            for (uint32_t k = 0; k < COS_V74_DEPTH_BUCKETS; ++k) {
                tile_score += cos_v74__popcount64(x & r->bucket_mask[k]);
            }
        }
        total_score += tile_score;

        uint32_t h = cos_v74_hv_hamming(&r->tile_tag[t], claim_tag);
        if (h > worst_hamming) worst_hamming = h;   /* bound only; never
                                                     * affects branchless
                                                     * decision below    */
    }
    if (out_score) *out_score = total_score;
    uint8_t vis_ok  = (uint8_t)(total_score   <= r->vis_budget);
    uint8_t view_ok = (uint8_t)(worst_hamming <= r->view_tol);
    return (uint8_t)(vis_ok & view_ok);
}

/* ====================================================================
 *  8.  Neural upscale + frame-gen gate.
 * ==================================================================== */

uint8_t cos_v74_upscale_ok(const cos_v74_upscale_t *u) {
    if (!u) return 0;
    uint8_t factor_ok  = (uint8_t)((u->gen_factor >= 1u) &
                                   (u->gen_factor <= COS_V74_FRAMEGEN_FACTOR_MAX));
    uint8_t display_ok = (uint8_t)((uint64_t)u->base_hz * u->gen_factor <= u->display_hz);
    uint8_t quality_ok = (uint8_t)(u->quality_score >= u->quality_min);
    return (uint8_t)(factor_ok & display_ok & quality_ok);
}

/* ====================================================================
 *  9.  Second-render interactive-world synth.
 * ==================================================================== */

uint8_t cos_v74_world_second(cos_v74_hv_t       *tiles,
                             uint32_t            n_tiles,
                             uint64_t            seed,
                             const cos_v74_hv_t *style_hv,
                             uint32_t            second_budget,
                             uint32_t            coherence_tol,
                             uint32_t           *out_used_units)
{
    if (!tiles || n_tiles == 0 || n_tiles > COS_V74_WORLD_TILES_MAX) return 0;
    uint64_t s = seed ^ 0x93C467E37DB0C7A4ULL;
    uint32_t used = 0;

    cos_v74_hv_t style;
    if (style_hv) cos_v74_hv_copy(&style, style_hv);
    else          cos_v74_hv_zero(&style);

    for (uint32_t i = 0; i < n_tiles; ++i) {
        cos_v74_hv_t raw;
        for (uint32_t w = 0; w < COS_V74_HV_WORDS; ++w) {
            raw.w[w] = cos_v74__splitmix64(&s);
        }
        cos_v74_hv_t p;
        cos_v74_hv_permute(&p, &raw, COS_V74_DOM_WORLD);
        cos_v74_hv_xor(&tiles[i], &p, &style);
        used += 1;
    }

    if (out_used_units) *out_used_units = used;

    uint8_t budget_ok = (uint8_t)(used <= second_budget);
    uint32_t h = cos_v74_hv_hamming(&tiles[0], &tiles[n_tiles - 1]);
    uint8_t coh_ok = (uint8_t)(h <= coherence_tol);
    return (uint8_t)(budget_ok & coh_ok);
}

/* ====================================================================
 * 10.  XPL bytecode VM.
 * ==================================================================== */

struct cos_v74_xpl_state {
    uint64_t units;
    uint8_t  target_ok;
    uint8_t  layout_ok;
    uint8_t  basis_ok;
    uint8_t  slot_ok;
    uint8_t  expert_ok;
    uint8_t  skill_ok;
    uint8_t  render_ok;
    uint8_t  upscale_ok;
    uint8_t  world_second_ok;
    uint8_t  gated;
    uint8_t  abstained;
    uint8_t  v74_ok;
};

static const uint32_t COS_V74_OP_COST[COS_V74_OP_GATE + 1] = {
    [COS_V74_OP_HALT]    = 1,
    [COS_V74_OP_TARGET]  = 2,
    [COS_V74_OP_LAYOUT]  = 4,
    [COS_V74_OP_BASIS]   = 2,
    [COS_V74_OP_SLOT]    = 2,
    [COS_V74_OP_EXPERT]  = 8,
    [COS_V74_OP_RENDER]  = 8,
    [COS_V74_OP_UPSCALE] = 4,
    [COS_V74_OP_WORLD]   = 16,
    [COS_V74_OP_GATE]    = 1,
};

cos_v74_xpl_state_t *cos_v74_xpl_new(void) {
    cos_v74_xpl_state_t *s = (cos_v74_xpl_state_t *)cos_v74__aligned_alloc(sizeof(*s));
    return s;
}

void cos_v74_xpl_free(cos_v74_xpl_state_t *s) { free(s); }

void cos_v74_xpl_reset(cos_v74_xpl_state_t *s) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
}

uint64_t cos_v74_xpl_units          (const cos_v74_xpl_state_t *s) { return s ? s->units : 0; }
uint8_t  cos_v74_xpl_target_ok      (const cos_v74_xpl_state_t *s) { return s ? s->target_ok : 0; }
uint8_t  cos_v74_xpl_layout_ok      (const cos_v74_xpl_state_t *s) { return s ? s->layout_ok : 0; }
uint8_t  cos_v74_xpl_basis_ok       (const cos_v74_xpl_state_t *s) { return s ? s->basis_ok : 0; }
uint8_t  cos_v74_xpl_slot_ok        (const cos_v74_xpl_state_t *s) { return s ? s->slot_ok : 0; }
uint8_t  cos_v74_xpl_expert_ok      (const cos_v74_xpl_state_t *s) { return s ? s->expert_ok : 0; }
uint8_t  cos_v74_xpl_skill_ok       (const cos_v74_xpl_state_t *s) { return s ? s->skill_ok : 0; }
uint8_t  cos_v74_xpl_render_ok      (const cos_v74_xpl_state_t *s) { return s ? s->render_ok : 0; }
uint8_t  cos_v74_xpl_upscale_ok     (const cos_v74_xpl_state_t *s) { return s ? s->upscale_ok : 0; }
uint8_t  cos_v74_xpl_world_second_ok(const cos_v74_xpl_state_t *s) { return s ? s->world_second_ok : 0; }
uint8_t  cos_v74_xpl_abstained      (const cos_v74_xpl_state_t *s) { return s ? s->abstained : 0; }
uint8_t  cos_v74_xpl_v74_ok         (const cos_v74_xpl_state_t *s) { return s ? s->v74_ok : 0; }

int cos_v74_xpl_exec(cos_v74_xpl_state_t       *s,
                     const cos_v74_xpl_inst_t  *prog,
                     uint32_t                   n,
                     cos_v74_xpl_ctx_t         *ctx,
                     uint64_t                   unit_budget)
{
    if (!s || !prog || !ctx) return -1;
    cos_v74_xpl_reset(s);
    s->abstained = ctx->abstain;

    for (uint32_t pc = 0; pc < n; ++pc) {
        const cos_v74_xpl_inst_t *ins = &prog[pc];
        if (ins->op > COS_V74_OP_GATE) return -1;

        s->units += COS_V74_OP_COST[ins->op];
        if (s->units > unit_budget) return -2;

        switch (ins->op) {
        case COS_V74_OP_HALT:
            return 0;

        case COS_V74_OP_TARGET:
            if (!ctx->targets) return -3;
            s->target_ok = cos_v74_target_hit(ctx->targets, ctx->n_targets,
                                              ctx->cursor_x, ctx->cursor_y,
                                              ctx->target_threshold);
            break;

        case COS_V74_OP_LAYOUT:
            if (!ctx->layout || !ctx->targets || !ctx->slot_to_target
                || !ctx->slot_cx || !ctx->slot_cy) return -3;
            s->layout_ok = cos_v74_layout_verify(ctx->layout, ctx->targets,
                                                 ctx->slot_to_target,
                                                 ctx->slot_cx, ctx->slot_cy,
                                                 NULL);
            break;

        case COS_V74_OP_BASIS:
            if (!ctx->basis || !ctx->user_hv) return -3;
            s->basis_ok = cos_v74_basis_ok(ctx->basis, ctx->user_hv,
                                           ctx->basis_tol, ctx->basis_margin);
            break;

        case COS_V74_OP_SLOT:
            if (!ctx->slot_mutable || !ctx->slot_frozen || !ctx->slot_edit)
                return -3;
            s->slot_ok = cos_v74_squire_verify(ctx->slot_mutable,
                                               ctx->slot_frozen,
                                               ctx->slot_edit,
                                               ctx->slot_row_words);
            break;

        case COS_V74_OP_EXPERT: {
            if (!ctx->expert || !ctx->expert_query || !ctx->skill_doms
                || !ctx->skill_claim) return -3;
            cos_v74_topk_t tk;
            cos_v74_expert_topk(ctx->expert, ctx->expert_query,
                                ctx->expert_k, &tk);
            s->expert_ok = cos_v74_expert_route_ok(&tk, ctx->expert_margin);
            s->skill_ok  = cos_v74_skill_verify(ctx->expert, &tk,
                                                ctx->skill_doms,
                                                ctx->skill_claim,
                                                ctx->skill_tol);
        } break;

        case COS_V74_OP_RENDER:
            if (!ctx->render || !ctx->render_claim) return -3;
            s->render_ok = cos_v74_render_step(ctx->render,
                                               ctx->render_claim, NULL);
            break;

        case COS_V74_OP_UPSCALE:
            if (!ctx->upscale) return -3;
            s->upscale_ok = cos_v74_upscale_ok(ctx->upscale);
            break;

        case COS_V74_OP_WORLD: {
            if (!ctx->world_tiles || ctx->world_n_tiles == 0) return -3;
            s->world_second_ok = cos_v74_world_second(
                ctx->world_tiles,
                ctx->world_n_tiles,
                ctx->world_seed,
                ctx->world_style,
                ctx->world_second_budget,
                ctx->world_coherence_tol,
                NULL);
        } break;

        case COS_V74_OP_GATE: {
            uint8_t budget_ok = (uint8_t)(s->units <= unit_budget);
            s->gated  = 1;
            s->v74_ok = (uint8_t)(budget_ok
                                 & s->target_ok
                                 & s->layout_ok
                                 & s->basis_ok
                                 & s->slot_ok
                                 & s->expert_ok
                                 & s->skill_ok
                                 & s->render_ok
                                 & s->upscale_ok
                                 & s->world_second_ok
                                 & (uint8_t)(s->abstained ^ 1u));
        } break;
        }
    }
    return 0;
}

/* ====================================================================
 *  Composed 15-bit branchless decision.
 * ==================================================================== */

cos_v74_decision_t cos_v74_compose_decision(uint8_t v60_ok,
                                            uint8_t v61_ok,
                                            uint8_t v62_ok,
                                            uint8_t v63_ok,
                                            uint8_t v64_ok,
                                            uint8_t v65_ok,
                                            uint8_t v66_ok,
                                            uint8_t v67_ok,
                                            uint8_t v68_ok,
                                            uint8_t v69_ok,
                                            uint8_t v70_ok,
                                            uint8_t v71_ok,
                                            uint8_t v72_ok,
                                            uint8_t v73_ok,
                                            uint8_t v74_ok)
{
    cos_v74_decision_t d;
    d.v60_ok = (uint8_t)(v60_ok & 1u);
    d.v61_ok = (uint8_t)(v61_ok & 1u);
    d.v62_ok = (uint8_t)(v62_ok & 1u);
    d.v63_ok = (uint8_t)(v63_ok & 1u);
    d.v64_ok = (uint8_t)(v64_ok & 1u);
    d.v65_ok = (uint8_t)(v65_ok & 1u);
    d.v66_ok = (uint8_t)(v66_ok & 1u);
    d.v67_ok = (uint8_t)(v67_ok & 1u);
    d.v68_ok = (uint8_t)(v68_ok & 1u);
    d.v69_ok = (uint8_t)(v69_ok & 1u);
    d.v70_ok = (uint8_t)(v70_ok & 1u);
    d.v71_ok = (uint8_t)(v71_ok & 1u);
    d.v72_ok = (uint8_t)(v72_ok & 1u);
    d.v73_ok = (uint8_t)(v73_ok & 1u);
    d.v74_ok = (uint8_t)(v74_ok & 1u);
    d.allow  = (uint8_t)(d.v60_ok & d.v61_ok & d.v62_ok & d.v63_ok & d.v64_ok
                       & d.v65_ok & d.v66_ok & d.v67_ok & d.v68_ok & d.v69_ok
                       & d.v70_ok & d.v71_ok & d.v72_ok & d.v73_ok & d.v74_ok);
    return d;
}
