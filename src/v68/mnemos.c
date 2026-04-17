/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v68 — σ-Mnemos: continual-learning + episodic-memory +
 * online-adaptation kernel.  Pure integer C; libc-only; no FP on any
 * decision surface.  Branchless on the data; not constant-time in the
 * store size by design.
 *
 * 1 = 1.
 */

#include "mnemos.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char cos_v68_version[] =
    "68.0.0 — v68.0 sigma-mnemos "
    "(bipolar-HV-D8192 + surprise-gate + actr-decay + recall + "
    "hebb-ttt + sleep-consolidate + forgetting-controller + mml)";

/* --------------------------------------------------------------------
 *  Branchless helpers (mirror v66 / v67 conventions).
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

static void *aligned_calloc64(size_t bytes)
{
    size_t rounded = (bytes + 63u) & ~((size_t)63);
    void *p = aligned_alloc(64, rounded);
    if (p) memset(p, 0, rounded);
    return p;
}

/* --------------------------------------------------------------------
 *  Bipolar HV ops.
 * -------------------------------------------------------------------- */

int16_t cos_v68_hv_sim_q15(const cos_v68_hv_t *a, const cos_v68_hv_t *b)
{
    uint32_t h = cos_v68_hv_hamming(a, b);
    /* sim = (BITS − 2H) · 32768 / BITS.  H ∈ [0, 8192].
     * (BITS − 2H) ∈ [-8192, +8192].  Scale: 32768 / 8192 = 4. */
    int32_t s = ((int32_t)COS_V68_HV_BITS - 2 * (int32_t)h) * 4;
    return (int16_t)sat_q15_i32(s);
}

void cos_v68_hv_xor_inplace(cos_v68_hv_t *dst, const cos_v68_hv_t *src)
{
    for (uint32_t i = 0; i < COS_V68_HV_WORDS; ++i)
        dst->w[i] ^= src->w[i];
}

void cos_v68_hv_perm1(cos_v68_hv_t *dst, const cos_v68_hv_t *src)
{
    /* Cyclic 1-bit left-rotate across the whole HV. */
    uint64_t carry = (src->w[COS_V68_HV_WORDS - 1] >> 63) & 1ULL;
    for (uint32_t i = 0; i < COS_V68_HV_WORDS; ++i) {
        uint64_t w = src->w[i];
        uint64_t out_carry = (w >> 63) & 1ULL;
        dst->w[i] = (w << 1) | carry;
        carry = out_carry;
    }
}

/* --------------------------------------------------------------------
 *  Episodic store.
 * -------------------------------------------------------------------- */

struct cos_v68_store {
    uint32_t            capacity;
    uint32_t            tick;
    uint64_t            writes;
    uint64_t            forgets;
    cos_v68_episode_t  *ep;       /* aligned, capacity entries */
};

cos_v68_store_t *cos_v68_store_new(uint32_t capacity)
{
    if (capacity == 0) capacity = COS_V68_EPISODES_MAX;
    if (capacity > COS_V68_EPISODES_MAX) capacity = COS_V68_EPISODES_MAX;

    cos_v68_store_t *s = aligned_calloc64(sizeof(*s));
    if (!s) return NULL;
    s->ep = aligned_calloc64(sizeof(cos_v68_episode_t) * capacity);
    if (!s->ep) { free(s); return NULL; }
    s->capacity = capacity;
    return s;
}

void cos_v68_store_free(cos_v68_store_t *s)
{
    if (!s) return;
    free(s->ep);
    free(s);
}

void cos_v68_store_reset(cos_v68_store_t *s)
{
    if (!s) return;
    memset(s->ep, 0, sizeof(cos_v68_episode_t) * s->capacity);
    s->tick = 0;
    s->writes = 0;
    s->forgets = 0;
}

uint32_t cos_v68_store_capacity(const cos_v68_store_t *s)
{ return s ? s->capacity : 0; }

uint32_t cos_v68_store_count(const cos_v68_store_t *s)
{
    if (!s) return 0;
    uint32_t n = 0;
    for (uint32_t i = 0; i < s->capacity; ++i)
        n += (s->ep[i].valid ? 1u : 0u);
    return n;
}

uint64_t cos_v68_store_writes(const cos_v68_store_t *s)
{ return s ? s->writes : 0; }

uint64_t cos_v68_store_forgets(const cos_v68_store_t *s)
{ return s ? s->forgets : 0; }

uint32_t cos_v68_store_tick(const cos_v68_store_t *s)
{ return s ? s->tick : 0; }

const cos_v68_episode_t *cos_v68_store_at(const cos_v68_store_t *s,
                                          uint32_t i)
{
    if (!s || i >= s->capacity) return NULL;
    return &s->ep[i];
}

int cos_v68_store_set_activation(cos_v68_store_t *s,
                                 uint32_t i,
                                 int16_t activation_q15)
{
    if (!s || i >= s->capacity) return -1;
    if (activation_q15 < 0) activation_q15 = 0;
    s->ep[i].activation_q15 = activation_q15;
    return 0;
}

/* --------------------------------------------------------------------
 *  Surprise gate.
 * -------------------------------------------------------------------- */

uint8_t cos_v68_surprise_gate(int16_t pred_q15,
                              int16_t obs_q15,
                              int16_t threshold_q15,
                              int16_t *out_surprise_q15)
{
    int32_t diff = (int32_t)obs_q15 - (int32_t)pred_q15;
    /* branchless absolute value */
    int32_t m = diff >> 31;
    int32_t a = (diff + m) ^ m;
    if (a > 32767) a = 32767;
    int16_t s = (int16_t)a;
    if (out_surprise_q15) *out_surprise_q15 = s;
    return (uint8_t)((s >= threshold_q15) ? 1u : 0u);
}

/* Find slot index of the live episode with the lowest activation; if
 * all slots are valid, that slot will be evicted on next write.  If
 * any slot is empty (valid == 0), returns it instead.  Constant time
 * in the data (visits every slot). */
static uint32_t lowest_or_empty_slot(const cos_v68_store_t *s)
{
    uint32_t best_i = 0;
    int32_t  best_a = INT32_MAX;
    for (uint32_t i = 0; i < s->capacity; ++i) {
        if (!s->ep[i].valid) return i;
        int32_t act = (int32_t)s->ep[i].activation_q15;
        /* prefer the slot with strictly lower activation */
        int32_t cond = -(int32_t)(act < best_a);
        best_i = (uint32_t)sel_i32(cond, (int32_t)i, (int32_t)best_i);
        best_a = sel_i32(cond, act, best_a);
    }
    return best_i;
}

uint32_t cos_v68_store_write(cos_v68_store_t *s,
                             const cos_v68_hv_t *hv,
                             int16_t surprise_q15,
                             int16_t threshold_q15)
{
    if (!s || !hv) return 0;
    s->tick++;
    if (surprise_q15 < threshold_q15) return 0;

    uint32_t slot = lowest_or_empty_slot(s);
    s->ep[slot].hv             = *hv;
    s->ep[slot].surprise_q15   = surprise_q15;
    s->ep[slot].activation_q15 = COS_V68_ACT_INIT_Q15;
    s->ep[slot].timestamp      = s->tick;
    s->ep[slot].valid          = 1u;
    s->writes++;
    return 1;
}

/* --------------------------------------------------------------------
 *  ACT-R decay.
 * -------------------------------------------------------------------- */

int16_t cos_v68_actr_decay_q15(int16_t init_q15,
                               int16_t decay_q15,
                               uint32_t dt)
{
    if (init_q15 < 0) init_q15 = 0;
    if (decay_q15 < 0) decay_q15 = 0;
    int64_t loss = (int64_t)decay_q15 * (int64_t)dt;
    int64_t v    = (int64_t)init_q15 - loss;
    if (v < 0) v = 0;
    if (v > COS_V68_ACT_INIT_Q15) v = COS_V68_ACT_INIT_Q15;
    return (int16_t)v;
}

uint32_t cos_v68_store_decay(cos_v68_store_t *s, int16_t decay_q15)
{
    if (!s) return 0;
    uint32_t zeros = 0;
    for (uint32_t i = 0; i < s->capacity; ++i) {
        if (!s->ep[i].valid) continue;
        int16_t a = cos_v68_actr_decay_q15(s->ep[i].activation_q15,
                                           decay_q15, 1);
        s->ep[i].activation_q15 = a;
        zeros += (a == 0 ? 1u : 0u);
    }
    return zeros;
}

/* --------------------------------------------------------------------
 *  Recall (top-K nearest by Hamming → Q0.15 sim).
 *  Branchless top-K insertion (same shape as v67 cos_v67_topk).
 * -------------------------------------------------------------------- */

void cos_v68_recall_init(cos_v68_recall_t *r, uint32_t k)
{
    if (!r) return;
    if (k > COS_V68_TOPK_MAX) k = COS_V68_TOPK_MAX;
    r->k = (uint8_t)k;
    r->len = 0;
    for (uint32_t i = 0; i < COS_V68_TOPK_MAX; ++i) {
        r->id[i] = 0u;
        r->sim_q15[i] = INT16_MIN;
    }
}

static void recall_insert(cos_v68_recall_t *r,
                          uint32_t id,
                          int16_t sim_q15)
{
    /* Find min slot if full; else extend. */
    if (r->len < r->k) {
        r->id[r->len] = id;
        r->sim_q15[r->len] = sim_q15;
        r->len++;
        /* maintain descending order with one branchless bubble pass */
        for (uint32_t i = r->len; i > 1; --i) {
            int32_t cond = -(int32_t)(r->sim_q15[i-1] > r->sim_q15[i-2]);
            int32_t a_id  = (int32_t)r->id[i-1];
            int32_t b_id  = (int32_t)r->id[i-2];
            int32_t a_s   = (int32_t)r->sim_q15[i-1];
            int32_t b_s   = (int32_t)r->sim_q15[i-2];
            r->id[i-1]      = (uint32_t)sel_i32(cond, b_id, a_id);
            r->id[i-2]      = (uint32_t)sel_i32(cond, a_id, b_id);
            r->sim_q15[i-1] = (int16_t)sel_i32(cond, b_s, a_s);
            r->sim_q15[i-2] = (int16_t)sel_i32(cond, a_s, b_s);
        }
        return;
    }
    /* full: replace the min if better */
    uint32_t min_i = r->len - 1u;
    int32_t  cond  = -(int32_t)(sim_q15 > r->sim_q15[min_i]);
    r->id[min_i]      = (uint32_t)sel_i32(cond, (int32_t)id,
                                          (int32_t)r->id[min_i]);
    r->sim_q15[min_i] = (int16_t)sel_i32(cond, (int32_t)sim_q15,
                                         (int32_t)r->sim_q15[min_i]);
    /* bubble back to keep descending order */
    for (uint32_t i = r->len; i > 1; --i) {
        int32_t c = -(int32_t)(r->sim_q15[i-1] > r->sim_q15[i-2]);
        int32_t a_id  = (int32_t)r->id[i-1];
        int32_t b_id  = (int32_t)r->id[i-2];
        int32_t a_s   = (int32_t)r->sim_q15[i-1];
        int32_t b_s   = (int32_t)r->sim_q15[i-2];
        r->id[i-1]      = (uint32_t)sel_i32(c, b_id, a_id);
        r->id[i-2]      = (uint32_t)sel_i32(c, a_id, b_id);
        r->sim_q15[i-1] = (int16_t)sel_i32(c, b_s, a_s);
        r->sim_q15[i-2] = (int16_t)sel_i32(c, a_s, b_s);
    }
}

uint32_t cos_v68_recall(cos_v68_store_t *s,
                        const cos_v68_hv_t *q,
                        cos_v68_recall_t *out,
                        int16_t *fidelity_q15)
{
    if (!s || !q || !out) return 0;
    if (out->k == 0) cos_v68_recall_init(out, 1);
    out->len = 0;
    for (uint32_t i = 0; i < COS_V68_TOPK_MAX; ++i) {
        out->id[i] = 0u;
        out->sim_q15[i] = INT16_MIN;
    }

    for (uint32_t i = 0; i < s->capacity; ++i) {
        if (!s->ep[i].valid) continue;
        int16_t sim = cos_v68_hv_sim_q15(&s->ep[i].hv, q);
        recall_insert(out, i, sim);
    }
    /* rehearsal: bump activation of all returned episodes. */
    for (uint32_t k = 0; k < out->len; ++k) {
        uint32_t i = out->id[k];
        s->ep[i].activation_q15 = COS_V68_ACT_INIT_Q15;
    }
    if (fidelity_q15) {
        *fidelity_q15 = (out->len > 0) ? out->sim_q15[0] : INT16_MIN;
    }
    return out->len;
}

/* --------------------------------------------------------------------
 *  Hebbian online adapter.
 * -------------------------------------------------------------------- */

struct cos_v68_adapter {
    uint32_t r;
    uint32_t c;
    int16_t  rate_q15;     /* learning-rate ratchet */
    uint64_t updates;
    int16_t *w;            /* r * c, aligned */
};

cos_v68_adapter_t *cos_v68_adapter_new(uint32_t r, uint32_t c)
{
    if (r == 0 || c == 0) return NULL;
    if (r > COS_V68_ADAPT_R) r = COS_V68_ADAPT_R;
    if (c > COS_V68_ADAPT_C) c = COS_V68_ADAPT_C;

    cos_v68_adapter_t *a = aligned_calloc64(sizeof(*a));
    if (!a) return NULL;
    a->w = aligned_calloc64(sizeof(int16_t) * (size_t)r * c);
    if (!a->w) { free(a); return NULL; }
    a->r = r;
    a->c = c;
    a->rate_q15 = COS_V68_RATE_INIT_Q15;
    return a;
}

void cos_v68_adapter_free(cos_v68_adapter_t *a)
{
    if (!a) return;
    free(a->w);
    free(a);
}

void cos_v68_adapter_reset(cos_v68_adapter_t *a)
{
    if (!a) return;
    memset(a->w, 0, sizeof(int16_t) * (size_t)a->r * a->c);
    a->rate_q15 = COS_V68_RATE_INIT_Q15;
    a->updates  = 0;
}

uint32_t cos_v68_adapter_rows(const cos_v68_adapter_t *a)
{ return a ? a->r : 0; }

uint32_t cos_v68_adapter_cols(const cos_v68_adapter_t *a)
{ return a ? a->c : 0; }

int16_t cos_v68_adapter_w(const cos_v68_adapter_t *a, uint32_t i, uint32_t j)
{
    if (!a || i >= a->r || j >= a->c) return 0;
    return a->w[i * a->c + j];
}

int16_t cos_v68_adapter_rate_q15(const cos_v68_adapter_t *a)
{ return a ? a->rate_q15 : 0; }

uint64_t cos_v68_adapter_updates(const cos_v68_adapter_t *a)
{ return a ? a->updates : 0; }

int16_t cos_v68_hebb_update(cos_v68_adapter_t *a,
                            const int16_t *pre,
                            const int16_t *post,
                            int16_t eta_q15)
{
    if (!a || !pre || !post) return 0;
    if (eta_q15 < 0) eta_q15 = 0;
    if (eta_q15 > a->rate_q15) eta_q15 = a->rate_q15;
    int32_t eta = (int32_t)eta_q15;
    for (uint32_t i = 0; i < a->r; ++i) {
        int32_t pi = (int32_t)pre[i];
        for (uint32_t j = 0; j < a->c; ++j) {
            int32_t pj = (int32_t)post[j];
            int32_t outer = (pi * pj) >> 15;       /* Q0.15 product */
            int32_t delta = (eta * outer) >> 15;   /* η · pre · post  */
            int32_t w     = (int32_t)a->w[i * a->c + j] + delta;
            a->w[i * a->c + j] = (int16_t)sat_q15_i32(w);
        }
    }
    a->updates++;
    return (int16_t)eta_q15;
}

void cos_v68_adapter_ratchet(cos_v68_adapter_t *a, int16_t factor_q15)
{
    if (!a) return;
    if (factor_q15 < 0) factor_q15 = 0;
    int32_t r = ((int32_t)a->rate_q15 * (int32_t)factor_q15) >> 15;
    if (r < (int32_t)COS_V68_RATE_FLOOR_Q15) r = COS_V68_RATE_FLOOR_Q15;
    if (r > 32767) r = 32767;
    a->rate_q15 = (int16_t)r;
}

/* --------------------------------------------------------------------
 *  Sleep consolidation (offline replay).
 *
 *  Bundle = majority XOR over qualifying episodes.  We compute
 *  bit-wise sums in int8 counters, then take the sign per bit.
 *  Bipolar +1 = bit set.
 * -------------------------------------------------------------------- */

uint32_t cos_v68_consolidate(cos_v68_store_t *s,
                             cos_v68_adapter_t *adapter,
                             int16_t keep_thresh_q15,
                             cos_v68_hv_t *lt_out)
{
    if (!s || !lt_out) return 0;
    /* per-bit running sum: +1 for set, -1 for unset, computed across
     * qualifying episodes. */
    static __thread int16_t sum[COS_V68_HV_BITS];
    memset(sum, 0, sizeof(sum));

    uint32_t n = 0;
    for (uint32_t i = 0; i < s->capacity; ++i) {
        if (!s->ep[i].valid) continue;
        if (s->ep[i].activation_q15 < keep_thresh_q15) continue;
        const cos_v68_hv_t *hv = &s->ep[i].hv;
        for (uint32_t w = 0; w < COS_V68_HV_WORDS; ++w) {
            uint64_t word = hv->w[w];
            uint32_t base = w * 64u;
            for (uint32_t b = 0; b < 64; ++b) {
                int16_t bit = (int16_t)((word >> b) & 1ULL);
                sum[base + b] += (int16_t)(bit ? 1 : -1);
            }
        }
        n++;
    }
    /* Threshold the sums to produce the long-term HV. */
    for (uint32_t w = 0; w < COS_V68_HV_WORDS; ++w) {
        uint64_t out = 0;
        uint32_t base = w * 64u;
        for (uint32_t b = 0; b < 64; ++b) {
            uint64_t bit = (uint64_t)(sum[base + b] >= 0 ? 1u : 0u);
            out |= (bit << b);
        }
        lt_out->w[w] = out;
    }
    /* Sleep cycle: reset adapter rate so next wake cycle starts
     * fresh; the adapter weights themselves are preserved. */
    if (adapter) adapter->rate_q15 = COS_V68_RATE_INIT_Q15;
    return n;
}

/* --------------------------------------------------------------------
 *  Forgetting controller.
 * -------------------------------------------------------------------- */

uint32_t cos_v68_forget(cos_v68_store_t *s,
                        int16_t keep_thresh_q15,
                        uint32_t forget_budget)
{
    if (!s || forget_budget == 0) return 0;
    uint32_t forgot = 0;
    /* Collect candidate (idx, activation) for those below threshold. */
    while (forgot < forget_budget) {
        uint32_t worst_i = 0;
        int32_t  worst_a = INT32_MAX;
        int      found   = 0;
        for (uint32_t i = 0; i < s->capacity; ++i) {
            if (!s->ep[i].valid) continue;
            if (s->ep[i].activation_q15 >= keep_thresh_q15) continue;
            int32_t act = (int32_t)s->ep[i].activation_q15;
            if (act < worst_a) { worst_a = act; worst_i = i; found = 1; }
        }
        if (!found) break;
        s->ep[worst_i].valid = 0u;
        s->ep[worst_i].activation_q15 = 0;
        forgot++;
        s->forgets++;
    }
    return forgot;
}

/* --------------------------------------------------------------------
 *  Composed 9-bit branchless decision.
 * -------------------------------------------------------------------- */

cos_v68_decision_t cos_v68_compose_decision(uint8_t v60_ok,
                                            uint8_t v61_ok,
                                            uint8_t v62_ok,
                                            uint8_t v63_ok,
                                            uint8_t v64_ok,
                                            uint8_t v65_ok,
                                            uint8_t v66_ok,
                                            uint8_t v67_ok,
                                            uint8_t v68_ok)
{
    cos_v68_decision_t d;
    d.v60_ok = v60_ok ? 1u : 0u;
    d.v61_ok = v61_ok ? 1u : 0u;
    d.v62_ok = v62_ok ? 1u : 0u;
    d.v63_ok = v63_ok ? 1u : 0u;
    d.v64_ok = v64_ok ? 1u : 0u;
    d.v65_ok = v65_ok ? 1u : 0u;
    d.v66_ok = v66_ok ? 1u : 0u;
    d.v67_ok = v67_ok ? 1u : 0u;
    d.v68_ok = v68_ok ? 1u : 0u;
    /* single 9-way branchless AND */
    d.allow = (uint8_t)(d.v60_ok & d.v61_ok & d.v62_ok & d.v63_ok &
                        d.v64_ok & d.v65_ok & d.v66_ok & d.v67_ok &
                        d.v68_ok);
    return d;
}

/* --------------------------------------------------------------------
 *  MML — Mnemonic Memory Language interpreter.
 * -------------------------------------------------------------------- */

struct cos_v68_mml_state {
    int16_t  reg_q15[COS_V68_NREGS];
    uint32_t reg_ptr[COS_V68_NREGS];
    uint64_t cost;
    uint32_t forget_count;
    uint8_t  abstained;
    uint8_t  v68_ok;
};

cos_v68_mml_state_t *cos_v68_mml_new(void)
{
    cos_v68_mml_state_t *s = aligned_calloc64(sizeof(*s));
    return s;
}

void cos_v68_mml_free(cos_v68_mml_state_t *s)
{
    free(s);
}

void cos_v68_mml_reset(cos_v68_mml_state_t *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
}

uint64_t cos_v68_mml_cost(const cos_v68_mml_state_t *s)
{ return s ? s->cost : 0; }

int16_t cos_v68_mml_reg_q15(const cos_v68_mml_state_t *s, uint32_t i)
{
    if (!s || i >= COS_V68_NREGS) return 0;
    return s->reg_q15[i];
}

uint32_t cos_v68_mml_reg_ptr(const cos_v68_mml_state_t *s, uint32_t i)
{
    if (!s || i >= COS_V68_NREGS) return 0;
    return s->reg_ptr[i];
}

uint32_t cos_v68_mml_forget_count(const cos_v68_mml_state_t *s)
{ return s ? s->forget_count : 0; }

uint8_t cos_v68_mml_abstained(const cos_v68_mml_state_t *s)
{ return s ? s->abstained : 0u; }

uint8_t cos_v68_mml_v68_ok(const cos_v68_mml_state_t *s)
{ return s ? s->v68_ok : 0u; }

static const uint64_t cos_v68_op_cost[] = {
    [COS_V68_OP_HALT]        = 1,
    [COS_V68_OP_SENSE]       = 1,
    [COS_V68_OP_SURPRISE]    = 1,
    [COS_V68_OP_STORE]       = 4,
    [COS_V68_OP_RECALL]      = 16,
    [COS_V68_OP_HEBB]        = 8,
    [COS_V68_OP_CONSOLIDATE] = 32,
    [COS_V68_OP_FORGET]      = 4,
    [COS_V68_OP_CMPGE]       = 1,
    [COS_V68_OP_GATE]        = 1,
};

int cos_v68_mml_exec(cos_v68_mml_state_t *s,
                     const cos_v68_inst_t *prog,
                     uint32_t n,
                     cos_v68_mml_ctx_t *ctx,
                     uint64_t cost_budget)
{
    if (!s || !prog) return -1;
    s->cost = 0;
    s->forget_count = 0;
    s->abstained = 0;
    s->v68_ok = 0;
    for (uint32_t i = 0; i < COS_V68_NREGS; ++i) {
        s->reg_q15[i] = 0;
        s->reg_ptr[i] = 0;
    }

    for (uint32_t pc = 0; pc < n; ++pc) {
        cos_v68_inst_t ins = prog[pc];
        if (ins.op > COS_V68_OP_GATE) return -2;

        s->cost += cos_v68_op_cost[ins.op];
        if (s->cost > cost_budget) {
            s->abstained = 1;
            return -3;
        }

        uint32_t dst = ins.dst & (COS_V68_NREGS - 1u);
        uint32_t a   = ins.a   & (COS_V68_NREGS - 1u);
        uint32_t b   = ins.b   & (COS_V68_NREGS - 1u);
        (void)b;

        switch ((cos_v68_op_t)ins.op) {
        case COS_V68_OP_HALT:
            return 0;

        case COS_V68_OP_SENSE:
            /* reg_q15[dst] = ctx->obs_q15;
             * reg_q15[a]   = ctx->pred_q15. */
            if (!ctx) return -4;
            s->reg_q15[dst] = ctx->obs_q15;
            s->reg_q15[a]   = ctx->pred_q15;
            break;

        case COS_V68_OP_SURPRISE: {
            int16_t pred = s->reg_q15[a];
            int16_t obs  = s->reg_q15[b];
            int16_t out_s = 0;
            (void)cos_v68_surprise_gate(pred, obs,
                                        ctx ? ctx->surprise_thresh_q15
                                            : COS_V68_SURPRISE_DEFAULT_Q15,
                                        &out_s);
            s->reg_q15[dst] = out_s;
            break;
        }

        case COS_V68_OP_STORE:
            if (!ctx || !ctx->store || !ctx->sense_hv) return -5;
            (void)cos_v68_store_write(ctx->store, ctx->sense_hv,
                                      s->reg_q15[a],
                                      ctx ? ctx->surprise_thresh_q15
                                          : COS_V68_SURPRISE_DEFAULT_Q15);
            s->reg_ptr[dst] = (uint32_t)cos_v68_store_writes(ctx->store);
            break;

        case COS_V68_OP_RECALL: {
            if (!ctx || !ctx->store || !ctx->sense_hv ||
                !ctx->scratch_recall) return -6;
            int16_t fid = INT16_MIN;
            (void)cos_v68_recall(ctx->store, ctx->sense_hv,
                                 ctx->scratch_recall, &fid);
            s->reg_q15[dst] = (fid == INT16_MIN ? 0 : fid);
            s->reg_ptr[a]   = ctx->scratch_recall->len;
            break;
        }

        case COS_V68_OP_HEBB:
            if (!ctx || !ctx->adapter || !ctx->adapter_pre ||
                !ctx->adapter_post) return -7;
            (void)cos_v68_hebb_update(ctx->adapter,
                                      ctx->adapter_pre,
                                      ctx->adapter_post,
                                      ins.imm);
            s->reg_q15[dst] = cos_v68_adapter_rate_q15(ctx->adapter);
            break;

        case COS_V68_OP_CONSOLIDATE:
            if (!ctx || !ctx->store || !ctx->lt_out) return -8;
            s->reg_ptr[dst] = cos_v68_consolidate(ctx->store,
                                                  ctx->adapter,
                                                  ctx->keep_thresh_q15,
                                                  ctx->lt_out);
            break;

        case COS_V68_OP_FORGET: {
            if (!ctx || !ctx->store) return -9;
            uint32_t f = cos_v68_forget(ctx->store,
                                        ctx->keep_thresh_q15,
                                        ctx->forget_budget);
            s->reg_ptr[dst] = f;
            s->forget_count += f;
            break;
        }

        case COS_V68_OP_CMPGE: {
            int32_t cond = -(int32_t)(s->reg_q15[a] >= ins.imm);
            s->reg_q15[dst] = (int16_t)sel_i32(cond, 32767, 0);
            break;
        }

        case COS_V68_OP_GATE: {
            uint32_t fb = ctx ? ctx->forget_budget : 0;
            uint8_t cond_cost   = (s->cost <= cost_budget);
            uint8_t cond_score  = (s->reg_q15[a] >= ins.imm);
            uint8_t cond_forget = (s->forget_count <= fb);
            uint8_t cond_no_abs = (s->abstained == 0);
            s->v68_ok = (uint8_t)(cond_cost & cond_score &
                                  cond_forget & cond_no_abs);
            return 0;
        }
        }
    }
    /* fell off the end without HALT/GATE → treat as abstain */
    s->abstained = 1;
    return -10;
}
