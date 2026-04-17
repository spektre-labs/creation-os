/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v65 — σ-Hypercortex self-test and microbench driver.
 *
 * ≥260 deterministic assertions across:
 *    - 64 rows of 6-bit composition truth table (64 × 7 = 448 lines),
 *      sampled to ~220 assertions for time budget.
 *    - HV primitives: bind self-inverse, orthogonality variance, copy,
 *      zero, permute round-trip, sim bounds.
 *    - Cleanup memory: insert, exact recall, noisy recall.
 *    - Record/unbind: role→filler recovery via cleanup.
 *    - Analogy: A:B::C:? closed under XOR-bind, cleanup returns D.
 *    - Sequence memory: position-permuted bundle decode.
 *    - HVL: LOAD / BIND / SIM / CMPGE / GATE end-to-end flow.
 *
 *    ./creation_os_v65                 run self-tests
 *    ./creation_os_v65 --self-test     same
 *    ./creation_os_v65 --bench         microbench (popcount, bundle, cleanup, HVL)
 *    ./creation_os_v65 --version       print v65 version
 *    ./creation_os_v65 --decision v60 v61 v62 v63 v64 v65
 *                                      emit 6-bit JSON verdict
 */

#include "hypercortex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

static uint32_t g_pass = 0;
static uint32_t g_fail = 0;

static void ck(int cond, const char *name)
{
    if (cond) {
        g_pass++;
    } else {
        g_fail++;
        fprintf(stderr, "FAIL: %s\n", name);
    }
}

/* ------------------------------------------------------------------
 *  Monotonic wall clock in ns (for microbench, never for decisions).
 * ------------------------------------------------------------------ */
static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* ================================================================
 *  1. Six-bit composition truth table (full 64 rows, 7 checks each
 *     = 448 assertions).
 * ================================================================ */
static void test_composition(void)
{
    for (uint32_t row = 0; row < 64u; ++row) {
        uint8_t a = (row >> 0) & 1;
        uint8_t b = (row >> 1) & 1;
        uint8_t c = (row >> 2) & 1;
        uint8_t d = (row >> 3) & 1;
        uint8_t e = (row >> 4) & 1;
        uint8_t f = (row >> 5) & 1;
        cos_v65_decision_t v =
            cos_v65_compose_decision(a, b, c, d, e, f);
        ck(v.v60_ok == a, "compose row v60_ok");
        ck(v.v61_ok == b, "compose row v61_ok");
        ck(v.v62_ok == c, "compose row v62_ok");
        ck(v.v63_ok == d, "compose row v63_ok");
        ck(v.v64_ok == e, "compose row v64_ok");
        ck(v.v65_ok == f, "compose row v65_ok");
        uint8_t expect = a & b & c & d & e & f;
        ck(v.allow == expect, "compose row allow");
    }
    /* Non-zero input bits (>1) must still be reduced to 0/1. */
    cos_v65_decision_t v = cos_v65_compose_decision(7, 9, 11, 13, 255, 1);
    ck(v.v60_ok == 1 && v.v61_ok == 1 && v.v62_ok == 1 &&
       v.v63_ok == 1 && v.v64_ok == 1 && v.v65_ok == 1,
       "compose: any nonzero → 1");
    ck(v.allow == 1, "compose: any nonzero allow");
}

/* ================================================================
 *  2. Primitive HV ops.
 * ================================================================ */
static void test_primitives(void)
{
    cos_v65_hv_t a, b, c, z;
    cos_v65_hv_zero(&z);

    cos_v65_hv_from_seed(&a, 0x1111111111111111ULL);
    cos_v65_hv_from_seed(&b, 0x2222222222222222ULL);
    cos_v65_hv_from_seed(&c, 0x1111111111111111ULL);

    ck(cos_v65_hv_hamming(&a, &c) == 0, "prim: same seed → 0 hamming");
    ck(cos_v65_hv_similarity_q15(&a, &c) == 32768,
       "prim: identical sim = +32768");
    ck(cos_v65_hv_similarity_q15(&z, &z) == 32768,
       "prim: zero–zero sim = +32768");

    uint32_t h_ab = cos_v65_hv_hamming(&a, &b);
    ck(h_ab > 0, "prim: distinct seeds → nonzero hamming");
    /* Orthogonality: two random HVs ought to be near D/2 ± √D (~128).
     * Loose bound [D/2 − 512, D/2 + 512]. */
    int32_t lo = (int32_t)COS_V65_HV_DIM_BITS / 2 - 512;
    int32_t hi = (int32_t)COS_V65_HV_DIM_BITS / 2 + 512;
    ck((int32_t)h_ab >= lo && (int32_t)h_ab <= hi,
       "prim: random HVs near-orthogonal (|H − D/2| ≤ 512)");

    /* Bind self-inverse: (a ⊗ b) ⊗ b == a. */
    cos_v65_hv_t ab, abb;
    cos_v65_hv_bind(&ab,  &a, &b);
    cos_v65_hv_bind(&abb, &ab, &b);
    ck(cos_v65_hv_hamming(&abb, &a) == 0, "prim: bind self-inverse");

    /* Bind commutative. */
    cos_v65_hv_t ba;
    cos_v65_hv_bind(&ba, &b, &a);
    ck(cos_v65_hv_hamming(&ab, &ba) == 0, "prim: bind commutative");

    /* Bind associative. */
    cos_v65_hv_t ab_c, a_bc;
    cos_v65_hv_bind(&ab_c, &ab, &c);
    cos_v65_hv_bind(&ba,   &b,  &c);      /* reuse as bc */
    cos_v65_hv_bind(&a_bc, &a,  &ba);
    ck(cos_v65_hv_hamming(&ab_c, &a_bc) == 0, "prim: bind associative");

    /* Permute round-trip: perm(perm(a, +k), -k) == a. */
    for (int32_t k = -333; k <= 333; k += 111) {
        cos_v65_hv_t p1, p2;
        cos_v65_hv_permute(&p1, &a, k);
        cos_v65_hv_permute(&p2, &p1, -k);
        ck(cos_v65_hv_hamming(&p2, &a) == 0, "prim: permute round-trip");
    }

    /* Permute moves bits: for k != 0 mod D, perm(a, k) != a. */
    cos_v65_hv_t pa;
    cos_v65_hv_permute(&pa, &a, 7);
    ck(cos_v65_hv_hamming(&pa, &a) > 0,
       "prim: permute by nonzero shift changes HV");

    /* Permute preserves Hamming to any fixed HV (both shifted
     * equally, so H(perm(x,k), perm(y,k)) == H(x,y)). */
    cos_v65_hv_t pb;
    cos_v65_hv_permute(&pb, &b, 7);
    ck(cos_v65_hv_hamming(&pa, &pb) == cos_v65_hv_hamming(&a, &b),
       "prim: permute preserves Hamming (shift by same k)");

    /* Zero / copy. */
    cos_v65_hv_t cpy;
    cos_v65_hv_copy(&cpy, &a);
    ck(cos_v65_hv_hamming(&cpy, &a) == 0, "prim: copy produces identical HV");
}

/* ================================================================
 *  3. Bundle majority (threshold) + cleanup.
 * ================================================================ */
static void test_bundle_cleanup(void)
{
    int16_t *tally = (int16_t *)calloc(COS_V65_HV_DIM_BITS, sizeof(int16_t));
    ck(tally != NULL, "bundle: tally alloc");

    /* Build 5 random HVs.  Their majority bundle should be closer to
     * each of them than to a fresh unrelated HV. */
    cos_v65_hv_t hvs[5];
    const cos_v65_hv_t *ptrs[5];
    for (int i = 0; i < 5; ++i) {
        cos_v65_hv_from_seed(&hvs[i], 0xABCDEF00ULL + (uint64_t)i);
        ptrs[i] = &hvs[i];
    }
    cos_v65_hv_t bundled;
    int rc = cos_v65_hv_bundle_majority(&bundled, ptrs, 5u, tally);
    ck(rc == 0, "bundle: success rc=0");

    cos_v65_hv_t unrelated;
    cos_v65_hv_from_seed(&unrelated, 0xDEADCAFE12345678ULL);

    int32_t sim_to_unrelated = cos_v65_hv_similarity_q15(&bundled, &unrelated);
    for (int i = 0; i < 5; ++i) {
        int32_t sim_to_member = cos_v65_hv_similarity_q15(&bundled, &hvs[i]);
        ck(sim_to_member > sim_to_unrelated,
           "bundle: members closer than an unrelated HV");
    }

    /* Cleanup memory: insert the 5 HVs and query each. */
    cos_v65_cleanup_t *mem = cos_v65_cleanup_new(16);
    ck(mem != NULL, "cleanup: alloc");
    for (int i = 0; i < 5; ++i) {
        int idx = cos_v65_cleanup_insert(mem, &hvs[i],
                                         (uint32_t)(100 + i), 30000);
        ck(idx == i, "cleanup: insert slot index");
    }
    for (int i = 0; i < 5; ++i) {
        uint32_t label = 0; int32_t sim = 0;
        uint32_t idx = cos_v65_cleanup_query(mem, &hvs[i], &label, &sim);
        ck(idx   == (uint32_t)i,           "cleanup: exact nearest");
        ck(label == (uint32_t)(100 + i),   "cleanup: exact label");
        ck(sim   == 32768,                 "cleanup: exact sim = +32768");
    }

    /* Noisy query: flip ≤ D/8 bits of hvs[2] — cleanup must still
     * map back to label 102. */
    cos_v65_hv_t noisy = hvs[2];
    for (uint32_t i = 0; i < 512u; ++i) {
        uint32_t bit  = i * 31u % COS_V65_HV_DIM_BITS;
        uint32_t word = bit / 64u;
        uint32_t off  = bit % 64u;
        noisy.w[word] ^= (1ULL << off);
    }
    uint32_t lbl = 0; int32_t sim = 0;
    uint32_t idx = cos_v65_cleanup_query(mem, &noisy, &lbl, &sim);
    ck(idx == 2,        "cleanup: noisy near hvs[2]");
    ck(lbl == 102,      "cleanup: noisy label");
    ck(sim < 32768,     "cleanup: noisy sim < +32768");
    ck(sim > 0,         "cleanup: noisy sim still positive (on-manifold)");

    /* Insert overflow. */
    cos_v65_cleanup_t *tiny = cos_v65_cleanup_new(1);
    ck(tiny != NULL, "cleanup: tiny alloc");
    ck(cos_v65_cleanup_insert(tiny, &hvs[0], 0, 0) == 0,
       "cleanup: tiny insert[0]");
    ck(cos_v65_cleanup_insert(tiny, &hvs[1], 1, 0) == -1,
       "cleanup: tiny overflow rejected");
    cos_v65_cleanup_free(tiny);

    cos_v65_cleanup_free(mem);
    free(tally);
}

/* ================================================================
 *  4. Record / role-filler + analogy.
 * ================================================================ */
static void test_record_analogy(void)
{
    int16_t       *tally    = (int16_t *)calloc(COS_V65_HV_DIM_BITS, sizeof(int16_t));
    cos_v65_hv_t  *scratch  = (cos_v65_hv_t *)calloc(8, sizeof(cos_v65_hv_t));
    ck(tally != NULL && scratch != NULL, "record: scratch alloc");

    /* Roles (POSITION-like) + fillers (CONTENT-like). */
    cos_v65_hv_t role_name, role_city, role_year;
    cos_v65_hv_t fill_alice, fill_paris, fill_2026;
    cos_v65_hv_from_seed(&role_name, 0xA001ULL);
    cos_v65_hv_from_seed(&role_city, 0xA002ULL);
    cos_v65_hv_from_seed(&role_year, 0xA003ULL);
    cos_v65_hv_from_seed(&fill_alice, 0xB001ULL);
    cos_v65_hv_from_seed(&fill_paris, 0xB002ULL);
    cos_v65_hv_from_seed(&fill_2026,  0xB003ULL);

    cos_v65_pair_t pairs[3] = {
        { .role = &role_name, .filler = &fill_alice },
        { .role = &role_city, .filler = &fill_paris },
        { .role = &role_year, .filler = &fill_2026  }
    };

    cos_v65_hv_t record;
    int rc = cos_v65_record_build(&record, pairs, 3u, scratch, tally);
    ck(rc == 0, "record: build rc=0");

    cos_v65_cleanup_t *mem = cos_v65_cleanup_new(8);
    ck(mem != NULL, "record: cleanup alloc");
    (void)cos_v65_cleanup_insert(mem, &fill_alice, 1, 30000);
    (void)cos_v65_cleanup_insert(mem, &fill_paris, 2, 30000);
    (void)cos_v65_cleanup_insert(mem, &fill_2026,  3, 30000);

    cos_v65_hv_t probe;
    cos_v65_record_unbind(&probe, &record, &role_name);
    uint32_t lbl = 0; int32_t sim = 0;
    (void)cos_v65_cleanup_query(mem, &probe, &lbl, &sim);
    ck(lbl == 1, "record: unbind(name) → alice");
    ck(sim > 0,  "record: unbind(name) above cleanup floor");

    cos_v65_record_unbind(&probe, &record, &role_city);
    (void)cos_v65_cleanup_query(mem, &probe, &lbl, &sim);
    ck(lbl == 2, "record: unbind(city) → paris");

    cos_v65_record_unbind(&probe, &record, &role_year);
    (void)cos_v65_cleanup_query(mem, &probe, &lbl, &sim);
    ck(lbl == 3, "record: unbind(year) → 2026");

    /* Analogy: build a fresh cleanup memory of known fillers;
     * "alice : paris :: 2026 : ?" via D = alice ⊗ paris ⊗ 2026.
     * That exact vector is not in memory, but verify the closed-form
     * identity: D ⊗ 2026 ⊗ paris == alice, so a cleanup query on
     * (D ⊗ 2026 ⊗ paris) must return label 1 (alice). */
    cos_v65_hv_t ab;
    cos_v65_hv_bind(&ab, &fill_alice, &fill_paris);
    cos_v65_hv_t abc;
    cos_v65_hv_bind(&abc, &ab, &fill_2026);  /* D */

    cos_v65_hv_t back, back2;
    cos_v65_hv_bind(&back,  &abc,   &fill_2026);
    cos_v65_hv_bind(&back2, &back,  &fill_paris);
    (void)cos_v65_cleanup_query(mem, &back2, &lbl, &sim);
    ck(lbl == 1,    "analogy: (D ⊗ 2026 ⊗ paris) → alice label");
    ck(sim == 32768,"analogy: exact recovery");

    /* Direct analogy helper. */
    cos_v65_hv_t scratch1;
    uint32_t an_lbl = 0; int32_t an_sim = 0;
    (void)cos_v65_analogy(mem, &fill_alice, &fill_paris,
                          &fill_2026, &scratch1, &an_lbl, &an_sim);
    ck(an_sim > 0,
       "analogy: cleanup returns some label above 0 for unknown D");

    cos_v65_cleanup_free(mem);
    free(scratch);
    free(tally);
}

/* ================================================================
 *  5. Sequence memory.
 * ================================================================ */
static void test_sequence(void)
{
    int16_t      *tally   = (int16_t *)calloc(COS_V65_HV_DIM_BITS, sizeof(int16_t));
    cos_v65_hv_t *scratch = (cos_v65_hv_t *)calloc(8, sizeof(cos_v65_hv_t));
    ck(tally != NULL && scratch != NULL, "seq: scratch alloc");

    cos_v65_hv_t items[4];
    const cos_v65_hv_t *ptrs[4];
    for (int i = 0; i < 4; ++i) {
        cos_v65_hv_from_seed(&items[i], 0xC000ULL + (uint64_t)i);
        ptrs[i] = &items[i];
    }

    cos_v65_hv_t seq;
    int rc = cos_v65_sequence_build(&seq, ptrs, 4u, 11,
                                    scratch, tally);
    ck(rc == 0, "seq: build rc=0");

    cos_v65_cleanup_t *mem = cos_v65_cleanup_new(8);
    for (int i = 0; i < 4; ++i) {
        (void)cos_v65_cleanup_insert(mem, &items[i], (uint32_t)(200 + i), 30000);
    }
    for (uint32_t pos = 0; pos < 4u; ++pos) {
        cos_v65_hv_t at;
        cos_v65_hv_t rolled;
        cos_v65_sequence_at(&at, &seq, pos, 11);
        (void)cos_v65_hv_copy(&rolled, &at);
        uint32_t lbl = 0; int32_t sim = 0;
        (void)cos_v65_cleanup_query(mem, &rolled, &lbl, &sim);
        ck(lbl == (uint32_t)(200 + (int)pos), "seq: decode position");
        ck(sim > 0, "seq: decoded position above 0");
    }
    cos_v65_cleanup_free(mem);
    free(scratch);
    free(tally);
}

/* ================================================================
 *  6. HVL bytecode interpreter.
 * ================================================================ */
static void test_hvl(void)
{
    cos_v65_hvl_state_t *s = cos_v65_hvl_new(4);
    ck(s != NULL, "hvl: new");

    /* Arena of three labeled hypervectors. */
    cos_v65_hv_t A, B, C;
    cos_v65_hv_from_seed(&A, 0xAAAA0001ULL);
    cos_v65_hv_from_seed(&B, 0xBBBB0002ULL);
    cos_v65_hv_from_seed(&C, 0xCCCC0003ULL);
    const cos_v65_hv_t *arena[3] = { &A, &B, &C };

    /* Cleanup memory with labels {10, 20, 30}. */
    cos_v65_cleanup_t *mem = cos_v65_cleanup_new(8);
    (void)cos_v65_cleanup_insert(mem, &A, 10, 30000);
    (void)cos_v65_cleanup_insert(mem, &B, 20, 30000);
    (void)cos_v65_cleanup_insert(mem, &C, 30, 30000);

    /* Program:
     *   LOAD  r0, 0      ; r0 ← A
     *   LOAD  r1, 1      ; r1 ← B
     *   BIND  r2, r0, r1 ; r2 ← A⊗B
     *   BIND  r3, r2, r1 ; r3 ← A (bind self-inverse)
     *   SIM   _, r0, r3  ; state.sim ← sim(A, A) = +32768
     *   CMPGE imm=20000  ; state.flag ← sim ≥ 20000 (true)
     *   GATE             ; state.v65_ok ← flag & under_budget
     *   LOOKUP _, r3     ; state.label ← cleanup(A) = 10
     *   HALT
     */
    cos_v65_inst_t prog[] = {
        { .op = COS_V65_OP_LOAD,   .dst = 0, .a = 0, .b = 0, .imm = 0    },
        { .op = COS_V65_OP_LOAD,   .dst = 1, .a = 0, .b = 0, .imm = 1    },
        { .op = COS_V65_OP_BIND,   .dst = 2, .a = 0, .b = 1, .imm = 0    },
        { .op = COS_V65_OP_BIND,   .dst = 3, .a = 2, .b = 1, .imm = 0    },
        { .op = COS_V65_OP_SIM,    .dst = 0, .a = 0, .b = 3, .imm = 0    },
        { .op = COS_V65_OP_CMPGE,  .dst = 0, .a = 0, .b = 0, .imm = 20000},
        { .op = COS_V65_OP_GATE,   .dst = 0, .a = 0, .b = 0, .imm = 0    },
        { .op = COS_V65_OP_LOOKUP, .dst = 0, .a = 3, .b = 0, .imm = 0    },
        { .op = COS_V65_OP_HALT,   .dst = 0, .a = 0, .b = 0, .imm = 0    }
    };
    int rc = cos_v65_hvl_exec(s, arena, 3u, mem, prog,
                              (uint32_t)(sizeof(prog) / sizeof(prog[0])),
                              /*budget=*/ (uint64_t)1 << 30);
    ck(rc      == 0,      "hvl: exec ok");
    ck(s->sim_q15 == 32768, "hvl: sim(A, A) = +32768");
    ck(s->flag == 1,      "hvl: CMPGE flag set");
    ck(s->v65_ok == 1,    "hvl: GATE allows under budget");
    ck(s->label == 10,    "hvl: LOOKUP → label 10 (A)");
    ck(s->halted == 1,    "hvl: HALT observed");

    /* Negative: tiny budget → GATE refuses. */
    rc = cos_v65_hvl_exec(s, arena, 3u, mem, prog,
                          (uint32_t)(sizeof(prog) / sizeof(prog[0])),
                          /*budget=*/ 2u);
    ck(rc == -2, "hvl: over-budget returns -2");
    ck(s->v65_ok == 0, "hvl: over-budget → v65_ok=0");

    /* Negative: malformed opcode. */
    cos_v65_inst_t bad[] = {
        { .op = 99, .dst = 0, .a = 0, .b = 0, .imm = 0 }
    };
    rc = cos_v65_hvl_exec(s, arena, 3u, mem, bad, 1u, (uint64_t)1 << 30);
    ck(rc == -1, "hvl: malformed op → -1");

    /* Negative: out-of-range register. */
    cos_v65_inst_t oor[] = {
        { .op = COS_V65_OP_BIND, .dst = 99, .a = 0, .b = 0, .imm = 0 }
    };
    rc = cos_v65_hvl_exec(s, arena, 3u, mem, oor, 1u, (uint64_t)1 << 30);
    ck(rc == -1, "hvl: register out-of-range → -1");

    cos_v65_cleanup_free(mem);
    cos_v65_hvl_free(s);
}

/* ================================================================
 *  Top-level driver.
 * ================================================================ */
static int run_self_test(void)
{
    test_composition();
    test_primitives();
    test_bundle_cleanup();
    test_record_analogy();
    test_sequence();
    test_hvl();
    printf("v65 self-test: %u pass, %u fail\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

/* ---------------- microbench ------------------------------------ */

static void bench_hamming(void)
{
    cos_v65_hv_t a, b;
    cos_v65_hv_from_seed(&a, 0x1111);
    cos_v65_hv_from_seed(&b, 0x2222);
    const uint32_t N = 1u << 20;    /* 1 M pairs */
    uint64_t t0 = now_ns();
    volatile uint32_t sink = 0;
    for (uint32_t i = 0; i < N; ++i) {
        sink ^= cos_v65_hv_hamming(&a, &b);
    }
    uint64_t dt = now_ns() - t0;
    double sec = (double)dt / 1e9;
    double bytes = (double)N * (double)COS_V65_HV_DIM_BYTES * 2.0; /* a+b */
    printf("  hamming:       %9.0f ops/s   (%.2f GB/s, %u bytes/HV)\n",
           (double)N / sec, bytes / sec / 1e9, COS_V65_HV_DIM_BYTES);
    (void)sink;
}

static void bench_bind(void)
{
    cos_v65_hv_t a, b, c;
    cos_v65_hv_from_seed(&a, 0x1111);
    cos_v65_hv_from_seed(&b, 0x2222);
    const uint32_t N = 1u << 20;
    uint64_t t0 = now_ns();
    for (uint32_t i = 0; i < N; ++i) {
        cos_v65_hv_bind(&c, &a, &b);
    }
    uint64_t dt = now_ns() - t0;
    double sec = (double)dt / 1e9;
    double bytes = (double)N * (double)COS_V65_HV_DIM_BYTES * 3.0;
    printf("  bind (XOR):    %9.0f ops/s   (%.2f GB/s)\n",
           (double)N / sec, bytes / sec / 1e9);
}

static void bench_cleanup(void)
{
    const uint32_t M = 1024;    /* 1024 prototypes */
    cos_v65_cleanup_t *mem = cos_v65_cleanup_new(M);
    for (uint32_t i = 0; i < M; ++i) {
        cos_v65_hv_t hv;
        cos_v65_hv_from_seed(&hv, 0xE000ULL + i);
        (void)cos_v65_cleanup_insert(mem, &hv, i, 30000);
    }
    cos_v65_hv_t q;
    cos_v65_hv_from_seed(&q, 0xE000ULL + 333);
    const uint32_t N = 5000;
    uint64_t t0 = now_ns();
    volatile uint32_t sink = 0;
    for (uint32_t i = 0; i < N; ++i) {
        uint32_t lbl = 0; int32_t sim = 0;
        (void)cos_v65_cleanup_query(mem, &q, &lbl, &sim);
        sink ^= lbl;
    }
    uint64_t dt = now_ns() - t0;
    double sec = (double)dt / 1e9;
    printf("  cleanup (1024 protos): %9.0f queries/s  (%.0f proto·cmps/s)\n",
           (double)N / sec, (double)N * M / sec);
    cos_v65_cleanup_free(mem);
    (void)sink;
}

static void bench_hvl(void)
{
    cos_v65_hvl_state_t *s = cos_v65_hvl_new(4);
    cos_v65_hv_t A, B;
    cos_v65_hv_from_seed(&A, 0xAAAA);
    cos_v65_hv_from_seed(&B, 0xBBBB);
    const cos_v65_hv_t *arena[2] = { &A, &B };
    cos_v65_cleanup_t *mem = cos_v65_cleanup_new(8);
    (void)cos_v65_cleanup_insert(mem, &A, 1, 30000);
    (void)cos_v65_cleanup_insert(mem, &B, 2, 30000);

    cos_v65_inst_t prog[] = {
        { .op = COS_V65_OP_LOAD,  .dst = 0, .a = 0, .b = 0, .imm = 0 },
        { .op = COS_V65_OP_LOAD,  .dst = 1, .a = 0, .b = 0, .imm = 1 },
        { .op = COS_V65_OP_BIND,  .dst = 2, .a = 0, .b = 1, .imm = 0 },
        { .op = COS_V65_OP_SIM,   .dst = 0, .a = 0, .b = 0, .imm = 0 },
        { .op = COS_V65_OP_CMPGE, .dst = 0, .a = 0, .b = 0, .imm = 0 },
        { .op = COS_V65_OP_GATE,  .dst = 0, .a = 0, .b = 0, .imm = 0 },
        { .op = COS_V65_OP_HALT,  .dst = 0, .a = 0, .b = 0, .imm = 0 }
    };
    const uint32_t N = 100000;
    uint64_t t0 = now_ns();
    for (uint32_t i = 0; i < N; ++i) {
        (void)cos_v65_hvl_exec(s, arena, 2u, mem, prog,
                               (uint32_t)(sizeof(prog) / sizeof(prog[0])),
                               (uint64_t)1 << 30);
    }
    uint64_t dt = now_ns() - t0;
    double sec = (double)dt / 1e9;
    printf("  hvl (7-op prog): %9.0f progs/s  (%.2f M ops/s)\n",
           (double)N / sec,
           (double)N * 7.0 / sec / 1e6);
    cos_v65_cleanup_free(mem);
    cos_v65_hvl_free(s);
}

static int run_bench(void)
{
    printf("v65 σ-Hypercortex microbench (D = %u bits, %u B per HV):\n",
           COS_V65_HV_DIM_BITS, COS_V65_HV_DIM_BYTES);
    bench_hamming();
    bench_bind();
    bench_cleanup();
    bench_hvl();
    return 0;
}

/* ---------------- main ------------------------------------------ */

static int cmd_decision(int argc, char **argv)
{
    if (argc < 8) {
        fprintf(stderr, "usage: %s --decision v60 v61 v62 v63 v64 v65\n", argv[0]);
        return 2;
    }
    uint8_t lanes[6];
    for (int i = 0; i < 6; ++i) {
        long v = strtol(argv[2 + i], NULL, 10);
        lanes[i] = (v != 0) ? 1u : 0u;
    }
    cos_v65_decision_t d = cos_v65_compose_decision(
        lanes[0], lanes[1], lanes[2], lanes[3], lanes[4], lanes[5]);
    int reason = (d.v60_ok << 0) | (d.v61_ok << 1) | (d.v62_ok << 2) |
                 (d.v63_ok << 3) | (d.v64_ok << 4) | (d.v65_ok << 5);
    printf("{\"allow\":%u,\"reason\":%d,"
           "\"v60_ok\":%u,\"v61_ok\":%u,\"v62_ok\":%u,"
           "\"v63_ok\":%u,\"v64_ok\":%u,\"v65_ok\":%u}\n",
           d.allow, reason,
           d.v60_ok, d.v61_ok, d.v62_ok,
           d.v63_ok, d.v64_ok, d.v65_ok);
    return d.allow ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s\n", cos_v65_version);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--bench") == 0) {
        return run_bench();
    }
    if (argc >= 2 && strcmp(argv[1], "--decision") == 0) {
        return cmd_decision(argc, argv);
    }
    return run_self_test();
}
