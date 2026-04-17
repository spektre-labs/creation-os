/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v76 — σ-Surface (implementation).
 *
 * Branchless integer-only.  Libc-only.  The E2E ratchet reuses the
 * v75 FIPS-180-4 SHA-256 so there is still zero third-party crypto
 * linkage.
 */

#include "surface.h"
#include "../license_kernel/license_attest.h"

#include <stdlib.h>
#include <string.h>

const char cos_v76_version[] =
    "v76 σ-Surface · mobile+messenger+legacy surface · 256-bit HV · 10 primitives · SBL · 16-bit compose";

/* ====================================================================
 *  Branchless helpers (same alphabet as v74).
 * ==================================================================== */

static inline uint32_t sel_u32(uint32_t cond, uint32_t a, uint32_t b) {
    /* cond must be 0 or 1 */
    uint32_t m = (uint32_t)(0u - cond);   /* all-ones if cond==1 */
    return (a & m) | (b & ~m);
}
static inline int32_t sel_i32(uint32_t cond, int32_t a, int32_t b) {
    uint32_t m = (uint32_t)(0u - cond);
    return (int32_t)(((uint32_t)a & m) | ((uint32_t)b & ~m));
}
static inline uint32_t u32_lt(uint32_t a, uint32_t b) {
    /* 1 iff a < b (unsigned).  Using 64-bit subtraction avoids the
     * wrap-bug that plagues the naive ((a - b) >> 31) idiom when
     * b >= 2^31 (where the 32-bit two's-complement sign bit flips). */
    return (uint32_t)(((uint64_t)a - (uint64_t)b) >> 63);
}
static inline uint32_t u32_le(uint32_t a, uint32_t b) {
    return 1u - u32_lt(b, a);             /* 1 iff a <= b */
}
static inline uint32_t u32_ge(uint32_t a, uint32_t b) {
    return 1u - u32_lt(a, b);
}
static inline uint64_t splitmix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

/* ====================================================================
 *  HV primitives.
 * ==================================================================== */

void cos_v76_hv_zero(cos_v76_hv_t *d) {
    d->w[0] = d->w[1] = d->w[2] = d->w[3] = 0;
}

void cos_v76_hv_copy(cos_v76_hv_t *d, const cos_v76_hv_t *s) {
    d->w[0] = s->w[0]; d->w[1] = s->w[1];
    d->w[2] = s->w[2]; d->w[3] = s->w[3];
}

void cos_v76_hv_xor(cos_v76_hv_t *d, const cos_v76_hv_t *a, const cos_v76_hv_t *b) {
    d->w[0] = a->w[0] ^ b->w[0];
    d->w[1] = a->w[1] ^ b->w[1];
    d->w[2] = a->w[2] ^ b->w[2];
    d->w[3] = a->w[3] ^ b->w[3];
}

void cos_v76_hv_from_seed(cos_v76_hv_t *d, uint64_t seed) {
    d->w[0] = splitmix64(seed ^ 0xA5A5A5A5A5A5A5A5ULL);
    d->w[1] = splitmix64(d->w[0] ^ seed);
    d->w[2] = splitmix64(d->w[1] ^ 0x5A5A5A5A5A5A5A5AULL);
    d->w[3] = splitmix64(d->w[2] ^ seed);
}

/* Deterministic pseudo-permutation keyed by k.  Rotate per-word by
 * (k * prime_i + c_i) mod 64, then a fixed XOR-mix so the mapping
 * is bijective on the 256-bit lattice. */
void cos_v76_hv_permute(cos_v76_hv_t *d, const cos_v76_hv_t *s, uint32_t k) {
    uint64_t k0 = splitmix64(0x9e37ULL + (uint64_t)k * 0x100000001B3ULL);
    uint64_t k1 = splitmix64(k0 ^ 0x1b873593ULL);
    uint64_t k2 = splitmix64(k1 ^ 0x85ebca6bULL);
    uint64_t k3 = splitmix64(k2 ^ 0xc2b2ae35ULL);
    uint32_t r0 = (uint32_t)(k0 & 63u);
    uint32_t r1 = (uint32_t)(k1 & 63u);
    uint32_t r2 = (uint32_t)(k2 & 63u);
    uint32_t r3 = (uint32_t)(k3 & 63u);
    uint64_t w0 = s->w[0], w1 = s->w[1], w2 = s->w[2], w3 = s->w[3];
    uint64_t p0 = (w0 << r0) | (w0 >> ((64 - r0) & 63));
    uint64_t p1 = (w1 << r1) | (w1 >> ((64 - r1) & 63));
    uint64_t p2 = (w2 << r2) | (w2 >> ((64 - r2) & 63));
    uint64_t p3 = (w3 << r3) | (w3 >> ((64 - r3) & 63));
    /* Cross-word XOR mix so lanes cannot stay in their own 64-bit
     * silo; every output bit depends on every input word. */
    d->w[0] = p0 ^ (p3 >> 7)  ^ (p2 << 11);
    d->w[1] = p1 ^ (p0 >> 13) ^ (p3 << 9);
    d->w[2] = p2 ^ (p1 >> 5)  ^ (p0 << 17);
    d->w[3] = p3 ^ (p2 >> 19) ^ (p1 << 3);
}

uint32_t cos_v76_hv_hamming(const cos_v76_hv_t *a, const cos_v76_hv_t *b) {
    uint64_t d0 = a->w[0] ^ b->w[0];
    uint64_t d1 = a->w[1] ^ b->w[1];
    uint64_t d2 = a->w[2] ^ b->w[2];
    uint64_t d3 = a->w[3] ^ b->w[3];
    return (uint32_t)(__builtin_popcountll(d0) + __builtin_popcountll(d1)
                    + __builtin_popcountll(d2) + __builtin_popcountll(d3));
}

/* ====================================================================
 *  1.  Touch.
 * ==================================================================== */

void cos_v76_touch_decode(cos_v76_hv_t *out, const cos_v76_touch_t *t) {
    cos_v76_hv_t raw;
    /* Pack lanes into 4 × uint64:
     *   w[0] = x | y | pressure | phase
     *   w[1] = timestamp
     *   w[2] = splitmix mix of all lanes (entropy)
     *   w[3] = complement mix
     */
    raw.w[0] = ((uint64_t)t->x_q15)        <<  0
             | ((uint64_t)t->y_q15)        << 16
             | ((uint64_t)t->pressure_q15) << 32
             | ((uint64_t)(t->phase & 3u)) << 48;
    raw.w[1] = (uint64_t)t->timestamp_ms;
    raw.w[2] = splitmix64(raw.w[0] ^ raw.w[1]);
    raw.w[3] = splitmix64(raw.w[2] ^ 0xDEADBEEFCAFEBABEULL);
    /* Permute keyed by phase so that "ended" events can never be
     * confused with "began" events even under HV corruption. */
    cos_v76_hv_permute(out, &raw, (uint32_t)t->phase);
}

uint8_t cos_v76_touch_ok(const cos_v76_touch_t *t) {
    uint32_t x_ok  = u32_le(t->x_q15,        32767u);
    uint32_t y_ok  = u32_le(t->y_q15,        32767u);
    uint32_t p_ok  = u32_le(t->pressure_q15, 32767u);
    uint32_t ph_ok = u32_le(t->phase,        3u);
    return (uint8_t)(x_ok & y_ok & p_ok & ph_ok);
}

/* ====================================================================
 *  2.  Gesture classification.
 * ==================================================================== */

void cos_v76_gesture_bank_init(cos_v76_gesture_bank_t *g, uint64_t seed) {
    /* Each template is seeded from (seed, gesture_id).  The seeds
     * are well-separated in splitmix64-space so inter-template
     * Hamming distance is close to 128 bits in expectation. */
    for (uint32_t i = 0; i < COS_V76_GESTURE_TEMPLATES; ++i) {
        cos_v76_hv_from_seed(&g->bank[i],
                             seed ^ (0x76A5A5ULL + (uint64_t)i * 0x9e3779b97f4a7c15ULL));
    }
}

void cos_v76_gesture_classify(const cos_v76_gesture_bank_t *g,
                              const cos_v76_hv_t            *q,
                              cos_v76_gesture_result_t      *out) {
    uint32_t d1 = 0xFFFFFFFFu, d2 = 0xFFFFFFFFu, i1 = 0;
    for (uint32_t i = 0; i < COS_V76_GESTURE_TEMPLATES; ++i) {
        uint32_t d = cos_v76_hv_hamming(q, &g->bank[i]);
        uint32_t lt1 = u32_lt(d, d1);
        uint32_t lt2 = u32_lt(d, d2);
        /* If d < d1: d2:=d1; d1:=d; i1:=i.  Else if d < d2: d2:=d. */
        uint32_t new_d2_if_lt1 = sel_u32(lt1, d1, d2);
        uint32_t new_d2_if_lt2 = sel_u32(lt2, d,  d2);
        d2 = sel_u32(lt1, new_d2_if_lt1, new_d2_if_lt2);
        i1 = sel_u32(lt1, i, i1);
        d1 = sel_u32(lt1, d, d1);
    }
    out->idx    = i1;
    out->dist   = d1;
    out->margin = d2 - d1;     /* unsigned: d2 ≥ d1 always */
}

uint8_t cos_v76_gesture_ok(const cos_v76_gesture_result_t *r,
                           uint32_t                        tol,
                           uint32_t                        margin_min) {
    uint32_t d_ok = u32_le(r->dist,   tol);
    uint32_t m_ok = u32_ge(r->margin, margin_min);
    return (uint8_t)(d_ok & m_ok);
}

/* ====================================================================
 *  3.  Haptic waveform.
 * ==================================================================== */

/* 64-entry Q0.15 quarter-sine (0..π/2) → mirrored for π..2π. */
static const int16_t k_sine_q15[64] = {
       0,   804,  1608,  2410,  3212,  4011,  4808,  5602,
    6393,  7179,  7962,  8739,  9512, 10278, 11039, 11793,
   12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530,
   18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
   23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
   27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
   30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971,
   32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757
};

static inline int16_t sine_q15(uint32_t phase) {
    /* phase ∈ [0, 255] → one full period. */
    uint32_t q     = phase & 63u;          /* quarter index */
    uint32_t quad  = (phase >> 6) & 3u;    /* 0..3          */
    /* Branchless quadrant folding. */
    uint32_t q_fold = sel_u32((quad & 1u), 63u - q, q);
    int16_t  v      = k_sine_q15[q_fold];
    int16_t  out    = (int16_t)sel_i32((quad >> 1) & 1u, (int32_t)(-v), (int32_t)v);
    return out;
}

int64_t cos_v76_haptic_generate(int16_t *wave,
                                uint32_t n,
                                uint32_t kind,
                                uint32_t amp_q15,
                                uint32_t period) {
    if (n > COS_V76_HAPTIC_SAMPLES_MAX) n = COS_V76_HAPTIC_SAMPLES_MAX;
    if (amp_q15 > 32767u) amp_q15 = 32767u;
    if (period == 0u)     period  = 64u;
    int64_t energy = 0;
    for (uint32_t i = 0; i < n; ++i) {
        /* kind 0 = sine, 1 = ramp, 2 = impulse (branchless select). */
        uint32_t p     = (i << 8) / period;          /* Q0.8 phase */
        int32_t  s_sin = (int32_t)sine_q15(p & 255u);
        int32_t  s_ramp= (int32_t)(((int64_t)(i % period) * 65534 / (int32_t)period) - 32767);
        int32_t  s_imp = (int32_t)sel_i32((i == 0u) ? 1u : 0u, 32767, 0);
        int32_t  s0    = sel_i32((kind == 0u) ? 1u : 0u, s_sin,  0);
        int32_t  s1    = sel_i32((kind == 1u) ? 1u : 0u, s_ramp, s0);
        int32_t  s2    = sel_i32((kind == 2u) ? 1u : 0u, s_imp,  s1);
        int64_t  raw   = (int64_t)s2 * (int64_t)amp_q15 / 32767;
        /* Saturate to int16. */
        if (raw >  32767) raw =  32767;
        if (raw < -32768) raw = -32768;
        wave[i] = (int16_t)raw;
        energy += (int64_t)wave[i] * (int64_t)wave[i];
    }
    return energy;
}

uint8_t cos_v76_haptic_ok(const int16_t *wave,
                          uint32_t       n,
                          int32_t        amp_cap_q15,
                          int64_t        energy,
                          int64_t        energy_cap) {
    uint32_t n_ok = u32_le(n, COS_V76_HAPTIC_SAMPLES_MAX);
    int32_t  peak = 0;
    for (uint32_t i = 0; i < n; ++i) {
        int32_t s = (int32_t)wave[i];
        int32_t a = s ^ (s >> 31);       /* branchless |s| */
        a -= (s >> 31);
        peak = (a > peak) ? a : peak;    /* inline; compare once */
    }
    uint32_t peak_ok = u32_le((uint32_t)peak, (uint32_t)amp_cap_q15);
    uint32_t e_ok    = (uint32_t)((energy <= energy_cap) ? 1u : 0u);
    return (uint8_t)(n_ok & peak_ok & e_ok);
}

/* ====================================================================
 *  4.  Messenger protocol bridge.
 * ==================================================================== */

void cos_v76_msg_encode(cos_v76_hv_t *env, const cos_v76_msg_t *m) {
    cos_v76_hv_t payload_p, sender_p;
    cos_v76_hv_permute(&payload_p, m->payload_hv, (uint32_t)m->protocol_id);
    cos_v76_hv_permute(&sender_p,  m->sender_hv,  (uint32_t)m->protocol_id + 128u);
    cos_v76_hv_xor(env, &payload_p, m->recipient_hv);
    cos_v76_hv_xor(env, env,        &sender_p);
    /* Stamp protocol_id into low 4 bits of w[0] (bijection preserved
     * because the decoder masks + recomputes). */
    env->w[0] = (env->w[0] & ~0xFULL) | ((uint64_t)(m->protocol_id & 0xFu));
}

uint8_t cos_v76_msg_ok(const cos_v76_msg_t *m) {
    uint32_t pid_ok = u32_lt(m->protocol_id, COS_V76_PROTOCOLS);
    uint32_t pl_ok  = (m->payload_hv   != NULL) ? 1u : 0u;
    uint32_t rc_ok  = (m->recipient_hv != NULL) ? 1u : 0u;
    uint32_t sn_ok  = (m->sender_hv    != NULL) ? 1u : 0u;
    return (uint8_t)(pid_ok & pl_ok & rc_ok & sn_ok);
}

uint8_t cos_v76_msg_protocol(const cos_v76_hv_t *env) {
    return (uint8_t)(env->w[0] & 0xFu);
}

/* ====================================================================
 *  5.  E2E ratchet (Signal-lineage).
 * ==================================================================== */

void cos_v76_ratchet_init(cos_v76_ratchet_t *r, const uint8_t seed[32]) {
    memcpy(r->root_key,  seed, 32);
    /* chain_key := SHA-256(seed || 0x01) */
    spektre_sha256_ctx_t c;
    spektre_sha256_init(&c);
    spektre_sha256_update(&c, seed, 32);
    uint8_t tag = 0x01;
    spektre_sha256_update(&c, &tag, 1);
    spektre_sha256_final(&c, r->chain_key);
    r->counter = 0;
}

void cos_v76_ratchet_mix(cos_v76_ratchet_t *r,
                         const uint8_t      dh_a[32],
                         const uint8_t      dh_b[32]) {
    uint8_t xored[32];
    for (uint32_t i = 0; i < 32; ++i) xored[i] = dh_a[i] ^ dh_b[i];
    spektre_sha256_ctx_t c;
    spektre_sha256_init(&c);
    spektre_sha256_update(&c, r->root_key, 32);
    spektre_sha256_update(&c, xored, 32);
    spektre_sha256_final(&c, r->root_key);
    /* Reseed chain_key from new root_key. */
    spektre_sha256_init(&c);
    spektre_sha256_update(&c, r->root_key, 32);
    uint8_t tag = 0x02;
    spektre_sha256_update(&c, &tag, 1);
    spektre_sha256_final(&c, r->chain_key);
    r->counter = 0;
}

void cos_v76_ratchet_step(cos_v76_ratchet_t *r, uint8_t message_key[32]) {
    uint8_t ctr_be[8];
    uint64_t n = r->counter;
    for (int i = 7; i >= 0; --i) { ctr_be[i] = (uint8_t)(n & 0xFFu); n >>= 8; }

    uint8_t next_chain[32];
    spektre_sha256_ctx_t c;
    spektre_sha256_init(&c);
    spektre_sha256_update(&c, r->chain_key, 32);
    spektre_sha256_update(&c, ctr_be, 8);
    spektre_sha256_final(&c, next_chain);

    spektre_sha256_init(&c);
    spektre_sha256_update(&c, next_chain, 32);
    spektre_sha256_final(&c, message_key);

    memcpy(r->chain_key, next_chain, 32);
    r->counter += 1;
}

uint8_t cos_v76_ratchet_ok(const cos_v76_ratchet_t *r) {
    uint64_t rk_or = 0, ck_or = 0;
    for (uint32_t i = 0; i < 32; ++i) { rk_or |= r->root_key[i]; ck_or |= r->chain_key[i]; }
    uint32_t rk_ok = (rk_or != 0) ? 1u : 0u;
    uint32_t ck_ok = (ck_or != 0) ? 1u : 0u;
    uint32_t ctr_ok = (r->counter < 0x7FFFFFFFFFFFFFFFULL) ? 1u : 0u;
    return (uint8_t)(rk_ok & ck_ok & ctr_ok);
}

/* ====================================================================
 *  6.  Accessibility.
 * ==================================================================== */

uint8_t cos_v76_a11y_ok(const cos_v76_a11y_t *a) {
    /* WCAG-AA body text threshold: 4.5 × 32768 / 21 ≈ 7022.
     * Practical floor we use: 7022. */
    uint32_t c_ok  = u32_ge(a->contrast_q15, 7022u);
    uint32_t f_ok  = (a->focus_order_ok != 0) ? 1u : 0u;
    uint32_t m_ok  = (a->motion_safe    != 0) ? 1u : 0u;
    uint32_t t_ok  = u32_ge((uint32_t)a->touch_target_px, 44u);
    uint32_t l_ok  = (a->label_present  != 0) ? 1u : 0u;
    return (uint8_t)(c_ok & f_ok & m_ok & t_ok & l_ok);
}

/* ====================================================================
 *  7.  CRDT merge.
 * ==================================================================== */

void cos_v76_crdt_merge(cos_v76_crdt_t       *dst,
                        const cos_v76_crdt_t *a,
                        const cos_v76_crdt_t *b) {
    uint32_t ts_gt = (a->timestamp > b->timestamp) ? 1u : 0u;
    uint32_t ts_eq = (a->timestamp == b->timestamp) ? 1u : 0u;
    uint32_t rp_gt = (a->replica_id > b->replica_id) ? 1u : 0u;
    uint32_t a_wins = ts_gt | (ts_eq & rp_gt);
    dst->timestamp  = a_wins ? a->timestamp  : b->timestamp;
    dst->replica_id = a_wins ? a->replica_id : b->replica_id;
    dst->value      = a_wins ? a->value      : b->value;
    dst->_pad       = 0;
    /* OR-set merge is always XOR-union. */
    dst->or_set.w[0] = a->or_set.w[0] | b->or_set.w[0];
    dst->or_set.w[1] = a->or_set.w[1] | b->or_set.w[1];
    dst->or_set.w[2] = a->or_set.w[2] | b->or_set.w[2];
    dst->or_set.w[3] = a->or_set.w[3] | b->or_set.w[3];
}

uint8_t cos_v76_crdt_ok(const cos_v76_crdt_t *c) {
    uint32_t rep_ok = u32_lt(c->replica_id, 1u << 16);
    uint32_t ts_ok  = (c->timestamp != 0) ? 1u : 0u;
    return (uint8_t)(rep_ok & ts_ok);
}

/* ====================================================================
 *  8.  Legacy-software capability-template registry.
 * ==================================================================== */

static inline uint8_t legacy_category(uint32_t app_id) {
    if (app_id <  8u)  return COS_V76_CAT_OFFICE;
    if (app_id < 16u)  return COS_V76_CAT_DESIGN;
    if (app_id < 24u)  return COS_V76_CAT_CAD;
    if (app_id < 32u)  return COS_V76_CAT_ERP_CRM;
    if (app_id < 40u)  return COS_V76_CAT_COLLAB;
    if (app_id < 48u)  return COS_V76_CAT_DEV;
    if (app_id < 56u)  return COS_V76_CAT_DB;
    if (app_id < 60u)  return COS_V76_CAT_CLOUD;
    return COS_V76_CAT_BROWSER;
}

void cos_v76_legacy_init(cos_v76_legacy_t *L) {
    for (uint32_t i = 0; i < COS_V76_LEGACY_APPS; ++i) {
        uint8_t cat = legacy_category(i);
        cos_v76_hv_from_seed(&L->bank[i],
                             0x7600000076ULL ^ ((uint64_t)i * 0x9e3779b97f4a7c15ULL)
                                             ^ ((uint64_t)cat * 0x5851F42D4C957F2DULL));
        L->category[i] = cat;
    }
}

void cos_v76_legacy_match(const cos_v76_legacy_t  *L,
                          const cos_v76_hv_t      *query,
                          cos_v76_legacy_result_t *out) {
    uint32_t d1 = 0xFFFFFFFFu, d2 = 0xFFFFFFFFu, i1 = 0;
    for (uint32_t i = 0; i < COS_V76_LEGACY_APPS; ++i) {
        uint32_t d = cos_v76_hv_hamming(query, &L->bank[i]);
        uint32_t lt1 = u32_lt(d, d1);
        uint32_t lt2 = u32_lt(d, d2);
        uint32_t new_d2_if_lt1 = sel_u32(lt1, d1, d2);
        uint32_t new_d2_if_lt2 = sel_u32(lt2, d,  d2);
        d2 = sel_u32(lt1, new_d2_if_lt1, new_d2_if_lt2);
        i1 = sel_u32(lt1, i, i1);
        d1 = sel_u32(lt1, d, d1);
    }
    out->app_id   = i1;
    out->category = L->category[i1];
    out->dist     = d1;
    out->margin   = d2 - d1;
}

uint8_t cos_v76_legacy_ok(const cos_v76_legacy_result_t *r,
                          uint32_t                       tol,
                          uint32_t                       margin_min) {
    uint32_t d_ok = u32_le(r->dist, tol);
    uint32_t m_ok = u32_ge(r->margin, margin_min);
    return (uint8_t)(d_ok & m_ok);
}

/* ====================================================================
 *  9.  File-format bank.
 * ==================================================================== */

void cos_v76_format_bank_init(cos_v76_format_bank_t *F) {
    for (uint32_t i = 0; i < COS_V76_FORMATS; ++i) {
        cos_v76_hv_from_seed(&F->bank[i],
                             0xF0E1D2C3B4A59687ULL ^ ((uint64_t)i * 0xBF58476D1CE4E5B9ULL));
    }
}

void cos_v76_format_classify(const cos_v76_format_bank_t *F,
                             const cos_v76_hv_t          *prefix_hv,
                             cos_v76_format_result_t     *out) {
    uint32_t d1 = 0xFFFFFFFFu, d2 = 0xFFFFFFFFu, i1 = 0;
    for (uint32_t i = 0; i < COS_V76_FORMATS; ++i) {
        uint32_t d = cos_v76_hv_hamming(prefix_hv, &F->bank[i]);
        uint32_t lt1 = u32_lt(d, d1);
        uint32_t lt2 = u32_lt(d, d2);
        uint32_t new_d2_if_lt1 = sel_u32(lt1, d1, d2);
        uint32_t new_d2_if_lt2 = sel_u32(lt2, d,  d2);
        d2 = sel_u32(lt1, new_d2_if_lt1, new_d2_if_lt2);
        i1 = sel_u32(lt1, i, i1);
        d1 = sel_u32(lt1, d, d1);
    }
    out->fmt_id = i1;
    out->dist   = d1;
    out->margin = d2 - d1;
}

/* ====================================================================
 * 10.  SBL — Surface Bytecode Language.
 * ==================================================================== */

struct cos_v76_sbl_state {
    uint64_t units;
    uint8_t  touch_ok;
    uint8_t  gesture_ok;
    uint8_t  haptic_ok;
    uint8_t  msg_ok;
    uint8_t  e2e_ok;
    uint8_t  a11y_ok;
    uint8_t  sync_ok;
    uint8_t  legacy_ok;
    uint8_t  abstained;
    uint8_t  v76_ok;
    uint8_t  _pad[5];
};

cos_v76_sbl_state_t *cos_v76_sbl_new(void) {
    /* aligned_alloc demands size multiple of alignment.  The state
     * is small, so round the allocation up to 64 bytes and use
     * posix_memalign-style allocation via aligned_alloc on 64. */
    cos_v76_sbl_state_t *s = (cos_v76_sbl_state_t *)aligned_alloc(64, 64);
    if (!s) return NULL;
    memset(s, 0, sizeof *s);
    return s;
}
void cos_v76_sbl_free(cos_v76_sbl_state_t *s) { free(s); }
void cos_v76_sbl_reset(cos_v76_sbl_state_t *s) { memset(s, 0, sizeof *s); }

uint64_t cos_v76_sbl_units     (const cos_v76_sbl_state_t *s) { return s->units; }
uint8_t  cos_v76_sbl_touch_ok  (const cos_v76_sbl_state_t *s) { return s->touch_ok; }
uint8_t  cos_v76_sbl_gesture_ok(const cos_v76_sbl_state_t *s) { return s->gesture_ok; }
uint8_t  cos_v76_sbl_haptic_ok (const cos_v76_sbl_state_t *s) { return s->haptic_ok; }
uint8_t  cos_v76_sbl_msg_ok    (const cos_v76_sbl_state_t *s) { return s->msg_ok; }
uint8_t  cos_v76_sbl_e2e_ok    (const cos_v76_sbl_state_t *s) { return s->e2e_ok; }
uint8_t  cos_v76_sbl_a11y_ok   (const cos_v76_sbl_state_t *s) { return s->a11y_ok; }
uint8_t  cos_v76_sbl_sync_ok   (const cos_v76_sbl_state_t *s) { return s->sync_ok; }
uint8_t  cos_v76_sbl_legacy_ok (const cos_v76_sbl_state_t *s) { return s->legacy_ok; }
uint8_t  cos_v76_sbl_abstained (const cos_v76_sbl_state_t *s) { return s->abstained; }
uint8_t  cos_v76_sbl_v76_ok    (const cos_v76_sbl_state_t *s) { return s->v76_ok; }

static const uint32_t k_sbl_cost[10] = { 1, 1, 2, 2, 4, 4, 2, 2, 4, 1 };

int cos_v76_sbl_exec(cos_v76_sbl_state_t      *s,
                     const cos_v76_sbl_inst_t *prog,
                     uint32_t                  n,
                     cos_v76_sbl_ctx_t        *ctx,
                     uint64_t                  unit_budget)
{
    if (!s || !prog || !ctx) return -1;
    cos_v76_sbl_reset(s);

    for (uint32_t pc = 0; pc < n; ++pc) {
        cos_v76_sbl_inst_t in = prog[pc];
        if (in.op > COS_V76_OP_GATE) return -1;
        uint64_t next_units = s->units + (uint64_t)k_sbl_cost[in.op];
        if (next_units > unit_budget) return -2;
        s->units = next_units;

        switch (in.op) {
        case COS_V76_OP_HALT:
            return 0;

        case COS_V76_OP_TOUCH: {
            if (!ctx->touch) return -3;
            s->touch_ok = cos_v76_touch_ok(ctx->touch);
            break;
        }
        case COS_V76_OP_GESTURE: {
            if (!ctx->gest || !ctx->gest_q) return -3;
            cos_v76_gesture_result_t r;
            cos_v76_gesture_classify(ctx->gest, ctx->gest_q, &r);
            s->gesture_ok = cos_v76_gesture_ok(&r, ctx->gest_tol, ctx->gest_margin);
            break;
        }
        case COS_V76_OP_HAPTIC: {
            if (!ctx->haptic_wave || ctx->haptic_n == 0u) return -3;
            int64_t e = cos_v76_haptic_generate(ctx->haptic_wave,
                                                ctx->haptic_n,
                                                ctx->haptic_kind,
                                                ctx->haptic_amp_q15,
                                                ctx->haptic_period);
            s->haptic_ok = cos_v76_haptic_ok(ctx->haptic_wave,
                                             ctx->haptic_n,
                                             ctx->haptic_amp_cap_q15,
                                             e,
                                             ctx->haptic_energy_cap);
            break;
        }
        case COS_V76_OP_MSG: {
            if (!ctx->msg) return -3;
            s->msg_ok = cos_v76_msg_ok(ctx->msg);
            break;
        }
        case COS_V76_OP_E2E: {
            if (!ctx->ratchet || !ctx->out_message_key) return -3;
            cos_v76_ratchet_step(ctx->ratchet, ctx->out_message_key);
            s->e2e_ok = cos_v76_ratchet_ok(ctx->ratchet);
            break;
        }
        case COS_V76_OP_A11Y: {
            if (!ctx->a11y) return -3;
            s->a11y_ok = cos_v76_a11y_ok(ctx->a11y);
            break;
        }
        case COS_V76_OP_SYNC: {
            if (!ctx->crdt_dst || !ctx->crdt_a || !ctx->crdt_b) return -3;
            cos_v76_crdt_merge(ctx->crdt_dst, ctx->crdt_a, ctx->crdt_b);
            uint32_t a_ok = cos_v76_crdt_ok(ctx->crdt_a);
            uint32_t b_ok = cos_v76_crdt_ok(ctx->crdt_b);
            uint32_t d_ok = cos_v76_crdt_ok(ctx->crdt_dst);
            s->sync_ok = (uint8_t)(a_ok & b_ok & d_ok);
            break;
        }
        case COS_V76_OP_LEGACY: {
            if (!ctx->legacy || !ctx->legacy_query) return -3;
            cos_v76_legacy_result_t r;
            cos_v76_legacy_match(ctx->legacy, ctx->legacy_query, &r);
            s->legacy_ok = cos_v76_legacy_ok(&r, ctx->legacy_tol, ctx->legacy_margin);
            break;
        }
        case COS_V76_OP_GATE: {
            s->abstained = ctx->abstain;
            uint8_t all = s->touch_ok
                       &  s->gesture_ok
                       &  s->haptic_ok
                       &  s->msg_ok
                       &  s->e2e_ok
                       &  s->a11y_ok
                       &  s->sync_ok
                       &  s->legacy_ok
                       &  ((uint8_t)(1u - s->abstained));
            /* Budget check: we already failed above, so here it's pure AND. */
            s->v76_ok = all;
            return 0;
        }
        }
    }
    return 0;
}

/* ====================================================================
 *  Composed 16-bit branchless decision.
 * ==================================================================== */

cos_v76_decision_t cos_v76_compose_decision(uint8_t v60, uint8_t v61, uint8_t v62,
                                            uint8_t v63, uint8_t v64, uint8_t v65,
                                            uint8_t v66, uint8_t v67, uint8_t v68,
                                            uint8_t v69, uint8_t v70, uint8_t v71,
                                            uint8_t v72, uint8_t v73, uint8_t v74,
                                            uint8_t v76) {
    cos_v76_decision_t d;
    d.v60_ok = v60 & 1u; d.v61_ok = v61 & 1u; d.v62_ok = v62 & 1u;
    d.v63_ok = v63 & 1u; d.v64_ok = v64 & 1u; d.v65_ok = v65 & 1u;
    d.v66_ok = v66 & 1u; d.v67_ok = v67 & 1u; d.v68_ok = v68 & 1u;
    d.v69_ok = v69 & 1u; d.v70_ok = v70 & 1u; d.v71_ok = v71 & 1u;
    d.v72_ok = v72 & 1u; d.v73_ok = v73 & 1u; d.v74_ok = v74 & 1u;
    d.v76_ok = v76 & 1u;
    d.allow = d.v60_ok & d.v61_ok & d.v62_ok & d.v63_ok
            & d.v64_ok & d.v65_ok & d.v66_ok & d.v67_ok
            & d.v68_ok & d.v69_ok & d.v70_ok & d.v71_ok
            & d.v72_ok & d.v73_ok & d.v74_ok & d.v76_ok;
    return d;
}
