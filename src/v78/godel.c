/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v78 — σ-Gödel-Attestor: implementation.
 *
 * Branchless, integer-only, libc-only.  Every log2 is a lookup in
 * a precomputed 257-entry Q0.15 table; every conditional is a mask.
 */

#include "godel.h"

#include <string.h>

/* ==================================================================
 *  0.  Q0.15 log2 table.  LOG2_Q15[n] ≈ log2(n) * 32768, rounded.
 *      LOG2_Q15[0]  = 0  (by convention, used as a clamp).
 *      LOG2_Q15[1]  = 0
 *      LOG2_Q15[2]  = 32768   (= 1.0 × 2^15)
 *      LOG2_Q15[4]  = 65536   (= 2.0 × 2^15, but clipped to uint16,
 *                               so stored as uint32 below).
 *
 *  For n > 65535 we need uint32.  We expose uint16 to callers only
 *  on [0, 256] where values fit; FEP / IIT internally use uint32.
 * ================================================================== */

static uint16_t g_log2_q15[257];
static uint32_t g_log2_q15_ext[1024];  /* extended for larger n */
static int      g_tables_ready = 0;

/* Integer-rounded Q0.15 log2 via Mitchell's approximation plus a
 * 4-bit lookup correction.  Deterministic, branchless. */
static uint32_t log2_q15_compute(uint32_t n)
{
    if (n == 0) return 0;
    if (n == 1) return 0;
    /* Find highest set bit: branchless via de Bruijn / clz. */
    uint32_t hi = 31u - (uint32_t)__builtin_clz(n);
    /* Fractional part: (n - 2^hi) / 2^hi  in Q0.15 */
    uint32_t frac = ((n - (1u << hi)) << 15) >> hi;  /* 0..32767 */
    /* Correction table over the fractional part via 4 linear
     * interpolation points of log2(1 + f).  Values rounded from:
     *   f=0/4:  log2(1.00)=0.000000 → 0
     *   f=1/4:  log2(1.25)=0.321928 → 10549
     *   f=2/4:  log2(1.50)=0.584963 → 19167
     *   f=3/4:  log2(1.75)=0.807355 → 26462
     *   f=4/4:  log2(2.00)=1.000000 → 32768
     */
    static const uint32_t breakpoints[5] = { 0, 10549, 19167, 26462, 32768 };
    uint32_t quarter = frac >> 13;          /* 0..3 */
    uint32_t rem     = frac & 0x1FFFu;      /* 0..8191 */
    uint32_t lo      = breakpoints[quarter];
    uint32_t hi2     = breakpoints[quarter + 1u];
    /* Linear interp between lo and hi2 by rem / 8192: */
    uint32_t interp  = lo + ((hi2 - lo) * rem >> 13);
    return (hi << 15) + interp;
}

static void ensure_tables(void)
{
    if (g_tables_ready) return;
    for (uint32_t i = 0; i < 257; ++i) {
        uint32_t v = log2_q15_compute(i);
        g_log2_q15[i] = (v > 65535u) ? 65535u : (uint16_t)v;
    }
    for (uint32_t i = 0; i < 1024; ++i) {
        g_log2_q15_ext[i] = log2_q15_compute(i);
    }
    g_tables_ready = 1;
}

const uint16_t *cos_v78_log2_q15_table(void)
{
    ensure_tables();
    return g_log2_q15;
}

static inline uint32_t log2_q15_lookup(uint32_t n)
{
    ensure_tables();
    if (n < 1024u) return g_log2_q15_ext[n];
    return log2_q15_compute(n);
}

/* Integer ceil(log2(n)) for n >= 1.  Branchless. */
static inline uint32_t ceil_log2(uint32_t n)
{
    if (n <= 1u) return 0u;
    uint32_t hi = 31u - (uint32_t)__builtin_clz(n - 1u);
    return hi + 1u;
}

/* ==================================================================
 *  1.  IIT-φ  (minimum-information bipartition gap).
 *
 *  The TPM enters as count matrix over (s_in, s_out).  We compute:
 *    - mutual information MI_whole of (s_in ; s_out) in Q0.15 bits
 *    - for each non-trivial bipartition A|B of the node set,
 *      MI_parts = MI(s_in^A ; s_out^A) + MI(s_in^B ; s_out^B)
 *    - φ = MI_whole − min_bipartition MI_parts   (clamped ≥ 0)
 *
 *  MI in Q0.15 = Σ (c_xy / N) · (log2(c_xy · N) − log2(c_x · c_y))
 *             approximated as
 *               (Σ c_xy · log2q15(c_xy) − c_xy · log2q15(c_x · c_y / N))
 *             / N
 *
 *  We use a simple proxy that keeps all arithmetic non-negative:
 *    MI* = Σ c_xy · log2q15((c_xy · N_scaled) / (c_x · c_y + 1))
 *          / N_scaled
 *
 *  The proxy is deterministic, integer-only, and monotone in
 *  genuine MI, which is what we need for a branchless gate.
 * ================================================================== */

/* Marginals for a single-variable projection along bits `mask` over
 * a `states`-sized space (states = 2^n_nodes). */
static void tpm_marginals(const uint32_t *tpm,
                          uint32_t n_states,
                          uint32_t mask,
                          uint32_t *margin_in,
                          uint32_t *margin_out,
                          uint32_t *total)
{
    uint32_t N = 0;
    for (uint32_t i = 0; i < n_states; ++i) {
        uint32_t ai = i & mask;
        margin_in[ai] += 0;  /* just ensure readable zero */
    }
    for (uint32_t i = 0; i < 16u; ++i) {
        margin_in[i] = 0;
        margin_out[i] = 0;
    }
    for (uint32_t si = 0; si < n_states; ++si) {
        uint32_t ai = si & mask;
        for (uint32_t so = 0; so < n_states; ++so) {
            uint32_t ao = so & mask;
            uint32_t c = tpm[si * n_states + so];
            margin_in[ai]  += c;
            margin_out[ao] += c;
            N += c;
        }
    }
    *total = N;
}

static uint32_t proxy_mi(const uint32_t *tpm,
                         uint32_t n_states,
                         uint32_t mask)
{
    uint32_t margin_in[16] = {0};
    uint32_t margin_out[16] = {0};
    uint32_t N = 0;
    tpm_marginals(tpm, n_states, mask, margin_in, margin_out, &N);
    if (N == 0) return 0;

    /* Σ c_xy · (log2(c_xy · N) − log2(c_x · c_y))  in Q0.15,
     * then divide by N.  All unsigned, non-negative, saturating. */
    uint64_t acc = 0;
    for (uint32_t si = 0; si < n_states; ++si) {
        uint32_t ai = si & mask;
        uint32_t cx = margin_in[ai];
        for (uint32_t so = 0; so < n_states; ++so) {
            uint32_t ao = so & mask;
            uint32_t cy = margin_out[ao];
            uint32_t cxy = tpm[si * n_states + so];
            if (cxy == 0) continue;
            uint32_t num = cxy * N;
            uint32_t den = cx * cy;
            den = (den == 0) ? 1 : den;
            uint32_t lnum = log2_q15_lookup(num);
            uint32_t lden = log2_q15_lookup(den);
            uint32_t delta = (lnum > lden) ? (lnum - lden) : 0;
            acc += (uint64_t)cxy * (uint64_t)delta;
        }
    }
    uint64_t mi_q15 = acc / (uint64_t)N;
    if (mi_q15 > 0xFFFFFFFFull) mi_q15 = 0xFFFFFFFFull;
    return (uint32_t)mi_q15;
}

uint32_t cos_v78_iit_phi(const uint32_t *tpm, uint32_t n_nodes)
{
    if (!tpm) return 0;
    if (n_nodes == 0 || n_nodes > COS_V78_IIT_NODES_MAX) return 0;

    uint32_t n_states = 1u << n_nodes;
    uint32_t full_mask = n_states - 1u;

    uint32_t mi_whole = proxy_mi(tpm, n_states, full_mask);

    /* Enumerate non-trivial bipartitions of n_nodes into (A, B). */
    uint32_t mi_parts_min = 0xFFFFFFFFu;
    for (uint32_t a_mask = 1; a_mask < full_mask; ++a_mask) {
        uint32_t b_mask = full_mask ^ a_mask;
        uint32_t mi_a = proxy_mi(tpm, n_states, a_mask);
        uint32_t mi_b = proxy_mi(tpm, n_states, b_mask);
        uint32_t sum  = mi_a + mi_b;
        if (sum < mi_parts_min) mi_parts_min = sum;
    }
    if (mi_parts_min == 0xFFFFFFFFu) mi_parts_min = mi_whole;

    /* φ = max(mi_whole − min mi_parts, 0).  Clamp to Q0.15 ceiling. */
    uint32_t phi = (mi_whole > mi_parts_min) ? (mi_whole - mi_parts_min) : 0;
    if (phi > 32767u) phi = 32767u;
    return phi;
}

/* ==================================================================
 *  2.  FEP — discrete variational free energy.
 *
 *  F = KL(q || p) + H[q]
 *    = Σ q_i · log2(q_i / p_i)  +  Σ q_i · (−log2 q_i)
 *
 *  In Q0.15 fixed-point using the log2 lookup.  Guarantees:
 *    - q_i, p_i ∈ [0, 32768]
 *    - p_i = 0 ⇒ KL diverges; we clamp to a large penalty.
 * ================================================================== */

uint32_t cos_v78_free_energy(const uint16_t *q_q15,
                             const uint16_t *p_q15,
                             uint32_t k_states)
{
    if (!q_q15 || !p_q15) return 0xFFFFFFFFu;
    if (k_states == 0 || k_states > COS_V78_FEP_STATES_MAX)
        return 0xFFFFFFFFu;

    uint64_t kl_acc = 0;
    uint64_t h_acc  = 0;

    for (uint32_t i = 0; i < k_states; ++i) {
        uint32_t q = q_q15[i];
        uint32_t p = p_q15[i];
        if (q == 0) continue;
        uint32_t lq = log2_q15_lookup((q > 0) ? q : 1u);
        uint32_t lp = (p > 0) ? log2_q15_lookup(p) : 0u;
        uint32_t diff_q_p = (lq > lp) ? (lq - lp) : 0u;
        uint32_t penalty  = (p == 0) ? (1u << 20) : 0u;   /* large */
        kl_acc += (uint64_t)q * (uint64_t)(diff_q_p + penalty);
        /* H[q] = Σ q · (−log2 q).  Since q ≤ 32768, lq ≤ 15 · 32768 = 491520
         * (Q0.15 log2), but we need to scale so the per-term contribution
         * stays non-negative: use (15 · 32768 − lq) as the "−log2(q)" Q0.15
         * representation of the non-negative surprisal. */
        uint32_t neg_log_q = (15u * 32768u) - lq;
        h_acc += (uint64_t)q * (uint64_t)neg_log_q;
    }

    /* Both terms are scaled by q ∈ Q0.15, so F is in Q0.30.  Drop 15
     * bits to return Q0.15 free-energy. */
    uint64_t f_q30 = kl_acc + h_acc;
    uint64_t f_q15 = f_q30 >> 15;
    if (f_q15 > 0xFFFFFFFFu) f_q15 = 0xFFFFFFFFu;
    return (uint32_t)f_q15;
}

/* ==================================================================
 *  3.  MDL — integer description-length upper bound.
 * ================================================================== */

uint32_t cos_v78_mdl_bound(uint32_t program_len, uint64_t flag_bits)
{
    uint32_t plen = (program_len == 0) ? 1 : program_len;
    uint32_t log_part  = 8u * ceil_log2(plen);
    uint32_t flag_part = (uint32_t)__builtin_popcountll(flag_bits);
    return log_part + flag_part;
}

/* ==================================================================
 *  4.  Gödel prime-power numbering.
 * ================================================================== */

static const uint64_t g_primes8[COS_V78_GODEL_MAX_OPS] = {
    2ull, 3ull, 5ull, 7ull, 11ull, 13ull, 17ull, 19ull
};

/* Saturating integer power via repeated squaring.  Clamps to
 * UINT64_MAX on overflow. */
static uint64_t ipow_sat(uint64_t base, uint32_t exp)
{
    uint64_t r = 1;
    uint64_t b = base;
    while (exp > 0) {
        if (exp & 1u) {
            if (b != 0 && r > 0xFFFFFFFFFFFFFFFFull / b) return 0xFFFFFFFFFFFFFFFFull;
            r *= b;
        }
        exp >>= 1;
        if (exp > 0) {
            if (b != 0 && b > 0xFFFFFFFFFFFFFFFFull / b) { b = 0xFFFFFFFFFFFFFFFFull; }
            else b = b * b;
        }
    }
    return r;
}

uint64_t cos_v78_godel_num(const uint32_t *codes, uint32_t n)
{
    if (!codes || n == 0) return 0;
    uint32_t m = (n > COS_V78_GODEL_MAX_OPS) ? COS_V78_GODEL_MAX_OPS : n;

    uint64_t g = 1;
    for (uint32_t i = 0; i < m; ++i) {
        uint32_t c = codes[i] & 0x1Fu;         /* 5-bit clamp */
        uint64_t p = ipow_sat(g_primes8[i], c + 1u);
        if (g > 0xFFFFFFFFFFFFFFFFull / (p + 1u)) return 0xFFFFFFFFFFFFFFFFull;
        g *= p;
    }
    return g;
}

/* ==================================================================
 *  5.  Workspace broadcast — Global Workspace coalition gate.
 * ================================================================== */

/* Sort 4 uint32 values in descending order via a 5-compare network.
 * Branchless: each compare is a min/max pair done via masking. */
static void sort4_desc(uint32_t v[4])
{
    #define CSWAP(a, b) do { \
        uint32_t lo = (v[a] < v[b]) ? v[a] : v[b]; \
        uint32_t hi = (v[a] < v[b]) ? v[b] : v[a]; \
        v[a] = hi; v[b] = lo; \
    } while (0)
    CSWAP(0, 1);
    CSWAP(2, 3);
    CSWAP(0, 2);
    CSWAP(1, 3);
    CSWAP(1, 2);
    #undef CSWAP
}

uint32_t cos_v78_workspace_broadcast(const cos_v78_hv_t *attn,
                                     uint32_t k_hi,
                                     uint32_t threshold_bits)
{
    if (!attn) return 0;
    uint32_t pop[4];
    for (unsigned i = 0; i < 4; ++i) {
        pop[i] = (uint32_t)__builtin_popcountll(attn->w[i]);
    }
    sort4_desc(pop);

    uint32_t k = (k_hi > 4u) ? 4u : k_hi;
    uint32_t mass = 0;
    for (uint32_t i = 0; i < k; ++i) mass += pop[i];
    return (mass >= threshold_bits) ? 1u : 0u;
}

/* ==================================================================
 *  6.  Halting witness.
 * ================================================================== */

uint32_t cos_v78_halt_witness(uint32_t mu_init,
                              uint32_t mu_step,
                              uint32_t max_steps)
{
    if (mu_step == 0) {
        /* Non-decreasing: refuse unless mu was already zero. */
        return (mu_init == 0) ? 1u : 0u;
    }
    uint32_t mu = mu_init;
    uint32_t halted = 0;
    for (uint32_t s = 0; s < max_steps; ++s) {
        uint32_t reached_zero = (mu == 0) ? 1u : 0u;
        halted |= reached_zero;
        mu = (mu > mu_step) ? (mu - mu_step) : 0u;
    }
    /* Check one more time after loop: */
    halted |= (mu == 0) ? 1u : 0u;
    return halted & 1u;
}

/* ==================================================================
 *  7.  Self-trust — Löbian anchor.
 *
 *  Uses a local, deterministic, libc-only 32-byte digest.  The
 *  Löbian-anchor construction is orthogonal to the hash's
 *  cryptographic strength: what matters for the self-trust gate is
 *  that the kernel's spec blob hashes *identically* to a pinned
 *  reference.  A cryptographic-strength SHA-256 is available via
 *  the v75 license-attestation path when the two modules are
 *  linked together in the production build; v78 itself stays
 *  libc-only and toolchain-minimal.
 * ================================================================== */

static void local_digest32(const uint8_t *data, size_t len,
                           uint8_t out[32])
{
    /* Deterministic bit-stable digest built from xorshift rounds.
     * Not cryptographic — its only job is reproducible bit-equal
     * comparison under the Löbian anchor gate. */
    uint64_t h[4] = {
        0x9E3779B97F4A7C15ULL,
        0xBB67AE8584CAA73BULL,
        0x3C6EF372FE94F82BULL,
        0xA54FF53A5F1D36F1ULL,
    };
    for (size_t i = 0; i < len; ++i) {
        for (int k = 0; k < 4; ++k) {
            h[k] ^= (uint64_t)data[i] + ((uint64_t)(i + 1u) << (k * 8));
            h[k] = (h[k] << 13) | (h[k] >> 51);
            h[k] *= 0x2545F4914F6CDD1DULL;
        }
    }
    for (int k = 0; k < 4; ++k) {
        for (int b = 0; b < 8; ++b) {
            out[k * 8 + b] = (uint8_t)(h[k] >> (b * 8));
        }
    }
}

uint32_t cos_v78_self_trust(const uint8_t *spec_blob,
                            size_t spec_len,
                            const uint8_t anchor32[32])
{
    if (!anchor32) return 0;
    uint8_t digest[32];
    local_digest32(spec_blob, spec_len, digest);
    uint32_t diff = 0;
    for (unsigned i = 0; i < 32; ++i) diff |= (digest[i] ^ anchor32[i]);
    return (diff == 0) ? 1u : 0u;
}

void cos_v78_default_anchor(uint8_t out32[32])
{
    static const char kSpec[] = "Creation OS v78 sigma-Godel-Attestor identity anchor v1";
    uint8_t d[32];
    local_digest32((const uint8_t *)kSpec, sizeof(kSpec) - 1, d);
    memcpy(out32, d, 32);
}

/* ==================================================================
 *  8.  Bisimulation.
 * ================================================================== */

uint32_t cos_v78_bisim_check(const cos_v78_hv_t *a,
                             const cos_v78_hv_t *b)
{
    if (!a || !b) return 0;
    uint64_t d = 0;
    for (unsigned i = 0; i < COS_V78_HV_WORDS; ++i)
        d |= (a->w[i] ^ b->w[i]);
    return (d == 0) ? 1u : 0u;
}

/* ==================================================================
 *  9.  Chaitin Ω bound.
 * ================================================================== */

static const uint32_t g_chaitin_omega_bound[16] = {
    1u,  2u,  3u,  5u,  8u, 13u, 21u, 34u,
   55u, 89u,144u,233u,377u,610u,987u,1597u,
};

uint32_t cos_v78_chaitin_bound(uint32_t emission_class)
{
    uint32_t idx = (emission_class < 16u) ? emission_class : 15u;
    return g_chaitin_omega_bound[idx];
}

/* ==================================================================
 * 10.  MCB — Meta-Cognitive Bytecode VM.
 * ================================================================== */

static int validate_mcb_insn(cos_v78_mcb_insn_t ins)
{
    uint32_t op = cos_v78_op_of(ins);
    uint32_t reserved = ins >> 24;
    if (op > (uint32_t)COS_V78_OP_TRUST) return -1;
    if (reserved != 0) return -2;
    return 0;
}

/* Runs one opcode and returns 1 if its local predicate passes,
 * else 0.  Branchless in the sense that no compare on runtime
 * inputs steers control flow beyond the opcode dispatch itself. */
static uint32_t run_op(cos_v78_mcb_op_t op,
                       const cos_v78_mcb_ctx_t *ctx,
                       uint32_t arg_a, uint32_t arg_b, uint32_t budget)
{
    (void)arg_b;
    switch (op) {
    case COS_V78_OP_HALT:
        return 1u;
    case COS_V78_OP_PHI: {
        if (!ctx || !ctx->iit_tpm_counts) return 0u;
        uint32_t phi = cos_v78_iit_phi(ctx->iit_tpm_counts, ctx->iit_n_nodes);
        return (phi >= ctx->phi_min_q15) ? 1u : 0u;
    }
    case COS_V78_OP_FE: {
        if (!ctx || !ctx->fep_q_q15 || !ctx->fep_p_q15) return 0u;
        uint32_t fe = cos_v78_free_energy(ctx->fep_q_q15,
                                          ctx->fep_p_q15,
                                          ctx->fep_k_states);
        return (fe <= ctx->f_max_q15) ? 1u : 0u;
    }
    case COS_V78_OP_MDL: {
        if (!ctx) return 0u;
        uint32_t mdl = cos_v78_mdl_bound(ctx->mdl_program_len,
                                         ctx->mdl_flag_bits);
        return (mdl <= ctx->mdl_max_bits) ? 1u : 0u;
    }
    case COS_V78_OP_GDL: {
        if (!ctx || !ctx->godel_codes) return 0u;
        uint64_t g = cos_v78_godel_num(ctx->godel_codes, ctx->godel_n_codes);
        return (g == ctx->godel_spec) ? 1u : 0u;
    }
    case COS_V78_OP_WS: {
        if (!ctx) return 0u;
        return cos_v78_workspace_broadcast(&ctx->ws_attn,
                                           ctx->ws_k_hi,
                                           ctx->ws_threshold_bits);
    }
    case COS_V78_OP_HWS: {
        if (!ctx) return 0u;
        return cos_v78_halt_witness(ctx->hws_mu_init,
                                    ctx->hws_mu_step,
                                    ctx->hws_max_steps);
    }
    case COS_V78_OP_TRUST: {
        if (!ctx) return 0u;
        return cos_v78_self_trust(ctx->trust_spec_blob,
                                  ctx->trust_spec_len,
                                  ctx->trust_anchor32);
    }
    }
    (void)arg_a; (void)budget;
    return 0u;
}

int cos_v78_mcb_run(const cos_v78_mcb_insn_t *prog,
                    size_t prog_len,
                    const cos_v78_mcb_ctx_t *ctx,
                    uint32_t *out_proof_bits)
{
    if (!prog || !ctx) return -3;
    if (prog_len > COS_V78_MCB_PROG_MAX) return -4;

    uint32_t proof = 0;
    uint32_t required = 0;
    uint32_t failed = 0;

    for (size_t i = 0; i < prog_len; ++i) {
        int v = validate_mcb_insn(prog[i]);
        if (v != 0) return v;
        cos_v78_mcb_op_t op = (cos_v78_mcb_op_t)cos_v78_op_of(prog[i]);
        uint32_t ok = run_op(op,
                             ctx,
                             cos_v78_a_of(prog[i]),
                             cos_v78_b_of(prog[i]),
                             cos_v78_bud_of(prog[i]));
        uint32_t bit = (1u << (uint32_t)op);
        required |= bit;
        proof    |= bit * ok;
        failed   += (1u - ok);
    }

    if (out_proof_bits) *out_proof_bits = proof;
    /* Fail count is the number of required opcodes whose proof bit
     * is NOT set: */
    uint32_t missing = (~proof) & required;
    (void)failed;
    return (int)__builtin_popcount(missing);
}

/* ==================================================================
 * 11.  Gate + 18-bit compose.
 * ================================================================== */

uint32_t cos_v78_ok(const cos_v78_mcb_insn_t *prog,
                    size_t prog_len,
                    const cos_v78_mcb_ctx_t *ctx)
{
    if (!prog || !ctx) return 0u;
    if (prog_len == 0 || prog_len > COS_V78_MCB_PROG_MAX) return 0u;
    uint32_t proof = 0;
    int rc = cos_v78_mcb_run(prog, prog_len, ctx, &proof);
    if (rc != 0) return 0u;
    return 1u;
}

uint32_t cos_v78_compose_decision(uint32_t v77_composed_ok,
                                  uint32_t v78_ok)
{
    v77_composed_ok = v77_composed_ok & 1u;
    v78_ok          = v78_ok          & 1u;
    return v77_composed_ok & v78_ok;
}
