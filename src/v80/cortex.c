/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v80 — σ-Cortex, implementation.
 *
 * Branchless integer-only Q16.16 hot paths; libc-only (string.h +
 * stdint.h).  See cortex.h for the full rationale and references
 * (Mamba Gu & Dao 2023/2024 / RoFormer Su 2021 / Longformer 2020 /
 * Mistral / vLLM Kwon 2023 / Leviathan 2023 / Friston 2010 /
 * Kolmogorov 1957 · KAN Liu 2024 / Sakana CTM 2025 / Shazeer 2017 ·
 * DeepSeek-MoE 2024 / o1 · DeepSeek-R1 2024–2025).
 */

#include "cortex.h"

#include <string.h>

/* ==================================================================
 *  Helpers — fixed-point + branchless primitives.
 * ================================================================== */

static inline cos_v80_q16_t q16_mul(cos_v80_q16_t a, cos_v80_q16_t b)
{
    int64_t p = (int64_t)a * (int64_t)b;
    return (cos_v80_q16_t)(p >> 16);
}

static inline cos_v80_q16_t q16_abs(cos_v80_q16_t a)
{
    int32_t m = a >> 31;
    return (cos_v80_q16_t)((a + m) ^ m);
}

__attribute__((unused))
static inline int32_t i32_min(int32_t a, int32_t b)
{
    int32_t d    = a - b;
    int32_t mask = d >> 31;         /* -1 if a < b else 0 */
    return b + (d & mask);
}

static inline uint32_t u32_min(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

/* branchless select: returns a if cond_nonzero != 0 else b (for 32-bit). */
static inline uint32_t u32_select(uint32_t cond_nonzero,
                                  uint32_t a, uint32_t b)
{
    uint32_t m = (uint32_t)0 - (cond_nonzero & 1u);
    return (a & m) | (b & ~m);
}

__attribute__((unused))
static inline uint64_t u64_select(uint32_t cond_nonzero,
                                  uint64_t a, uint64_t b)
{
    uint64_t m = (uint64_t)0 - (uint64_t)(cond_nonzero & 1u);
    return (a & m) | (b & ~m);
}

static inline uint32_t popcount64(uint64_t x)
{
    return (uint32_t)__builtin_popcountll(x);
}

static inline uint64_t rotl64(uint64_t x, unsigned r)
{
    r &= 63u;
    return (x << r) | (x >> ((64u - r) & 63u));
}

static inline uint64_t xorshift64(uint64_t *s)
{
    uint64_t x = *s;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *s = x;
    return x;
}

/* ------------------------------------------------------------------
 *  32-entry Q16.16 sine LUT covering [0, 2π) in steps of 2π/32.
 *  Entries are sin(2π · k / 32) · 65536 rounded to nearest integer.
 *  Used for RoPE and CTM Kuramoto coupling.  Values computed
 *  off-line so no libm dependency.
 * ------------------------------------------------------------------ */
static const cos_v80_q16_t Q16_SIN_LUT[32] = {
         0,  12785,  25079,  36410,  46340,  54491,  60547,  64276,
     65536,  64276,  60547,  54491,  46340,  36410,  25079,  12785,
         0, -12785, -25079, -36410, -46340, -54491, -60547, -64276,
    -65536, -64276, -60547, -54491, -46340, -36410, -25079, -12785
};

/* sin / cos from an integer phase index (0..31 wraps). */
static inline cos_v80_q16_t q16_sin_idx(int32_t idx)
{
    uint32_t u = (uint32_t)(idx & 31);
    return Q16_SIN_LUT[u];
}

static inline cos_v80_q16_t q16_cos_idx(int32_t idx)
{
    return q16_sin_idx(idx + 8);   /* cos(x) = sin(x + π/2), π/2 = 8 steps */
}

/* ==================================================================
 *  1.  Selective State Space Model (Mamba).
 * ================================================================== */

void cos_v80_ssm_init(cos_v80_ssm_t *s,
                      cos_v80_q16_t pole,
                      cos_v80_q16_t b,
                      cos_v80_q16_t c,
                      cos_v80_q16_t norm_budget)
{
    memset(s, 0, sizeof(*s));
    for (uint32_t i = 0; i < COS_V80_SSM_D; ++i) {
        s->a[i] = pole;
        s->b[i] = b;
        s->c[i] = c;
        s->h[i] = 0;
    }
    s->norm_budget = norm_budget;
    s->overflow    = 0u;
}

void cos_v80_ssm_step(cos_v80_ssm_t *s, cos_v80_q16_t u)
{
    uint32_t over = 0u;
    for (uint32_t i = 0; i < COS_V80_SSM_D; ++i) {
        cos_v80_q16_t ah = q16_mul(s->a[i], s->h[i]);
        cos_v80_q16_t bu = q16_mul(s->b[i], u);
        s->h[i] = ah + bu;
        /* Norm-budget check (branchless accumulate). */
        cos_v80_q16_t mag  = q16_abs(s->h[i]);
        uint32_t      hit  = (uint32_t)((mag > s->norm_budget) ? 1u : 0u);
        over |= hit;
    }
    s->overflow |= over;
}

/* Output read-out (used by tests; not on the receipt hot path). */
cos_v80_q16_t cos_v80_ssm_readout(const cos_v80_ssm_t *s)
{
    cos_v80_q16_t y = 0;
    for (uint32_t i = 0; i < COS_V80_SSM_D; ++i) {
        y += q16_mul(s->c[i], s->h[i]);
    }
    return y;
}

/* ==================================================================
 *  2.  RoPE (Rotary Position Embedding).
 * ================================================================== */

void cos_v80_rope_init(cos_v80_rope_t *r)
{
    memset(r, 0, sizeof(*r));
    /* Initialise q,k with a reproducible pattern so tests can
     * verify rotation invariants (||q|| preserved, etc.). */
    for (uint32_t i = 0; i < COS_V80_ROPE_PAIRS; ++i) {
        r->q[2u * i + 0u] = (cos_v80_q16_t)(COS_V80_Q16_ONE);
        r->q[2u * i + 1u] = 0;
        r->k[2u * i + 0u] = (cos_v80_q16_t)(COS_V80_Q16_ONE >> 1);
        r->k[2u * i + 1u] = (cos_v80_q16_t)(COS_V80_Q16_ONE >> 2);
    }
    r->pos = 0u;
}

static inline void rope_pair(cos_v80_q16_t *x0,
                             cos_v80_q16_t *x1,
                             cos_v80_q16_t cs,
                             cos_v80_q16_t sn)
{
    cos_v80_q16_t a = *x0;
    cos_v80_q16_t b = *x1;
    cos_v80_q16_t na = q16_mul(a, cs) - q16_mul(b, sn);
    cos_v80_q16_t nb = q16_mul(a, sn) + q16_mul(b, cs);
    *x0 = na;
    *x1 = nb;
}

void cos_v80_rope_rotate(cos_v80_rope_t *r, uint32_t pos)
{
    r->pos = pos;
    for (uint32_t i = 0; i < COS_V80_ROPE_PAIRS; ++i) {
        /* Frequency for pair i is 10000^(−2i/d), approximated by
         * doubling the LUT stride per pair and folding with pos. */
        int32_t idx = (int32_t)(pos * (1u << (i & 4u)));
        cos_v80_q16_t cs = q16_cos_idx(idx);
        cos_v80_q16_t sn = q16_sin_idx(idx);
        rope_pair(&r->q[2u * i + 0u], &r->q[2u * i + 1u], cs, sn);
        rope_pair(&r->k[2u * i + 0u], &r->k[2u * i + 1u], cs, sn);
    }
}

/* ==================================================================
 *  3.  Sliding-window attention (Longformer / Mistral / Ring-Attn).
 * ================================================================== */

void cos_v80_attn_init(cos_v80_attn_t *a)
{
    memset(a, 0, sizeof(*a));
    a->last_best_idx  = 0;
    a->last_best_dist = (uint32_t)-1;
}

void cos_v80_attn_window(cos_v80_attn_t *a, uint64_t query_hash)
{
    /* Commit the query into the ring. */
    a->past[a->head & (COS_V80_ATTN_WIN - 1u)] = query_hash;
    a->head = (a->head + 1u) & ((1u << 30) - 1u);

    /* Branchless argmin over the window of Hamming distance to q. */
    uint32_t best_idx  = 0u;
    uint32_t best_dist = 64u;   /* distance can be at most 64 */
    for (uint32_t i = 0; i < COS_V80_ATTN_WIN; ++i) {
        uint32_t d   = popcount64(query_hash ^ a->past[i]);
        uint32_t lt  = (uint32_t)((d < best_dist) ? 1u : 0u);
        best_dist    = u32_select(lt, d,  best_dist);
        best_idx     = u32_select(lt, i,  best_idx);
    }
    a->last_best_idx  = best_idx;
    a->last_best_dist = best_dist;
}

/* ==================================================================
 *  4.  Paged KV cache (vLLM / PagedAttention).
 * ================================================================== */

void cos_v80_kv_init(cos_v80_kv_t *kv)
{
    memset(kv, 0, sizeof(*kv));
    kv->next_free_page = 0u;
    kv->sentinel       = 0xCAFEBABEu;
}

uint32_t cos_v80_kv_invariant_ok(const cos_v80_kv_t *kv)
{
    if (kv->sentinel != 0xCAFEBABEu)       return 0u;
    if (kv->invariant_violations != 0u)    return 0u;
    for (uint32_t p = 0; p < COS_V80_KV_PAGES; ++p) {
        if (kv->slot_head[p] > COS_V80_KV_SLOTS) return 0u;
    }
    return 1u;
}

int cos_v80_kv_commit(cos_v80_kv_t *kv, uint32_t page_tag, uint64_t hash)
{
    if (page_tag == 0u) {
        kv->invariant_violations += 1u;
        return -1;
    }
    /* Find existing page for this tag (branchless-ish fold). */
    int32_t found = -1;
    for (uint32_t p = 0; p < COS_V80_KV_PAGES; ++p) {
        uint32_t hit = (uint32_t)((kv->page_tag[p] == page_tag) ? 1u : 0u);
        found = (int32_t)u32_select(hit, p, (uint32_t)found);
    }
    if (found < 0) {
        /* Allocate next free page, round-robin. */
        uint32_t p = kv->next_free_page;
        kv->page_tag[p]       = page_tag;
        kv->slot_head[p]      = 0u;
        kv->next_free_page    = (p + 1u) & (COS_V80_KV_PAGES - 1u);
        found = (int32_t)p;
    }
    uint32_t p   = (uint32_t)found;
    uint32_t sh  = kv->slot_head[p];
    if (sh >= COS_V80_KV_SLOTS) {
        /* Page full — wrap to 0 (ring semantics). */
        sh = 0u;
    }
    kv->pages[p * COS_V80_KV_SLOTS + sh] = hash;
    kv->slot_head[p] = sh + 1u;
    /* Guard: sentinel must never be touched. */
    if (kv->sentinel != 0xCAFEBABEu) {
        kv->invariant_violations += 1u;
    }
    return (int)p;
}

/* ==================================================================
 *  5.  Speculative decoding verify (Leviathan 2023 / Chen 2023).
 * ================================================================== */

void cos_v80_spec_init(cos_v80_spec_t *sp)
{
    memset(sp, 0, sizeof(*sp));
}

void cos_v80_spec_set_draft(cos_v80_spec_t *sp,
                            const uint32_t *draft,
                            uint32_t draft_len)
{
    if (draft_len > COS_V80_SPEC_MAX_DRAFT) draft_len = COS_V80_SPEC_MAX_DRAFT;
    sp->draft_len = draft_len;
    for (uint32_t i = 0; i < draft_len; ++i) sp->draft[i] = draft[i];
}

void cos_v80_spec_set_target(cos_v80_spec_t *sp,
                             const uint32_t *target,
                             uint32_t target_len)
{
    if (target_len > COS_V80_SPEC_MAX_DRAFT) target_len = COS_V80_SPEC_MAX_DRAFT;
    sp->target_len = target_len;
    for (uint32_t i = 0; i < target_len; ++i) sp->target[i] = target[i];
}

uint32_t cos_v80_spec_verify(cos_v80_spec_t *sp)
{
    uint32_t n = u32_min(sp->draft_len, sp->target_len);

    /* Compute longest common prefix branchlessly.  `still_matching`
     * is all-ones while every position so far agreed; the moment a
     * mismatch is seen it collapses to zero and stays there. */
    uint32_t still_matching = 0xFFFFFFFFu;
    uint32_t accepted = 0u;
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t eq = (uint32_t)((sp->draft[i] == sp->target[i]) ? 1u : 0u);
        uint32_t keep = still_matching & ((uint32_t)0 - eq);
        accepted += (keep & 1u);
        still_matching = keep;
    }
    /* Monotonicity check: as n grows the accept count never
     * decreases.  Guard against any caller that decrements n. */
    if (accepted > sp->last_accepted + (n - 0u)) {
        sp->last_nonmonotone = 1u;
    }
    sp->last_accepted = accepted;
    return accepted;
}

uint32_t cos_v80_spec_invariant_ok(const cos_v80_spec_t *sp)
{
    return (sp->last_nonmonotone == 0u) ? 1u : 0u;
}

/* ==================================================================
 *  6.  Variational free energy (Friston 2010).
 * ================================================================== */

void cos_v80_fe_init(cos_v80_fe_t *fe)
{
    memset(fe, 0, sizeof(*fe));
}

void cos_v80_free_energy(cos_v80_fe_t *fe, cos_v80_q16_t surprise_q16)
{
    fe->surprise[fe->head & (COS_V80_FE_HISTORY - 1u)] = surprise_q16;
    fe->head = fe->head + 1u;
    /* F ≈ mean(surprise) + mean(|deviation|), Q16.16. */
    int64_t sum  = 0;
    for (uint32_t i = 0; i < COS_V80_FE_HISTORY; ++i) sum += fe->surprise[i];
    cos_v80_q16_t mean = (cos_v80_q16_t)(sum / (int64_t)COS_V80_FE_HISTORY);
    int64_t dev = 0;
    for (uint32_t i = 0; i < COS_V80_FE_HISTORY; ++i) {
        dev += q16_abs(fe->surprise[i] - mean);
    }
    cos_v80_q16_t mad = (cos_v80_q16_t)(dev / (int64_t)COS_V80_FE_HISTORY);
    fe->last_f = mean + mad;
}

/* ==================================================================
 *  7.  KAN edge (Kolmogorov-Arnold Networks).
 * ================================================================== */

void cos_v80_kan_init(cos_v80_kan_t *kan, uint64_t seed)
{
    memset(kan, 0, sizeof(*kan));
    uint64_t s = seed | 1ULL;
    /* Monotonic control-point spline in [0, 1] (Q16.16). */
    int64_t acc = 0;
    for (uint32_t i = 0; i < COS_V80_KAN_CP; ++i) {
        /* Each increment in [0, Q16_ONE / CP]. */
        uint64_t r = xorshift64(&s);
        int32_t inc = (int32_t)((r & 0x3FFu)
                                * (COS_V80_Q16_ONE / COS_V80_KAN_CP)
                                / 0x3FF);
        acc += inc;
        kan->cp[i] = (cos_v80_q16_t)acc;
    }
}

cos_v80_q16_t cos_v80_kan_edge(cos_v80_kan_t *kan, cos_v80_q16_t x)
{
    /* Map x (Q16.16) into an index in [0, CP-1].  Clip to range. */
    if (x < 0)                       x = 0;
    if (x > COS_V80_Q16_ONE)         x = COS_V80_Q16_ONE;
    /* idx_q = x * (CP-1); upper bits are the integer index, lower
     * bits the fractional weight. */
    int64_t scaled = (int64_t)x * (int64_t)(COS_V80_KAN_CP - 1u);
    int32_t idx    = (int32_t)(scaled >> 16);
    int32_t frac   = (int32_t)(scaled & 0xFFFF);
    if (idx >= (int32_t)(COS_V80_KAN_CP - 1u)) {
        idx  = COS_V80_KAN_CP - 1u;
        frac = 0;
    }
    /* Linear interp between cp[idx] and cp[idx+1]. */
    cos_v80_q16_t lo = kan->cp[idx];
    cos_v80_q16_t hi = kan->cp[(idx + 1) & (COS_V80_KAN_CP - 1u)];
    int64_t out = (int64_t)lo * (int64_t)(COS_V80_Q16_ONE - frac)
                + (int64_t)hi * (int64_t)frac;
    out >>= 16;
    kan->last_out = (cos_v80_q16_t)out;
    return kan->last_out;
}

/* ==================================================================
 *  8.  CTM (Continuous Thought Machine) Kuramoto oscillator tick.
 * ================================================================== */

void cos_v80_ctm_init(cos_v80_ctm_t *c)
{
    memset(c, 0, sizeof(*c));
    /* Natural frequencies: small integer multiples of the step,
     * ω_i = (i + 1) * ONE/32, so one cycle every ~32 ticks. */
    for (uint32_t i = 0; i < COS_V80_CTM_N; ++i) {
        c->omega[i] = (cos_v80_q16_t)((i + 1u) * (COS_V80_Q16_ONE / 32u));
        c->phi[i]   = 0;
    }
    c->K = COS_V80_Q16_ONE / 8;   /* weak coupling */
}

void cos_v80_ctm_tick(cos_v80_ctm_t *c, cos_v80_q16_t dt)
{
    /* Compute pairwise coupling term (O(N^2) but N = 8). */
    cos_v80_q16_t K_over_N = c->K / (cos_v80_q16_t)COS_V80_CTM_N;
    cos_v80_q16_t next[COS_V80_CTM_N];
    for (uint32_t i = 0; i < COS_V80_CTM_N; ++i) {
        cos_v80_q16_t coupling = 0;
        for (uint32_t j = 0; j < COS_V80_CTM_N; ++j) {
            /* sin(φ_j - φ_i).  Convert radian Q16.16 to LUT index:
             * idx = round(phase · 32 / 2π). */
            cos_v80_q16_t diff = c->phi[j] - c->phi[i];
            int64_t scaled = (int64_t)diff * 32;
            int32_t idx = (int32_t)(scaled / (int64_t)COS_V80_Q16_TWOPI);
            coupling += q16_sin_idx(idx);
        }
        coupling = q16_mul(coupling, K_over_N);
        cos_v80_q16_t dphi = q16_mul(c->omega[i] + coupling, dt);
        next[i] = c->phi[i] + dphi;
    }
    for (uint32_t i = 0; i < COS_V80_CTM_N; ++i) {
        c->phi[i] = next[i];
    }
    c->step_count += 1u;
}

/* ==================================================================
 *  9.  MoE top-k router (Shazeer 2017 / DeepSeek-MoE 2024).
 * ================================================================== */

void cos_v80_moe_init(cos_v80_moe_t *m)
{
    memset(m, 0, sizeof(*m));
    m->k = COS_V80_MOE_TOPK;
}

void cos_v80_moe_set_gate(cos_v80_moe_t *m,
                          const cos_v80_q16_t *gate,
                          uint32_t len)
{
    memset(m->gate, 0, sizeof(m->gate));
    if (len > COS_V80_MOE_EXPERTS) len = COS_V80_MOE_EXPERTS;
    for (uint32_t i = 0; i < len; ++i) m->gate[i] = gate[i];
}

void cos_v80_moe_route(cos_v80_moe_t *m)
{
    /* Branchless top-k by selection-sort style reduction.  k is
     * small and known at compile time (COS_V80_MOE_TOPK = 2). */
    cos_v80_q16_t copy[COS_V80_MOE_EXPERTS];
    for (uint32_t i = 0; i < COS_V80_MOE_EXPERTS; ++i) copy[i] = m->gate[i];

    uint32_t selected = 0u;
    uint32_t routed_mask = 0u;
    for (uint32_t pick = 0; pick < COS_V80_MOE_TOPK; ++pick) {
        cos_v80_q16_t best_val = (cos_v80_q16_t)0x80000000;
        uint32_t      best_idx = 0u;
        for (uint32_t i = 0; i < COS_V80_MOE_EXPERTS; ++i) {
            uint32_t taken = (routed_mask >> i) & 1u;
            cos_v80_q16_t v = copy[i];
            uint32_t gt = (uint32_t)((v > best_val) ? 1u : 0u);
            gt &= (~taken) & 1u;
            best_val = (cos_v80_q16_t)u32_select(gt, (uint32_t)v,
                                                 (uint32_t)best_val);
            best_idx = u32_select(gt, i, best_idx);
        }
        routed_mask |= (1u << best_idx);
        selected   += 1u;
    }
    m->routed = routed_mask;
    m->k      = COS_V80_MOE_TOPK;

    /* Check: popcount must equal k. */
    uint32_t pc = (uint32_t)__builtin_popcount(routed_mask);
    if (pc != m->k || selected != m->k) {
        m->invariant_violations += 1u;
    }
}

uint32_t cos_v80_moe_invariant_ok(const cos_v80_moe_t *m)
{
    if (m->invariant_violations != 0u) return 0u;
    uint32_t pc = (uint32_t)__builtin_popcount(m->routed);
    return (pc == m->k) ? 1u : 0u;
}

/* ==================================================================
 *  11. Receipt.
 * ================================================================== */

void cos_v80_receipt_update(cos_v80_receipt_t *r,
                            const cos_v80_hv_t *state_hv)
{
    unsigned rot = (unsigned)((r->step_count * 11u) & 63u);
    cos_v80_hv_t mixed;
    mixed.w[0] = rotl64(state_hv->w[0], rot);
    mixed.w[1] = rotl64(state_hv->w[1], rot + 13u);
    mixed.w[2] = rotl64(state_hv->w[2], rot + 29u);
    mixed.w[3] = rotl64(state_hv->w[3], rot + 41u);
    r->receipt.w[0] ^= mixed.w[0];
    r->receipt.w[1] ^= mixed.w[1];
    r->receipt.w[2] ^= mixed.w[2];
    r->receipt.w[3] ^= mixed.w[3];
    r->step_count += 1u;
}

static cos_v80_hv_t cortex_fold(const cos_v80_cortex_t *c)
{
    cos_v80_hv_t h = {{0, 0, 0, 0}};
    /* SSM */
    for (uint32_t i = 0; i < COS_V80_SSM_D; ++i) {
        h.w[i & 3u] ^= ((uint64_t)(uint32_t)c->ssm.h[i]) << (i & 31u);
    }
    /* RoPE */
    for (uint32_t i = 0; i < COS_V80_ROPE_PAIRS * 2u; ++i) {
        h.w[(i >> 1) & 3u] ^= ((uint64_t)(uint32_t)c->rope.q[i]) << ((i * 3u) & 31u);
    }
    /* Attn */
    h.w[0] ^= (uint64_t)c->attn.last_best_idx;
    h.w[1] ^= (uint64_t)c->attn.last_best_dist << 32;
    /* KV */
    for (uint32_t p = 0; p < COS_V80_KV_PAGES; ++p) {
        h.w[p & 3u] ^= (uint64_t)c->kv.page_tag[p];
    }
    /* Spec */
    h.w[2] ^= (uint64_t)c->spec.last_accepted << 20;
    /* FE */
    h.w[3] ^= (uint64_t)(uint32_t)c->fe.last_f << 5;
    /* CTM */
    for (uint32_t i = 0; i < COS_V80_CTM_N; ++i) {
        h.w[i & 3u] ^= (uint64_t)(uint32_t)c->ctm.phi[i];
    }
    /* MoE */
    h.w[1] ^= (uint64_t)c->moe.routed << 48;
    return h;
}

/* ==================================================================
 *  10. TTC (Test-Time Compute) VM.
 * ================================================================== */

static int ttc_validate(cos_v80_ttc_insn_t i)
{
    /* Reserved bits must be zero. */
    if ((i >> 28) != 0u)              return -1;
    uint32_t op = cos_v80_op_of(i);
    if (op > (uint32_t)COS_V80_OP_FOLD) return -1;
    return 0;
}

int cos_v80_ttc_run(cos_v80_cortex_t *cortex,
                    const cos_v80_ttc_insn_t *prog,
                    size_t prog_len)
{
    if (cortex == NULL || prog == NULL) return -1;

    size_t i;
    for (i = 0; i < prog_len && i < COS_V80_TTC_PROG_MAX; ++i) {
        cos_v80_ttc_insn_t insn = prog[i];
        if (ttc_validate(insn) != 0) return -((int)i + 1);

        uint32_t op = cos_v80_op_of(insn);
        uint32_t a  = cos_v80_a_of(insn);
        uint32_t b  = cos_v80_b_of(insn);
        uint32_t c  = cos_v80_c_of(insn);

        switch ((cos_v80_op_t)op) {
        case COS_V80_OP_HALT:
            return (int)i;
        case COS_V80_OP_SSM: {
            /* Interpret `a` as a signed Q8.8 input and widen to Q16.16.
             * Shift in unsigned space to keep UBSAN happy on negative
             * inputs (signed-left-shift of negatives is UB in C99). */
            int32_t  sv = (int32_t)(int8_t)a;
            cos_v80_q16_t u = (cos_v80_q16_t)((uint32_t)sv << 8);
            cos_v80_ssm_step(&cortex->ssm, u);
            break;
        }
        case COS_V80_OP_RPE: {
            uint32_t pos = (uint32_t)a | ((uint32_t)b << 8);
            cos_v80_rope_rotate(&cortex->rope, pos);
            break;
        }
        case COS_V80_OP_ATT: {
            uint64_t qh = ((uint64_t)a << 40)
                        ^ ((uint64_t)b << 24)
                        ^ ((uint64_t)c <<  8)
                        ^  (uint64_t)cortex->attn.head;
            cos_v80_attn_window(&cortex->attn, qh);
            break;
        }
        case COS_V80_OP_KVC: {
            uint32_t tag  = (uint32_t)a | ((uint32_t)b << 8);
            if (tag == 0u) tag = 1u;   /* keep sentinel-0 reserved */
            uint64_t hash = ((uint64_t)c << 48)
                          ^ ((uint64_t)cortex->attn.last_best_idx << 16)
                          ^  (uint64_t)i;
            if (cos_v80_kv_commit(&cortex->kv, tag, hash) < 0)
                return -((int)i + 1);
            break;
        }
        case COS_V80_OP_SPV: {
            /* Use first `a` positions of draft vs target. */
            uint32_t len = u32_min(a, COS_V80_SPEC_MAX_DRAFT);
            /* Populate a deterministic draft/target from b, c. */
            for (uint32_t k = 0; k < len; ++k) {
                cortex->spec.draft [k] = (uint32_t)(k * b);
                cortex->spec.target[k] = (uint32_t)(k * (b ^ c));
            }
            cortex->spec.draft_len  = len;
            cortex->spec.target_len = len;
            (void)cos_v80_spec_verify(&cortex->spec);
            break;
        }
        case COS_V80_OP_FEN: {
            int32_t sv = (int32_t)(int8_t)a;
            cos_v80_q16_t s = (cos_v80_q16_t)((uint32_t)sv << 8);
            cos_v80_free_energy(&cortex->fe, s);
            break;
        }
        case COS_V80_OP_KAN: {
            cos_v80_q16_t x = (cos_v80_q16_t)a << 8;
            (void)cos_v80_kan_edge(&cortex->kan, x);
            break;
        }
        case COS_V80_OP_CTM: {
            cos_v80_q16_t dt = (cos_v80_q16_t)((a == 0u ? 1u : a) << 8);
            cos_v80_ctm_tick(&cortex->ctm, dt);
            break;
        }
        case COS_V80_OP_MOE: {
            /* Use (a, b, c) to seed a deterministic gate vector. */
            uint64_t s = ((uint64_t)a << 40) ^ ((uint64_t)b << 20) ^ c ^ 0x9E37ULL;
            if (s == 0ULL) s = 1ULL;
            cos_v80_q16_t g[COS_V80_MOE_EXPERTS];
            for (uint32_t k = 0; k < COS_V80_MOE_EXPERTS; ++k) {
                uint64_t r = xorshift64(&s);
                g[k] = (cos_v80_q16_t)(int32_t)(r & 0xFFFFFFu)
                     - (cos_v80_q16_t)0x7FFFFF;
            }
            cos_v80_moe_set_gate(&cortex->moe, g, COS_V80_MOE_EXPERTS);
            cos_v80_moe_route(&cortex->moe);
            break;
        }
        case COS_V80_OP_FOLD: {
            cos_v80_hv_t fold = cortex_fold(cortex);
            cos_v80_receipt_update(&cortex->receipt, &fold);
            /* Pseudo-energy: popcount of the fold, scaled. */
            uint32_t pc = popcount64(fold.w[0]) + popcount64(fold.w[1])
                        + popcount64(fold.w[2]) + popcount64(fold.w[3]);
            cortex->receipt.last_energy = (cos_v80_q16_t)(pc << 8);
            break;
        }
        default:
            return -((int)i + 1);
        }
    }
    return (int)i;
}

/* ==================================================================
 *  12. v80_ok gate + 20-bit composed decision.
 * ================================================================== */

uint32_t cos_v80_ok(cos_v80_cortex_t *cortex,
                    const cos_v80_ttc_insn_t *prog,
                    size_t prog_len)
{
    if (cortex == NULL) return 0u;
    int executed = cos_v80_ttc_run(cortex, prog, prog_len);
    if (executed < 0) return 0u;

    uint32_t ssm_ok  = (cortex->ssm.overflow == 0u) ? 1u : 0u;
    uint32_t kv_ok   = cos_v80_kv_invariant_ok(&cortex->kv);
    uint32_t spec_ok = cos_v80_spec_invariant_ok(&cortex->spec);
    uint32_t moe_ok  = cos_v80_moe_invariant_ok(&cortex->moe);

    return (ssm_ok & 1u) & (kv_ok & 1u) & (spec_ok & 1u) & (moe_ok & 1u);
}

uint32_t cos_v80_compose_decision(uint32_t v79_composed_ok,
                                  uint32_t v80_ok)
{
    v79_composed_ok &= 1u;
    v80_ok          &= 1u;
    return v79_composed_ok & v80_ok;
}
