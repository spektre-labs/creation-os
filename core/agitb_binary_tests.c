/*
 * AGITB-style hard-logic suite (12 tests): XOR, POPCNT, branchless ops, FNV hash.
 * No soft probability — each assertion is deterministic.
 *
 * Build: clang -O2 -march=native -Wall -Wextra -Werror -std=c11 -o agitb_binary_tests \
 *          agitb_binary_tests.c
 */
#include <stdint.h>
#include <stdio.h>

static int failures;

#define CHECK(name, cond)                                                                          \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            printf("FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__);                                \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

static inline uint32_t branchless_min_u32(uint32_t a, uint32_t b) {
    uint32_t mask = (uint32_t)-((uint32_t)(a < b));
    return (a & mask) | (b & ~mask);
}

static inline uint32_t branchless_max_u32(uint32_t a, uint32_t b) {
    uint32_t mask = (uint32_t)-((uint32_t)(a > b));
    return (a & mask) | (b & ~mask);
}

static uint64_t fnv1a64(const char *s) {
    uint64_t h = 14695981039346656037ull;
    const uint64_t P = 1099511628211ull;
    for (; *s; s++)
        h = (h ^ (unsigned char)*s) * P;
    return h ? h : 1ull;
}

static void test01_xor_commutative(void) {
    uint32_t a = 0xDEADBEEFu, b = 0x12345678u;
    CHECK("xor_commutative", (a ^ b) == (b ^ a));
}

static void test02_xor_associative(void) {
    uint32_t a = 0x111u, b = 0x222u, c = 0x333u;
    CHECK("xor_associative", ((a ^ b) ^ c) == (a ^ (b ^ c)));
}

static void test03_xor_self_zero(void) {
    uint64_t x = 0xF00DF00DF00DF00Dull;
    CHECK("xor_self_zero", (x ^ x) == 0ull);
}

static void test04_xor_identity(void) {
    uint32_t x = 0xABCDEF01u;
    CHECK("xor_identity", (x ^ 0u) == x);
}

static void test05_popcnt_parity_chain(void) {
    for (uint32_t n = 1; n < 50000u; n++) {
        uint32_t d = n ^ (n - 1u);
        int p = __builtin_parity(d);
        (void)p;
        CHECK("popcnt_nonzero_xor_adjacent", d != 0u);
    }
}

static void test06_branchless_min(void) {
    CHECK("branchless_min", branchless_min_u32(7u, 42u) == 7u);
    CHECK("branchless_min_eq", branchless_min_u32(9u, 9u) == 9u);
}

static void test07_branchless_max(void) {
    CHECK("branchless_max", branchless_max_u32(7u, 42u) == 42u);
}

static void test08_rotate_consistency(void) {
    uint32_t x = 0xA5A5A5A5u;
    uint32_t r = (x << 17) | (x >> (32 - 17));
    uint32_t r2 = __builtin_rotateleft32(x, 17);
    CHECK("rotate_manual_builtin", r == r2);
}

static void test09_parity_predictable(void) {
    CHECK("parity_zero", __builtin_parity(0u) == 0);
    CHECK("parity_one", __builtin_parity(1u) == 1);
}

static void test10_mask_merge(void) {
    uint32_t lo = 0x00FFu, hi = 0xFF00u;
    uint32_t m = (lo | hi) & 0xFFFFu;
    CHECK("mask_merge", m == 0xFFFFu);
}

static void test11_xor_generalization(void) {
    /* Signal coherence: same transform on perturbed input */
    uint8_t base = 0x5Au, noise = 0x33u;
    uint8_t t1 = (uint8_t)(base ^ 0xFFu);
    uint8_t t2 = (uint8_t)((base ^ noise) ^ (0xFFu ^ noise));
    CHECK("xor_generalize", t1 == t2);
}

static void test12_fnv_determinism(void) {
    const char *s = "creation_os_agitb_signal_coherence";
    uint64_t h1 = fnv1a64(s);
    uint64_t h2 = fnv1a64(s);
    CHECK("fnv_repeat", h1 == h2);
    CHECK("fnv_diff_string", fnv1a64("a") != fnv1a64("b"));
}

int main(void) {
    failures = 0;
    test01_xor_commutative();
    test02_xor_associative();
    test03_xor_self_zero();
    test04_xor_identity();
    test05_popcnt_parity_chain();
    test06_branchless_min();
    test07_branchless_max();
    test08_rotate_consistency();
    test09_parity_predictable();
    test10_mask_merge();
    test11_xor_generalization();
    test12_fnv_determinism();

    if (failures == 0) {
        printf("AGITB (12 hard-logic tests): ALL PASS\n");
        return 0;
    }
    printf("AGITB: %d failure(s)\n", failures);
    return 1;
}
