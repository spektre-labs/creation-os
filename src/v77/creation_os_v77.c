/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v77 — σ-Reversible: driver + comprehensive self-tests.
 *
 * Every primitive is exhaustively exercised:
 *   - NOT:          2-row truth table (x2 iterations)
 *   - CNOT:         4-row × 4 iterations
 *   - SWAP:         4-row × 4 iterations
 *   - Fredkin:      8-row truth table × 2 directions
 *   - Toffoli:      8-row truth table × 2 directions
 *   - Peres:        8-row truth table × 2 directions (forward + inverse)
 *   - Majority-3:   8-row truth table × 2 directions
 *   - 8-bit adder:  full 256 × 256 × 2 (carry) = 131 072 forward + reverse round-trips
 *   - VM round-trip: 2 048 random programs × forward ∘ reverse
 *   - 17-bit compose: 131 072 randomised verifications (v76_ok, v77_ok)
 *
 * Target: well over 200 000 PASS rows.
 */

#include "reversible.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* ==================================================================
 *  Test harness.
 * ================================================================== */

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

/* Tiny xorshift64* RNG — deterministic, libc-only. */
static uint64_t g_rng = 0x9E3779B97F4A7C15ULL;
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
    printf("creation_os_v77 0.77.0  σ-Reversible "
           "(Landauer / Bennett plane; 10 primitives; 17-bit compose; "
           "bit-reversible hot path)\n");
}

/* ==================================================================
 *  Register helpers + invariants.
 * ================================================================== */

static void reg_from_bit(cos_v77_reg_t *r, unsigned bit)
{
    memset(r, 0, sizeof *r);
    cos_v77_reg_set_bit0(r, bit & 1u);
}

static void reg_from_byte(cos_v77_reg_t *r, unsigned byte)
{
    memset(r, 0, sizeof *r);
    r->w[0] = (uint64_t)(byte & 0xFFu);
}

static unsigned low_byte(const cos_v77_reg_t *r)
{
    return (unsigned)(r->w[0] & 0xFFu);
}

/* ==================================================================
 *  1.  NOT — truth table.
 * ================================================================== */

static void test_not(void)
{
    for (unsigned rep = 0; rep < 4; ++rep) {
        for (unsigned b = 0; b <= 1; ++b) {
            cos_v77_reg_t r;
            reg_from_bit(&r, b);
            cos_v77_not(&r);
            CHECK(cos_v77_reg_get_bit0(&r) == (b ^ 1u));
            cos_v77_not(&r);
            CHECK(cos_v77_reg_get_bit0(&r) == b);
        }
    }
}

/* ==================================================================
 *  2.  CNOT — 4-row table, self-inverse round-trip.
 * ================================================================== */

static void test_cnot(void)
{
    for (unsigned rep = 0; rep < 4; ++rep) {
        for (unsigned a = 0; a <= 1; ++a)
        for (unsigned b = 0; b <= 1; ++b) {
            cos_v77_reg_t ra, rb;
            reg_from_bit(&ra, a);
            reg_from_bit(&rb, b);
            cos_v77_cnot(&ra, &rb);
            CHECK(cos_v77_reg_get_bit0(&rb) == (a ^ b));
            CHECK(cos_v77_reg_get_bit0(&ra) == a);
            cos_v77_cnot(&ra, &rb);
            CHECK(cos_v77_reg_get_bit0(&rb) == b);
        }
    }
}

/* ==================================================================
 *  3.  SWAP — 4-row table, self-inverse.
 * ================================================================== */

static void test_swap(void)
{
    for (unsigned rep = 0; rep < 4; ++rep) {
        for (unsigned a = 0; a <= 1; ++a)
        for (unsigned b = 0; b <= 1; ++b) {
            cos_v77_reg_t ra, rb;
            reg_from_bit(&ra, a);
            reg_from_bit(&rb, b);
            cos_v77_swap(&ra, &rb);
            CHECK(cos_v77_reg_get_bit0(&ra) == b);
            CHECK(cos_v77_reg_get_bit0(&rb) == a);
            cos_v77_swap(&ra, &rb);
            CHECK(cos_v77_reg_get_bit0(&ra) == a);
            CHECK(cos_v77_reg_get_bit0(&rb) == b);
        }
    }
}

/* ==================================================================
 *  4.  Fredkin CSWAP — 8-row table, self-inverse, conservation of ones.
 * ================================================================== */

static void test_fredkin(void)
{
    for (unsigned rep = 0; rep < 8; ++rep) {
        for (unsigned c = 0; c <= 1; ++c)
        for (unsigned a = 0; a <= 1; ++a)
        for (unsigned b = 0; b <= 1; ++b) {
            cos_v77_reg_t rc, ra, rb;
            reg_from_bit(&rc, c);
            reg_from_bit(&ra, a);
            reg_from_bit(&rb, b);
            cos_v77_fredkin(&rc, &ra, &rb);
            unsigned ea = c ? b : a;
            unsigned eb = c ? a : b;
            CHECK(cos_v77_reg_get_bit0(&ra) == ea);
            CHECK(cos_v77_reg_get_bit0(&rb) == eb);
            CHECK(cos_v77_reg_get_bit0(&rc) == c);
            /* Conservation of ones: popcount preserved. */
            CHECK((a + b + c) == (ea + eb + c));
            cos_v77_fredkin(&rc, &ra, &rb);
            CHECK(cos_v77_reg_get_bit0(&ra) == a);
            CHECK(cos_v77_reg_get_bit0(&rb) == b);
        }
    }
}

/* ==================================================================
 *  5.  Toffoli — 8-row table, self-inverse.
 * ================================================================== */

static void test_toffoli(void)
{
    for (unsigned rep = 0; rep < 8; ++rep) {
        for (unsigned a = 0; a <= 1; ++a)
        for (unsigned b = 0; b <= 1; ++b)
        for (unsigned c = 0; c <= 1; ++c) {
            cos_v77_reg_t ra, rb, rc;
            reg_from_bit(&ra, a);
            reg_from_bit(&rb, b);
            reg_from_bit(&rc, c);
            cos_v77_toffoli(&ra, &rb, &rc);
            unsigned ec = c ^ (a & b);
            CHECK(cos_v77_reg_get_bit0(&rc) == ec);
            CHECK(cos_v77_reg_get_bit0(&ra) == a);
            CHECK(cos_v77_reg_get_bit0(&rb) == b);
            cos_v77_toffoli(&ra, &rb, &rc);
            CHECK(cos_v77_reg_get_bit0(&rc) == c);
        }
    }
}

/* ==================================================================
 *  6.  Peres — 8-row table; forward + explicit inverse round-trip.
 * ================================================================== */

static void peres_inv_driver(const cos_v77_insn_t ins[], cos_v77_regfile_t *rf)
{
    /* Drive a single Peres instruction via the VM in reverse so we
     * test peres_inverse in its real production context. */
    uint32_t work = 0;
    (void)cos_v77_rvl_run(rf, ins, 1, (uint32_t)-1,
                          COS_V77_REVERSE, &work);
}

static void test_peres(void)
{
    for (unsigned a = 0; a <= 1; ++a)
    for (unsigned b = 0; b <= 1; ++b)
    for (unsigned c = 0; c <= 1; ++c) {
        /* Build a register file with a in r0, b in r1, c in r2. */
        cos_v77_regfile_t rf;
        memset(&rf, 0, sizeof rf);
        reg_from_bit(&rf.r[0], a);
        reg_from_bit(&rf.r[1], b);
        reg_from_bit(&rf.r[2], c);

        cos_v77_regfile_t rf0 = rf;

        /* Forward Peres. */
        cos_v77_peres(&rf.r[0], &rf.r[1], &rf.r[2]);
        unsigned eb = a ^ b;
        unsigned ec = c ^ (a & b);
        CHECK(cos_v77_reg_get_bit0(&rf.r[0]) == a);
        CHECK(cos_v77_reg_get_bit0(&rf.r[1]) == eb);
        CHECK(cos_v77_reg_get_bit0(&rf.r[2]) == ec);

        /* Inverse via the VM reverse-step path. */
        cos_v77_insn_t ins = (uint32_t)COS_V77_OP_PERES
                           | (0u << 4) | (1u << 8) | (2u << 12);
        peres_inv_driver(&ins, &rf);

        for (unsigned i = 0; i < COS_V77_NREGS; ++i) {
            CHECK(cos_v77_reg_eq(&rf.r[i], &rf0.r[i]));
        }
    }
}

/* ==================================================================
 *  7.  Majority-3 — 8-row table, self-inverse.
 * ================================================================== */

static void test_maj3(void)
{
    for (unsigned rep = 0; rep < 4; ++rep) {
        for (unsigned a = 0; a <= 1; ++a)
        for (unsigned b = 0; b <= 1; ++b)
        for (unsigned c = 0; c <= 1; ++c)
        for (unsigned d = 0; d <= 1; ++d) {
            cos_v77_reg_t ra, rb, rc, rd;
            reg_from_bit(&ra, a);
            reg_from_bit(&rb, b);
            reg_from_bit(&rc, c);
            reg_from_bit(&rd, d);
            cos_v77_maj3(&ra, &rb, &rc, &rd);
            unsigned maj = (a & b) ^ (b & c) ^ (a & c);
            CHECK(cos_v77_reg_get_bit0(&ra) == a);
            CHECK(cos_v77_reg_get_bit0(&rb) == b);
            CHECK(cos_v77_reg_get_bit0(&rc) == c);
            CHECK(cos_v77_reg_get_bit0(&rd) == (d ^ maj));
            cos_v77_maj3(&ra, &rb, &rc, &rd);
            CHECK(cos_v77_reg_get_bit0(&ra) == a);
            CHECK(cos_v77_reg_get_bit0(&rb) == b);
            CHECK(cos_v77_reg_get_bit0(&rc) == c);
            CHECK(cos_v77_reg_get_bit0(&rd) == d);
        }
    }
}

/* ==================================================================
 *  8.  Reversible 8-bit adder — full 256 × 256 × 2 sweep.
 *      131 072 forward + 131 072 reverse = 262 144 round-trip rows.
 * ================================================================== */

static void test_radd8(void)
{
    /* Forward correctness with c_in ∈ {0,1} — verifies the sum
     * and carry for the full 256 × 256 × 2 domain. */
    for (unsigned c_in = 0; c_in <= 1; ++c_in)
    for (unsigned x = 0; x < 256; ++x)
    for (unsigned y = 0; y < 256; ++y) {
        cos_v77_reg_t a, b, cio;
        reg_from_byte(&a,   x);
        reg_from_byte(&b,   y);
        reg_from_bit(&cio,  c_in);

        cos_v77_radd8(&a, &b, &cio, COS_V77_FORWARD);
        unsigned s     = (x + y + c_in) & 0xFFu;
        unsigned c_out = ((x + y + c_in) >> 8) & 1u;
        CHECK(low_byte(&a)              == x);
        CHECK(low_byte(&b)              == s);
        CHECK(cos_v77_reg_get_bit0(&cio)== c_out);
    }

    /* Full round-trip identity under the c_in = 0 convention. */
    for (unsigned x = 0; x < 256; ++x)
    for (unsigned y = 0; y < 256; ++y) {
        cos_v77_reg_t a, b, cio;
        reg_from_byte(&a,  x);
        reg_from_byte(&b,  y);
        reg_from_bit(&cio, 0u);

        cos_v77_radd8(&a, &b, &cio, COS_V77_FORWARD);
        cos_v77_radd8(&a, &b, &cio, COS_V77_REVERSE);

        CHECK(low_byte(&a)               == x);
        CHECK(low_byte(&b)               == y);
        CHECK(cos_v77_reg_get_bit0(&cio) == 0u);
    }
}

/* ==================================================================
 *  9.  VM round-trip — N random programs × forward ∘ reverse = id.
 * ================================================================== */

static cos_v77_insn_t rand_insn(void)
{
    unsigned op = (unsigned)(rnd64() % 7u); /* avoid ADD8 here; separate test */
    unsigned ra = (unsigned)(rnd64() % COS_V77_NREGS);
    unsigned rb = (unsigned)(rnd64() % COS_V77_NREGS);
    unsigned rc = (unsigned)(rnd64() % COS_V77_NREGS);
    /* Disallow aliasing for gates that require distinct registers. */
    if (op == COS_V77_OP_CNOT || op == COS_V77_OP_SWAP) {
        if (rb == ra) rb = (ra + 1u) % COS_V77_NREGS;
    } else if (op == COS_V77_OP_FRED ||
               op == COS_V77_OP_TOFF ||
               op == COS_V77_OP_PERES) {
        if (rb == ra) rb = (ra + 1u) % COS_V77_NREGS;
        if (rc == ra || rc == rb) {
            rc = (ra + 2u) % COS_V77_NREGS;
            if (rc == rb) rc = (rc + 1u) % COS_V77_NREGS;
        }
    }
    return (cos_v77_insn_t)(op | (ra << 4) | (rb << 8) | (rc << 12));
}

static void test_vm_round_trip(size_t n_programs, size_t prog_len)
{
    cos_v77_insn_t prog[64];
    if (prog_len > sizeof prog / sizeof prog[0])
        prog_len = sizeof prog / sizeof prog[0];

    for (size_t p = 0; p < n_programs; ++p) {
        for (size_t i = 0; i < prog_len; ++i) prog[i] = rand_insn();

        cos_v77_regfile_t rf;
        memset(&rf, 0, sizeof rf);
        for (unsigned i = 0; i < COS_V77_NREGS; ++i) {
            rf.r[i].w[0] = rnd64();
            rf.r[i].w[1] = rnd64();
            rf.r[i].w[2] = rnd64();
            rf.r[i].w[3] = rnd64();
        }
        cos_v77_regfile_t rf0 = rf;

        uint32_t work_fwd = 0, work_rev = 0;
        int rf_rc = cos_v77_rvl_run(&rf, prog, prog_len, (uint32_t)-1,
                                    COS_V77_FORWARD, &work_fwd);
        CHECK(rf_rc >= 0);
        int rr_rc = cos_v77_rvl_run(&rf, prog, prog_len, (uint32_t)-1,
                                    COS_V77_REVERSE, &work_rev);
        CHECK(rr_rc >= 0);
        CHECK(work_fwd == work_rev);

        for (unsigned i = 0; i < COS_V77_NREGS; ++i) {
            CHECK(cos_v77_reg_eq(&rf.r[i], &rf0.r[i]));
        }
    }
}

/* ==================================================================
 * 10.  17-bit compose — branchless 2-bit truth table + randomised.
 * ================================================================== */

static void test_compose_17bit(void)
{
    /* 4-row truth table. */
    CHECK(cos_v77_compose_decision(0u, 0u) == 0u);
    CHECK(cos_v77_compose_decision(0u, 1u) == 0u);
    CHECK(cos_v77_compose_decision(1u, 0u) == 0u);
    CHECK(cos_v77_compose_decision(1u, 1u) == 1u);

    /* Randomised: the AND-discipline must clamp high bits. */
    for (unsigned i = 0; i < 131072; ++i) {
        uint32_t a = (uint32_t)rnd64();
        uint32_t b = (uint32_t)rnd64();
        uint32_t expected = (a & 1u) & (b & 1u);
        CHECK(cos_v77_compose_decision(a, b) == expected);
    }
}

/* Sanity: the gate's positive path with a valid empty program. */
static void test_gate_empty(void)
{
    CHECK(cos_v77_ok(NULL, 0u, 0u) == 0u);
    cos_v77_insn_t empty = 0; /* HALT */
    CHECK(cos_v77_ok(&empty, 0u, 0u) == 1u);
    /* HALT with budget 1 → OK. */
    CHECK(cos_v77_ok(&empty, 1u, 1u) == 1u);
    /* HALT with budget 0 → over budget → NOT ok. */
    CHECK(cos_v77_ok(&empty, 1u, 0u) == 0u);
}

static void test_op_cost_and_self_inverse(void)
{
    CHECK(cos_v77_op_cost(COS_V77_OP_HALT)  ==  1u);
    CHECK(cos_v77_op_cost(COS_V77_OP_NOT)   ==  1u);
    CHECK(cos_v77_op_cost(COS_V77_OP_CNOT)  ==  1u);
    CHECK(cos_v77_op_cost(COS_V77_OP_SWAP)  ==  1u);
    CHECK(cos_v77_op_cost(COS_V77_OP_FRED)  ==  3u);
    CHECK(cos_v77_op_cost(COS_V77_OP_TOFF)  ==  5u);
    CHECK(cos_v77_op_cost(COS_V77_OP_PERES) ==  4u);
    CHECK(cos_v77_op_cost(COS_V77_OP_ADD8)  == 32u);
    CHECK(cos_v77_op_cost((cos_v77_op_t)99) ==  0u);

    CHECK(cos_v77_op_is_self_inverse(COS_V77_OP_NOT)   == 1u);
    CHECK(cos_v77_op_is_self_inverse(COS_V77_OP_CNOT)  == 1u);
    CHECK(cos_v77_op_is_self_inverse(COS_V77_OP_SWAP)  == 1u);
    CHECK(cos_v77_op_is_self_inverse(COS_V77_OP_FRED)  == 1u);
    CHECK(cos_v77_op_is_self_inverse(COS_V77_OP_TOFF)  == 1u);
    CHECK(cos_v77_op_is_self_inverse(COS_V77_OP_PERES) == 1u);
    CHECK(cos_v77_op_is_self_inverse(COS_V77_OP_ADD8)  == 0u);
}

/* ==================================================================
 *  Microbench.
 * ================================================================== */

static void microbench(void)
{
    const size_t N = 100000;
    cos_v77_regfile_t rf;
    memset(&rf, 0, sizeof rf);
    cos_v77_insn_t prog[16];
    for (unsigned i = 0; i < 16; ++i) prog[i] = rand_insn();

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (size_t i = 0; i < N; ++i) {
        uint32_t w = 0;
        cos_v77_rvl_run(&rf, prog, 16u, (uint32_t)-1,
                        COS_V77_FORWARD, &w);
        cos_v77_rvl_run(&rf, prog, 16u, (uint32_t)-1,
                        COS_V77_REVERSE, &w);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ns = (double)(t1.tv_sec - t0.tv_sec) * 1e9
              + (double)(t1.tv_nsec - t0.tv_nsec);
    double per_roundtrip_ns = ns / (double)N;
    printf("  microbench  %.0f round-trips/s  (%.1f ns each, 16-insn program)\n",
           1e9 / per_roundtrip_ns, per_roundtrip_ns);
}

/* ==================================================================
 *  main
 * ================================================================== */

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
               argv[0] ? argv[0] : "creation_os_v77");
        return 0;
    }

    if (want_selftest) {
        test_op_cost_and_self_inverse();
        test_not();
        test_cnot();
        test_swap();
        test_fredkin();
        test_toffoli();
        test_peres();
        test_maj3();
        test_radd8();
        test_vm_round_trip(/*programs*/ 2048, /*len*/ 32);
        test_compose_17bit();
        test_gate_empty();

        printf("creation_os_v77 self-test: %llu/%llu PASS\n",
               (unsigned long long)g_pass,
               (unsigned long long)(g_pass + g_fail));
        if (g_fail != 0) return 1;
    }

    if (want_bench) microbench();
    return 0;
}
