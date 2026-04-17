/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v66 — σ-Silicon (the matrix substrate).
 *
 * Implementation file.  Branchless on the inner loop wherever the
 * arithmetic permits, integer-only on the decision surface, libc-only.
 * NEON `vdotq_s32` / `vmlal_s16` paths on AArch64; portable scalar
 * fallback on every other host.  The SME path is reserved at compile
 * time behind `COS_V66_SME=1` so the default build never emits an
 * SME instruction (avoids SIGILL on hosts without FEAT_SME).
 */

#include "silicon.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
  #include <sys/sysctl.h>
#endif

#if defined(__aarch64__) && defined(__ARM_NEON)
  #include <arm_neon.h>
  #define COS_V66_HAVE_NEON 1
#else
  #define COS_V66_HAVE_NEON 0
#endif

const char cos_v66_version[] =
    "66.0.0 — v66.0 sigma-silicon "
    "(int8-gemv + ternary-gemv + native-ternary-wire + cfc-conformal + hsl)";

/* ====================================================================
 *  0.  CPU feature detection — sysctl on Darwin, getauxval on Linux,
 *      cached after the first call.
 * ==================================================================== */

static uint32_t g_features_cache;
static int      g_features_ready;

#if defined(__APPLE__)
static int cos_v66_sysctl_int(const char *name)
{
    int      val = 0;
    size_t   len = sizeof(val);
    if (sysctlbyname(name, &val, &len, NULL, 0) != 0) return 0;
    return val;
}
#endif

uint32_t cos_v66_features(void)
{
    if (g_features_ready) return g_features_cache;
    uint32_t f = 0;

#if COS_V66_HAVE_NEON
    f |= COS_V66_FEAT_NEON;
#endif

#if defined(__APPLE__) && defined(__aarch64__)
    if (cos_v66_sysctl_int("hw.optional.arm.FEAT_DotProd")) f |= COS_V66_FEAT_DOTPROD;
    if (cos_v66_sysctl_int("hw.optional.arm.FEAT_I8MM"))    f |= COS_V66_FEAT_I8MM;
    if (cos_v66_sysctl_int("hw.optional.arm.FEAT_BF16"))    f |= COS_V66_FEAT_BF16;
    if (cos_v66_sysctl_int("hw.optional.arm.FEAT_SVE"))     f |= COS_V66_FEAT_SVE;
    if (cos_v66_sysctl_int("hw.optional.arm.FEAT_SME"))     f |= COS_V66_FEAT_SME;
    if (cos_v66_sysctl_int("hw.optional.arm.FEAT_SME2"))    f |= COS_V66_FEAT_SME2;
#endif

    g_features_cache = f;
    g_features_ready = 1;
    return f;
}

size_t cos_v66_features_describe(char *buf, size_t cap)
{
    if (!buf || cap == 0) return 0;
    uint32_t f = cos_v66_features();

    /* Branchless concatenation via fixed table. */
    static const struct { uint32_t bit; const char *name; } tab[] = {
        { COS_V66_FEAT_NEON,    "neon"    },
        { COS_V66_FEAT_DOTPROD, "dotprod" },
        { COS_V66_FEAT_I8MM,    "i8mm"    },
        { COS_V66_FEAT_BF16,    "bf16"    },
        { COS_V66_FEAT_SVE,     "sve"     },
        { COS_V66_FEAT_SME,     "sme"     },
        { COS_V66_FEAT_SME2,    "sme2"    },
    };
    size_t off = 0;
    for (size_t i = 0; i < sizeof(tab) / sizeof(tab[0]); ++i) {
        if (!(f & tab[i].bit)) continue;
        const char *n = tab[i].name;
        size_t      l = strlen(n);
        size_t      need = (off ? 1 : 0) + l;
        if (off + need + 1 > cap) break;
        if (off) buf[off++] = ',';
        memcpy(buf + off, n, l);
        off += l;
    }
    if (off == 0 && cap >= 5) {
        memcpy(buf, "none", 4);
        off = 4;
    }
    buf[off] = '\0';
    return off;
}

/* ====================================================================
 *  1.  7-bit composed decision.  Branchless AND.
 * ==================================================================== */

cos_v66_decision_t
cos_v66_compose_decision(uint8_t v60_ok, uint8_t v61_ok, uint8_t v62_ok,
                         uint8_t v63_ok, uint8_t v64_ok, uint8_t v65_ok,
                         uint8_t v66_ok)
{
    cos_v66_decision_t d;
    d.v60_ok = v60_ok ? 1u : 0u;
    d.v61_ok = v61_ok ? 1u : 0u;
    d.v62_ok = v62_ok ? 1u : 0u;
    d.v63_ok = v63_ok ? 1u : 0u;
    d.v64_ok = v64_ok ? 1u : 0u;
    d.v65_ok = v65_ok ? 1u : 0u;
    d.v66_ok = v66_ok ? 1u : 0u;
    d.allow  = (uint8_t)(d.v60_ok & d.v61_ok & d.v62_ok &
                         d.v63_ok & d.v64_ok & d.v65_ok & d.v66_ok);
    return d;
}

/* ====================================================================
 *  2.  INT8 GEMV.
 *
 *      y_q15[m] = saturate_q15( bias[m] + Σ_k W[m,k] * x[k] )
 *
 *  W is row-major INT8, 64-byte aligned.  cols % 16 == 0.
 *  4-accumulator NEON inner loop on AArch64 with a 64-byte prefetch
 *  ahead of the load (the canonical .cursorrules pattern).  Scalar
 *  fallback is bit-identical row-for-row.
 * ==================================================================== */

static inline int16_t cos_v66_sat_q15(int32_t v)
{
    if (v >  32767) v =  32767;
    if (v < -32768) v = -32768;
    return (int16_t)v;
}

#if !COS_V66_HAVE_NEON
static int32_t cos_v66_int8_dot_scalar(const int8_t *w,
                                       const int8_t *x,
                                       uint32_t      n)
{
    int32_t acc = 0;
    for (uint32_t i = 0; i < n; ++i) {
        acc += (int32_t)w[i] * (int32_t)x[i];
    }
    return acc;
}
#endif

#if COS_V66_HAVE_NEON
static int32_t cos_v66_int8_dot_neon(const int8_t *w,
                                     const int8_t *x,
                                     uint32_t      n)
{
    int32x4_t a0 = vdupq_n_s32(0);
    int32x4_t a1 = vdupq_n_s32(0);
    int32x4_t a2 = vdupq_n_s32(0);
    int32x4_t a3 = vdupq_n_s32(0);

    uint32_t i = 0;
    for (; i + 64u <= n; i += 64u) {
        __builtin_prefetch(w + i + 64, 0, 3);

        int8x16_t w0 = vld1q_s8(w + i +  0);
        int8x16_t w1 = vld1q_s8(w + i + 16);
        int8x16_t w2 = vld1q_s8(w + i + 32);
        int8x16_t w3 = vld1q_s8(w + i + 48);

        int8x16_t x0 = vld1q_s8(x + i +  0);
        int8x16_t x1 = vld1q_s8(x + i + 16);
        int8x16_t x2 = vld1q_s8(x + i + 32);
        int8x16_t x3 = vld1q_s8(x + i + 48);

        int16x8_t p0a = vmull_s8(vget_low_s8(w0),  vget_low_s8(x0));
        int16x8_t p0b = vmull_high_s8(w0, x0);
        int16x8_t p1a = vmull_s8(vget_low_s8(w1),  vget_low_s8(x1));
        int16x8_t p1b = vmull_high_s8(w1, x1);
        int16x8_t p2a = vmull_s8(vget_low_s8(w2),  vget_low_s8(x2));
        int16x8_t p2b = vmull_high_s8(w2, x2);
        int16x8_t p3a = vmull_s8(vget_low_s8(w3),  vget_low_s8(x3));
        int16x8_t p3b = vmull_high_s8(w3, x3);

        a0 = vpadalq_s16(a0, p0a);
        a0 = vpadalq_s16(a0, p0b);
        a1 = vpadalq_s16(a1, p1a);
        a1 = vpadalq_s16(a1, p1b);
        a2 = vpadalq_s16(a2, p2a);
        a2 = vpadalq_s16(a2, p2b);
        a3 = vpadalq_s16(a3, p3a);
        a3 = vpadalq_s16(a3, p3b);
    }
    int32x4_t s = vaddq_s32(vaddq_s32(a0, a1), vaddq_s32(a2, a3));
    int32_t   acc = vaddvq_s32(s);

    /* Tail: 16-byte chunks then scalar.  Use vaddlvq_s16 (long add)
     * because vaddvq_s16 returns int16 and overflows on sums of full
     * int8 × int8 products. */
    for (; i + 16u <= n; i += 16u) {
        int8x16_t wv = vld1q_s8(w + i);
        int8x16_t xv = vld1q_s8(x + i);
        int16x8_t pa = vmull_s8(vget_low_s8(wv), vget_low_s8(xv));
        int16x8_t pb = vmull_high_s8(wv, xv);
        acc += vaddlvq_s16(pa);
        acc += vaddlvq_s16(pb);
    }
    for (; i < n; ++i) acc += (int32_t)w[i] * (int32_t)x[i];
    return acc;
}
#endif

int cos_v66_gemv_int8(const int8_t  *W,
                      const int8_t  *x,
                      const int32_t *bias,
                      int16_t       *y_q15,
                      uint32_t       rows,
                      uint32_t       cols)
{
    if (!W || !x || !y_q15) return -1;
    if (cols == 0u || (cols % 16u) != 0u) return -1;

    for (uint32_t m = 0; m < rows; ++m) {
        const int8_t *row = W + (size_t)m * (size_t)cols;
#if COS_V66_HAVE_NEON
        int32_t acc = cos_v66_int8_dot_neon(row, x, cols);
#else
        int32_t acc = cos_v66_int8_dot_scalar(row, x, cols);
#endif
        if (bias) acc += bias[m];
        y_q15[m] = cos_v66_sat_q15(acc);
    }
    return 0;
}

/* ====================================================================
 *  3.  Ternary GEMV (BitNet b1.58, packed 2-bits-per-weight).
 *
 *  Encoding: 00 → 0, 01 → +1, 10 → −1, 11 → 0 (defensive).
 *  Each row is `cols/4` bytes.  Row-by-row scan, branchless inner
 *  loop, constant time per row.
 * ==================================================================== */

/* Decode one packed byte into four signed bytes {-1, 0, +1}. */
static inline void cos_v66_unpack4(uint8_t pk, int8_t out[4])
{
    /* Per-2-bit code table, branchless. */
    static const int8_t T[4] = { 0, +1, -1, 0 };
    out[0] = T[(pk >> 0) & 0x3u];
    out[1] = T[(pk >> 2) & 0x3u];
    out[2] = T[(pk >> 4) & 0x3u];
    out[3] = T[(pk >> 6) & 0x3u];
}

int cos_v66_gemv_ternary(const uint8_t *W_packed,
                         const int8_t  *x,
                         const int32_t *bias,
                         int16_t       *y_q15,
                         uint32_t       rows,
                         uint32_t       cols)
{
    if (!W_packed || !x || !y_q15) return -1;
    if (cols == 0u || (cols % 4u) != 0u) return -1;

    const uint32_t row_bytes = cols / 4u;

    for (uint32_t m = 0; m < rows; ++m) {
        const uint8_t *row = W_packed + (size_t)m * (size_t)row_bytes;
        int32_t        acc = 0;

        for (uint32_t b = 0; b < row_bytes; ++b) {
            int8_t  q[4];
            cos_v66_unpack4(row[b], q);
            const int8_t *xs = x + (size_t)b * 4u;
            acc += (int32_t)q[0] * (int32_t)xs[0];
            acc += (int32_t)q[1] * (int32_t)xs[1];
            acc += (int32_t)q[2] * (int32_t)xs[2];
            acc += (int32_t)q[3] * (int32_t)xs[3];
        }
        if (bias) acc += bias[m];
        y_q15[m] = cos_v66_sat_q15(acc);
    }
    return 0;
}

/* ====================================================================
 *  4.  NativeTernary wire format (arXiv:2604.03336).
 *
 *  Self-delimiting unary-run-length: one byte is 8 packed 2-bit
 *  codes from the same {00, 01, 10} alphabet (ternary).  Bit-stream
 *  reads: 4 weights per byte, MSB-first per pair within byte to mirror
 *  the paper's packing.  We follow the same 2.0 bpw envelope by
 *  *always* emitting two bits per weight (no run compression in v66 —
 *  the wire compatibility surface here is the 10-state stateless
 *  decoder pattern, not the optional run-length extension).
 *
 *  This decoder is the branchless, table-driven baseline.  An
 *  optional run-length extension (variable-length) is reserved for
 *  v66.1 once the ConformalHDC path stabilises.
 * ==================================================================== */

int32_t cos_v66_ntw_encode(const int8_t *weights,
                           uint32_t      n_weights,
                           uint8_t      *dst_wire,
                           uint32_t      dst_cap)
{
    if (!weights || !dst_wire) return -1;
    uint32_t need_bytes = (n_weights + 3u) / 4u;
    if (need_bytes > dst_cap) return -1;

    memset(dst_wire, 0, need_bytes);
    for (uint32_t i = 0; i < n_weights; ++i) {
        uint8_t code = 0;
        switch (weights[i]) {
            case  0: code = 0u; break;
            case  1: code = 1u; break;
            case -1: code = 2u; break;
            default: code = 0u; break;
        }
        uint32_t byte_idx = i >> 2;
        uint32_t shift    = (i & 3u) * 2u;
        dst_wire[byte_idx] |= (uint8_t)(code << shift);
    }
    return (int32_t)need_bytes;
}

int32_t cos_v66_ntw_decode(const uint8_t *wire,
                           uint32_t       wire_len,
                           uint8_t       *dst_packed,
                           uint32_t       n_weights)
{
    if (!wire || !dst_packed) return -1;
    uint32_t need_in  = (n_weights + 3u) / 4u;
    uint32_t need_out = need_in;            /* same packing */
    if (wire_len < need_in) return -1;

    /* Defensive: clamp 11 codes to 0 during copy. */
    for (uint32_t i = 0; i < need_out; ++i) {
        uint8_t b = wire[i];
        uint8_t out = 0;
        for (uint32_t k = 0; k < 4; ++k) {
            uint8_t c = (uint8_t)((b >> (k * 2u)) & 0x3u);
            uint8_t safe = (c == 3u) ? 0u : c;
            out |= (uint8_t)(safe << (k * 2u));
        }
        dst_packed[i] = out;
    }
    return (int32_t)n_weights;
}

/* ====================================================================
 *  5.  Conformal abstention gate (CFC, arXiv:2603.27403).
 * ==================================================================== */

static void *cos_v66_aalloc64(size_t n)
{
    if (n == 0) n = 1;
    /* aligned_alloc requires size to be a multiple of alignment. */
    size_t pad = (n + 63u) & ~((size_t)63u);
    return aligned_alloc(64, pad);
}

cos_v66_conformal_t *cos_v66_conformal_new(uint32_t n_groups,
                                           int16_t  global_tau_q15,
                                           int16_t  target_eps_q15)
{
    if (n_groups == 0u) return NULL;
    cos_v66_conformal_t *g = (cos_v66_conformal_t *)
        cos_v66_aalloc64(sizeof(*g));
    if (!g) return NULL;
    memset(g, 0, sizeof(*g));

    g->thr_q15 = (int16_t  *)cos_v66_aalloc64(sizeof(int16_t)  * n_groups);
    g->seen    = (uint32_t *)cos_v66_aalloc64(sizeof(uint32_t) * n_groups);
    g->miscov  = (uint32_t *)cos_v66_aalloc64(sizeof(uint32_t) * n_groups);
    if (!g->thr_q15 || !g->seen || !g->miscov) {
        cos_v66_conformal_free(g);
        return NULL;
    }
    for (uint32_t i = 0; i < n_groups; ++i) {
        g->thr_q15[i] = global_tau_q15;
        g->seen[i]    = 0u;
        g->miscov[i]  = 0u;
    }
    g->n_groups        = n_groups;
    g->global_tau_q15  = global_tau_q15;
    g->target_eps_q15  = target_eps_q15;
    return g;
}

void cos_v66_conformal_free(cos_v66_conformal_t *g)
{
    if (!g) return;
    free(g->thr_q15);
    free(g->seen);
    free(g->miscov);
    free(g);
}

void cos_v66_conformal_update(cos_v66_conformal_t *g,
                              uint32_t             group,
                              int16_t              score_q15,
                              int                  was_factual)
{
    if (!g || group >= g->n_groups) return;

    g->seen[group] += 1u;
    if (!was_factual) g->miscov[group] += 1u;

    /* Streaming threshold update with ratio-preserving overflow shift,
     * mirrors v64 Reflexion ratchet:
     *
     *   if empirical_miscov_rate > target_eps  → tighten (raise thr)
     *   if empirical_miscov_rate < target_eps  → relax  (lower thr)
     *
     * Step is small and integer.  Threshold stays in [0, 32767]. */
    if (g->seen[group] >= 4u) {
        /* miscov_rate_q15 = (miscov * 32768) / seen. */
        int32_t rate_q15 = (int32_t)(((uint64_t)g->miscov[group] * 32768u) / g->seen[group]);
        int32_t target   = (int32_t)g->target_eps_q15;
        int32_t step     = (rate_q15 > target) ? +8 : -8;
        int32_t cur      = (int32_t)g->thr_q15[group] + step;
        if (cur < 0)     cur = 0;
        if (cur > 32767) cur = 32767;
        g->thr_q15[group] = (int16_t)cur;

        /* Overflow shift: keep ratio observable but bounded. */
        if (g->seen[group] > 65535u) {
            g->seen[group]   >>= 1;
            g->miscov[group] >>= 1;
        }
    }
}

uint8_t cos_v66_conformal_gate(const cos_v66_conformal_t *g,
                               uint32_t                   group,
                               int16_t                    score_q15)
{
    if (!g) return 0u;
    /* Out-of-range group → strictest deny.  Branchless via mask. */
    uint32_t safe   = (group < g->n_groups) ? group : 0u;
    int32_t  thr    = (group < g->n_groups) ? g->thr_q15[safe] : 32767;
    int32_t  glob   = g->global_tau_q15;
    int32_t  s      = score_q15;
    uint32_t pass1  = (uint32_t)(s >= thr);
    uint32_t pass2  = (uint32_t)(s >= glob);
    uint32_t inrng  = (uint32_t)(group < g->n_groups);
    return (uint8_t)(pass1 & pass2 & inrng);
}

/* ====================================================================
 *  6.  HSL — Hardware Substrate Language interpreter.
 * ==================================================================== */

#define COS_V66_HSL_MAX_REG 16u

struct cos_v66_hsl_state {
    int32_t  reg[COS_V66_HSL_MAX_REG];   /* generic 32-bit registers   */
    uint8_t  rkind[COS_V66_HSL_MAX_REG]; /* 0 = empty, 1 = scalar, 2 = vec ref */
    uint8_t  nreg;
    uint8_t  gate_bit;
    uint8_t  v66_ok;
    uint8_t  _pad;
    uint64_t cost;
};

cos_v66_hsl_state_t *cos_v66_hsl_new(uint8_t nreg)
{
    if (nreg == 0 || nreg > COS_V66_HSL_MAX_REG) return NULL;
    cos_v66_hsl_state_t *s = (cos_v66_hsl_state_t *)
        cos_v66_aalloc64(sizeof(*s));
    if (!s) return NULL;
    memset(s, 0, sizeof(*s));
    s->nreg = nreg;
    return s;
}

void cos_v66_hsl_free(cos_v66_hsl_state_t *s)
{
    free(s);
}

void cos_v66_hsl_reset(cos_v66_hsl_state_t *s)
{
    if (!s) return;
    s->cost     = 0;
    s->gate_bit = 0;
    s->v66_ok   = 0;
    for (uint8_t r = 0; r < s->nreg; ++r) {
        s->reg[r]   = 0;
        s->rkind[r] = 0;
    }
}

uint64_t cos_v66_hsl_cost(const cos_v66_hsl_state_t *s) { return s ? s->cost : 0; }
uint8_t  cos_v66_hsl_gate_bit(const cos_v66_hsl_state_t *s) { return s ? s->gate_bit : 0; }
uint8_t  cos_v66_hsl_v66_ok(const cos_v66_hsl_state_t *s) { return s ? s->v66_ok : 0; }

int cos_v66_hsl_exec(cos_v66_hsl_state_t       *s,
                     const cos_v66_inst_t      *prog,
                     uint32_t                   nprog,
                     const int8_t       *const *w_i8_slots,
                     const uint32_t            *w_i8_rows,
                     const uint32_t            *w_i8_cols,
                     const int8_t       *const *x_i8_slots,
                     const uint8_t      *const *w_t_slots,
                     const uint32_t            *w_t_rows,
                     const uint32_t            *w_t_cols,
                     int16_t            * const *y_q15_slots,
                     const cos_v66_conformal_t *cg,
                     uint64_t                   cost_budget)
{
    if (!s || !prog) return -1;

    cos_v66_hsl_reset(s);

    for (uint32_t pc = 0; pc < nprog; ++pc) {
        const cos_v66_inst_t *I = &prog[pc];
        const uint8_t rd = I->rd;
        if (I->op != COS_V66_OP_HALT && rd >= s->nreg) return -1;

        switch ((cos_v66_opcode_t)I->op) {
        case COS_V66_OP_HALT:
            goto done;

        case COS_V66_OP_LOAD:
            /* a = scalar value, b = kind tag.  Used to seed registers
             * deterministically from the test driver. */
            s->reg[rd]   = (int32_t)(int16_t)I->a;
            s->rkind[rd] = (uint8_t)(I->b ? I->b : 1u);
            s->cost     += 1u;
            break;

        case COS_V66_OP_GEMV_I8: {
            /* a = i8 weight slot, b = i8 input slot, c = output slot.
             * Computes y = W·x and stores y[0] into reg[rd] for cmp. */
            uint32_t wi = I->a, xi = I->b, yi = I->c;
            if (!w_i8_slots || !x_i8_slots || !y_q15_slots ||
                !w_i8_rows || !w_i8_cols)            return -1;
            const int8_t  *W = w_i8_slots[wi];
            const int8_t  *x = x_i8_slots[xi];
            int16_t       *y = y_q15_slots[yi];
            uint32_t       R = w_i8_rows[wi];
            uint32_t       C = w_i8_cols[wi];
            int rc = cos_v66_gemv_int8(W, x, NULL, y, R, C);
            if (rc != 0) return -1;
            s->reg[rd]   = (int32_t)y[0];
            s->rkind[rd] = 1u;
            s->cost     += (uint64_t)R * (uint64_t)C;
            break;
        }

        case COS_V66_OP_GEMV_T: {
            uint32_t wi = I->a, xi = I->b, yi = I->c;
            if (!w_t_slots || !x_i8_slots || !y_q15_slots ||
                !w_t_rows || !w_t_cols)              return -1;
            const uint8_t *W = w_t_slots[wi];
            const int8_t  *x = x_i8_slots[xi];
            int16_t       *y = y_q15_slots[yi];
            uint32_t       R = w_t_rows[wi];
            uint32_t       C = w_t_cols[wi];
            int rc = cos_v66_gemv_ternary(W, x, NULL, y, R, C);
            if (rc != 0) return -1;
            s->reg[rd]   = (int32_t)y[0];
            s->rkind[rd] = 1u;
            s->cost     += ((uint64_t)R * (uint64_t)C) / 4u;
            break;
        }

        case COS_V66_OP_DECODE_NTW:
            /* a = wire len.  Nothing to verify at the ISA level —
             * decoder is exercised directly in the self-test. */
            s->reg[rd]   = (int32_t)I->a;
            s->rkind[rd] = 1u;
            s->cost     += (uint64_t)I->a;
            break;

        case COS_V66_OP_ABSTAIN: {
            /* a = group, b = score_q15, write 0/1 to reg[rd]. */
            uint8_t g = cos_v66_conformal_gate(cg, (uint32_t)I->a,
                                               (int16_t)I->b);
            s->reg[rd]   = (int32_t)g;
            s->rkind[rd] = 1u;
            s->cost     += 1u;
            break;
        }

        case COS_V66_OP_CMPGE: {
            int32_t v   = s->reg[rd];
            int32_t thr = (int32_t)(int16_t)I->a;
            uint32_t pass = (uint32_t)(v >= thr);
            s->gate_bit |= (uint8_t)pass;
            s->cost     += 1u;
            break;
        }

        case COS_V66_OP_GATE: {
            uint32_t budget_ok = (uint32_t)(s->cost <= cost_budget);
            s->v66_ok = (uint8_t)(s->gate_bit & budget_ok);
            break;
        }

        default:
            return -1;
        }

        if (s->cost > cost_budget) {
            /* Branchless overrun — clear gate, zero v66_ok. */
            s->v66_ok = 0u;
            return 0;
        }
    }

done:
    return 0;
}
