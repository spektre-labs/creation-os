/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v91 — σ-Quantum self-test driver.
 *
 * Covers: init determinism, Pauli-X/Z/H/CNOT correctness, Hadamard
 * self-inverse, uniform superposition after full Hadamard layer,
 * Grover amplification toward marked state for all 16 markers,
 * probability conservation under the full gate set, and the 31-bit
 * compose truth table.
 */

#include "quantum.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t pass_count = 0;
static uint32_t fail_count = 0;

#define CHECK(cond) do {                                                 \
    if (cond) { ++pass_count; }                                          \
    else {                                                               \
        ++fail_count;                                                    \
        if (fail_count <= 10u)                                           \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
    }                                                                    \
} while (0)

static uint64_t xs(uint64_t *s)
{
    uint64_t x = *s; x ^= x << 13; x ^= x >> 7; x ^= x << 17; *s = x;
    return x;
}

/* ------------------------------------------------------------------ */
/*  1. init: |0000>                                                    */
/* ------------------------------------------------------------------ */
static void test_init(void)
{
    cos_v91_reg_t r;
    cos_v91_reg_init(&r);
    CHECK(r.sentinel == COS_V91_SENTINEL);
    CHECK(r.a[0] == COS_V91_Q1);
    for (uint32_t x = 1; x < COS_V91_DIM; ++x) CHECK(r.a[x] == 0);
    CHECK(r.ops == 0u);
}

/* ------------------------------------------------------------------ */
/*  2. Pauli-X: X|0> = |1> on each qubit                              */
/* ------------------------------------------------------------------ */
static void test_pauli_x(void)
{
    for (uint32_t q = 0; q < COS_V91_NQUBITS; ++q) {
        cos_v91_reg_t r;
        cos_v91_reg_init(&r);
        cos_v91_x(&r, q);
        uint32_t expect = 1u << q;
        CHECK(r.a[expect] == COS_V91_Q1);
        CHECK(r.a[0] == 0);
        /* X^2 = I */
        cos_v91_x(&r, q);
        CHECK(r.a[0] == COS_V91_Q1);
        CHECK(r.a[expect] == 0);
    }
}

/* ------------------------------------------------------------------ */
/*  3. Pauli-Z: Z|0> = |0>, Z|1> = -|1>                               */
/* ------------------------------------------------------------------ */
static void test_pauli_z(void)
{
    for (uint32_t q = 0; q < COS_V91_NQUBITS; ++q) {
        cos_v91_reg_t r;
        cos_v91_reg_init(&r);
        cos_v91_z(&r, q);
        CHECK(r.a[0] == COS_V91_Q1);          /* bit q is 0 */

        cos_v91_reg_init(&r);
        cos_v91_x(&r, q);
        cos_v91_z(&r, q);
        uint32_t expect = 1u << q;
        CHECK(r.a[expect] == -(int32_t)COS_V91_Q1);
    }
}

/* ------------------------------------------------------------------ */
/*  4. Hadamard: H|0> = (|0>+|1>)/√2                                   */
/* ------------------------------------------------------------------ */
static void test_hadamard(void)
{
    for (uint32_t q = 0; q < COS_V91_NQUBITS; ++q) {
        cos_v91_reg_t r;
        cos_v91_reg_init(&r);
        cos_v91_h(&r, q);
        uint32_t one = 1u << q;
        CHECK(r.a[0]   == COS_V91_INV_SQRT2);
        CHECK(r.a[one] == COS_V91_INV_SQRT2);

        /* H^2 ≈ I (up to Q16.16 rounding). */
        cos_v91_h(&r, q);
        CHECK((r.a[0] >= COS_V91_Q1 - 4) && (r.a[0] <= COS_V91_Q1 + 4));
        CHECK((r.a[one] >= -4) && (r.a[one] <= 4));
    }
}

/* ------------------------------------------------------------------ */
/*  5. Full Hadamard layer → uniform superposition                     */
/* ------------------------------------------------------------------ */
static void test_uniform_super(void)
{
    cos_v91_reg_t r;
    cos_v91_reg_init(&r);
    for (uint32_t q = 0; q < COS_V91_NQUBITS; ++q) cos_v91_h(&r, q);
    /* Each amplitude should be 1/√16 = 1/4 = 16384 in Q16.16. */
    for (uint32_t x = 0; x < COS_V91_DIM; ++x) {
        int32_t e = 16384;
        int32_t d = r.a[x] - e;
        if (d < 0) d = -d;
        CHECK(d <= 8);                  /* quantization tolerance */
    }
}

/* ------------------------------------------------------------------ */
/*  6. CNOT: CNOT_{0,1} |10> = |11>                                   */
/* ------------------------------------------------------------------ */
static void test_cnot(void)
{
    cos_v91_reg_t r;
    cos_v91_reg_init(&r);
    cos_v91_x(&r, 0);             /* |0001> */
    cos_v91_cnot(&r, 0, 1);       /* |0011> */
    CHECK(r.a[0b0011] == COS_V91_Q1);
    CHECK(r.a[0b0001] == 0);

    /* CNOT when control=0 is identity. */
    cos_v91_reg_init(&r);
    cos_v91_cnot(&r, 0, 1);
    CHECK(r.a[0] == COS_V91_Q1);
    for (uint32_t x = 1; x < COS_V91_DIM; ++x) CHECK(r.a[x] == 0);
}

/* ------------------------------------------------------------------ */
/*  7. Grover amplification for every possible marked index           */
/* ------------------------------------------------------------------ */
static void test_grover_all(void)
{
    for (uint32_t m = 0; m < COS_V91_DIM; ++m) {
        cos_v91_reg_t r;
        cos_v91_reg_init(&r);
        cos_v91_grover(&r, m);
        uint32_t am = cos_v91_argmax(&r);
        CHECK(am == m);
        /* Marked amplitude squared should exceed 0.9 (Q16.16 = 58982). */
        int32_t v = r.a[m];
        int32_t sq = (int32_t)(((int64_t)v * (int64_t)v) >> 16);
        CHECK(sq > 58982);                 /* > 0.9 */
        CHECK(cos_v91_ok(&r, m) == 1u);
    }
}

/* ------------------------------------------------------------------ */
/*  8. Probability conservation under random Clifford chain           */
/* ------------------------------------------------------------------ */
static void test_prob_conservation(uint64_t seed)
{
    uint64_t s = seed;
    for (uint32_t trial = 0; trial < 100u; ++trial) {
        cos_v91_reg_t r;
        cos_v91_reg_init(&r);
        for (uint32_t q = 0; q < COS_V91_NQUBITS; ++q) cos_v91_h(&r, q);
        for (uint32_t step = 0; step < 20u; ++step) {
            uint32_t op = (uint32_t)(xs(&s) & 3u);
            uint32_t q  = (uint32_t)(xs(&s) & 3u);
            uint32_t t  = (uint32_t)(xs(&s) & 3u);
            if (t == q) t = (t + 1u) & 3u;
            switch (op) {
                case 0: cos_v91_x(&r, q); break;
                case 1: cos_v91_z(&r, q); break;
                case 2: cos_v91_cnot(&r, q, t); break;
                default: /* skip H to avoid accumulating rounding */ break;
            }
        }
        int64_t pm = cos_v91_prob_mass_q32(&r);
        int64_t one = (int64_t)1 << 32;
        int64_t d = pm - one;
        if (d < 0) d = -d;
        /* Clifford gates (X, Z, CNOT) preserve the amplitude vector
         * exactly; Hadamard at the start adds O(DIM) quantization
         * error bounded well below 1/256. */
        CHECK(d < (one >> 8));
    }
}

/* ------------------------------------------------------------------ */
/*  9. Determinism                                                     */
/* ------------------------------------------------------------------ */
static void test_determinism(uint64_t seed)
{
    uint64_t s = seed;
    for (uint32_t trial = 0; trial < 50u; ++trial) {
        cos_v91_reg_t a, b;
        cos_v91_reg_init(&a);
        cos_v91_reg_init(&b);
        uint32_t marked = (uint32_t)(xs(&s) & (COS_V91_DIM - 1u));
        cos_v91_grover(&a, marked);
        cos_v91_grover(&b, marked);
        CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    }
}

/* ------------------------------------------------------------------ */
/*  10. compose truth table                                            */
/* ------------------------------------------------------------------ */
static void test_compose(void)
{
    CHECK(cos_v91_compose_decision(0u, 0u) == 0u);
    CHECK(cos_v91_compose_decision(0u, 1u) == 0u);
    CHECK(cos_v91_compose_decision(1u, 0u) == 0u);
    CHECK(cos_v91_compose_decision(1u, 1u) == 1u);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v91 σ-Quantum — 4-qubit integer quantum "
               "simulator with Grover amplitude amplification "
               "(31-bit composed decision)\n");
        return 0;
    }

    const uint64_t seed = 0x91A11011C0DEBEEFULL;

    test_init();
    test_pauli_x();
    test_pauli_z();
    test_hadamard();
    test_uniform_super();
    test_cnot();
    test_grover_all();
    test_prob_conservation(seed);
    test_determinism(seed);
    test_compose();

    printf("v91 σ-Quantum: %u PASS / %u FAIL\n", pass_count, fail_count);
    return (fail_count == 0) ? 0 : 1;
}
