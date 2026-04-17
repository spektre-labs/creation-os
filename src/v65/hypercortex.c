/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v65 — σ-Hypercortex implementation.
 *
 * Branchless, integer-only, dependency-free C11.  On the hot path:
 *
 *   - Hamming / similarity:  loop of 256 × uint64_t XOR +
 *                            __builtin_popcountll  (NEON `cnt` +
 *                            horizontal add on AArch64).
 *   - Bind:                  wordwise XOR, 256 uint64_t per call.
 *   - Permute:               cyclic bit rotation, wordwise.
 *   - Bundle (majority):     integer tally of length D with a
 *                            deterministic tie-breaker hypervector.
 *   - Cleanup sweep:         constant-time per capacity (no early
 *                            exit, no FP), branchless argmin update.
 *   - HVL dispatch:          single switch; every opcode is O(D/64).
 *
 * Zero floating point anywhere.  No heap allocation on the read path.
 * All caller-owned arenas are aligned_alloc(64, ...) (M4 cache line).
 */

#include "hypercortex.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------
 *  Utility: aligned arena allocation.  Matches v64's convention —
 *  always 64-byte aligned; exact bytes rounded up to a 64-byte
 *  multiple as required by aligned_alloc.
 * ------------------------------------------------------------------ */
static void *v65_aligned(size_t bytes)
{
    size_t aligned = (bytes + 63u) & ~(size_t)63u;
    return aligned_alloc(64, aligned);
}

/* ------------------------------------------------------------------
 *  splitmix64 — deterministic, seed-expanding PRNG used to fill
 *  pseudorandom hypervectors.  No cryptographic claim: just good
 *  equidistribution so the generated HVs are approximately orthogonal
 *  (expected similarity ≈ 0 ± √D).
 * ------------------------------------------------------------------ */
static inline uint64_t splitmix64(uint64_t *x)
{
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* ====================================================================
 *  6-bit composed decision — branchless AND of six lanes.
 * ==================================================================== */

cos_v65_decision_t
cos_v65_compose_decision(uint8_t v60_ok, uint8_t v61_ok, uint8_t v62_ok,
                         uint8_t v63_ok, uint8_t v64_ok, uint8_t v65_ok)
{
    cos_v65_decision_t d;
    d.v60_ok = v60_ok & 1u;
    d.v61_ok = v61_ok & 1u;
    d.v62_ok = v62_ok & 1u;
    d.v63_ok = v63_ok & 1u;
    d.v64_ok = v64_ok & 1u;
    d.v65_ok = v65_ok & 1u;
    d.allow  = (uint8_t)(d.v60_ok & d.v61_ok & d.v62_ok &
                         d.v63_ok & d.v64_ok & d.v65_ok);
    d._pad   = 0;
    return d;
}

/* ====================================================================
 *  Primitive hypervector ops.
 * ==================================================================== */

void cos_v65_hv_zero(cos_v65_hv_t *dst)
{
    if (!dst) return;
    memset(dst->w, 0, sizeof(dst->w));
}

void cos_v65_hv_copy(cos_v65_hv_t *dst, const cos_v65_hv_t *src)
{
    if (!dst || !src) return;
    memcpy(dst->w, src->w, sizeof(dst->w));
}

void cos_v65_hv_from_seed(cos_v65_hv_t *dst, uint64_t seed)
{
    if (!dst) return;
    uint64_t s = seed ? seed : 0xD1B54A32D192ED03ULL;
    for (uint32_t i = 0; i < COS_V65_HV_DIM_WORDS; ++i) {
        dst->w[i] = splitmix64(&s);
    }
}

void cos_v65_hv_bind(cos_v65_hv_t       *dst,
                     const cos_v65_hv_t *a,
                     const cos_v65_hv_t *b)
{
    if (!dst || !a || !b) return;
    for (uint32_t i = 0; i < COS_V65_HV_DIM_WORDS; ++i) {
        dst->w[i] = a->w[i] ^ b->w[i];
    }
}

uint32_t cos_v65_hv_hamming(const cos_v65_hv_t *a, const cos_v65_hv_t *b)
{
    if (!a || !b) return 0;
    uint32_t h0 = 0, h1 = 0, h2 = 0, h3 = 0;
    for (uint32_t i = 0; i < COS_V65_HV_DIM_WORDS; i += 4) {
        h0 += (uint32_t)__builtin_popcountll(a->w[i + 0] ^ b->w[i + 0]);
        h1 += (uint32_t)__builtin_popcountll(a->w[i + 1] ^ b->w[i + 1]);
        h2 += (uint32_t)__builtin_popcountll(a->w[i + 2] ^ b->w[i + 2]);
        h3 += (uint32_t)__builtin_popcountll(a->w[i + 3] ^ b->w[i + 3]);
    }
    return h0 + h1 + h2 + h3;
}

int32_t cos_v65_hv_similarity_q15(const cos_v65_hv_t *a, const cos_v65_hv_t *b)
{
    uint32_t h = cos_v65_hv_hamming(a, b);
    int32_t  diff = (int32_t)COS_V65_HV_DIM_BITS - 2 * (int32_t)h;
    /* D = 16384, so (D − 2H) * (32768 / D) = (D − 2H) * 2. */
    return diff * 2;
}

void cos_v65_hv_permute(cos_v65_hv_t *dst, const cos_v65_hv_t *src, int32_t shift)
{
    if (!dst || !src) return;
    const int32_t D = (int32_t)COS_V65_HV_DIM_BITS;
    int32_t r = shift % D;
    if (r < 0) r += D;
    uint32_t k       = (uint32_t)r;
    uint32_t wshift  = k / 64u;
    uint32_t bshift  = k % 64u;
    const uint32_t W = COS_V65_HV_DIM_WORDS;

    if (bshift == 0u) {
        for (uint32_t i = 0; i < W; ++i) {
            uint32_t src_i = (i + W - wshift) % W;
            dst->w[i] = src->w[src_i];
        }
        return;
    }
    for (uint32_t i = 0; i < W; ++i) {
        uint32_t i0 = (i + W - wshift)        % W;
        uint32_t i1 = (i + W - wshift - 1u)   % W;
        uint64_t lo = src->w[i0] << bshift;
        uint64_t hi = src->w[i1] >> (64u - bshift);
        dst->w[i] = lo | hi;
    }
}

int cos_v65_hv_bundle_majority(cos_v65_hv_t             *dst,
                               const cos_v65_hv_t *const *srcs,
                               uint32_t                   n,
                               int16_t                   *scratch)
{
    if (!dst || !srcs || !scratch || n == 0u) return -1;
    memset(scratch, 0, sizeof(int16_t) * COS_V65_HV_DIM_BITS);

    /* Integer tally: +1 for each set bit across sources. */
    for (uint32_t s = 0; s < n; ++s) {
        const uint64_t *w = srcs[s]->w;
        for (uint32_t wi = 0; wi < COS_V65_HV_DIM_WORDS; ++wi) {
            uint64_t x = w[wi];
            uint32_t base = wi * 64u;
            /* Unrolled per-bit: clang lowers well on AArch64. */
            for (uint32_t bi = 0; bi < 64u; ++bi) {
                scratch[base + bi] += (int16_t)((x >> bi) & 1ULL);
            }
        }
    }

    /* Deterministic tie-breaker hypervector (only used when n is even
     * and tally*2 == n for some bit position). */
    cos_v65_hv_t tie;
    cos_v65_hv_from_seed(&tie, 0xDEADBEEFCAFEBABEULL ^ (uint64_t)n);

    const int32_t two_n = (int32_t)(n * 2u);
    for (uint32_t wi = 0; wi < COS_V65_HV_DIM_WORDS; ++wi) {
        uint64_t out  = 0;
        uint32_t base = wi * 64u;
        for (uint32_t bi = 0; bi < 64u; ++bi) {
            int32_t t2 = 2 * (int32_t)scratch[base + bi];
            /* gt=1 if t*2>n, eq=1 if t*2==n, else both 0 → clear. */
            uint64_t gt  = (uint64_t)(t2 >  two_n);
            uint64_t eq  = (uint64_t)(t2 == two_n);
            uint64_t tb  = (tie.w[wi] >> bi) & 1ULL;
            uint64_t bit = gt | (eq & tb);
            out |= (bit << bi);
        }
        dst->w[wi] = out;
    }
    return 0;
}

/* ====================================================================
 *  Cleanup memory.
 * ==================================================================== */

cos_v65_cleanup_t *cos_v65_cleanup_new(uint32_t cap)
{
    if (cap == 0u) return NULL;
    cos_v65_cleanup_t *m = (cos_v65_cleanup_t *)calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->items    = (cos_v65_hv_t *)v65_aligned((size_t)cap * sizeof(cos_v65_hv_t));
    m->labels   = (uint32_t     *)v65_aligned((size_t)cap * sizeof(uint32_t));
    m->conf_q15 = (uint16_t     *)v65_aligned((size_t)cap * sizeof(uint16_t));
    if (!m->items || !m->labels || !m->conf_q15) {
        cos_v65_cleanup_free(m);
        return NULL;
    }
    m->cap = cap;
    m->len = 0;
    return m;
}

void cos_v65_cleanup_free(cos_v65_cleanup_t *m)
{
    if (!m) return;
    free(m->items);
    free(m->labels);
    free(m->conf_q15);
    free(m);
}

int cos_v65_cleanup_insert(cos_v65_cleanup_t  *m,
                           const cos_v65_hv_t *hv,
                           uint32_t            label,
                           uint16_t            conf_q15)
{
    if (!m || !hv) return -1;
    if (m->len >= m->cap) return -1;
    memcpy(&m->items[m->len], hv, sizeof(*hv));
    m->labels[m->len]   = label;
    m->conf_q15[m->len] = conf_q15;
    return (int)(m->len++);
}

uint32_t cos_v65_cleanup_query(const cos_v65_cleanup_t *m,
                               const cos_v65_hv_t      *q,
                               uint32_t                *out_label,
                               int32_t                 *out_sim_q15)
{
    if (!m || !q || !out_label || !out_sim_q15 || m->len == 0u) {
        if (out_label)   *out_label   = UINT32_MAX;
        if (out_sim_q15) *out_sim_q15 = 0;
        return UINT32_MAX;
    }

    uint32_t best_idx = 0;
    uint32_t best_h   = cos_v65_hv_hamming(q, &m->items[0]);
    for (uint32_t i = 1; i < m->len; ++i) {
        uint32_t h      = cos_v65_hv_hamming(q, &m->items[i]);
        uint32_t better = (uint32_t)(h < best_h);
        /* Branchless select (ternary lowers to cmov/csel). */
        best_idx = better ? i : best_idx;
        best_h   = better ? h : best_h;
    }
    int32_t diff = (int32_t)COS_V65_HV_DIM_BITS - 2 * (int32_t)best_h;
    *out_sim_q15 = diff * 2;
    *out_label   = m->labels[best_idx];
    return best_idx;
}

/* ====================================================================
 *  Record / role-filler.
 * ==================================================================== */

int cos_v65_record_build(cos_v65_hv_t          *dst,
                         const cos_v65_pair_t  *pairs,
                         uint32_t               n,
                         cos_v65_hv_t          *scratch_hv,
                         int16_t               *scratch_tally)
{
    if (!dst || !pairs || !scratch_hv || !scratch_tally || n == 0u) return -1;
    /* Stage A: bind each (role, filler) → scratch_hv[i]. */
    for (uint32_t i = 0; i < n; ++i) {
        cos_v65_hv_bind(&scratch_hv[i], pairs[i].role, pairs[i].filler);
    }
    /* Stage B: bundle by majority. */
    const cos_v65_hv_t *ptrs_stack[16];
    const cos_v65_hv_t **ptrs = ptrs_stack;
    const cos_v65_hv_t **heap = NULL;
    if (n > 16u) {
        heap = (const cos_v65_hv_t **)malloc(sizeof(*heap) * n);
        if (!heap) return -1;
        ptrs = heap;
    }
    for (uint32_t i = 0; i < n; ++i) ptrs[i] = &scratch_hv[i];
    int rc = cos_v65_hv_bundle_majority(dst, ptrs, n, scratch_tally);
    if (heap) free(heap);
    return rc;
}

void cos_v65_record_unbind(cos_v65_hv_t       *dst,
                           const cos_v65_hv_t *record,
                           const cos_v65_hv_t *role)
{
    cos_v65_hv_bind(dst, record, role);
}

/* ====================================================================
 *  Analogy.
 * ==================================================================== */

uint32_t cos_v65_analogy(const cos_v65_cleanup_t *mem,
                         const cos_v65_hv_t      *A,
                         const cos_v65_hv_t      *B,
                         const cos_v65_hv_t      *C,
                         cos_v65_hv_t            *scratch,
                         uint32_t                *out_label,
                         int32_t                 *out_sim_q15)
{
    if (!mem || !A || !B || !C || !scratch || !out_label || !out_sim_q15) {
        if (out_label)   *out_label   = UINT32_MAX;
        if (out_sim_q15) *out_sim_q15 = 0;
        return UINT32_MAX;
    }
    /* query = A ⊗ B ⊗ C  (XOR is associative). */
    cos_v65_hv_t tmp;
    cos_v65_hv_bind(&tmp,     A, B);
    cos_v65_hv_bind(scratch,  &tmp, C);
    return cos_v65_cleanup_query(mem, scratch, out_label, out_sim_q15);
}

/* ====================================================================
 *  Sequence memory.
 * ==================================================================== */

int cos_v65_sequence_build(cos_v65_hv_t              *dst,
                           const cos_v65_hv_t *const *items,
                           uint32_t                   n,
                           int32_t                    base_shift,
                           cos_v65_hv_t              *scratch_hv,
                           int16_t                   *scratch_tally)
{
    if (!dst || !items || !scratch_hv || !scratch_tally || n == 0u) return -1;
    for (uint32_t i = 0; i < n; ++i) {
        cos_v65_hv_permute(&scratch_hv[i], items[i],
                           (int32_t)i * base_shift);
    }
    const cos_v65_hv_t *ptrs_stack[16];
    const cos_v65_hv_t **ptrs = ptrs_stack;
    const cos_v65_hv_t **heap = NULL;
    if (n > 16u) {
        heap = (const cos_v65_hv_t **)malloc(sizeof(*heap) * n);
        if (!heap) return -1;
        ptrs = heap;
    }
    for (uint32_t i = 0; i < n; ++i) ptrs[i] = &scratch_hv[i];
    int rc = cos_v65_hv_bundle_majority(dst, ptrs, n, scratch_tally);
    if (heap) free(heap);
    return rc;
}

void cos_v65_sequence_at(cos_v65_hv_t       *dst,
                         const cos_v65_hv_t *seq,
                         uint32_t            pos,
                         int32_t             base_shift)
{
    if (!dst || !seq) return;
    cos_v65_hv_permute(dst, seq, -((int32_t)pos) * base_shift);
}

/* ====================================================================
 *  HVL — HyperVector Language bytecode interpreter.
 * ==================================================================== */

cos_v65_hvl_state_t *cos_v65_hvl_new(uint8_t nreg)
{
    if (nreg == 0u) return NULL;
    cos_v65_hvl_state_t *s = (cos_v65_hvl_state_t *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->regs          = (cos_v65_hv_t *)v65_aligned((size_t)nreg * sizeof(cos_v65_hv_t));
    s->scratch_hv    = (cos_v65_hv_t *)v65_aligned(sizeof(cos_v65_hv_t));
    s->scratch_tally = (int16_t *)v65_aligned((size_t)COS_V65_HV_DIM_BITS * sizeof(int16_t));
    if (!s->regs || !s->scratch_hv || !s->scratch_tally) {
        cos_v65_hvl_free(s);
        return NULL;
    }
    s->nreg = nreg;
    memset(s->regs, 0, (size_t)nreg * sizeof(cos_v65_hv_t));
    return s;
}

void cos_v65_hvl_free(cos_v65_hvl_state_t *s)
{
    if (!s) return;
    free(s->regs);
    free(s->scratch_hv);
    free(s->scratch_tally);
    free(s);
}

/* Cost accounting is measured in 64-bit popcount words consumed by
 * the op: Hamming / bind / permute over one hypervector = W words. */
#define V65_COST_OP_WORDS   ((uint64_t)COS_V65_HV_DIM_WORDS)

int cos_v65_hvl_exec(cos_v65_hvl_state_t       *s,
                     const cos_v65_hv_t *const *arena,
                     uint32_t                   arena_len,
                     const cos_v65_cleanup_t   *mem,
                     const cos_v65_inst_t      *prog,
                     uint32_t                   nprog,
                     uint64_t                   cost_budget)
{
    if (!s || !prog) return -1;

    s->halted     = 0;
    s->flag       = 0;
    s->v65_ok     = 0;
    s->sim_q15    = 0;
    s->label      = UINT32_MAX;
    s->steps      = 0;
    s->cost_words = 0;

    for (uint32_t pc = 0; pc < nprog; ++pc) {
        const cos_v65_inst_t *ins = &prog[pc];
        s->steps++;

        switch (ins->op) {
        case COS_V65_OP_HALT:
            s->halted = 1;
            return 0;

        case COS_V65_OP_LOAD: {
            if (ins->dst >= s->nreg) return -1;
            uint32_t idx = (uint32_t)(uint16_t)ins->imm;
            if (!arena || idx >= arena_len || !arena[idx]) return -1;
            cos_v65_hv_copy(&s->regs[ins->dst], arena[idx]);
            s->cost_words += V65_COST_OP_WORDS;
            break;
        }

        case COS_V65_OP_BIND: {
            if (ins->dst >= s->nreg || ins->a >= s->nreg || ins->b >= s->nreg)
                return -1;
            cos_v65_hv_bind(&s->regs[ins->dst],
                            &s->regs[ins->a], &s->regs[ins->b]);
            s->cost_words += V65_COST_OP_WORDS;
            break;
        }

        case COS_V65_OP_BUNDLE: {
            /* imm holds the third source register. */
            uint8_t c = (uint8_t)(ins->imm & 0xFF);
            if (ins->dst >= s->nreg || ins->a >= s->nreg ||
                ins->b >= s->nreg   || c >= s->nreg)
                return -1;
            const cos_v65_hv_t *srcs[3] = {
                &s->regs[ins->a], &s->regs[ins->b], &s->regs[c]
            };
            int rc = cos_v65_hv_bundle_majority(&s->regs[ins->dst],
                                                srcs, 3u, s->scratch_tally);
            if (rc != 0) return -1;
            s->cost_words += 3ull * V65_COST_OP_WORDS;
            break;
        }

        case COS_V65_OP_PERM: {
            if (ins->dst >= s->nreg || ins->a >= s->nreg) return -1;
            cos_v65_hv_permute(&s->regs[ins->dst],
                               &s->regs[ins->a], (int32_t)ins->imm);
            s->cost_words += V65_COST_OP_WORDS;
            break;
        }

        case COS_V65_OP_LOOKUP: {
            if (ins->a >= s->nreg) return -1;
            if (!mem) return -1;
            uint32_t lbl = 0;
            int32_t  sim = 0;
            cos_v65_cleanup_query(mem, &s->regs[ins->a], &lbl, &sim);
            s->label   = lbl;
            s->sim_q15 = sim;
            s->cost_words += (uint64_t)mem->len * V65_COST_OP_WORDS;
            break;
        }

        case COS_V65_OP_SIM: {
            if (ins->a >= s->nreg || ins->b >= s->nreg) return -1;
            s->sim_q15 = cos_v65_hv_similarity_q15(&s->regs[ins->a],
                                                   &s->regs[ins->b]);
            s->cost_words += V65_COST_OP_WORDS;
            break;
        }

        case COS_V65_OP_CMPGE:
            s->flag = (uint8_t)(s->sim_q15 >= (int32_t)ins->imm);
            break;

        case COS_V65_OP_GATE: {
            uint8_t under_budget =
                (uint8_t)(s->cost_words <= cost_budget);
            s->v65_ok = (uint8_t)(s->flag & under_budget);
            break;
        }

        default:
            return -1;
        }

        if (s->cost_words > cost_budget) {
            /* Over-budget: GATE will refuse, but signal the caller too. */
            s->v65_ok = 0;
            return -2;
        }
    }
    /* Fell off the end without a HALT.  Still legal; caller can
     * inspect steps / v65_ok. */
    return 0;
}

const char cos_v65_version[] =
    "65.0.0 — v65.0 sigma-hypercortex "
    "(bipolar-HDC + VSA bind/bundle/permute + cleanup + HVL)";
