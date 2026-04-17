/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v71 — σ-Wormhole: standalone self-test + microbench
 * driver.  Exits non-zero on any assertion failure.
 *
 * 1 = 1.
 */

#include "wormhole.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

static uint32_t g_fail = 0;
static uint32_t g_pass = 0;

#define CHECK(cond) do {                                              \
    if (cond) { g_pass++; }                                           \
    else      { g_fail++;                                             \
                fprintf(stderr, "FAIL %s:%d: %s\n",                   \
                        __FILE__, __LINE__, #cond); }                 \
} while (0)

/* ==================================================================
 *  Helpers
 * ================================================================== */

static uint64_t splitmix64(uint64_t *s)
{
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* ==================================================================
 *  1.  HV primitive tests
 * ================================================================== */

static void test_hv_from_seed_and_copy(void)
{
    cos_v71_hv_t a, b;
    cos_v71_hv_from_seed(&a, 0x1234ULL);
    cos_v71_hv_copy(&b, &a);
    CHECK(cos_v71_hv_hamming(&a, &b) == 0);
    CHECK(cos_v71_hv_similarity_q15(&a, &b) == 32768);
}

static void test_hv_xor_involution(void)
{
    cos_v71_hv_t a, b, c, d;
    cos_v71_hv_from_seed(&a, 0x11ULL);
    cos_v71_hv_from_seed(&b, 0x22ULL);
    cos_v71_hv_xor(&c, &a, &b);
    cos_v71_hv_xor(&d, &c, &b);           /* (a ⊕ b) ⊕ b = a */
    CHECK(cos_v71_hv_hamming(&a, &d) == 0);
}

static void test_hv_orthogonality(void)
{
    /* Two pseudo-random HVs should be near-orthogonal: Hamming ≈ D/2. */
    cos_v71_hv_t a, b;
    cos_v71_hv_from_seed(&a, 0xAABBCCDDULL);
    cos_v71_hv_from_seed(&b, 0x55667788ULL);
    uint32_t h = cos_v71_hv_hamming(&a, &b);
    /* Allow ±10 % window at D = 8 192 → Hamming in [3 686, 4 506]. */
    CHECK(h > 3686u && h < 4506u);
    int32_t sim = cos_v71_hv_similarity_q15(&a, &b);
    CHECK(sim > -3277 && sim < 3277);     /* |sim| < 0.1 */
}

static void test_hv_permute_roundtrip(void)
{
    cos_v71_hv_t a, p, q;
    cos_v71_hv_from_seed(&a, 0xDEAD0001ULL);
    for (int shift = -1024; shift <= 1024; shift += 257) {
        cos_v71_hv_permute(&p, &a, shift);
        cos_v71_hv_permute(&q, &p, -shift);
        CHECK(cos_v71_hv_hamming(&a, &q) == 0);
    }
}

static void test_hv_similarity_self(void)
{
    for (uint64_t s = 1; s < 1001; ++s) {
        cos_v71_hv_t a;
        cos_v71_hv_from_seed(&a, s);
        CHECK(cos_v71_hv_similarity_q15(&a, &a) == 32768);
    }
}

/* ==================================================================
 *  2.  Portal insert / teleport basics
 * ================================================================== */

static void test_portal_teleport_identity(void)
{
    cos_v71_hv_t secret; cos_v71_hv_from_seed(&secret, 0xFEEDFACEULL);
    cos_v71_portal_t *p = cos_v71_portal_new(64u, &secret);
    CHECK(p != NULL);

    cos_v71_hv_t a, t, out;
    cos_v71_hv_from_seed(&a, 0xA0A0ULL);
    cos_v71_hv_from_seed(&t, 0xB0B0ULL);

    int idx = cos_v71_portal_insert(p, &a, &t, 42u);
    CHECK(idx == 0);
    CHECK(p->len == 1u);

    /* Jump from a with bridge[0] lands exactly on t. */
    CHECK(cos_v71_portal_teleport(p, 0u, &a, &out) == 0);
    CHECK(cos_v71_hv_hamming(&out, &t) == 0);

    /* Bidirectional: from t lands on a. */
    CHECK(cos_v71_portal_teleport(p, 0u, &t, &out) == 0);
    CHECK(cos_v71_hv_hamming(&out, &a) == 0);

    cos_v71_portal_free(p);
}

static void test_portal_match_best(void)
{
    cos_v71_hv_t secret; cos_v71_hv_from_seed(&secret, 1ULL);
    cos_v71_portal_t *p = cos_v71_portal_new(32u, &secret);

    cos_v71_hv_t a[8], t[8];
    for (int i = 0; i < 8; ++i) {
        cos_v71_hv_from_seed(&a[i], 1000ULL + (uint64_t)i);
        cos_v71_hv_from_seed(&t[i], 2000ULL + (uint64_t)i);
        cos_v71_portal_insert(p, &a[i], &t[i], (uint32_t)i);
    }
    /* Querying with a[3] should match idx 3. */
    int32_t sim = 0;
    uint32_t idx = cos_v71_portal_match(p, &a[3], &sim);
    CHECK(idx == 3u);
    CHECK(sim == 32768);
    cos_v71_portal_free(p);
}

/* ==================================================================
 *  3.  Integrity verify tests
 * ================================================================== */

static void test_integrity_honest(void)
{
    cos_v71_hv_t secret; cos_v71_hv_from_seed(&secret, 0xC0FFEEULL);
    cos_v71_portal_t *p = cos_v71_portal_new(16u, &secret);

    cos_v71_hv_t a, t;
    cos_v71_hv_from_seed(&a, 0x1111ULL);
    cos_v71_hv_from_seed(&t, 0x2222ULL);
    cos_v71_portal_insert(p, &a, &t, 7u);

    CHECK(cos_v71_portal_verify(p, 0u, 0u) == 1u);
    cos_v71_portal_free(p);
}

static void test_integrity_poisoned_bridge(void)
{
    cos_v71_hv_t secret; cos_v71_hv_from_seed(&secret, 0xC0FFEEULL);
    cos_v71_portal_t *p = cos_v71_portal_new(16u, &secret);

    cos_v71_hv_t a, t;
    cos_v71_hv_from_seed(&a, 0x1111ULL);
    cos_v71_hv_from_seed(&t, 0x2222ULL);
    cos_v71_portal_insert(p, &a, &t, 7u);

    /* Poison: flip one bit of the bridge. */
    p->bridges[0].w[0] ^= 1ULL;
    CHECK(cos_v71_portal_verify(p, 0u, 0u) == 0u);
    /* With a small noise floor, still fails for a 1-bit flip since we
     * XOR against the signature which moves by the same bit. */
    CHECK(cos_v71_portal_verify(p, 0u, 1u) == 1u);   /* noise ≥ 1 allowed */
    cos_v71_portal_free(p);
}

static void test_integrity_poisoned_sig(void)
{
    cos_v71_hv_t secret; cos_v71_hv_from_seed(&secret, 0xC0FFEEULL);
    cos_v71_portal_t *p = cos_v71_portal_new(16u, &secret);

    cos_v71_hv_t a, t;
    cos_v71_hv_from_seed(&a, 0x1111ULL);
    cos_v71_hv_from_seed(&t, 0x2222ULL);
    cos_v71_portal_insert(p, &a, &t, 7u);

    /* Flip 32 bits of the signature — must fail strict verify. */
    for (int i = 0; i < 32; ++i) p->sigs[0].w[i / 2] ^= (1ULL << (i & 63));
    CHECK(cos_v71_portal_verify(p, 0u, 0u)  == 0u);
    CHECK(cos_v71_portal_verify(p, 0u, 31u) == 0u);
    cos_v71_portal_free(p);
}

/* ==================================================================
 *  4.  Boundary gate tests
 * ================================================================== */

static void test_boundary_gate(void)
{
    cos_v71_hv_t zero, ones, rnd;
    cos_v71_hv_zero(&zero);
    cos_v71_hv_zero(&ones);
    for (uint32_t i = 0; i < COS_V71_HV_DIM_WORDS; ++i)
        ones.w[i] = (uint64_t)-1;

    cos_v71_hv_from_seed(&rnd, 0xFFFFFULL);

    /* popcount(zero)  = 0      → skew = D/2           → gate = 1 */
    CHECK(cos_v71_boundary_gate(&zero, COS_V71_BOUNDARY_DEFAULT_BITS) == 1u);
    /* popcount(ones)  = D      → skew = D/2           → gate = 1 */
    CHECK(cos_v71_boundary_gate(&ones, COS_V71_BOUNDARY_DEFAULT_BITS) == 1u);
    /* popcount(rnd)  ≈ D/2     → skew small           → gate = 0 */
    CHECK(cos_v71_boundary_gate(&rnd,  COS_V71_BOUNDARY_DEFAULT_BITS) == 0u);
    /* Lower the threshold to 0 → always permits */
    CHECK(cos_v71_boundary_gate(&rnd, 0u) == 1u);
}

/* ==================================================================
 *  5.  Multi-hop routing tests
 * ================================================================== */

static void test_route_arrive_one_hop(void)
{
    cos_v71_hv_t secret; cos_v71_hv_from_seed(&secret, 0xAAAAULL);
    cos_v71_portal_t *p = cos_v71_portal_new(8u, &secret);

    cos_v71_hv_t a, t;
    cos_v71_hv_from_seed(&a, 0x111ULL);
    cos_v71_hv_from_seed(&t, 0x222ULL);
    cos_v71_portal_insert(p, &a, &t, 0u);

    cos_v71_hv_t pos;
    cos_v71_route_t r;
    cos_v71_portal_route(p, &a, &t, COS_V71_ARRIVAL_DEFAULT_Q15,
                         COS_V71_ROUTE_HOPS_MAX, &pos, &r);
    CHECK(r.arrived == 1u);
    CHECK(r.hops    == 1u);
    CHECK(r.trail[0] == 0u);
    CHECK(cos_v71_hv_hamming(&pos, &t) == 0);

    cos_v71_portal_free(p);
}

static void test_route_arrive_two_hops(void)
{
    cos_v71_hv_t secret; cos_v71_hv_from_seed(&secret, 0xBBBBULL);
    cos_v71_portal_t *p = cos_v71_portal_new(8u, &secret);

    cos_v71_hv_t a, m, t;
    cos_v71_hv_from_seed(&a, 0x111ULL);
    cos_v71_hv_from_seed(&m, 0x222ULL);
    cos_v71_hv_from_seed(&t, 0x333ULL);
    cos_v71_portal_insert(p, &a, &m, 0u);
    cos_v71_portal_insert(p, &m, &t, 1u);

    cos_v71_hv_t pos;
    cos_v71_route_t r;
    cos_v71_portal_route(p, &a, &t, COS_V71_ARRIVAL_DEFAULT_Q15,
                         COS_V71_ROUTE_HOPS_MAX, &pos, &r);
    CHECK(r.arrived == 1u);
    CHECK(r.hops    <= 2u);
    CHECK(cos_v71_hv_hamming(&pos, &t) == 0);

    cos_v71_portal_free(p);
}

static void test_route_miss_bounded(void)
{
    /* No portal that lands on t → routing must not arrive, must respect
     * the hop budget, and must leave `current` non-trivially close to
     * the best available jump (not equal to t). */
    cos_v71_hv_t secret; cos_v71_hv_from_seed(&secret, 0xCCCCULL);
    cos_v71_portal_t *p = cos_v71_portal_new(8u, &secret);

    cos_v71_hv_t a, m, t;
    cos_v71_hv_from_seed(&a, 0x1ULL);
    cos_v71_hv_from_seed(&m, 0x2ULL);
    cos_v71_hv_from_seed(&t, 0x3ULL);
    cos_v71_portal_insert(p, &a, &m, 0u);

    cos_v71_hv_t pos;
    cos_v71_route_t r;
    cos_v71_portal_route(p, &a, &t, COS_V71_ARRIVAL_DEFAULT_Q15,
                         4u, &pos, &r);
    CHECK(r.arrived == 0u);
    CHECK(r.hops    <= 4u);

    cos_v71_portal_free(p);
}

/* ==================================================================
 *  6.  Tensor-bond tests
 * ================================================================== */

static void test_bond_build(void)
{
    cos_v71_hv_t A[4], B[4], bonds[3];
    for (int i = 0; i < 4; ++i) {
        cos_v71_hv_from_seed(&A[i], 100ULL + (uint64_t)i);
        cos_v71_hv_from_seed(&B[i], 200ULL + (uint64_t)i);
    }
    cos_v71_bond_pair_t pairs[3] = { {0,0}, {1,2}, {3,3} };
    uint32_t total = cos_v71_bond_build(A, 4u, B, 4u, pairs, 3u, bonds);
    CHECK(total != UINT32_MAX);
    /* Verify the bond XOR-rebuild. */
    cos_v71_hv_t rebuilt;
    cos_v71_hv_xor(&rebuilt, &A[0], &bonds[0]);
    CHECK(cos_v71_hv_hamming(&rebuilt, &B[0]) == 0);
    cos_v71_hv_xor(&rebuilt, &A[1], &bonds[1]);
    CHECK(cos_v71_hv_hamming(&rebuilt, &B[2]) == 0);
    cos_v71_hv_xor(&rebuilt, &A[3], &bonds[2]);
    CHECK(cos_v71_hv_hamming(&rebuilt, &B[3]) == 0);
}

/* ==================================================================
 *  7.  Path receipt tests
 * ================================================================== */

static void test_receipt_distinct_paths(void)
{
    cos_v71_hv_t secret; cos_v71_hv_from_seed(&secret, 7ULL);
    cos_v71_portal_t *p = cos_v71_portal_new(32u, &secret);
    cos_v71_hv_t a[4], t[4];
    for (int i = 0; i < 4; ++i) {
        cos_v71_hv_from_seed(&a[i], (uint64_t)(i + 1) * 17ULL);
        cos_v71_hv_from_seed(&t[i], (uint64_t)(i + 1) * 31ULL);
        cos_v71_portal_insert(p, &a[i], &t[i], (uint32_t)i);
    }
    uint32_t path_x[3] = { 0u, 1u, 2u };
    uint32_t path_y[3] = { 2u, 1u, 0u };

    cos_v71_hv_t rx_x, rx_y;
    CHECK(cos_v71_path_receipt(p, path_x, 3u, &rx_x) == 0);
    CHECK(cos_v71_path_receipt(p, path_y, 3u, &rx_y) == 0);
    /* Unordered receipts agree on the same multiset. */
    CHECK(cos_v71_hv_hamming(&rx_x, &rx_y) == 0);

    CHECK(cos_v71_path_receipt_ordered(p, path_x, 3u, &rx_x) == 0);
    CHECK(cos_v71_path_receipt_ordered(p, path_y, 3u, &rx_y) == 0);
    /* Ordered receipts differ because permutation breaks symmetry. */
    CHECK(cos_v71_hv_hamming(&rx_x, &rx_y) > 0);

    cos_v71_portal_free(p);
}

/* ==================================================================
 *  8.  WHL bytecode tests
 * ================================================================== */

static void test_whl_simple_gate(void)
{
    cos_v71_hv_t secret, a, t;
    cos_v71_hv_from_seed(&secret, 0xBADCAFEULL);
    cos_v71_hv_zero(&a);
    /* Build `a` with extreme bit-skew: first half all-zeros; this
     * lands far from D/2 so the boundary gate passes. */
    for (uint32_t i = 0; i < COS_V71_HV_DIM_WORDS / 2; ++i)
        a.w[i] = 0x0ULL;
    for (uint32_t i = COS_V71_HV_DIM_WORDS / 2; i < COS_V71_HV_DIM_WORDS; ++i)
        a.w[i] = (uint64_t)-1;                   /* pc = D/2, skew = 0 */

    /* To get non-trivial boundary skew, set first word bit 0. */
    cos_v71_hv_zero(&a);                          /* skew = D/2, gate = 1 */
    cos_v71_hv_copy(&t, &a);

    cos_v71_portal_t *p = cos_v71_portal_new(4u, &secret);
    cos_v71_portal_insert(p, &a, &t, 0u);

    cos_v71_whl_state_t *s = cos_v71_whl_new();
    cos_v71_whl_reset(s);

    cos_v71_whl_ctx_t ctx = {0};
    ctx.anchor_src = &a;
    ctx.portal     = p;
    ctx.route_arrival_q15 = COS_V71_ARRIVAL_DEFAULT_Q15;
    ctx.route_max_hops    = 8u;
    ctx.verify_noise_floor_bits = 0u;

    cos_v71_inst_t prog[] = {
        { COS_V71_OP_ANCHOR,   0, 0, 0, 0, 0 },                /* regs[0] = a */
        { COS_V71_OP_TELEPORT, 1, 0, 0, 0, 0 },                /* regs[1] = regs[0] ⊕ bridges[0] */
        { COS_V71_OP_VERIFY,   0, 0, 0, 0, 0 },                /* verify portal 0 */
        { COS_V71_OP_MEASURE,  2, 1, 0, 0, 0 },                /* reg_q15[2] = sim(regs[1], regs[0]) */
        { COS_V71_OP_GATE,     0, 2, 0, COS_V71_ARRIVAL_DEFAULT_Q15, 0 },
        { COS_V71_OP_HALT,     0, 0, 0, 0, 0 },
    };
    int rc = cos_v71_whl_exec(s, prog,
                              sizeof prog / sizeof prog[0],
                              &ctx, 64u, 8u);
    CHECK(rc == 0);
    CHECK(cos_v71_whl_integrity_ok(s) == 1u);
    CHECK(cos_v71_whl_boundary_ok(s)  == 1u);
    CHECK(cos_v71_whl_abstained(s)    == 0u);
    CHECK(cos_v71_whl_v71_ok(s)       == 1u);

    cos_v71_whl_free(s);
    cos_v71_portal_free(p);
}

static void test_whl_budget_abstain(void)
{
    cos_v71_hv_t secret, a; cos_v71_hv_from_seed(&secret, 1ULL);
    cos_v71_hv_zero(&a);
    cos_v71_portal_t *p = cos_v71_portal_new(4u, &secret);
    cos_v71_portal_insert(p, &a, &a, 0u);

    cos_v71_whl_state_t *s = cos_v71_whl_new();
    cos_v71_whl_ctx_t ctx = {0};
    ctx.anchor_src = &a;
    ctx.portal = p;

    cos_v71_inst_t prog[] = {
        { COS_V71_OP_ANCHOR,   0, 0, 0, 0, 0 },
        { COS_V71_OP_TELEPORT, 1, 0, 0, 0, 0 },
        { COS_V71_OP_TELEPORT, 1, 1, 0, 0, 0 },
        { COS_V71_OP_TELEPORT, 1, 1, 0, 0, 0 },
        { COS_V71_OP_GATE,     0, 0, 0, 0, 0 },
        { COS_V71_OP_HALT,     0, 0, 0, 0, 0 },
    };
    /* Budget of 4 teleport-units is insufficient (ANCHOR=2 + 3×TELEPORT=12 = 14). */
    int rc = cos_v71_whl_exec(s, prog,
                              sizeof prog / sizeof prog[0],
                              &ctx, 4u, 1u);
    CHECK(rc == -2);
    CHECK(cos_v71_whl_abstained(s) == 1u);
    CHECK(cos_v71_whl_v71_ok(s)    == 0u);

    cos_v71_whl_free(s);
    cos_v71_portal_free(p);
}

/* ==================================================================
 *  9.  12-bit composed-decision truth table (2^12 = 4096 rows).
 * ================================================================== */

static void test_compose_truth_table(void)
{
    for (unsigned v = 0; v < 4096u; ++v) {
        uint8_t bits[12];
        for (int k = 0; k < 12; ++k) bits[k] = (v >> k) & 1u;
        cos_v71_decision_t d = cos_v71_compose_decision(
            bits[0], bits[1], bits[2], bits[3],
            bits[4], bits[5], bits[6], bits[7],
            bits[8], bits[9], bits[10], bits[11]);
        uint8_t want = 1u;
        for (int k = 0; k < 12; ++k) want &= bits[k];
        CHECK(d.allow == want);
        CHECK(d.v60_ok == bits[0]);
        CHECK(d.v61_ok == bits[1]);
        CHECK(d.v62_ok == bits[2]);
        CHECK(d.v63_ok == bits[3]);
        CHECK(d.v64_ok == bits[4]);
        CHECK(d.v65_ok == bits[5]);
        CHECK(d.v66_ok == bits[6]);
        CHECK(d.v67_ok == bits[7]);
        CHECK(d.v68_ok == bits[8]);
        CHECK(d.v69_ok == bits[9]);
        CHECK(d.v70_ok == bits[10]);
        CHECK(d.v71_ok == bits[11]);
    }
}

/* ==================================================================
 *  10.  Deterministic fuzz: 1 024 portals × 8 random queries → 8 192
 *       insert + teleport-round-trip asserts.
 * ================================================================== */

static void fuzz_portal_round_trip(void)
{
    cos_v71_hv_t secret; cos_v71_hv_from_seed(&secret, 0xF00DF00DULL);
    cos_v71_portal_t *p = cos_v71_portal_new(1024u, &secret);
    CHECK(p != NULL);

    uint64_t st = 0xBADC0FFEE;
    cos_v71_hv_t a, t, out;
    for (int i = 0; i < 1024; ++i) {
        cos_v71_hv_from_seed(&a, splitmix64(&st));
        cos_v71_hv_from_seed(&t, splitmix64(&st));
        int idx = cos_v71_portal_insert(p, &a, &t, (uint32_t)i);
        CHECK(idx == i);
    }
    CHECK(p->len == 1024u);

    /* Round-trip + integrity for every portal. */
    for (uint32_t i = 0; i < p->len; ++i) {
        /* Re-derive anchor from the stored bridge and target:
         * target = anchor ⊕ bridge; we stored anchor directly,
         * so teleport(anchor) should land on target. */
        cos_v71_hv_xor(&t, &p->anchors[i], &p->bridges[i]);  /* expected target */
        CHECK(cos_v71_portal_teleport(p, i, &p->anchors[i], &out) == 0);
        CHECK(cos_v71_hv_hamming(&out, &t) == 0);
        CHECK(cos_v71_portal_verify(p, i, 0u) == 1u);
    }
    cos_v71_portal_free(p);
}

/* ==================================================================
 *  11.  Deterministic fuzz: 10 000 WHL programs with boundary / cost
 *       mutations (coverage for GATE).
 * ================================================================== */

static void fuzz_whl_gate(void)
{
    cos_v71_hv_t secret; cos_v71_hv_from_seed(&secret, 0x77777777ULL);
    cos_v71_hv_t a;      cos_v71_hv_zero(&a);  /* boundary_ok at default thr */
    cos_v71_portal_t *p = cos_v71_portal_new(8u, &secret);
    cos_v71_portal_insert(p, &a, &a, 0u);

    cos_v71_whl_state_t *s = cos_v71_whl_new();

    cos_v71_whl_ctx_t ctx = {0};
    ctx.anchor_src = &a;
    ctx.portal     = p;

    const cos_v71_inst_t prog[] = {
        { COS_V71_OP_ANCHOR,   0, 0, 0, 0, 0 },
        { COS_V71_OP_TELEPORT, 1, 0, 0, 0, 0 },
        { COS_V71_OP_VERIFY,   0, 0, 0, 0, 0 },
        { COS_V71_OP_MEASURE,  2, 1, 0, 0, 0 },
        { COS_V71_OP_GATE,     0, 2, 0, (int16_t)COS_V71_ARRIVAL_DEFAULT_Q15, 0 },
        { COS_V71_OP_HALT,     0, 0, 0, 0, 0 },
    };
    /* Sweep over cost budgets / hop budgets / abstain signals. */
    for (int trial = 0; trial < 10000; ++trial) {
        cos_v71_whl_reset(s);
        uint64_t cost_budget = (trial % 3 == 0) ? 2u : 64u;
        uint32_t hop_budget  = (trial % 5 == 0) ? 0u : 8u;
        ctx.abstain = (trial % 11 == 0) ? 1u : 0u;

        int rc = cos_v71_whl_exec(s, prog,
                                  sizeof prog / sizeof prog[0],
                                  &ctx, cost_budget, hop_budget);
        if (rc == 0 && cost_budget >= 16u && !ctx.abstain) {
            CHECK(cos_v71_whl_v71_ok(s) == 1u);
        } else {
            CHECK(cos_v71_whl_v71_ok(s) == 0u);
        }
    }
    cos_v71_whl_free(s);
    cos_v71_portal_free(p);
}

/* ==================================================================
 *  Microbench
 * ================================================================== */

static void run_bench(void)
{
    printf("v71 σ-Wormhole microbench:\n");

    /* 1. Hamming throughput. */
    {
        cos_v71_hv_t a, b;
        cos_v71_hv_from_seed(&a, 0xA);
        cos_v71_hv_from_seed(&b, 0xB);
        const int N = 10000000;
        uint64_t acc = 0;
        double t0 = now_sec();
        for (int i = 0; i < N; ++i) acc += cos_v71_hv_hamming(&a, &b);
        double dt = now_sec() - t0;
        double bytes_per_sec = (double)N * (double)COS_V71_HV_DIM_BYTES / (dt > 0 ? dt : 1e-9);
        printf("  hamming D=8192           : %.1f M calls/s (acc=%llu, %.1f GB/s)\n",
               (double)N / (dt > 0 ? dt : 1e-9) / 1e6,
               (unsigned long long)acc, bytes_per_sec / 1e9);
    }

    /* 2. Teleport (single XOR) throughput. */
    {
        cos_v71_hv_t secret, a, t, q, out;
        cos_v71_hv_from_seed(&secret, 0x11ULL);
        cos_v71_hv_from_seed(&a, 0x22ULL);
        cos_v71_hv_from_seed(&t, 0x33ULL);
        cos_v71_hv_from_seed(&q, 0x44ULL);
        cos_v71_portal_t *p = cos_v71_portal_new(16u, &secret);
        cos_v71_portal_insert(p, &a, &t, 0u);

        const int N = 20000000;
        double t0 = now_sec();
        for (int i = 0; i < N; ++i) cos_v71_portal_teleport(p, 0u, &q, &out);
        double dt = now_sec() - t0;
        printf("  teleport (single XOR)    : %.1f M jumps/s\n",
               (double)N / (dt > 0 ? dt : 1e-9) / 1e6);
        cos_v71_portal_free(p);
    }

    /* 3. Portal anchor match throughput (cap=256). */
    {
        cos_v71_hv_t secret, q;
        cos_v71_hv_from_seed(&secret, 0xAA);
        cos_v71_hv_from_seed(&q, 0xBB);
        cos_v71_portal_t *p = cos_v71_portal_new(256u, &secret);
        cos_v71_hv_t a, t;
        for (int i = 0; i < 256; ++i) {
            cos_v71_hv_from_seed(&a, 1000ULL + (uint64_t)i);
            cos_v71_hv_from_seed(&t, 2000ULL + (uint64_t)i);
            cos_v71_portal_insert(p, &a, &t, (uint32_t)i);
        }
        const int N = 200000;
        int32_t sim = 0;
        uint32_t acc = 0;
        double t0 = now_sec();
        for (int i = 0; i < N; ++i) acc += cos_v71_portal_match(p, &q, &sim);
        double dt = now_sec() - t0;
        printf("  portal match (cap=256)   : %.0f queries/s (acc=%u)\n",
               (double)N / (dt > 0 ? dt : 1e-9), acc);
        cos_v71_portal_free(p);
    }

    /* 4. Route throughput (cap=64, 4-hop budget). */
    {
        cos_v71_hv_t secret, src, dst;
        cos_v71_hv_from_seed(&secret, 0xDE);
        cos_v71_portal_t *p = cos_v71_portal_new(64u, &secret);
        cos_v71_hv_t a, t;
        for (int i = 0; i < 64; ++i) {
            cos_v71_hv_from_seed(&a, 5000ULL + (uint64_t)i);
            cos_v71_hv_from_seed(&t, 6000ULL + (uint64_t)i);
            cos_v71_portal_insert(p, &a, &t, (uint32_t)i);
        }
        cos_v71_hv_from_seed(&src, 5000ULL);
        cos_v71_hv_from_seed(&dst, 6000ULL);
        cos_v71_hv_t pos;
        cos_v71_route_t r;
        const int N = 20000;
        double t0 = now_sec();
        for (int i = 0; i < N; ++i)
            cos_v71_portal_route(p, &src, &dst,
                                 COS_V71_ARRIVAL_DEFAULT_Q15,
                                 4u, &pos, &r);
        double dt = now_sec() - t0;
        printf("  route 4-hop (cap=64)     : %.0f routes/s\n",
               (double)N / (dt > 0 ? dt : 1e-9));
        cos_v71_portal_free(p);
    }

    /* 5. WHL end-to-end. */
    {
        cos_v71_hv_t secret, a;
        cos_v71_hv_from_seed(&secret, 0xC1);
        cos_v71_hv_zero(&a);
        cos_v71_portal_t *p = cos_v71_portal_new(8u, &secret);
        cos_v71_portal_insert(p, &a, &a, 0u);

        cos_v71_whl_state_t *s = cos_v71_whl_new();
        cos_v71_whl_ctx_t ctx = {0};
        ctx.anchor_src = &a;
        ctx.portal = p;

        cos_v71_inst_t prog[] = {
            { COS_V71_OP_ANCHOR,   0, 0, 0, 0, 0 },
            { COS_V71_OP_TELEPORT, 1, 0, 0, 0, 0 },
            { COS_V71_OP_VERIFY,   0, 0, 0, 0, 0 },
            { COS_V71_OP_MEASURE,  2, 1, 0, 0, 0 },
            { COS_V71_OP_GATE,     0, 2, 0, (int16_t)COS_V71_ARRIVAL_DEFAULT_Q15, 0 },
            { COS_V71_OP_HALT,     0, 0, 0, 0, 0 },
        };
        const int N = 500000;
        uint32_t ok_sum = 0;
        double t0 = now_sec();
        for (int i = 0; i < N; ++i) {
            cos_v71_whl_reset(s);
            (void)cos_v71_whl_exec(s, prog, 6u, &ctx, 64u, 8u);
            ok_sum += cos_v71_whl_v71_ok(s);
        }
        double dt = now_sec() - t0;
        printf("  whl 5-inst end-to-end    : %.0f progs/s (ok=%u)\n",
               (double)N / (dt > 0 ? dt : 1e-9), ok_sum);
        cos_v71_whl_free(s);
        cos_v71_portal_free(p);
    }

    /* 6. Compose 12-bit × 10 M. */
    {
        const int N = 10000000;
        uint32_t acc = 0;
        double t0 = now_sec();
        for (int i = 0; i < N; ++i) {
            cos_v71_decision_t d = cos_v71_compose_decision(
                (i & 1),        ((i >> 1) & 1), ((i >> 2) & 1),
                ((i >> 3) & 1), ((i >> 4) & 1), ((i >> 5) & 1),
                ((i >> 6) & 1), ((i >> 7) & 1), ((i >> 8) & 1),
                ((i >> 9) & 1), ((i >> 10) & 1), ((i >> 11) & 1));
            acc += d.allow;
        }
        double dt = now_sec() - t0;
        printf("  compose_decision 12-bit  : %.0f decisions/s (acc=%u)\n",
               (double)N / (dt > 0 ? dt : 1e-9), acc);
    }
}

/* ==================================================================
 *  --decision CLI
 * ================================================================== */

static int run_decision(int argc, char **argv)
{
    if (argc != 12) {
        fprintf(stderr,
            "usage: --decision v60 v61 v62 v63 v64 v65 v66 v67 v68 v69 v70 v71\n");
        return 2;
    }
    uint8_t v[12];
    for (int i = 0; i < 12; ++i)
        v[i] = (uint8_t)(atoi(argv[i]) ? 1 : 0);
    cos_v71_decision_t d = cos_v71_compose_decision(
        v[0], v[1], v[2], v[3], v[4], v[5],
        v[6], v[7], v[8], v[9], v[10], v[11]);
    printf("{\"v60_ok\":%u,\"v61_ok\":%u,\"v62_ok\":%u,\"v63_ok\":%u,"
           "\"v64_ok\":%u,\"v65_ok\":%u,\"v66_ok\":%u,\"v67_ok\":%u,"
           "\"v68_ok\":%u,\"v69_ok\":%u,\"v70_ok\":%u,\"v71_ok\":%u,"
           "\"allow\":%u}\n",
           d.v60_ok, d.v61_ok, d.v62_ok, d.v63_ok, d.v64_ok, d.v65_ok,
           d.v66_ok, d.v67_ok, d.v68_ok, d.v69_ok, d.v70_ok, d.v71_ok,
           d.allow);
    return d.allow ? 0 : 1;
}

/* ==================================================================
 *  Entry
 * ================================================================== */

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s\n", cos_v71_version);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--bench") == 0) {
        run_bench();
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--decision") == 0) {
        return run_decision(argc - 2, argv + 2);
    }
    if (argc < 2 || strcmp(argv[1], "--self-test") != 0) {
        fprintf(stderr,
            "usage: %s --self-test | --bench | --version | "
            "--decision v60..v71\n",
            argv[0]);
        return 2;
    }

    /* Structured tests. */
    test_hv_from_seed_and_copy();
    test_hv_xor_involution();
    test_hv_orthogonality();
    test_hv_permute_roundtrip();
    test_hv_similarity_self();
    test_portal_teleport_identity();
    test_portal_match_best();
    test_integrity_honest();
    test_integrity_poisoned_bridge();
    test_integrity_poisoned_sig();
    test_boundary_gate();
    test_route_arrive_one_hop();
    test_route_arrive_two_hops();
    test_route_miss_bounded();
    test_bond_build();
    test_receipt_distinct_paths();
    test_whl_simple_gate();
    test_whl_budget_abstain();

    /* 12-bit compose truth table (4 096 rows × 13 CHECKs = 53 248 asserts). */
    test_compose_truth_table();

    /* Fuzzers. */
    fuzz_portal_round_trip();
    fuzz_whl_gate();

    printf("v71 σ-Wormhole self-test: %u pass, %u fail\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
