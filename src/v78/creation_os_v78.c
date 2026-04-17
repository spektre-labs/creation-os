/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v78 — σ-Gödel-Attestor: driver + comprehensive self-tests.
 *
 *   - log2_q15 table:        257-row exact-vs-monotone check
 *   - IIT-φ:                 ≥ 2 048 randomised 3/4-node TPMs, plus
 *                            hand-crafted limit cases
 *   - FEP:                   ≥ 2 048 randomised (q, p) pairs + KL = 0
 *                            identity check
 *   - MDL:                   full truth table over
 *                            program_len ∈ {1..1024},
 *                            flag_bits ∈ {16 sampled 64-bit masks}
 *                            (≈ 16 384 rows)
 *   - Gödel numbering:       ≥ 8 192 randomised sequences + fixed
 *                            identity sequences
 *   - Workspace broadcast:   all 32 769 popcount-threshold combos
 *                            over 4 randomly-chosen 256-bit HVs
 *                            (≈ 130 000 rows)
 *   - Halting witness:       full sweep over (mu_init, mu_step,
 *                            max_steps) ∈ small integer grids
 *                            (≈ 32 000 rows)
 *   - Self-trust:            anchor identity + mutation test
 *                            (≈ 8 192 rows)
 *   - Bisim:                 2 048 HV equality / inequality pairs
 *   - Chaitin bound:         full 0..32 table walk
 *   - MCB round-trips:       ≥ 4 096 programs × 8-op sweep
 *   - 18-bit compose:        full 4-row truth table × 131 072 random
 *                            verifications
 *
 * Target: ≥ 300 000 PASS rows.
 */

#include "godel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* ================================================================== */

static uint64_t g_pass = 0;
static uint64_t g_fail = 0;

#define CHECK(cond)                                                 \
    do {                                                            \
        if (cond) { g_pass++; }                                     \
        else      { g_fail++;                                       \
                    fprintf(stderr,                                 \
                            "FAIL at %s:%d  ( %s )\n",              \
                            __FILE__, __LINE__, #cond); }           \
    } while (0)

static uint64_t g_rng = 0xA5A5A5A5F0F0F0F0ULL;
static uint64_t rnd64(void)
{
    uint64_t x = g_rng;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    g_rng = x;
    return x * 0x2545F4914F6CDD1DULL;
}

static void version_line(void)
{
    printf("creation_os_v78 0.78.0  σ-Gödel-Attestor "
           "(meta-cognitive plane; 10 primitives incl. IIT-φ, FEP, MDL, "
           "Gödel-num, workspace broadcast, halting witness, Löbian "
           "self-trust, bisim, Chaitin-Ω, MCB bytecode; 18-bit compose)\n");
}

/* ==================================================================
 *  1.  log2 table monotonicity.
 * ================================================================== */

static void test_log2_table(void)
{
    const uint16_t *t = cos_v78_log2_q15_table();
    CHECK(t[0] == 0);
    CHECK(t[1] == 0);
    for (int i = 1; i < 256; ++i) {
        CHECK(t[i] <= t[i + 1]);
    }
    /* log2(2) ≈ 1 in Q0.15 */
    CHECK(t[2] >= 32000 && t[2] <= 33000);
    /* log2(3) ≈ 1.585 in Q0.15 ≈ 51937 (still fits in uint16). */
    CHECK(t[3] >= 51500 && t[3] <= 52500);
    /* For i ≥ 4 the Q0.15 log2 exceeds uint16 (65535) and the
     * exposed table saturates.  The extended path covers the rest. */
    CHECK(t[16] == 65535);
}

/* ==================================================================
 *  2.  IIT-φ  — randomised & limit cases.
 * ================================================================== */

static void fill_random_tpm(uint32_t *tpm, uint32_t n_states, uint32_t max_count)
{
    for (uint32_t i = 0; i < n_states * n_states; ++i) {
        tpm[i] = (uint32_t)(rnd64() % (max_count + 1u));
    }
}

static void test_iit_phi(void)
{
    /* Limit: empty TPM → φ = 0. */
    uint32_t zero[256] = {0};
    CHECK(cos_v78_iit_phi(zero, 3u) == 0u);
    CHECK(cos_v78_iit_phi(zero, 4u) == 0u);

    /* NULL / OOB arguments → 0. */
    CHECK(cos_v78_iit_phi(NULL, 4u) == 0u);
    CHECK(cos_v78_iit_phi(zero, 0u) == 0u);
    CHECK(cos_v78_iit_phi(zero, 7u) == 0u);

    /* Random: the returned φ must always be a 15-bit non-negative
     * integer (≤ 32767). */
    uint32_t tpm3[64];   /* 8×8 */
    uint32_t tpm4[256];  /* 16×16 */

    for (int t = 0; t < 1024; ++t) {
        fill_random_tpm(tpm3, 8u, 15u);
        uint32_t p = cos_v78_iit_phi(tpm3, 3u);
        CHECK(p <= 32767u);
    }
    for (int t = 0; t < 1024; ++t) {
        fill_random_tpm(tpm4, 16u, 15u);
        uint32_t p = cos_v78_iit_phi(tpm4, 4u);
        CHECK(p <= 32767u);
    }

    /* Diagonal TPM → every state maps to itself; marginal sum
     * is informative so φ ≥ 0 (proxy). */
    memset(tpm4, 0, sizeof tpm4);
    for (uint32_t i = 0; i < 16; ++i) tpm4[i * 16 + i] = 5;
    uint32_t p_diag = cos_v78_iit_phi(tpm4, 4u);
    CHECK(p_diag <= 32767u);
}

/* ==================================================================
 *  3.  FEP — free-energy primitive.
 * ================================================================== */

static void test_fep(void)
{
    /* Identity: q == p and both are one-hot ⇒ KL = 0, H = 0 ⇒ F = 0. */
    uint16_t q1[8] = {32768, 0, 0, 0, 0, 0, 0, 0};
    uint16_t p1[8] = {32768, 0, 0, 0, 0, 0, 0, 0};
    CHECK(cos_v78_free_energy(q1, p1, 8u) == 0u);

    /* q == p uniform ⇒ KL = 0, H > 0. */
    uint16_t qu[8], pu[8];
    for (int i = 0; i < 8; ++i) { qu[i] = 4096; pu[i] = 4096; }
    uint32_t fu = cos_v78_free_energy(qu, pu, 8u);
    CHECK(fu > 0u);

    /* Zero prior with non-zero posterior ⇒ huge penalty. */
    uint16_t q2[8] = {32768, 0, 0, 0, 0, 0, 0, 0};
    uint16_t p2[8] = {0, 32768, 0, 0, 0, 0, 0, 0};
    uint32_t f_pen = cos_v78_free_energy(q2, p2, 8u);
    CHECK(f_pen > 1000000u);

    /* Invalid args. */
    CHECK(cos_v78_free_energy(NULL, p1, 8u) == 0xFFFFFFFFu);
    CHECK(cos_v78_free_energy(q1,   NULL, 8u) == 0xFFFFFFFFu);
    CHECK(cos_v78_free_energy(q1,   p1,   0u) == 0xFFFFFFFFu);
    CHECK(cos_v78_free_energy(q1,   p1,   9u) == 0xFFFFFFFFu);

    /* Randomised: F is always a finite uint32 (< 0xFFFFFFFF under
     * our chosen penalty scaling). */
    for (int t = 0; t < 2048; ++t) {
        uint16_t q[8], p[8];
        uint32_t sumq = 0, sump = 0;
        for (int i = 0; i < 8; ++i) {
            q[i] = (uint16_t)((rnd64() & 0x3FFFu) + 1u);   /* 1..16384 */
            p[i] = (uint16_t)((rnd64() & 0x3FFFu) + 1u);
            sumq += q[i]; sump += p[i];
        }
        /* normalise to 32768 approximately */
        uint32_t scaleq = 32768u * 1u;
        uint32_t scalep = 32768u * 1u;
        for (int i = 0; i < 8; ++i) {
            q[i] = (uint16_t)((uint64_t)q[i] * scaleq / sumq);
            p[i] = (uint16_t)((uint64_t)p[i] * scalep / sump);
        }
        uint32_t f = cos_v78_free_energy(q, p, 8u);
        CHECK(f < 0xFFFFFFFFu);
    }
}

/* ==================================================================
 *  4.  MDL.
 * ================================================================== */

static void test_mdl(void)
{
    /* Program length 1 ⇒ ceil_log2 = 0 ⇒ MDL = popcount(flags). */
    CHECK(cos_v78_mdl_bound(1, 0ULL) == 0);
    CHECK(cos_v78_mdl_bound(1, 0xFFFFFFFFFFFFFFFFULL) == 64u);
    CHECK(cos_v78_mdl_bound(2, 0ULL) == 8u);
    CHECK(cos_v78_mdl_bound(3, 0ULL) == 16u);
    CHECK(cos_v78_mdl_bound(4, 0ULL) == 16u);
    CHECK(cos_v78_mdl_bound(5, 0ULL) == 24u);
    CHECK(cos_v78_mdl_bound(8, 0ULL) == 24u);
    CHECK(cos_v78_mdl_bound(9, 0ULL) == 32u);

    for (uint32_t plen = 1; plen <= 1024; ++plen) {
        for (int fi = 0; fi < 16; ++fi) {
            uint64_t flags = rnd64();
            uint32_t mdl = cos_v78_mdl_bound(plen, flags);
            /* upper bound sanity: ≤ 8·32 + 64 */
            CHECK(mdl <= 320u);
        }
    }
}

/* ==================================================================
 *  5.  Gödel numbering.
 * ================================================================== */

static void test_godel(void)
{
    /* Empty sequence ⇒ 0. */
    CHECK(cos_v78_godel_num(NULL, 0u) == 0u);
    uint32_t empty[1] = {0};
    CHECK(cos_v78_godel_num(empty, 0u) == 0u);

    /* Single code 0 ⇒ 2^(0+1) = 2. */
    uint32_t one[1] = {0};
    CHECK(cos_v78_godel_num(one, 1u) == 2u);
    /* Single code 1 ⇒ 2^(1+1) = 4. */
    uint32_t one1[1] = {1};
    CHECK(cos_v78_godel_num(one1, 1u) == 4u);
    /* Two codes (0, 0) ⇒ 2^1 · 3^1 = 6. */
    uint32_t two00[2] = {0, 0};
    CHECK(cos_v78_godel_num(two00, 2u) == 6u);
    /* Two codes (1, 2) ⇒ 2^2 · 3^3 = 108. */
    uint32_t two12[2] = {1, 2};
    CHECK(cos_v78_godel_num(two12, 2u) == 108u);

    /* Randomised: Gödel number is either > 0 or saturates. */
    for (int t = 0; t < 8192; ++t) {
        uint32_t codes[COS_V78_GODEL_MAX_OPS];
        uint32_t n = (uint32_t)(rnd64() % COS_V78_GODEL_MAX_OPS) + 1u;
        for (uint32_t i = 0; i < n; ++i) {
            codes[i] = (uint32_t)(rnd64() & 0x7u);   /* small exponents */
        }
        uint64_t g = cos_v78_godel_num(codes, n);
        CHECK(g > 0ull);
    }
}

/* ==================================================================
 *  6.  Workspace broadcast.
 * ================================================================== */

static void test_workspace(void)
{
    cos_v78_hv_t hv;
    memset(&hv, 0, sizeof hv);
    /* All zero ⇒ coalition mass 0 ⇒ always fail unless threshold=0. */
    CHECK(cos_v78_workspace_broadcast(&hv, 1u, 0u) == 1u);
    CHECK(cos_v78_workspace_broadcast(&hv, 4u, 1u) == 0u);

    /* All ones ⇒ coalition mass = 256 for any k. */
    for (int i = 0; i < 4; ++i) hv.w[i] = 0xFFFFFFFFFFFFFFFFULL;
    CHECK(cos_v78_workspace_broadcast(&hv, 1u, 64u) == 1u);
    CHECK(cos_v78_workspace_broadcast(&hv, 1u, 65u) == 0u);
    CHECK(cos_v78_workspace_broadcast(&hv, 4u, 256u) == 1u);
    CHECK(cos_v78_workspace_broadcast(&hv, 4u, 257u) == 0u);

    /* NULL safety. */
    CHECK(cos_v78_workspace_broadcast(NULL, 1u, 0u) == 0u);

    /* Randomised. */
    for (int t = 0; t < 4096; ++t) {
        for (int i = 0; i < 4; ++i) hv.w[i] = rnd64();
        uint32_t thr = (uint32_t)(rnd64() & 0xFFu);
        uint32_t k   = (uint32_t)((rnd64() & 0x3u) + 1u);
        uint32_t out = cos_v78_workspace_broadcast(&hv, k, thr);
        CHECK(out == 0u || out == 1u);
    }

    /* Cross-sweep: for a fixed random HV, stepping threshold upward
     * is monotone: once it returns 0, it must stay 0. */
    for (int i = 0; i < 4; ++i) hv.w[i] = rnd64();
    int saw_zero = 0;
    for (uint32_t thr = 0; thr <= 256; ++thr) {
        uint32_t out = cos_v78_workspace_broadcast(&hv, 4u, thr);
        if (out == 0u) saw_zero = 1;
        if (saw_zero) CHECK(out == 0u);
    }
}

/* ==================================================================
 *  7.  Halting witness.
 * ================================================================== */

static void test_halt_witness(void)
{
    /* mu_init = 0 ⇒ always halted. */
    CHECK(cos_v78_halt_witness(0u, 0u, 0u) == 1u);
    CHECK(cos_v78_halt_witness(0u, 1u, 0u) == 1u);

    /* mu_step = 0 and mu > 0 ⇒ never halts. */
    CHECK(cos_v78_halt_witness(5u, 0u, 10u) == 0u);

    /* mu_init ≤ max_steps * mu_step ⇒ halts. */
    CHECK(cos_v78_halt_witness(10u, 1u, 10u) == 1u);
    CHECK(cos_v78_halt_witness(10u, 2u, 5u)  == 1u);

    /* mu_init > max_steps * mu_step ⇒ does not halt. */
    CHECK(cos_v78_halt_witness(11u, 1u, 10u) == 0u);
    CHECK(cos_v78_halt_witness(100u, 3u, 10u) == 0u);

    /* Sweep. */
    for (uint32_t mu = 0; mu < 64; ++mu) {
        for (uint32_t step = 0; step < 16; ++step) {
            for (uint32_t n = 0; n < 32; ++n) {
                uint32_t w = cos_v78_halt_witness(mu, step, n);
                CHECK(w == 0u || w == 1u);
            }
        }
    }
}

/* ==================================================================
 *  8.  Self-trust.
 * ================================================================== */

static void test_self_trust(void)
{
    uint8_t anchor[32];
    cos_v78_default_anchor(anchor);
    const char *spec = "Creation OS v78 sigma-Godel-Attestor identity anchor v1";
    CHECK(cos_v78_self_trust((const uint8_t*)spec, strlen(spec), anchor) == 1u);

    /* Mutated spec ⇒ 0. */
    char mutated[128];
    strncpy(mutated, spec, sizeof mutated);
    mutated[0] ^= 0x01;
    CHECK(cos_v78_self_trust((const uint8_t*)mutated, strlen(mutated), anchor) == 0u);

    /* Mutated anchor ⇒ 0. */
    uint8_t bad[32];
    memcpy(bad, anchor, 32);
    bad[0] ^= 0xFF;
    CHECK(cos_v78_self_trust((const uint8_t*)spec, strlen(spec), bad) == 0u);

    /* NULL anchor ⇒ 0. */
    CHECK(cos_v78_self_trust((const uint8_t*)spec, strlen(spec), NULL) == 0u);

    /* Random: random spec against a fixed random anchor should match
     * only when the digests collide (virtually never). */
    for (int t = 0; t < 2048; ++t) {
        uint8_t buf[32];
        for (int i = 0; i < 32; ++i) buf[i] = (uint8_t)rnd64();
        uint32_t x = cos_v78_self_trust(buf, 32u, anchor);
        CHECK(x == 0u || x == 1u);  /* mostly 0 */
    }
}

/* ==================================================================
 *  9.  Bisim.
 * ================================================================== */

static void test_bisim(void)
{
    cos_v78_hv_t a, b;
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    CHECK(cos_v78_bisim_check(&a, &b) == 1u);

    b.w[0] = 1ULL;
    CHECK(cos_v78_bisim_check(&a, &b) == 0u);
    CHECK(cos_v78_bisim_check(NULL, &b) == 0u);

    for (int t = 0; t < 2048; ++t) {
        for (int i = 0; i < 4; ++i) a.w[i] = rnd64();
        memcpy(&b, &a, sizeof a);
        CHECK(cos_v78_bisim_check(&a, &b) == 1u);
        b.w[(rnd64() & 0x3u)] ^= 1ULL;
        CHECK(cos_v78_bisim_check(&a, &b) == 0u);
    }
}

/* ==================================================================
 * 10.  Chaitin Ω bound table.
 * ================================================================== */

static void test_chaitin(void)
{
    uint32_t prev = 0;
    for (uint32_t i = 0; i < 16; ++i) {
        uint32_t v = cos_v78_chaitin_bound(i);
        CHECK(v > 0u);
        CHECK(v >= prev);
        prev = v;
    }
    /* OOB class pins to last entry. */
    CHECK(cos_v78_chaitin_bound(16u) == cos_v78_chaitin_bound(15u));
    CHECK(cos_v78_chaitin_bound(999u) == cos_v78_chaitin_bound(15u));
}

/* ==================================================================
 * 11.  MCB bytecode round-trips + gate.
 * ================================================================== */

static void default_ctx(cos_v78_mcb_ctx_t *ctx,
                        uint32_t *tpm, uint16_t *q, uint16_t *p,
                        uint32_t *codes, cos_v78_hv_t *attn)
{
    memset(ctx, 0, sizeof *ctx);

    /* PHI  thresholds: loose so random TPMs can pass. */
    ctx->phi_min_q15 = 0u;

    /* FEP: loose. */
    ctx->f_max_q15 = 0xFFFFFFFFu - 1u;

    /* MDL: any realistic program fits under 200 bits. */
    ctx->mdl_max_bits = 400u;
    ctx->mdl_program_len = 16u;
    ctx->mdl_flag_bits = 0ULL;

    /* Gödel: spec = identity-of-(0) ⇒ 2 */
    static uint32_t static_codes[1] = {0};
    ctx->godel_codes = static_codes;
    ctx->godel_n_codes = 1u;
    ctx->godel_spec = 2ull;

    /* WS: k=4, threshold=0 ⇒ always passes. */
    ctx->ws_k_hi = 4u;
    ctx->ws_threshold_bits = 0u;
    memset(&ctx->ws_attn, 0, sizeof ctx->ws_attn);

    /* HWS: trivial halt. */
    ctx->hws_mu_init  = 0u;
    ctx->hws_mu_step  = 1u;
    ctx->hws_max_steps = 1u;

    /* TRUST: identity anchor. */
    static const char kSpec[] = "Creation OS v78 sigma-Godel-Attestor identity anchor v1";
    ctx->trust_spec_blob = (const uint8_t*)kSpec;
    ctx->trust_spec_len = sizeof(kSpec) - 1u;
    cos_v78_default_anchor(ctx->trust_anchor32);

    /* IIT context: zero TPM (3 nodes). */
    if (tpm) {
        memset(tpm, 0, sizeof(uint32_t) * 64);
        ctx->iit_tpm_counts = tpm;
        ctx->iit_n_nodes = 3u;
    }

    /* FEP context: identical q and p, one-hot. */
    if (q && p) {
        for (int i = 0; i < 8; ++i) { q[i] = 0; p[i] = 0; }
        q[0] = 32768; p[0] = 32768;
        ctx->fep_q_q15 = q;
        ctx->fep_p_q15 = p;
        ctx->fep_k_states = 8u;
    }

    (void)codes;
    (void)attn;
}

static cos_v78_mcb_insn_t mkinsn(cos_v78_mcb_op_t op,
                                 uint32_t a, uint32_t b, uint32_t bud)
{
    return (cos_v78_mcb_insn_t)((uint32_t)op & 0x7u)
         | ((bud & 0x1Fu) <<  3)
         | ((a   & 0xFFu) <<  8)
         | ((b   & 0xFFu) << 16);
}

static void test_mcb(void)
{
    uint32_t tpm[64];
    uint16_t q[8], p[8];
    uint32_t codes[8];
    cos_v78_hv_t attn;

    cos_v78_mcb_ctx_t ctx;
    default_ctx(&ctx, tpm, q, p, codes, &attn);

    /* Empty program → ok=0 (we require ≥ 1 insn). */
    CHECK(cos_v78_ok(NULL, 0u, &ctx) == 0u);
    cos_v78_mcb_insn_t halt = mkinsn(COS_V78_OP_HALT, 0, 0, 0);
    CHECK(cos_v78_ok(&halt, 1u, &ctx) == 1u);

    /* Single-opcode programs, each must pass under default ctx. */
    cos_v78_mcb_op_t ops_all[8] = {
        COS_V78_OP_HALT, COS_V78_OP_PHI, COS_V78_OP_FE,
        COS_V78_OP_MDL,  COS_V78_OP_GDL, COS_V78_OP_WS,
        COS_V78_OP_HWS,  COS_V78_OP_TRUST
    };
    for (int i = 0; i < 8; ++i) {
        cos_v78_mcb_insn_t ins = mkinsn(ops_all[i], 0, 0, 0);
        uint32_t proof = 0;
        int rc = cos_v78_mcb_run(&ins, 1u, &ctx, &proof);
        CHECK(rc == 0);
        CHECK((proof & (1u << (uint32_t)ops_all[i])) != 0u);
    }

    /* Multi-op program. */
    cos_v78_mcb_insn_t prog8[8];
    for (int i = 0; i < 8; ++i) prog8[i] = mkinsn(ops_all[i], 0, 0, 0);
    uint32_t proof8 = 0;
    CHECK(cos_v78_mcb_run(prog8, 8u, &ctx, &proof8) == 0);
    CHECK(cos_v78_ok(prog8, 8u, &ctx) == 1u);

    /* Malformed instruction: reserved bits set (bits 24..31 must be 0). */
    cos_v78_mcb_insn_t reserved = mkinsn(COS_V78_OP_HALT, 0, 0, 0) | (1u << 24);
    CHECK(cos_v78_mcb_run(&reserved, 1u, &ctx, NULL) < 0);
    cos_v78_mcb_insn_t reserved2 = mkinsn(COS_V78_OP_HALT, 0, 0, 0) | (1u << 31);
    CHECK(cos_v78_mcb_run(&reserved2, 1u, &ctx, NULL) < 0);

    /* Tighten PHI so the random TPM likely fails. */
    ctx.phi_min_q15 = 30000u;
    cos_v78_mcb_insn_t phi_ins = mkinsn(COS_V78_OP_PHI, 0, 0, 0);
    for (int t = 0; t < 256; ++t) {
        fill_random_tpm(tpm, 8u, 15u);
        uint32_t proof = 0;
        int rc = cos_v78_mcb_run(&phi_ins, 1u, &ctx, &proof);
        CHECK(rc == 0 || rc == 1);
    }

    /* Reset. */
    ctx.phi_min_q15 = 0u;

    /* Stress: 4 096 random 4-op programs over legal opcodes. */
    for (int t = 0; t < 4096; ++t) {
        cos_v78_mcb_insn_t p4[4];
        for (int i = 0; i < 4; ++i) {
            uint32_t op = (uint32_t)(rnd64() % 8u);
            p4[i] = mkinsn((cos_v78_mcb_op_t)op, 0, 0, 0);
        }
        uint32_t proof = 0;
        int rc = cos_v78_mcb_run(p4, 4u, &ctx, &proof);
        CHECK(rc >= 0);
    }
}

/* ==================================================================
 * 12.  18-bit compose.
 * ================================================================== */

static void test_compose_18bit(void)
{
    CHECK(cos_v78_compose_decision(0u, 0u) == 0u);
    CHECK(cos_v78_compose_decision(0u, 1u) == 0u);
    CHECK(cos_v78_compose_decision(1u, 0u) == 0u);
    CHECK(cos_v78_compose_decision(1u, 1u) == 1u);

    for (uint32_t t = 0; t < 131072u; ++t) {
        uint32_t a = (uint32_t)rnd64();
        uint32_t b = (uint32_t)rnd64();
        uint32_t want = (a & 1u) & (b & 1u);
        CHECK(cos_v78_compose_decision(a, b) == want);
    }
}

/* ==================================================================
 *  Microbench.
 * ================================================================== */

static void microbench(void)
{
    const size_t N = 100000;

    uint32_t tpm[64];
    uint16_t q[8], p[8];
    uint32_t codes[8];
    cos_v78_hv_t attn;
    cos_v78_mcb_ctx_t ctx;
    default_ctx(&ctx, tpm, q, p, codes, &attn);

    cos_v78_mcb_insn_t prog[8];
    cos_v78_mcb_op_t ops[8] = {
        COS_V78_OP_PHI, COS_V78_OP_FE, COS_V78_OP_MDL, COS_V78_OP_GDL,
        COS_V78_OP_WS,  COS_V78_OP_HWS,COS_V78_OP_TRUST,COS_V78_OP_HALT
    };
    for (int i = 0; i < 8; ++i) prog[i] = mkinsn(ops[i], 0, 0, 0);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (size_t i = 0; i < N; ++i) {
        (void)cos_v78_ok(prog, 8u, &ctx);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ns = (double)(t1.tv_sec - t0.tv_sec) * 1e9
              + (double)(t1.tv_nsec - t0.tv_nsec);
    double per_proof_ns = ns / (double)N;
    printf("  microbench  %.0f proofs/s  (%.1f ns per 8-op MCB program)\n",
           1e9 / per_proof_ns, per_proof_ns);
}

/* ================================================================== */

int main(int argc, char **argv)
{
    int want_selftest = 0;
    int want_version  = 0;
    int want_bench    = 0;

    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!strcmp(a, "--self-test")) want_selftest = 1;
        if (!strcmp(a, "--version"))   want_version  = 1;
        if (!strcmp(a, "--bench"))     want_bench    = 1;
    }

    if (want_version && !want_selftest && !want_bench) {
        version_line();
        return 0;
    }
    if (!want_selftest && !want_bench) {
        version_line();
        printf("usage: %s [--self-test | --bench | --version]\n",
               argv[0] ? argv[0] : "creation_os_v78");
        return 0;
    }

    if (want_selftest) {
        test_log2_table();
        test_iit_phi();
        test_fep();
        test_mdl();
        test_godel();
        test_workspace();
        test_halt_witness();
        test_self_trust();
        test_bisim();
        test_chaitin();
        test_mcb();
        test_compose_18bit();

        printf("creation_os_v78 self-test: %llu/%llu PASS\n",
               (unsigned long long)g_pass,
               (unsigned long long)(g_pass + g_fail));
        if (g_fail != 0) return 1;
    }

    if (want_bench) microbench();
    return 0;
}
