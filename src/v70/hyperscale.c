/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v70 — σ-Hyperscale: trillion-parameter / hyperscale-killer
 * substrate kernel.  Pure integer C; libc-only; no FP on any decision
 * surface.  Branchless on the data; not constant-time in the model size
 * by design (scaling with N is the entire point).
 *
 * 1 = 1.
 */

#include "hyperscale.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char cos_v70_version[] =
    "70.0.0 — v70.0 sigma-hyperscale "
    "(p2q-shiftadd + ssm-mamba2 + rwkv7-deltarule + moe10k-deepseekv3 + "
    "pim-popcount + photonic-wdm + spike-loihi3 + ring-allreduce + "
    "stream-lru + hsl)";

/* --------------------------------------------------------------------
 *  Branchless helpers (mirroring v69 discipline).
 * -------------------------------------------------------------------- */

static inline int32_t sat_i32_to_i16(int32_t x)
{
    if (x >  32767) x =  32767;
    if (x < -32768) x = -32768;
    return x;
}

static inline int32_t sat_add_i32(int32_t a, int32_t b)
{
    int64_t r = (int64_t)a + (int64_t)b;
    if (r >  (int64_t)INT32_MAX) r =  (int64_t)INT32_MAX;
    if (r <  (int64_t)INT32_MIN) r =  (int64_t)INT32_MIN;
    return (int32_t)r;
}

static inline int32_t sel_i32(int32_t cond, int32_t a, int32_t b)
{
    /* cond is 0 or -1 (all bits set). Returns a if cond, else b. */
    return (a & cond) | (b & ~cond);
}

static inline int32_t sat_q15_mul(int16_t a, int16_t b)
{
    int32_t p = (int32_t)a * (int32_t)b;
    return sat_i32_to_i16(p >> 15);
}

/* --------------------------------------------------------------------
 *  1. P2Q — Power-of-2 weight quantisation (ShiftAddLLM lineage).
 * -------------------------------------------------------------------- */

uint8_t cos_v70_p2q_pack(int8_t sign, uint8_t exp)
{
    uint8_t s = (sign < 0) ? 1u : 0u;
    if (exp > COS_V70_P2Q_EXP_MAX) exp = COS_V70_P2Q_EXP_MAX;
    return (uint8_t)((s << 3) | (exp & 0x07u));
}

void cos_v70_p2q_unpack(uint8_t nib, int8_t *sign, uint8_t *exp)
{
    uint8_t s = (uint8_t)((nib >> 3) & 0x01u);
    uint8_t e = (uint8_t)(nib & 0x07u);
    if (sign) *sign = (int8_t)(s ? -1 : +1);
    if (exp)  *exp  = e;
}

void cos_v70_p2q_gemv(const cos_v70_p2q_byte_t *weights,
                      const int16_t            *x_q15,
                      uint32_t                  rows,
                      uint32_t                  cols,
                      int32_t                  *out)
{
    if (!weights || !x_q15 || !out || rows == 0u || cols == 0u) return;
    if (cols > COS_V70_P2Q_DIM_MAX) cols = COS_V70_P2Q_DIM_MAX;

    /* Two nibbles per byte; cols may be odd. */
    for (uint32_t r = 0; r < rows; ++r) {
        const cos_v70_p2q_byte_t *row = weights + (size_t)r * ((cols + 1u) >> 1);
        int32_t acc = 0;
        for (uint32_t c = 0; c < cols; ++c) {
            uint8_t byte = row[c >> 1];
            uint8_t nib  = (uint8_t)((c & 1u) ? (byte >> 4) : (byte & 0x0Fu));
            uint8_t s    = (uint8_t)((nib >> 3) & 0x01u);
            uint8_t e    = (uint8_t)(nib & 0x07u);
            /* Shift as uint32 to avoid signed-shift UB; cast back for the
             * sign-flip.  Result fits in int32 because |x|<=32768 and e<=7. */
            uint32_t ush = ((uint32_t)(int32_t)x_q15[c]) << e;
            int32_t  shifted = (int32_t)ush;
            int32_t  mask    = -(int32_t)s;   /* 0xFFFFFFFF if s==1 else 0 */
            int32_t  signed_v = (shifted ^ mask) - mask;  /* branchless ±shifted */
            acc = sat_add_i32(acc, signed_v);
        }
        out[r] = acc;
    }
}

/* --------------------------------------------------------------------
 *  2. Mamba-2 selective SSM scan.
 * -------------------------------------------------------------------- */

void cos_v70_ssm_scan(const int16_t *a_q15,
                      const int16_t *b_q15,
                      const int16_t *c_q15,
                      const int16_t *x_q15,
                      uint32_t       n_tokens,
                      int16_t        h0_q15,
                      int16_t       *h_out,
                      int16_t       *y_out)
{
    if (!a_q15 || !b_q15 || !c_q15 || !x_q15 || n_tokens == 0u) return;
    int16_t h = h0_q15;
    for (uint32_t t = 0; t < n_tokens; ++t) {
        int32_t hh = (int32_t)sat_q15_mul(a_q15[t], h)
                   + (int32_t)sat_q15_mul(b_q15[t], x_q15[t]);
        h = (int16_t)sat_i32_to_i16(hh);
        if (h_out) h_out[t] = h;
        if (y_out) y_out[t] = (int16_t)sat_q15_mul(c_q15[t], h);
    }
}

/* --------------------------------------------------------------------
 *  3. RWKV-7 delta-rule update.
 * -------------------------------------------------------------------- */

void cos_v70_rwkv7_step(const int16_t *r_q15,
                        const int16_t *w_q15,
                        const int16_t *u_q15,
                        const int16_t *v_q15,
                        const int16_t *decay_q15,
                        const int16_t *x_q15,
                        int16_t       *state_q15,
                        int16_t       *y_out,
                        uint32_t       dim)
{
    if (!r_q15 || !w_q15 || !u_q15 || !v_q15 || !decay_q15 || !x_q15
        || !state_q15 || dim == 0u) return;
    if (dim > COS_V70_RWKV_DIM_MAX) dim = COS_V70_RWKV_DIM_MAX;
    for (uint32_t i = 0; i < dim; ++i) {
        int32_t gate    = (int32_t)sat_q15_mul(w_q15[i], x_q15[i]) + (int32_t)u_q15[i];
        int16_t gate16  = (int16_t)sat_i32_to_i16(gate);
        int32_t decay   = (int32_t)sat_q15_mul(decay_q15[i], state_q15[i]);
        int16_t v_eff32 = (int16_t)sat_i32_to_i16((int32_t)v_q15[i] - (int32_t)state_q15[i]);
        int32_t add     = (int32_t)sat_q15_mul(gate16, v_eff32);
        int32_t st_new  = sat_add_i32(decay, add);
        int16_t st16    = (int16_t)sat_i32_to_i16(st_new);
        state_q15[i] = st16;
        if (y_out) y_out[i] = (int16_t)sat_q15_mul(r_q15[i], st16);
    }
}

/* --------------------------------------------------------------------
 *  4. Block-sparse MoE with auxiliary-loss-free balancing.
 * -------------------------------------------------------------------- */

void cos_v70_moe_route_topk(const int16_t      *scores_q15,
                            const int16_t      *bias_q15,
                            uint32_t            n_experts,
                            uint32_t            k,
                            cos_v70_moe_route_t *out,
                            uint32_t           *load_counter)
{
    if (!scores_q15 || !out || n_experts == 0u || k == 0u) {
        if (out) { out->len = 0u; out->k = 0u; }
        return;
    }
    if (k > COS_V70_MOE_TOPK_MAX) k = COS_V70_MOE_TOPK_MAX;
    if (n_experts > COS_V70_MOE_EXPERTS_MAX) n_experts = COS_V70_MOE_EXPERTS_MAX;

    /* Initialise top-K table to (-INF score). */
    int32_t  topscore[COS_V70_MOE_TOPK_MAX];
    uint32_t topidx  [COS_V70_MOE_TOPK_MAX];
    for (uint32_t j = 0; j < k; ++j) { topscore[j] = INT32_MIN; topidx[j] = UINT32_MAX; }

    /* Branchless top-K via insertion bubble (k passes per element). */
    for (uint32_t e = 0; e < n_experts; ++e) {
        int32_t s = (int32_t)scores_q15[e] + (bias_q15 ? (int32_t)bias_q15[e] : 0);
        int32_t cand_s = s;
        uint32_t cand_i = e;
        for (uint32_t j = 0; j < k; ++j) {
            int32_t cond = -(int32_t)(cand_s > topscore[j]);
            int32_t old_s = topscore[j];
            uint32_t old_i = topidx[j];
            topscore[j] = sel_i32(cond, cand_s, old_s);
            topidx[j]   = (uint32_t)sel_i32(cond, (int32_t)cand_i, (int32_t)old_i);
            cand_s = sel_i32(cond, old_s, cand_s);
            cand_i = (uint32_t)sel_i32(cond, (int32_t)old_i, (int32_t)cand_i);
        }
    }

    out->k   = (uint8_t)k;
    out->len = (uint8_t)k;
    for (uint32_t j = 0; j < k; ++j) {
        out->selected[j] = topidx[j];
        out->selected_score_q15[j] =
            (int16_t)sat_i32_to_i16(topscore[j]);
        if (load_counter && topidx[j] < n_experts)
            load_counter[topidx[j]] += 1u;
    }
}

void cos_v70_moe_bias_update(int16_t        *bias_q15,
                             const uint32_t *load_counter,
                             uint32_t        n_experts,
                             int16_t         step_q15)
{
    if (!bias_q15 || !load_counter || n_experts == 0u) return;
    /* Compute average load (integer). */
    uint64_t total = 0;
    for (uint32_t e = 0; e < n_experts; ++e) total += load_counter[e];
    uint32_t avg = (uint32_t)(total / (uint64_t)n_experts);
    /* bias[e] += step * sign(avg - load[e]); branchless sign via compare. */
    for (uint32_t e = 0; e < n_experts; ++e) {
        int32_t diff = (int32_t)avg - (int32_t)load_counter[e];
        int32_t pos  = -(int32_t)(diff > 0);   /* -1 if diff>0 */
        int32_t neg  = -(int32_t)(diff < 0);   /* -1 if diff<0 */
        int32_t sign = (pos & 1) | (neg & (int32_t)-1); /* +1 or -1 or 0 */
        /* Re-derive a clean ternary sign: sign = (pos & 1) - (neg & 1) */
        sign = (int32_t)((pos & 1) - (neg & 1));
        int32_t add  = sign * (int32_t)step_q15;
        int32_t nb   = (int32_t)bias_q15[e] + add;
        bias_q15[e] = (int16_t)sat_i32_to_i16(nb);
    }
}

/* --------------------------------------------------------------------
 *  5. PIM bit-serial AND-popcount.
 * -------------------------------------------------------------------- */

void cos_v70_pim_and_popcount(uint64_t        act_word,
                              const uint64_t *weights_col,
                              uint32_t        n_cols,
                              uint32_t       *out)
{
    if (!weights_col || !out || n_cols == 0u) return;
    if (n_cols > COS_V70_PIM_COLS_MAX) n_cols = COS_V70_PIM_COLS_MAX;
    for (uint32_t c = 0; c < n_cols; ++c) {
        out[c] += (uint32_t)__builtin_popcountll(act_word & weights_col[c]);
    }
}

/* --------------------------------------------------------------------
 *  6. Photonic WDM dot product.
 * -------------------------------------------------------------------- */

int32_t cos_v70_wdm_dot(const int16_t *q_q15,
                        const int16_t *k_q15,
                        uint32_t       n_per_lane,
                        uint32_t       lanes,
                        int32_t       *lane_out)
{
    if (!q_q15 || !k_q15 || lanes == 0u || n_per_lane == 0u) return 0;
    if (lanes > COS_V70_WDM_LANES) lanes = COS_V70_WDM_LANES;
    int32_t lane_acc[COS_V70_WDM_LANES];
    for (uint32_t l = 0; l < lanes; ++l) lane_acc[l] = 0;
    for (uint32_t i = 0; i < n_per_lane; ++i) {
        for (uint32_t l = 0; l < lanes; ++l) {
            uint32_t idx = i * lanes + l;
            int32_t  p   = (int32_t)q_q15[idx] * (int32_t)k_q15[idx];
            lane_acc[l] = sat_add_i32(lane_acc[l], p >> 15);
        }
    }
    int32_t total = 0;
    for (uint32_t l = 0; l < lanes; ++l) {
        if (lane_out) lane_out[l] = lane_acc[l];
        total = sat_add_i32(total, lane_acc[l]);
    }
    return total;
}

/* --------------------------------------------------------------------
 *  7. Spike-coded sparse activation.
 * -------------------------------------------------------------------- */

uint32_t cos_v70_spike_encode(const int16_t *a_q15,
                              uint32_t       n,
                              int16_t        thresh_q15,
                              int16_t       *spikes_q15,
                              uint8_t       *fire_mask)
{
    if (!a_q15 || n == 0u) return 0u;
    if (n > COS_V70_SPIKE_NEURONS_MAX) n = COS_V70_SPIKE_NEURONS_MAX;
    uint32_t fired = 0u;
    for (uint32_t i = 0; i < n; ++i) {
        int32_t  diff = (int32_t)a_q15[i] - (int32_t)thresh_q15;
        int32_t  fire = -(int32_t)(diff >= 0);          /* 0xFFFFFFFF if firing */
        int16_t  pay  = (int16_t)sat_i32_to_i16(diff);
        int16_t  out  = (int16_t)sel_i32(fire, pay, 0);
        if (spikes_q15) spikes_q15[i] = out;
        if (fire_mask)  fire_mask [i] = (uint8_t)(fire & 1);
        fired += (uint32_t)(fire & 1);
    }
    return fired;
}

void cos_v70_spike_readout(const int16_t *spikes_q15,
                           const uint8_t *fire_mask,
                           const int16_t *w_q15,
                           uint32_t       rows,
                           uint32_t       n,
                           int32_t       *y_out)
{
    if (!spikes_q15 || !fire_mask || !w_q15 || !y_out
        || rows == 0u || n == 0u) return;
    if (n > COS_V70_SPIKE_NEURONS_MAX) n = COS_V70_SPIKE_NEURONS_MAX;
    for (uint32_t r = 0; r < rows; ++r) {
        int32_t acc = 0;
        for (uint32_t i = 0; i < n; ++i) {
            int32_t mask = -(int32_t)(fire_mask[i] & 1u);
            int32_t prod = ((int32_t)spikes_q15[i]
                          * (int32_t)w_q15[(size_t)r * n + i]) >> 15;
            acc = sat_add_i32(acc, prod & mask);
        }
        y_out[r] = acc;
    }
}

/* --------------------------------------------------------------------
 *  8. Ring all-reduce (Patarasuk-Yuan 2009).
 *
 *  Each rank r owns buffer of size N*chunk_len, sliced into N chunks.
 *  Phase A — reduce-scatter: in each of N-1 steps, rank r sends chunk
 *      ((r - step) mod N) to (r+1) mod N, who adds it to its local
 *      chunk of the same index.  After N-1 steps, rank r owns the
 *      *full* sum of chunk ((r + 1) mod N).
 *  Phase B — all-gather: in each of N-1 steps, rank r sends chunk
 *      ((r + 1 - step) mod N) to (r+1) mod N, who replaces its
 *      chunk with the received one.
 *  All arithmetic is `int32_t` saturating add.
 * -------------------------------------------------------------------- */

uint8_t cos_v70_ring_allreduce(int32_t **rank_buffers,
                               uint32_t  n_ranks,
                               uint32_t  chunk_len)
{
    if (!rank_buffers || n_ranks == 0u || chunk_len == 0u) return 0u;
    if (n_ranks   > COS_V70_RING_RANKS_MAX)  n_ranks   = COS_V70_RING_RANKS_MAX;
    if (chunk_len > COS_V70_RING_BUFFER_MAX) chunk_len = COS_V70_RING_BUFFER_MAX;

    for (uint32_t r = 0; r < n_ranks; ++r)
        if (!rank_buffers[r]) return 0u;

    if (n_ranks == 1u) return 1u;  /* trivially done */

    /* Reduce-scatter. */
    for (uint32_t step = 0; step < n_ranks - 1u; ++step) {
        for (uint32_t r = 0; r < n_ranks; ++r) {
            uint32_t send_chunk = (r + n_ranks - step)        % n_ranks;
            uint32_t dst        = (r + 1u)                    % n_ranks;
            int32_t *src_chunk = rank_buffers[r]   + (size_t)send_chunk * chunk_len;
            int32_t *dst_chunk = rank_buffers[dst] + (size_t)send_chunk * chunk_len;
            for (uint32_t i = 0; i < chunk_len; ++i)
                dst_chunk[i] = sat_add_i32(dst_chunk[i], src_chunk[i]);
        }
    }
    /* All-gather. */
    for (uint32_t step = 0; step < n_ranks - 1u; ++step) {
        for (uint32_t r = 0; r < n_ranks; ++r) {
            uint32_t send_chunk = (r + 1u + n_ranks - step) % n_ranks;
            uint32_t dst        = (r + 1u)                  % n_ranks;
            int32_t *src_chunk = rank_buffers[r]   + (size_t)send_chunk * chunk_len;
            int32_t *dst_chunk = rank_buffers[dst] + (size_t)send_chunk * chunk_len;
            for (uint32_t i = 0; i < chunk_len; ++i)
                dst_chunk[i] = src_chunk[i];
        }
    }
    /* Verify topology: every rank's chunk 0 must equal rank 0's chunk 0
     * (and so on for each index).  Branchless OR across mismatches. */
    int32_t mismatch = 0;
    for (uint32_t c = 0; c < n_ranks; ++c) {
        int32_t *ref = rank_buffers[0] + (size_t)c * chunk_len;
        for (uint32_t r = 1; r < n_ranks; ++r) {
            int32_t *buf = rank_buffers[r] + (size_t)c * chunk_len;
            for (uint32_t i = 0; i < chunk_len; ++i)
                mismatch |= (ref[i] ^ buf[i]);
        }
    }
    return (uint8_t)(mismatch == 0);
}

/* --------------------------------------------------------------------
 *  9. Streaming weight scheduler — N-slot LRU.
 * -------------------------------------------------------------------- */

void cos_v70_stream_init(cos_v70_stream_cache_t *c)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
}

static inline uint64_t cos_v70_stream_key(uint32_t layer, uint32_t expert)
{
    return ((uint64_t)layer << 32) | (uint64_t)expert;
}

void *cos_v70_stream_lookup(cos_v70_stream_cache_t *c,
                            uint32_t layer,
                            uint32_t expert)
{
    if (!c) return NULL;
    uint64_t key = cos_v70_stream_key(layer, expert);
    c->tick += 1u;
    void *hit = NULL;
    for (uint32_t s = 0; s < COS_V70_STREAM_SLOTS; ++s) {
        if (c->slots[s].valid && c->slots[s].key == key) {
            c->slots[s].last_used = c->tick;
            hit = c->slots[s].addr;
            break;
        }
    }
    if (hit) c->hits += 1u; else c->misses += 1u;
    return hit;
}

uint32_t cos_v70_stream_insert(cos_v70_stream_cache_t *c,
                               uint32_t layer,
                               uint32_t expert,
                               void    *addr,
                               int      prefetch_hint)
{
    if (!c) return UINT32_MAX;
    uint64_t key = cos_v70_stream_key(layer, expert);
    c->tick += 1u;

    /* If already present, refresh; refresh wins over insertion. */
    for (uint32_t s = 0; s < COS_V70_STREAM_SLOTS; ++s) {
        if (c->slots[s].valid && c->slots[s].key == key) {
            c->slots[s].addr      = addr;
            c->slots[s].last_used = c->tick;
            if (prefetch_hint && addr) __builtin_prefetch(addr, 0, 3);
            return s;
        }
    }

    /* Find the LRU slot (smallest last_used; invalid slots count as 0). */
    uint32_t victim = 0;
    uint64_t lru    = UINT64_MAX;
    for (uint32_t s = 0; s < COS_V70_STREAM_SLOTS; ++s) {
        if (!c->slots[s].valid) { victim = s; lru = 0; break; }
        if (c->slots[s].last_used < lru) { lru = c->slots[s].last_used; victim = s; }
    }
    if (c->slots[victim].valid) c->evictions += 1u;
    c->slots[victim].key       = key;
    c->slots[victim].addr      = addr;
    c->slots[victim].last_used = c->tick;
    c->slots[victim].valid     = 1u;
    if (prefetch_hint && addr) __builtin_prefetch(addr, 0, 3);
    return victim;
}

/* --------------------------------------------------------------------
 * 10. HSL — Hyperscale Language interpreter.
 * -------------------------------------------------------------------- */

struct cos_v70_hsl_state {
    int32_t  reg_q15[COS_V70_NREGS];   /* int32 to absorb sums */
    uint32_t reg_u32[COS_V70_NREGS];
    uint64_t cost;
    uint8_t  topology_ok;
    uint8_t  abstained;
    uint8_t  v70_ok;
    uint8_t  pad;
};

cos_v70_hsl_state_t *cos_v70_hsl_new(void)
{
    cos_v70_hsl_state_t *s = (cos_v70_hsl_state_t *)
        aligned_alloc(64, ((sizeof(*s) + 63u) & ~((size_t)63)));
    if (!s) return NULL;
    memset(s, 0, sizeof(*s));
    s->topology_ok = 1u;
    return s;
}

void cos_v70_hsl_free(cos_v70_hsl_state_t *s) { free(s); }

void cos_v70_hsl_reset(cos_v70_hsl_state_t *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->topology_ok = 1u;
}

uint64_t cos_v70_hsl_cost(const cos_v70_hsl_state_t *s)
    { return s ? s->cost : 0u; }
int16_t  cos_v70_hsl_reg_q15(const cos_v70_hsl_state_t *s, uint32_t i)
    { if (!s || i >= COS_V70_NREGS) return 0; return (int16_t)sat_i32_to_i16(s->reg_q15[i]); }
uint32_t cos_v70_hsl_reg_u32(const cos_v70_hsl_state_t *s, uint32_t i)
    { if (!s || i >= COS_V70_NREGS) return 0u; return s->reg_u32[i]; }
uint8_t  cos_v70_hsl_topology_ok(const cos_v70_hsl_state_t *s)
    { return s ? s->topology_ok : 0u; }
uint8_t  cos_v70_hsl_abstained(const cos_v70_hsl_state_t *s)
    { return s ? s->abstained : 1u; }
uint8_t  cos_v70_hsl_v70_ok(const cos_v70_hsl_state_t *s)
    { return s ? s->v70_ok : 0u; }

static const uint64_t k_cost[10] = {
    /* HALT    */ 1u,
    /* SHIFT   */ 1u,
    /* SCAN    */ 4u,
    /* TIMEMIX */ 4u,
    /* ROUTEK  */ 8u,
    /* PIMPOP  */ 2u,
    /* WDM     */ 2u,
    /* SPIKE   */ 1u,
    /* RING    */ 8u,
    /* GATE    */ 1u,
};

int cos_v70_hsl_exec(cos_v70_hsl_state_t *s,
                     const cos_v70_inst_t *prog,
                     uint32_t n,
                     cos_v70_hsl_ctx_t *ctx,
                     uint64_t cost_budget)
{
    if (!s || !prog || n == 0u || n > COS_V70_PROG_MAX) return -1;

    for (uint32_t i = 0; i < n; ++i) {
        const cos_v70_inst_t *ins = &prog[i];
        if (ins->op > COS_V70_OP_GATE) return -2;
        s->cost += k_cost[ins->op];
        uint8_t  dst = (uint8_t)(ins->dst % COS_V70_NREGS);
        uint8_t  a   = (uint8_t)(ins->a   % COS_V70_NREGS);
        (void)a;

        switch ((cos_v70_op_t)ins->op) {
        case COS_V70_OP_HALT:
            return 0;
        case COS_V70_OP_SHIFT: {
            if (!ctx || !ctx->p2q_weights || !ctx->p2q_x_q15 || !ctx->p2q_out)
                { s->abstained = 1u; break; }
            cos_v70_p2q_gemv(ctx->p2q_weights, ctx->p2q_x_q15,
                             ctx->p2q_rows, ctx->p2q_cols, ctx->p2q_out);
            int32_t mx = INT32_MIN;
            for (uint32_t r = 0; r < ctx->p2q_rows; ++r)
                if (ctx->p2q_out[r] > mx) mx = ctx->p2q_out[r];
            s->reg_q15[dst] = (int32_t)sat_i32_to_i16(mx);
            break;
        }
        case COS_V70_OP_SCAN: {
            if (!ctx || !ctx->ssm_a || !ctx->ssm_b || !ctx->ssm_c
                || !ctx->ssm_x) { s->abstained = 1u; break; }
            cos_v70_ssm_scan(ctx->ssm_a, ctx->ssm_b, ctx->ssm_c,
                             ctx->ssm_x, ctx->ssm_n, ctx->ssm_h0,
                             ctx->ssm_h_out, ctx->ssm_y_out);
            int16_t last_y = ctx->ssm_y_out ? ctx->ssm_y_out[ctx->ssm_n - 1u] : 0;
            s->reg_q15[dst] = (int32_t)last_y;
            break;
        }
        case COS_V70_OP_TIMEMIX: {
            if (!ctx || !ctx->rwkv_r || !ctx->rwkv_state)
                { s->abstained = 1u; break; }
            cos_v70_rwkv7_step(ctx->rwkv_r, ctx->rwkv_w, ctx->rwkv_u,
                               ctx->rwkv_v, ctx->rwkv_decay, ctx->rwkv_x,
                               ctx->rwkv_state, ctx->rwkv_y_out, ctx->rwkv_dim);
            int16_t last_y = (ctx->rwkv_y_out && ctx->rwkv_dim > 0u)
                ? ctx->rwkv_y_out[ctx->rwkv_dim - 1u] : 0;
            s->reg_q15[dst] = (int32_t)last_y;
            break;
        }
        case COS_V70_OP_ROUTEK: {
            if (!ctx || !ctx->moe_scores || !ctx->moe_out)
                { s->abstained = 1u; break; }
            cos_v70_moe_route_topk(ctx->moe_scores, ctx->moe_bias,
                                   ctx->moe_n_experts, ctx->moe_k,
                                   ctx->moe_out, ctx->moe_load_counter);
            s->reg_u32[dst] = ctx->moe_out->len;
            s->reg_q15[dst] = (int32_t)ctx->moe_out->selected_score_q15[0];
            break;
        }
        case COS_V70_OP_PIMPOP: {
            if (!ctx || !ctx->pim_weights_col || !ctx->pim_out)
                { s->abstained = 1u; break; }
            cos_v70_pim_and_popcount(ctx->pim_act_word, ctx->pim_weights_col,
                                     ctx->pim_n_cols, ctx->pim_out);
            uint32_t total = 0u;
            for (uint32_t c = 0; c < ctx->pim_n_cols; ++c) total += ctx->pim_out[c];
            s->reg_u32[dst] = total;
            break;
        }
        case COS_V70_OP_WDM: {
            if (!ctx || !ctx->wdm_q || !ctx->wdm_k)
                { s->abstained = 1u; break; }
            int32_t total = cos_v70_wdm_dot(ctx->wdm_q, ctx->wdm_k,
                                            ctx->wdm_n_per_lane, ctx->wdm_lanes,
                                            ctx->wdm_lane_out);
            s->reg_q15[dst] = (int32_t)sat_i32_to_i16(total);
            break;
        }
        case COS_V70_OP_SPIKE: {
            if (!ctx || !ctx->spike_a_q15)
                { s->abstained = 1u; break; }
            uint32_t fired = cos_v70_spike_encode(ctx->spike_a_q15, ctx->spike_n,
                                                  ctx->spike_thresh_q15,
                                                  ctx->spike_spikes_q15,
                                                  ctx->spike_fire_mask);
            if (ctx->spike_w_q15 && ctx->spike_y_out
                && ctx->spike_spikes_q15 && ctx->spike_fire_mask) {
                cos_v70_spike_readout(ctx->spike_spikes_q15, ctx->spike_fire_mask,
                                      ctx->spike_w_q15, ctx->spike_rows,
                                      ctx->spike_n, ctx->spike_y_out);
            }
            s->reg_u32[dst] = fired;
            break;
        }
        case COS_V70_OP_RING: {
            if (!ctx || !ctx->ring_buffers)
                { s->abstained = 1u; s->topology_ok = 0u; break; }
            uint8_t ok = cos_v70_ring_allreduce(ctx->ring_buffers,
                                                ctx->ring_n_ranks,
                                                ctx->ring_chunk_len);
            s->topology_ok = ok;
            if (!ok) s->abstained = 1u;
            break;
        }
        case COS_V70_OP_GATE: {
            uint8_t cost_ok       = (s->cost <= cost_budget) ? 1u : 0u;
            int32_t reg_v         = s->reg_q15[a];
            uint8_t throughput_ok = (reg_v >= (int32_t)ins->imm) ? 1u : 0u;
            uint8_t topo_ok       = s->topology_ok;
            uint8_t not_abst      = (s->abstained == 0u) ? 1u : 0u;
            s->v70_ok = (uint8_t)(cost_ok & throughput_ok & topo_ok & not_abst);
            return 0;
        }
        }
        if (s->cost > cost_budget) { s->abstained = 1u; return -3; }
    }
    return 0;
}

/* --------------------------------------------------------------------
 *  Composed 11-bit branchless decision.
 * -------------------------------------------------------------------- */

cos_v70_decision_t cos_v70_compose_decision(uint8_t v60_ok,
                                            uint8_t v61_ok,
                                            uint8_t v62_ok,
                                            uint8_t v63_ok,
                                            uint8_t v64_ok,
                                            uint8_t v65_ok,
                                            uint8_t v66_ok,
                                            uint8_t v67_ok,
                                            uint8_t v68_ok,
                                            uint8_t v69_ok,
                                            uint8_t v70_ok)
{
    cos_v70_decision_t d;
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
    d.allow  = (uint8_t)(d.v60_ok & d.v61_ok & d.v62_ok & d.v63_ok
                        & d.v64_ok & d.v65_ok & d.v66_ok & d.v67_ok
                        & d.v68_ok & d.v69_ok & d.v70_ok);
    return d;
}
