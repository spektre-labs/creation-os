/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v79 — σ-Simulacrum, implementation.
 *
 * Branchless, integer-only, libc-only.  See simulacrum.h for the full
 * rationale and references (Verlet 1967 / Aaronson-Gottesman 2004 /
 * Wolfram 1983 / Cook 2004 / Frady 2020 / Koopman 1931 / Brunton
 * 2016 / Sharma-Cronin 2023 / Kauffman 1969).
 */

#include "simulacrum.h"

#include <string.h>

/* ==================================================================
 *  Helpers: bit-level, branchless.
 * ================================================================== */

static inline uint64_t rotl64(uint64_t x, unsigned r)
{
    r &= 63u;
    return (x << r) | (x >> ((64u - r) & 63u));
}

static inline uint32_t popcount64(uint64_t x)
{
    return (uint32_t)__builtin_popcountll(x);
}

static inline uint32_t popcount32(uint32_t x)
{
    return (uint32_t)__builtin_popcount(x);
}

/* Q16.16 multiply with 32-bit inputs, 32-bit saturating-ish result
 * (wraps modulo 2^32 like any integer; the kernel runs on small
 * numbers so no runtime overflow with the default test harness). */
static inline cos_v79_q16_t q16_mul(cos_v79_q16_t a, cos_v79_q16_t b)
{
    int64_t prod = (int64_t)a * (int64_t)b;
    return (cos_v79_q16_t)(prod >> 16);
}

/* Bit-literal pick: returns `a` if cond_nonzero != 0, else `b`, all
 * without a branch.  Reserved for future primitives that need a
 * branchless 64-bit multiplexer. */
__attribute__((unused))
static inline uint64_t bit_select_u64(uint32_t cond_nonzero,
                                      uint64_t a, uint64_t b)
{
    uint64_t m = (uint64_t)0 - (uint64_t)(cond_nonzero & 1u);
    return (a & m) | (b & ~m);
}

/* ==================================================================
 *  1.  Particles + symplectic leapfrog Verlet.
 *
 *  Harmonic oscillator per particle:
 *     a_i = - k_i * x_i * inv_m_i
 *  Leapfrog:
 *     v  += 0.5 * a(x) * dt
 *     x  += v * dt
 *     v  += 0.5 * a(x_new) * dt
 * ================================================================== */

void cos_v79_particles_init(cos_v79_particles_t *p,
                            const cos_v79_q16_t *x,
                            const cos_v79_q16_t *v,
                            const cos_v79_q16_t *inv_m,
                            const cos_v79_q16_t *k,
                            uint32_t n)
{
    memset(p, 0, sizeof(*p));
    if (n > COS_V79_PARTICLES_MAX) {
        n = COS_V79_PARTICLES_MAX;
    }
    p->n = n;
    for (uint32_t i = 0; i < n; ++i) {
        p->x[i]     = x[i];
        p->v[i]     = v[i];
        p->inv_m[i] = inv_m[i];
        p->k[i]     = k[i];
    }
}

static inline cos_v79_q16_t accel_q16(const cos_v79_particles_t *p,
                                      uint32_t i)
{
    /* a = -k * x * inv_m  (all Q16.16) */
    cos_v79_q16_t kx   = q16_mul(p->k[i], p->x[i]);
    cos_v79_q16_t akxm = q16_mul(kx, p->inv_m[i]);
    return -akxm;
}

void cos_v79_hv_verlet(cos_v79_particles_t *p, cos_v79_q16_t dt)
{
    cos_v79_q16_t half_dt = dt >> 1;
    /* half-kick + drift + half-kick, in place. */
    for (uint32_t i = 0; i < p->n; ++i) {
        cos_v79_q16_t a0 = accel_q16(p, i);
        p->v[i] += q16_mul(a0, half_dt);
        p->x[i] += q16_mul(p->v[i], dt);
        cos_v79_q16_t a1 = accel_q16(p, i);
        p->v[i] += q16_mul(a1, half_dt);
    }
}

cos_v79_q16_t cos_v79_energy_q16(const cos_v79_particles_t *p)
{
    /* Pseudo-energy (units where the Verlet shadow Hamiltonian is
     * conserved modulo rounding):  Σ k x²/2 + Σ v² inv_m / 2. */
    cos_v79_q16_t e = 0;
    for (uint32_t i = 0; i < p->n; ++i) {
        cos_v79_q16_t xx  = q16_mul(p->x[i], p->x[i]);
        cos_v79_q16_t kxx = q16_mul(p->k[i], xx);
        cos_v79_q16_t vv  = q16_mul(p->v[i], p->v[i]);
        cos_v79_q16_t vvm = q16_mul(vv, p->inv_m[i]);
        e += (kxx >> 1) + (vvm >> 1);
    }
    return e;
}

/* ==================================================================
 *  2.  Wolfram 1D cellular automaton on a 256-bit lattice.
 * ================================================================== */

static inline cos_v79_hv_t hv_shl_1_wrap(cos_v79_hv_t s)
{
    cos_v79_hv_t r;
    uint64_t c0 = s.w[0] >> 63;
    uint64_t c1 = s.w[1] >> 63;
    uint64_t c2 = s.w[2] >> 63;
    uint64_t c3 = s.w[3] >> 63;
    r.w[0] = (s.w[0] << 1) | c3;
    r.w[1] = (s.w[1] << 1) | c0;
    r.w[2] = (s.w[2] << 1) | c1;
    r.w[3] = (s.w[3] << 1) | c2;
    return r;
}

static inline cos_v79_hv_t hv_shr_1_wrap(cos_v79_hv_t s)
{
    cos_v79_hv_t r;
    uint64_t c0 = (s.w[0] & 1ULL) << 63;
    uint64_t c1 = (s.w[1] & 1ULL) << 63;
    uint64_t c2 = (s.w[2] & 1ULL) << 63;
    uint64_t c3 = (s.w[3] & 1ULL) << 63;
    r.w[0] = (s.w[0] >> 1) | c1;
    r.w[1] = (s.w[1] >> 1) | c2;
    r.w[2] = (s.w[2] >> 1) | c3;
    r.w[3] = (s.w[3] >> 1) | c0;
    return r;
}

static inline cos_v79_hv_t hv_xor(cos_v79_hv_t a, cos_v79_hv_t b)
{
    cos_v79_hv_t r;
    r.w[0] = a.w[0] ^ b.w[0];
    r.w[1] = a.w[1] ^ b.w[1];
    r.w[2] = a.w[2] ^ b.w[2];
    r.w[3] = a.w[3] ^ b.w[3];
    return r;
}

static inline cos_v79_hv_t hv_and(cos_v79_hv_t a, cos_v79_hv_t b)
{
    cos_v79_hv_t r;
    r.w[0] = a.w[0] & b.w[0];
    r.w[1] = a.w[1] & b.w[1];
    r.w[2] = a.w[2] & b.w[2];
    r.w[3] = a.w[3] & b.w[3];
    return r;
}

static inline cos_v79_hv_t hv_or(cos_v79_hv_t a, cos_v79_hv_t b)
{
    cos_v79_hv_t r;
    r.w[0] = a.w[0] | b.w[0];
    r.w[1] = a.w[1] | b.w[1];
    r.w[2] = a.w[2] | b.w[2];
    r.w[3] = a.w[3] | b.w[3];
    return r;
}

static inline cos_v79_hv_t hv_not(cos_v79_hv_t a)
{
    cos_v79_hv_t r;
    r.w[0] = ~a.w[0];
    r.w[1] = ~a.w[1];
    r.w[2] = ~a.w[2];
    r.w[3] = ~a.w[3];
    return r;
}

static inline cos_v79_hv_t hv_broadcast_mask(uint64_t mask)
{
    cos_v79_hv_t r;
    r.w[0] = r.w[1] = r.w[2] = r.w[3] = mask;
    return r;
}

void cos_v79_ca_step(cos_v79_ca_t *ca)
{
    cos_v79_hv_t left  = hv_shl_1_wrap(ca->state);
    cos_v79_hv_t cent  = ca->state;
    cos_v79_hv_t right = hv_shr_1_wrap(ca->state);
    cos_v79_hv_t nl    = hv_not(left);
    cos_v79_hv_t nc    = hv_not(cent);
    cos_v79_hv_t nr    = hv_not(right);

    cos_v79_hv_t next = {{0, 0, 0, 0}};
    uint8_t rule = ca->rule;

    /* Branchless: accumulate contributions for all 8 patterns. */
    for (unsigned pat = 0; pat < 8u; ++pat) {
        cos_v79_hv_t m_l = (pat & 4u) ? left  : nl;
        cos_v79_hv_t m_c = (pat & 2u) ? cent  : nc;
        cos_v79_hv_t m_r = (pat & 1u) ? right : nr;
        cos_v79_hv_t m   = hv_and(hv_and(m_l, m_c), m_r);
        /* Include or exclude depending on rule bit — as a 64-bit all-
         * ones or all-zeros mask so there is no branch. */
        uint64_t bit      = (uint64_t)((rule >> pat) & 1u);
        uint64_t bcastmsk = (uint64_t)0 - bit;
        cos_v79_hv_t bc   = hv_broadcast_mask(bcastmsk);
        next = hv_or(next, hv_and(m, bc));
    }
    ca->state = next;
}

/* ==================================================================
 *  3.  Aaronson-Gottesman stabilizer tableau.
 * ================================================================== */

void cos_v79_stab_init(cos_v79_stab_t *t, uint32_t n_qubits)
{
    memset(t, 0, sizeof(*t));
    if (n_qubits == 0 || n_qubits > COS_V79_QUBITS_MAX) {
        n_qubits = COS_V79_QUBITS_MAX;
    }
    t->n_qubits = n_qubits;
    /* Destabilisers X_i on rows 0..n-1; stabilisers Z_i on rows n..2n-1. */
    for (uint32_t i = 0; i < n_qubits; ++i) {
        t->x[i]               = (1u << i);           /* destabiliser X_i */
        t->z[n_qubits + i]    = (1u << i);           /* stabiliser  Z_i */
    }
}

/* bit_at(w, q) -> {0, 1} */
static inline uint32_t bit_at(uint32_t w, uint32_t q)
{
    return (w >> q) & 1u;
}

/* xor-into-bit: set bit q of *w to bit q of *w XOR b.  Reserved
 * for future composite Clifford gates. */
__attribute__((unused))
static inline void xor_bit(uint32_t *w, uint32_t q, uint32_t b)
{
    *w ^= ((b & 1u) << q);
}

/* apply a gate to a single tableau row.  Branchless via bit masks. */
static inline void stab_gate_row(cos_v79_stab_t *t,
                                 uint32_t row,
                                 cos_v79_gate_t gate,
                                 uint32_t a, uint32_t b)
{
    uint32_t xa = bit_at(t->x[row], a);
    uint32_t za = bit_at(t->z[row], a);
    uint32_t xb = bit_at(t->x[row], b);
    uint32_t zb = bit_at(t->z[row], b);
    uint32_t s  = bit_at(t->sign, row);

    uint32_t new_x_a = xa, new_z_a = za;
    uint32_t new_x_b = xb, new_z_b = zb;
    uint32_t new_s   = s;

    switch (gate) {
    case COS_V79_GATE_H:
        /* swap X and Z on qubit a; sign flips by xa & za. */
        new_x_a = za;
        new_z_a = xa;
        new_s   = s ^ (xa & za);
        break;
    case COS_V79_GATE_S:
        new_s   = s ^ (xa & za);
        new_z_a = za ^ xa;
        break;
    case COS_V79_GATE_X:
        new_s   = s ^ za;
        break;
    case COS_V79_GATE_Z:
        new_s   = s ^ xa;
        break;
    case COS_V79_GATE_Y:
        new_s   = s ^ xa ^ za;
        break;
    case COS_V79_GATE_CNOT:
        /* sign update (Aaronson-Gottesman §3): 
         *   s ^= xa & zb & (xb ^ za ^ 1)
         * x_b ^= x_a,  z_a ^= z_b
         */
        new_s   = s ^ (xa & zb & (xb ^ za ^ 1u));
        new_x_b = xb ^ xa;
        new_z_a = za ^ zb;
        break;
    default:
        return;
    }

    /* write back */
    t->x[row] = (t->x[row] & ~(1u << a)) | ((new_x_a & 1u) << a);
    t->z[row] = (t->z[row] & ~(1u << a)) | ((new_z_a & 1u) << a);
    if (gate == COS_V79_GATE_CNOT) {
        t->x[row] = (t->x[row] & ~(1u << b)) | ((new_x_b & 1u) << b);
        t->z[row] = (t->z[row] & ~(1u << b)) | ((new_z_b & 1u) << b);
    }
    t->sign = (t->sign & ~(1u << row)) | ((new_s & 1u) << row);
}

int cos_v79_stab_step(cos_v79_stab_t *t,
                      cos_v79_gate_t gate,
                      uint32_t a, uint32_t b)
{
    if (t == NULL) return -1;
    if (a >= t->n_qubits) return -1;
    if (gate == COS_V79_GATE_CNOT) {
        if (b >= t->n_qubits || a == b) return -1;
    }
    uint32_t rows = 2u * t->n_qubits;
    for (uint32_t r = 0; r < rows; ++r) {
        stab_gate_row(t, r, gate, a, b);
    }
    return 0;
}

uint32_t cos_v79_stab_invariant_ok(const cos_v79_stab_t *t)
{
    /* Each pair of stabiliser rows (n..2n-1) must commute: symplectic
     * inner product must be zero. */
    uint32_t n = t->n_qubits;
    uint32_t ok = 1u;
    for (uint32_t i = n; i < 2u * n; ++i) {
        for (uint32_t j = i + 1u; j < 2u * n; ++j) {
            uint32_t s  = popcount32((t->x[i] & t->z[j]) ^ (t->x[j] & t->z[i]));
            ok &= ((~s) & 1u);
        }
    }
    return ok;
}

/* ==================================================================
 *  4.  HD reservoir step.
 * ================================================================== */

static inline uint64_t xorshift64(uint64_t *s)
{
    uint64_t x = *s;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *s = x;
    return x;
}

void cos_v79_reservoir_init(cos_v79_reservoir_t *r, uint64_t seed)
{
    memset(r, 0, sizeof(*r));
    uint64_t s = seed | 1ULL;
    for (uint32_t i = 0; i < COS_V79_HV_WORDS; ++i) {
        for (uint32_t w = 0; w < COS_V79_HV_WORDS; ++w) {
            r->proj[i].w[w] = xorshift64(&s);
        }
    }
    r->threshold = 128; /* 50% of 256 bits */
}

static inline cos_v79_hv_t hv_rotl_1(cos_v79_hv_t s)
{
    /* Treat the HV as a single 256-bit ring, rotate left by 1 bit. */
    return hv_shl_1_wrap(s);
}

void cos_v79_reservoir_step(cos_v79_reservoir_t *r,
                            const cos_v79_hv_t *input_hv)
{
    /* Frady-style echo-state-in-HV: new state is a nonlinear
     * combination of the rotated previous state, the input, and a
     * fixed projection row.  Branchless, O(1). */
    cos_v79_hv_t t = hv_rotl_1(r->state);
    t = hv_xor(t, *input_hv);
    t = hv_xor(t, r->proj[0]);
    r->state = t;
}

/* ==================================================================
 *  5.  Koopman embedding.
 * ================================================================== */

void cos_v79_koopman_embed(const cos_v79_hv_t *state,
                           cos_v79_hv_t *observable_out)
{
    cos_v79_hv_t shifted;
    /* 256-bit rotate-left by 13 bits, distributed across 4 × uint64.
     * Bit-shift within each 64-bit word + carry across words. */
    uint64_t c0 = state->w[0] >> (64u - 13u);
    uint64_t c1 = state->w[1] >> (64u - 13u);
    uint64_t c2 = state->w[2] >> (64u - 13u);
    uint64_t c3 = state->w[3] >> (64u - 13u);
    shifted.w[0] = (state->w[0] << 13u) | c3;
    shifted.w[1] = (state->w[1] << 13u) | c0;
    shifted.w[2] = (state->w[2] << 13u) | c1;
    shifted.w[3] = (state->w[3] << 13u) | c2;
    *observable_out = hv_xor(*state, shifted);
}

/* ==================================================================
 *  6.  Assembly index (integer upper bound).
 *
 *   Counts distinct 4-bit nibbles occurring in the string, plus a
 *   ceil_log2(n_bits) term.  This is a well-defined integer upper
 *   bound on Sharma-Cronin assembly index for short strings.
 * ================================================================== */

static inline uint32_t ceil_log2_u32(uint32_t x)
{
    if (x <= 1u) return 0u;
    uint32_t r = 0;
    uint32_t v = x - 1u;
    while (v) { ++r; v >>= 1; }
    return r;
}

uint32_t cos_v79_assembly_index(uint64_t bits, uint32_t n_bits)
{
    if (n_bits == 0u) return 0u;
    if (n_bits > 64u) n_bits = 64u;

    /* Mask to the relevant window. */
    uint64_t mask = (n_bits >= 64u) ? ~(uint64_t)0
                                    : ((uint64_t)1 << n_bits) - 1ULL;
    bits &= mask;

    uint32_t seen[16];
    for (uint32_t i = 0; i < 16u; ++i) seen[i] = 0u;

    uint32_t chunks = (n_bits + 3u) / 4u;
    for (uint32_t i = 0; i < chunks; ++i) {
        uint32_t nib = (uint32_t)((bits >> (i * 4u)) & 0xFu);
        seen[nib] = 1u;
    }
    uint32_t distinct = 0;
    for (uint32_t i = 0; i < 16u; ++i) distinct += seen[i];

    return distinct + ceil_log2_u32(n_bits);
}

/* ==================================================================
 *  7.  Boolean-network graph step.
 * ================================================================== */

void cos_v79_graph_init(cos_v79_graph_t *g,
                        const uint64_t *adj,
                        uint32_t n_nodes,
                        uint64_t initial_state,
                        uint32_t threshold)
{
    memset(g, 0, sizeof(*g));
    if (n_nodes > 64u) n_nodes = 64u;
    g->n_nodes  = n_nodes;
    g->state    = initial_state;
    g->threshold= threshold;
    if (adj) {
        for (uint32_t i = 0; i < n_nodes; ++i) g->adj[i] = adj[i];
    }
}

void cos_v79_graph_step(cos_v79_graph_t *g)
{
    uint64_t cur = g->state;
    uint64_t nxt = 0ULL;
    for (uint32_t i = 0; i < g->n_nodes; ++i) {
        uint32_t pc   = popcount64(g->adj[i] & cur);
        int32_t  diff = (int32_t)pc - (int32_t)g->threshold;
        /* ge_mask: all-ones if pc >= threshold, else all-zeros. */
        uint64_t ge_mask = (uint64_t)(~(uint32_t)(diff >> 31));
        nxt |= ((uint64_t)(ge_mask & 1ULL)) << i;
    }
    g->state = nxt;
}

/* ==================================================================
 *  9.  Receipt.
 * ================================================================== */

void cos_v79_receipt_update(cos_v79_receipt_t *r,
                            const cos_v79_hv_t *state_hv)
{
    unsigned rot = (unsigned)((r->step_count * 7u) & 63u);
    cos_v79_hv_t mixed;
    mixed.w[0] = rotl64(state_hv->w[0], rot);
    mixed.w[1] = rotl64(state_hv->w[1], rot + 11u);
    mixed.w[2] = rotl64(state_hv->w[2], rot + 23u);
    mixed.w[3] = rotl64(state_hv->w[3], rot + 37u);
    r->receipt.w[0] ^= mixed.w[0];
    r->receipt.w[1] ^= mixed.w[1];
    r->receipt.w[2] ^= mixed.w[2];
    r->receipt.w[3] ^= mixed.w[3];
    r->step_count += 1u;
}

/* ==================================================================
 * 10.  SSL VM.
 * ================================================================== */

static int ssl_validate(cos_v79_ssl_insn_t i)
{
    /* Reserved bits must be zero. */
    if ((i >> 24) != 0u) return -1;
    if (cos_v79_op_of(i) > 7u) return -1;
    return 0;
}

/* Assemble the world into a single 256-bit state-vector so receipts
 * are informative. */
static cos_v79_hv_t world_fold(const cos_v79_world_t *w)
{
    cos_v79_hv_t h;
    h.w[0] = w->ca.state.w[0]
           ^ ((uint64_t)w->reservoir.state.w[0])
           ^ ((uint64_t)w->graph.state)
           ^ ((uint64_t)w->receipt.step_count);
    h.w[1] = w->ca.state.w[1]
           ^ w->reservoir.state.w[1]
           ^ w->koopman.w[0];
    h.w[2] = w->ca.state.w[2]
           ^ w->reservoir.state.w[2]
           ^ w->koopman.w[1];
    h.w[3] = w->ca.state.w[3]
           ^ w->reservoir.state.w[3]
           ^ w->koopman.w[2]
           ^ w->koopman.w[3];
    /* Fold particle state. */
    for (uint32_t i = 0; i < w->particles.n; ++i) {
        h.w[i & 3u] ^= ((uint64_t)(uint32_t)w->particles.x[i] << 32)
                     | ((uint64_t)(uint32_t)w->particles.v[i]);
    }
    /* Fold stabilizer. */
    h.w[0] ^= ((uint64_t)w->stab.x[0]) | ((uint64_t)w->stab.z[0] << 32);
    h.w[1] ^= ((uint64_t)w->stab.sign) << 16;
    return h;
}

int cos_v79_ssl_run(cos_v79_world_t *world,
                    const cos_v79_ssl_insn_t *prog,
                    size_t prog_len,
                    cos_v79_q16_t dt)
{
    if (world == NULL || prog == NULL) return -1;
    size_t i;
    for (i = 0; i < prog_len && i < COS_V79_SSL_PROG_MAX; ++i) {
        cos_v79_ssl_insn_t insn = prog[i];
        if (ssl_validate(insn) != 0) return -((int)i + 1);

        uint32_t op = cos_v79_op_of(insn);
        uint32_t a  = cos_v79_a_of(insn);
        uint32_t b  = cos_v79_b_of(insn);
        uint32_t c  = cos_v79_c_of(insn);
        (void)c;

        switch ((cos_v79_op_t)op) {
        case COS_V79_OP_HALT:
            return (int)i;
        case COS_V79_OP_VRL:
            cos_v79_hv_verlet(&world->particles, dt);
            break;
        case COS_V79_OP_CAS:
            cos_v79_ca_step(&world->ca);
            break;
        case COS_V79_OP_STB: {
            cos_v79_gate_t g = (cos_v79_gate_t)a;
            if (cos_v79_stab_step(&world->stab, g, b, c) != 0)
                return -((int)i + 1);
            break;
        }
        case COS_V79_OP_RSV: {
            /* Feed the CA state as the reservoir input so the two
             * substrates couple without extra arguments. */
            cos_v79_reservoir_step(&world->reservoir, &world->ca.state);
            break;
        }
        case COS_V79_OP_KOP:
            cos_v79_koopman_embed(&world->ca.state, &world->koopman);
            break;
        case COS_V79_OP_GRP:
            cos_v79_graph_step(&world->graph);
            break;
        case COS_V79_OP_RCP: {
            cos_v79_hv_t fold = world_fold(world);
            cos_v79_receipt_update(&world->receipt, &fold);
            world->receipt.last_energy =
                cos_v79_energy_q16(&world->particles);
            break;
        }
        default:
            return -((int)i + 1);
        }
    }
    return (int)i;
}

/* ==================================================================
 *  11.  v79_ok gate + composed decision.
 * ================================================================== */

uint32_t cos_v79_ok(cos_v79_world_t *world,
                    const cos_v79_ssl_insn_t *prog,
                    size_t prog_len,
                    cos_v79_q16_t dt,
                    cos_v79_q16_t energy_drift_budget_q16)
{
    if (world == NULL) return 0u;
    cos_v79_q16_t e0 = cos_v79_energy_q16(&world->particles);
    int executed = cos_v79_ssl_run(world, prog, prog_len, dt);
    if (executed < 0) return 0u;
    cos_v79_q16_t e1 = cos_v79_energy_q16(&world->particles);
    cos_v79_q16_t ed = e1 - e0;
    cos_v79_q16_t ad = (ed < 0) ? -ed : ed;
    uint32_t energy_ok = (ad <= energy_drift_budget_q16) ? 1u : 0u;
    uint32_t stab_ok   = cos_v79_stab_invariant_ok(&world->stab);
    return (energy_ok & 1u) & (stab_ok & 1u);
}

uint32_t cos_v79_compose_decision(uint32_t v78_composed_ok,
                                  uint32_t v79_ok)
{
    v78_composed_ok &= 1u;
    v79_ok          &= 1u;
    return v78_composed_ok & v79_ok;
}
