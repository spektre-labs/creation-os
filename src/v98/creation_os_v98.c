/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v98 — σ-Topology self-test driver.
 */

#include "topology.h"

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

static void test_init(void)
{
    cos_v98_space_t a, b;
    cos_v98_init(&a);
    cos_v98_init(&b);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    CHECK(a.sentinel == COS_V98_SENTINEL);
    CHECK(cos_v98_ok(&a) == 1u);
}

/* Distance matrix: symmetric, diagonal zero, non-negative. */
static void test_distance_matrix_symmetric(void)
{
    cos_v98_space_t s;
    cos_v98_load_circle(&s, 0xCAFEF00Du);
    for (uint32_t i = 0u; i < COS_V98_N; ++i) {
        CHECK(s.d2[i][i] == 0);
        for (uint32_t j = i + 1u; j < COS_V98_N; ++j) {
            CHECK(s.d2[i][j] == s.d2[j][i]);
            CHECK(s.d2[i][j] >= 0);
        }
    }
}

/* ε = 0 → Betti-0 = N (no merges). */
static void test_epsilon_zero(void)
{
    cos_v98_space_t s;
    cos_v98_load_circle(&s, 0x42u);
    CHECK(cos_v98_betti0_at(&s, 0) == COS_V98_N);
    CHECK(cos_v98_edges_at(&s, 0) == 0u);
}

/* ε = ∞ → Betti-0 = 1 (fully merged). */
static void test_epsilon_infinity(void)
{
    cos_v98_space_t s;
    cos_v98_load_circle(&s, 0x42u);
    CHECK(cos_v98_betti0_at(&s, INT64_MAX) == 1u);
    CHECK(cos_v98_edges_at(&s, INT64_MAX) ==
          (COS_V98_N * (COS_V98_N - 1u)) / 2u);
}

/* Betti-0 is monotone non-increasing across a filtration. */
static void test_betti0_monotone(void)
{
    cos_v98_space_t s;
    cos_v98_load_circle(&s, 0xDEADBEEFu);

    int64_t schedule[64];
    for (uint32_t k = 0u; k < 64u; ++k) {
        /* sweep 0 … 8.0 in squared units */
        schedule[k] = ((int64_t)k * (int64_t)(8 * 65536LL) / 63LL) *
                      ((int64_t)k * (int64_t)(8 * 65536LL) / 63LL);
    }
    cos_v98_filtration_check(&s, schedule, 64u);
    CHECK(s.monotone_violations == 0u);
    CHECK(cos_v98_ok(&s) == 1u);
}

/* Circle data-set should briefly exhibit Betti-1 = 1 at a small range
 * of ε where the 1-skeleton forms a complete cycle but not yet a
 * filled-in disk. */
static void test_betti1_circle(void)
{
    cos_v98_space_t s;
    cos_v98_load_circle(&s, 0xDEADC0DEu);

    uint32_t saw_b1_positive = 0u;
    for (int64_t log_eps = 0; log_eps < 64; ++log_eps) {
        /* Q32.32 threshold increasing step-wise */
        int64_t eps_sq = (1LL << 30) + (log_eps * (1LL << 27));
        uint32_t b1 = cos_v98_betti1_at(&s, eps_sq);
        if (b1 >= 1u) { saw_b1_positive = 1u; break; }
    }
    CHECK(saw_b1_positive == 1u);
}

/* Identity: E − V + C = β₁ (Euler characteristic of the 1-skeleton). */
static void test_euler_identity(void)
{
    cos_v98_space_t s;
    cos_v98_load_circle(&s, 0xBEEFu);
    for (uint32_t k = 0u; k < 32u; ++k) {
        int64_t eps_sq = (int64_t)k * (int64_t)(1 << 25);
        uint32_t E = cos_v98_edges_at(&s, eps_sq);
        uint32_t C = cos_v98_betti0_at(&s, eps_sq);
        uint32_t B1 = cos_v98_betti1_at(&s, eps_sq);
        int64_t lhs = (int64_t)E - (int64_t)COS_V98_N + (int64_t)C;
        if (lhs < 0) lhs = 0;
        CHECK(B1 == (uint32_t)lhs);
    }
}

/* Determinism: identical seed → identical distances. */
static void test_determinism(void)
{
    cos_v98_space_t a, b;
    cos_v98_load_circle(&a, 0xFEEDFACEu);
    cos_v98_load_circle(&b, 0xFEEDFACEu);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
}

/* Large soak: over many seeds, sweep a fine filtration and check every
 * canonical TDA invariant at every ε. */
static void test_filtration_soak(void)
{
    for (uint32_t trial = 0u; trial < 64u; ++trial) {
        cos_v98_space_t s;
        cos_v98_load_circle(&s, 0xA5A5u + trial * 131u);

        uint32_t prev_b0 = COS_V98_N + 1u;
        for (uint32_t k = 0u; k <= 64u; ++k) {
            int64_t eps_sq = (int64_t)k * (int64_t)(1 << 26);
            uint32_t b0 = cos_v98_betti0_at(&s, eps_sq);
            uint32_t b1 = cos_v98_betti1_at(&s, eps_sq);
            uint32_t E  = cos_v98_edges_at(&s, eps_sq);
            CHECK(b0 <= prev_b0);                          /* monotone */
            CHECK(b0 >= 1u);
            CHECK(b0 <= COS_V98_N);
            CHECK(E  <= (COS_V98_N * (COS_V98_N - 1u)) / 2u);
            /* Euler identity. */
            int64_t rhs = (int64_t)E - (int64_t)COS_V98_N + (int64_t)b0;
            if (rhs < 0) rhs = 0;
            CHECK((int64_t)b1 == rhs);
            prev_b0 = b0;
        }
        CHECK(cos_v98_ok(&s) == 1u);
    }
}

/* Distance triangle inequality on the squared distances:
 *     √d2(i,k) ≤ √d2(i,j) + √d2(j,k)                                  *
 * We test the weaker (integer-friendly) consequence
 *     d2(i,k) ≤ 2·(d2(i,j) + d2(j,k))
 * which is implied by 2ab ≤ a² + b².                                  */
static void test_triangle(void)
{
    cos_v98_space_t s;
    cos_v98_load_circle(&s, 0xF00Du);
    for (uint32_t i = 0u; i < COS_V98_N; ++i) {
        for (uint32_t j = 0u; j < COS_V98_N; ++j) {
            if (i == j) continue;
            for (uint32_t k = 0u; k < COS_V98_N; ++k) {
                if (k == i || k == j) continue;
                CHECK(s.d2[i][k] <= 2LL * (s.d2[i][j] + s.d2[j][k]));
            }
        }
    }
}

static void test_compose(void)
{
    CHECK(cos_v98_compose_decision(0u, 0u) == 0u);
    CHECK(cos_v98_compose_decision(0u, 1u) == 0u);
    CHECK(cos_v98_compose_decision(1u, 0u) == 0u);
    CHECK(cos_v98_compose_decision(1u, 1u) == 1u);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v98 σ-Topology — Vietoris–Rips persistent homology "
               "with Betti-0 / Betti-1 filtration (38-bit composed decision)\n");
        return 0;
    }

    test_init();
    test_distance_matrix_symmetric();
    test_epsilon_zero();
    test_epsilon_infinity();
    test_betti0_monotone();
    test_betti1_circle();
    test_euler_identity();
    test_determinism();
    test_filtration_soak();
    test_triangle();
    test_compose();

    printf("v98 σ-Topology: %u PASS / %u FAIL\n", pass_count, fail_count);
    return (fail_count == 0) ? 0 : 1;
}
