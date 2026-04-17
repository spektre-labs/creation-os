/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v71 — σ-Wormhole (implementation).
 *
 * Ten branchless integer primitives + a 10-opcode WHL bytecode ISA +
 * a 12-way branchless AND gate.  Zero dependencies beyond libc on
 * the hot path.  All arenas `aligned_alloc(64, …)`; all similarity
 * math is `__builtin_popcountll` + integer arithmetic.
 *
 * 1 = 1.
 */

#include "wormhole.h"

#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------
 *  Tiny helpers.
 * -------------------------------------------------------------------- */

static inline uint64_t splitmix64(uint64_t *s)
{
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static void *al64(size_t bytes)
{
    size_t rounded = (bytes + 63u) & ~((size_t)63);
    if (rounded == 0) rounded = 64;
    return aligned_alloc(64, rounded);
}

/* --------------------------------------------------------------------
 *  1.  Primitive HV operations.
 * -------------------------------------------------------------------- */

void cos_v71_hv_zero(cos_v71_hv_t *dst)
{
    if (!dst) return;
    memset(dst->w, 0, sizeof(dst->w));
}

void cos_v71_hv_copy(cos_v71_hv_t *dst, const cos_v71_hv_t *src)
{
    if (!dst || !src) return;
    memcpy(dst->w, src->w, sizeof(dst->w));
}

void cos_v71_hv_from_seed(cos_v71_hv_t *dst, uint64_t seed)
{
    if (!dst) return;
    uint64_t s = seed ? seed : 0xD1B54A32D192ED03ULL;
    for (uint32_t i = 0; i < COS_V71_HV_DIM_WORDS; ++i)
        dst->w[i] = splitmix64(&s);
}

void cos_v71_hv_xor(cos_v71_hv_t       *dst,
                    const cos_v71_hv_t *a,
                    const cos_v71_hv_t *b)
{
    if (!dst || !a || !b) return;
    for (uint32_t i = 0; i < COS_V71_HV_DIM_WORDS; ++i)
        dst->w[i] = a->w[i] ^ b->w[i];
}

/* Cyclic bit rotation of the entire bit-string by `shift` bits. */
void cos_v71_hv_permute(cos_v71_hv_t       *dst,
                        const cos_v71_hv_t *src,
                        int32_t             shift)
{
    if (!dst || !src) return;

    /* Normalise shift into [0, D). */
    int64_t s = (int64_t)shift % (int64_t)COS_V71_HV_DIM_BITS;
    if (s < 0) s += (int64_t)COS_V71_HV_DIM_BITS;

    uint32_t word_shift = (uint32_t)(s / 64);
    uint32_t bit_shift  = (uint32_t)(s % 64);
    uint32_t W          = COS_V71_HV_DIM_WORDS;

    if (bit_shift == 0) {
        for (uint32_t i = 0; i < W; ++i)
            dst->w[(i + word_shift) % W] = src->w[i];
        return;
    }

    uint64_t tmp[COS_V71_HV_DIM_WORDS];
    uint64_t carry = 0;
    /* Move low→high with cross-word carry. */
    for (uint32_t i = 0; i < W; ++i) {
        uint64_t v = src->w[i];
        tmp[i]   = (v << bit_shift) | carry;
        carry    = v >> (64 - bit_shift);
    }
    /* Wrap carry into word 0. */
    tmp[0] |= carry;

    /* Apply word-level rotation. */
    for (uint32_t i = 0; i < W; ++i)
        dst->w[(i + word_shift) % W] = tmp[i];
}

uint32_t cos_v71_hv_popcount(const cos_v71_hv_t *a)
{
    if (!a) return 0;
    uint32_t acc = 0;
    for (uint32_t i = 0; i < COS_V71_HV_DIM_WORDS; ++i)
        acc += (uint32_t)__builtin_popcountll(a->w[i]);
    return acc;
}

uint32_t cos_v71_hv_hamming(const cos_v71_hv_t *a,
                            const cos_v71_hv_t *b)
{
    if (!a || !b) return COS_V71_HV_DIM_BITS;
    uint32_t acc = 0;
    for (uint32_t i = 0; i < COS_V71_HV_DIM_WORDS; ++i)
        acc += (uint32_t)__builtin_popcountll(a->w[i] ^ b->w[i]);
    return acc;
}

int32_t cos_v71_hv_similarity_q15(const cos_v71_hv_t *a,
                                  const cos_v71_hv_t *b)
{
    /* sim = (D - 2H) * (32768 / D); for D = 8192 → × 4. */
    uint32_t h = cos_v71_hv_hamming(a, b);
    int32_t  d = (int32_t)COS_V71_HV_DIM_BITS - 2 * (int32_t)h;
    return d * 4;
}

/* --------------------------------------------------------------------
 *  2.  Portal table — Einstein-Rosen bridges.
 * -------------------------------------------------------------------- */

cos_v71_portal_t *cos_v71_portal_new(uint32_t cap,
                                     const cos_v71_hv_t *secret)
{
    if (cap == 0 || cap > COS_V71_PORTAL_CAP_MAX) return NULL;

    cos_v71_portal_t *p = (cos_v71_portal_t *)al64(sizeof *p);
    if (!p) return NULL;
    memset(p, 0, sizeof *p);

    p->anchors = (cos_v71_hv_t *)al64((size_t)cap * sizeof(cos_v71_hv_t));
    p->bridges = (cos_v71_hv_t *)al64((size_t)cap * sizeof(cos_v71_hv_t));
    p->sigs    = (cos_v71_hv_t *)al64((size_t)cap * sizeof(cos_v71_hv_t));
    p->seeds   = (uint64_t    *)al64((size_t)cap * sizeof(uint64_t));
    p->labels  = (uint32_t    *)al64((size_t)cap * sizeof(uint32_t));
    p->secret  = (cos_v71_hv_t *)al64(sizeof(cos_v71_hv_t));

    if (!p->anchors || !p->bridges || !p->sigs ||
        !p->seeds || !p->labels || !p->secret) {
        cos_v71_portal_free(p);
        return NULL;
    }

    p->cap = cap;
    p->len = 0;
    memset(p->anchors, 0, (size_t)cap * sizeof(cos_v71_hv_t));
    memset(p->bridges, 0, (size_t)cap * sizeof(cos_v71_hv_t));
    memset(p->sigs,    0, (size_t)cap * sizeof(cos_v71_hv_t));
    memset(p->seeds,   0, (size_t)cap * sizeof(uint64_t));
    memset(p->labels,  0, (size_t)cap * sizeof(uint32_t));

    if (secret) {
        cos_v71_hv_copy(p->secret, secret);
    } else {
        cos_v71_hv_from_seed(p->secret, 0xA5A5A5A5A5A5A5A5ULL);
    }
    return p;
}

void cos_v71_portal_free(cos_v71_portal_t *p)
{
    if (!p) return;
    free(p->anchors);
    free(p->bridges);
    free(p->sigs);
    free(p->seeds);
    free(p->labels);
    free(p->secret);
    free(p);
}

int cos_v71_portal_insert(cos_v71_portal_t   *p,
                          const cos_v71_hv_t *anchor,
                          const cos_v71_hv_t *target,
                          uint32_t            label)
{
    if (!p || !anchor || !target) return -1;
    if (p->len >= p->cap)         return -1;

    uint32_t idx = p->len;
    cos_v71_hv_copy(&p->anchors[idx], anchor);
    cos_v71_hv_xor(&p->bridges[idx], anchor, target);

    /* Per-portal signature seed is deterministic: (len << 11) ^ label. */
    uint64_t seed = ((uint64_t)idx << 11) ^ (uint64_t)label;
    p->seeds[idx]  = seed;
    p->labels[idx] = label;

    /* sig = bridge ⊕ permute(secret, seed32). */
    cos_v71_hv_t perm_secret;
    cos_v71_hv_permute(&perm_secret, p->secret, (int32_t)(seed & 0x1FFFu));
    cos_v71_hv_xor(&p->sigs[idx], &p->bridges[idx], &perm_secret);

    p->len++;
    return (int)idx;
}

/* --------------------------------------------------------------------
 *  3.  Anchor match — constant-time cleanup.
 * -------------------------------------------------------------------- */

uint32_t cos_v71_portal_match(const cos_v71_portal_t *p,
                              const cos_v71_hv_t     *q,
                              int32_t                *out_sim_q15)
{
    if (out_sim_q15) *out_sim_q15 = 0;
    if (!p || !q || p->len == 0) return UINT32_MAX;

    uint32_t best_idx = 0;
    uint32_t best_ham = UINT32_MAX;
    for (uint32_t i = 0; i < p->len; ++i) {
        uint32_t h = cos_v71_hv_hamming(&p->anchors[i], q);
        /* Branchless-ish: select lower Hamming. */
        uint32_t is_better = (h < best_ham);
        best_idx = is_better ? i : best_idx;
        best_ham = is_better ? h : best_ham;
    }
    if (out_sim_q15) {
        int32_t d = (int32_t)COS_V71_HV_DIM_BITS - 2 * (int32_t)best_ham;
        *out_sim_q15 = d * 4;
    }
    return best_idx;
}

/* --------------------------------------------------------------------
 *  4.  Teleport — direct-address single-XOR jump.
 * -------------------------------------------------------------------- */

int cos_v71_portal_teleport(const cos_v71_portal_t *p,
                            uint32_t                idx,
                            const cos_v71_hv_t     *q,
                            cos_v71_hv_t           *dst)
{
    if (!p || !q || !dst)    return -1;
    if (idx >= p->len)        return -1;
    cos_v71_hv_xor(dst, q, &p->bridges[idx]);
    return 0;
}

/* --------------------------------------------------------------------
 *  5.  Multi-hop greedy routing (Kleinberg small-world).
 *
 *  At each hop, pick the portal whose *target* (= anchor ⊕ bridge) is
 *  closest in Hamming to `dst`, teleport, repeat.
 *
 *  Note: we compute the teleported position `current ⊕ bridge[i]` and
 *  measure Hamming(that, dst) directly.  Branchless selection; no
 *  early exit inside the per-portal loop (side-channel bounded).
 * -------------------------------------------------------------------- */

void cos_v71_portal_route(const cos_v71_portal_t *p,
                          const cos_v71_hv_t     *src,
                          const cos_v71_hv_t     *dst,
                          int32_t                 arrival_q15,
                          uint32_t                max_hops,
                          cos_v71_hv_t           *out_pos,
                          cos_v71_route_t        *out_route)
{
    if (out_route) memset(out_route, 0, sizeof *out_route);
    if (!p || !src || !dst || !out_pos) {
        if (out_route) out_route->arrived = 0;
        return;
    }

    cos_v71_hv_t current, probe;
    cos_v71_hv_copy(&current, src);

    uint32_t hop_cap = max_hops;
    if (hop_cap > COS_V71_ROUTE_HOPS_MAX) hop_cap = COS_V71_ROUTE_HOPS_MAX;

    int32_t sim = cos_v71_hv_similarity_q15(&current, dst);

    uint32_t hops = 0;
    uint8_t  arrived = (sim >= arrival_q15) ? 1u : 0u;

    while (!arrived && hops < hop_cap && p->len > 0) {
        /* Find best portal: minimise Hamming(current ⊕ bridge[i], dst). */
        uint32_t best_idx = 0;
        uint32_t best_ham = UINT32_MAX;
        for (uint32_t i = 0; i < p->len; ++i) {
            cos_v71_hv_xor(&probe, &current, &p->bridges[i]);
            uint32_t h = cos_v71_hv_hamming(&probe, dst);
            uint32_t is_better = (h < best_ham);
            best_idx = is_better ? i : best_idx;
            best_ham = is_better ? h : best_ham;
        }

        /* Teleport. */
        cos_v71_hv_xor(&current, &current, &p->bridges[best_idx]);

        /* Guarded trail write. */
        if (out_route && hops < COS_V71_ROUTE_HOPS_MAX)
            out_route->trail[hops] = best_idx;

        hops++;
        sim = cos_v71_hv_similarity_q15(&current, dst);
        arrived = (sim >= arrival_q15) ? 1u : 0u;

        /* Bail if no forward progress (local optimum). */
        if (!arrived && best_ham == COS_V71_HV_DIM_BITS) break;
    }

    cos_v71_hv_copy(out_pos, &current);
    if (out_route) {
        out_route->hops           = hops;
        out_route->final_sim_q15  = sim;
        out_route->arrived        = arrived;
    }
}

/* --------------------------------------------------------------------
 *  6.  Tensor-bond wormhole.
 * -------------------------------------------------------------------- */

uint32_t cos_v71_bond_build(const cos_v71_hv_t        *store_a,
                            uint32_t                   len_a,
                            const cos_v71_hv_t        *store_b,
                            uint32_t                   len_b,
                            const cos_v71_bond_pair_t *pairs,
                            uint32_t                   n_pairs,
                            cos_v71_hv_t              *bonds)
{
    if (!store_a || !store_b || !pairs || !bonds) return UINT32_MAX;
    uint32_t total_ham = 0;
    for (uint32_t i = 0; i < n_pairs; ++i) {
        uint32_t ra = pairs[i].row_a;
        uint32_t rb = pairs[i].row_b;
        if (ra >= len_a || rb >= len_b) return UINT32_MAX;
        cos_v71_hv_xor(&bonds[i], &store_a[ra], &store_b[rb]);
        total_ham += cos_v71_hv_popcount(&bonds[i]);
    }
    return total_ham;
}

/* --------------------------------------------------------------------
 *  7.  Integrity verify.
 * -------------------------------------------------------------------- */

uint8_t cos_v71_portal_verify(const cos_v71_portal_t *p,
                              uint32_t                idx,
                              uint32_t                noise_floor_bits)
{
    if (!p || idx >= p->len) return 0u;
    cos_v71_hv_t perm_secret, expected;
    cos_v71_hv_permute(&perm_secret, p->secret,
                       (int32_t)(p->seeds[idx] & 0x1FFFu));
    cos_v71_hv_xor(&expected, &p->bridges[idx], &perm_secret);
    uint32_t h = cos_v71_hv_hamming(&expected, &p->sigs[idx]);
    return (h <= noise_floor_bits) ? 1u : 0u;
}

/* --------------------------------------------------------------------
 *  8.  Hyperbolic / Poincaré-boundary gate.
 * -------------------------------------------------------------------- */

uint8_t cos_v71_boundary_gate(const cos_v71_hv_t *q,
                              uint32_t            boundary_threshold_bits)
{
    if (!q) return 0u;
    uint32_t pc = cos_v71_hv_popcount(q);
    int32_t  skew = (int32_t)pc - (int32_t)(COS_V71_HV_DIM_BITS / 2u);
    if (skew < 0) skew = -skew;
    return ((uint32_t)skew >= boundary_threshold_bits) ? 1u : 0u;
}

/* --------------------------------------------------------------------
 *  9.  Path receipts.
 * -------------------------------------------------------------------- */

int cos_v71_path_receipt(const cos_v71_portal_t *p,
                         const uint32_t         *trail,
                         uint32_t                n_hops,
                         cos_v71_hv_t           *out_receipt)
{
    if (!p || !trail || !out_receipt) return -1;
    cos_v71_hv_zero(out_receipt);
    for (uint32_t i = 0; i < n_hops; ++i) {
        uint32_t idx = trail[i];
        if (idx >= p->len) return -1;
        cos_v71_hv_xor(out_receipt, out_receipt, &p->anchors[idx]);
    }
    return 0;
}

int cos_v71_path_receipt_ordered(const cos_v71_portal_t *p,
                                 const uint32_t         *trail,
                                 uint32_t                n_hops,
                                 cos_v71_hv_t           *out_receipt)
{
    if (!p || !trail || !out_receipt) return -1;
    cos_v71_hv_t perm;
    cos_v71_hv_zero(out_receipt);
    for (uint32_t i = 0; i < n_hops; ++i) {
        uint32_t idx = trail[i];
        if (idx >= p->len) return -1;
        cos_v71_hv_permute(&perm, &p->anchors[idx], (int32_t)i);
        cos_v71_hv_xor(out_receipt, out_receipt, &perm);
    }
    return 0;
}

/* --------------------------------------------------------------------
 * 10.  WHL — Wormhole Language bytecode ISA.
 * -------------------------------------------------------------------- */

struct cos_v71_whl_state {
    cos_v71_hv_t *regs;           /* NREGS × 1 024 B, aligned */
    int32_t       reg_q15[COS_V71_NREGS];
    uint64_t      cost;
    uint32_t      hops;
    uint8_t       integrity_ok;   /* AND of all VERIFY outcomes */
    uint8_t       boundary_ok;    /* AND of all ANCHOR boundary gates */
    uint8_t       abstained;
    uint8_t       v71_ok;
};

static const uint8_t COS_V71_COST[10] = {
    /* HALT     */ 1,
    /* ANCHOR   */ 2,
    /* BIND     */ 2,
    /* TELEPORT */ 4,
    /* ROUTE    */ 16,
    /* BOND     */ 2,
    /* VERIFY   */ 2,
    /* HOP      */ 2,
    /* MEASURE  */ 2,
    /* GATE     */ 1,
};

cos_v71_whl_state_t *cos_v71_whl_new(void)
{
    cos_v71_whl_state_t *s = (cos_v71_whl_state_t *)al64(sizeof *s);
    if (!s) return NULL;
    memset(s, 0, sizeof *s);
    s->regs = (cos_v71_hv_t *)al64((size_t)COS_V71_NREGS * sizeof(cos_v71_hv_t));
    if (!s->regs) { free(s); return NULL; }
    cos_v71_whl_reset(s);
    return s;
}

void cos_v71_whl_free(cos_v71_whl_state_t *s)
{
    if (!s) return;
    free(s->regs);
    free(s);
}

void cos_v71_whl_reset(cos_v71_whl_state_t *s)
{
    if (!s) return;
    memset(s->regs, 0, (size_t)COS_V71_NREGS * sizeof(cos_v71_hv_t));
    memset(s->reg_q15, 0, sizeof s->reg_q15);
    s->cost = 0;
    s->hops = 0;
    s->integrity_ok = 1u;   /* vacuously true until first VERIFY fails */
    s->boundary_ok  = 1u;   /* vacuously true until first ANCHOR fails */
    s->abstained    = 0u;
    s->v71_ok       = 0u;
}

uint64_t cos_v71_whl_cost(const cos_v71_whl_state_t *s)        { return s ? s->cost : 0; }
int32_t  cos_v71_whl_reg_q15(const cos_v71_whl_state_t *s, uint32_t i)
{
    if (!s || i >= COS_V71_NREGS) return 0;
    return s->reg_q15[i];
}
uint32_t cos_v71_whl_hops(const cos_v71_whl_state_t *s)        { return s ? s->hops : 0; }
uint8_t  cos_v71_whl_integrity_ok(const cos_v71_whl_state_t *s){ return s ? s->integrity_ok : 0; }
uint8_t  cos_v71_whl_boundary_ok(const cos_v71_whl_state_t *s) { return s ? s->boundary_ok  : 0; }
uint8_t  cos_v71_whl_abstained(const cos_v71_whl_state_t *s)   { return s ? s->abstained : 0; }
uint8_t  cos_v71_whl_v71_ok(const cos_v71_whl_state_t *s)      { return s ? s->v71_ok : 0; }

int cos_v71_whl_exec(cos_v71_whl_state_t    *s,
                     const cos_v71_inst_t   *prog,
                     uint32_t                n,
                     cos_v71_whl_ctx_t      *ctx,
                     uint64_t                cost_budget,
                     uint32_t                hop_budget)
{
    if (!s || !prog || !ctx) return -1;
    if (n > COS_V71_PROG_MAX) return -1;

    /* External abstain signal from ctx. */
    if (ctx->abstain) s->abstained = 1u;

    for (uint32_t pc = 0; pc < n; ++pc) {
        const cos_v71_inst_t *in = &prog[pc];
        uint8_t op = in->op;
        if (op > COS_V71_OP_GATE) return -1;

        s->cost += COS_V71_COST[op];
        if (s->cost > cost_budget) s->abstained = 1u;

        uint8_t dst = in->dst, a = in->a, b = in->b;
        if (dst >= COS_V71_NREGS || a >= COS_V71_NREGS || b >= COS_V71_NREGS)
            return -1;

        switch (op) {
        case COS_V71_OP_HALT:
            goto halt;

        case COS_V71_OP_ANCHOR: {
            if (!ctx->anchor_src) { s->abstained = 1u; break; }
            cos_v71_hv_copy(&s->regs[dst], ctx->anchor_src);
            /* Boundary-regime gate using the default threshold unless imm > 0. */
            uint32_t thr = (in->imm > 0)
                         ? (uint32_t)in->imm
                         : COS_V71_BOUNDARY_DEFAULT_BITS;
            uint8_t ok = cos_v71_boundary_gate(ctx->anchor_src, thr);
            s->boundary_ok = s->boundary_ok & ok;
            break;
        }

        case COS_V71_OP_BIND:
        case COS_V71_OP_BOND:
            cos_v71_hv_xor(&s->regs[dst], &s->regs[a], &s->regs[b]);
            break;

        case COS_V71_OP_TELEPORT: {
            if (!ctx->portal) { s->abstained = 1u; break; }
            uint32_t idx = (uint32_t)(uint16_t)in->imm;
            if (idx >= ctx->portal->len) { s->abstained = 1u; break; }
            cos_v71_hv_xor(&s->regs[dst], &s->regs[a],
                           &ctx->portal->bridges[idx]);
            break;
        }

        case COS_V71_OP_ROUTE: {
            if (!ctx->portal) { s->abstained = 1u; break; }
            cos_v71_route_t r;
            cos_v71_portal_route(ctx->portal,
                                 &s->regs[a], &s->regs[b],
                                 ctx->route_arrival_q15,
                                 ctx->route_max_hops
                                   ? ctx->route_max_hops
                                   : hop_budget,
                                 &s->regs[dst],
                                 &r);
            s->hops += r.hops;
            s->reg_q15[dst] = r.final_sim_q15;
            /* If routing abstained-equivalent (did not arrive), flip
             * abstain only if the program GATE depends on arrival. */
            if (!r.arrived) { /* soft: arrival is checked in GATE */ }
            break;
        }

        case COS_V71_OP_VERIFY: {
            if (!ctx->portal) { s->abstained = 1u; break; }
            uint32_t idx = (uint32_t)(uint16_t)in->imm;
            uint8_t  ok  = cos_v71_portal_verify(ctx->portal, idx,
                                                 ctx->verify_noise_floor_bits);
            s->integrity_ok = s->integrity_ok & ok;
            break;
        }

        case COS_V71_OP_HOP:
            s->hops++;
            if (s->hops > hop_budget) s->abstained = 1u;
            break;

        case COS_V71_OP_MEASURE: {
            int32_t sim = cos_v71_hv_similarity_q15(&s->regs[a], &s->regs[b]);
            s->reg_q15[dst] = sim;
            break;
        }

        case COS_V71_OP_GATE: {
            uint8_t cost_ok      = (s->cost         <= cost_budget) ? 1u : 0u;
            uint8_t hops_ok      = (s->hops         <= hop_budget)  ? 1u : 0u;
            uint8_t arrival_ok   = (s->reg_q15[a]   >= (int32_t)in->imm) ? 1u : 0u;
            uint8_t not_abstain  = (s->abstained    == 0u) ? 1u : 0u;
            s->v71_ok = cost_ok
                      & hops_ok
                      & s->integrity_ok
                      & s->boundary_ok
                      & arrival_ok
                      & not_abstain;
            break;
        }
        }
    }

halt:
    if (s->cost > cost_budget) return -2;
    return 0;
}

/* --------------------------------------------------------------------
 *  Composed 12-bit branchless decision.
 * -------------------------------------------------------------------- */

cos_v71_decision_t cos_v71_compose_decision(uint8_t v60_ok,
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
                                            uint8_t v71_ok)
{
    cos_v71_decision_t d;
    d.v60_ok = v60_ok & 1u;
    d.v61_ok = v61_ok & 1u;
    d.v62_ok = v62_ok & 1u;
    d.v63_ok = v63_ok & 1u;
    d.v64_ok = v64_ok & 1u;
    d.v65_ok = v65_ok & 1u;
    d.v66_ok = v66_ok & 1u;
    d.v67_ok = v67_ok & 1u;
    d.v68_ok = v68_ok & 1u;
    d.v69_ok = v69_ok & 1u;
    d.v70_ok = v70_ok & 1u;
    d.v71_ok = v71_ok & 1u;
    d.allow  = d.v60_ok & d.v61_ok & d.v62_ok & d.v63_ok
             & d.v64_ok & d.v65_ok & d.v66_ok & d.v67_ok
             & d.v68_ok & d.v69_ok & d.v70_ok & d.v71_ok;
    d._pad[0] = d._pad[1] = d._pad[2] = 0;
    return d;
}

/* --------------------------------------------------------------------
 *  Version string.
 * -------------------------------------------------------------------- */

const char cos_v71_version[] =
    "v71.0 σ-Wormhole — hyperdimensional wormhole / portal-cognition plane "
    "(10 kernels: portal ER-bridge + anchor-cleanup + teleport + Kleinberg "
    "small-world route + ER=EPR tensor-bond + HMAC-HV integrity + "
    "Poincaré-boundary gate + hop budget + path receipt + WHL 10-op "
    "bytecode; 12-bit composed decision).  1 = 1.";
