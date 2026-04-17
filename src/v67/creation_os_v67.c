/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v67 — σ-Noesis self-test + microbench driver.
 *
 *   ./creation_os_v67                 run self-tests
 *   ./creation_os_v67 --self-test     same
 *   ./creation_os_v67 --bench         microbench (bm25, dense, graph, beam, nbl)
 *   ./creation_os_v67 --version       print v67 version
 *   ./creation_os_v67 --decision v60 v61 v62 v63 v64 v65 v66 v67
 *                                     emit 8-bit JSON verdict
 */

#include "noesis.h"

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

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static uint64_t splitmix64(uint64_t *s)
{
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* ================================================================
 *  1. Compose truth table — 256 rows × 9 assertions = 2304.
 * ================================================================ */
static void test_compose(void)
{
    for (uint32_t row = 0; row < 256; ++row) {
        uint8_t v60 = (row >> 0) & 1u;
        uint8_t v61 = (row >> 1) & 1u;
        uint8_t v62 = (row >> 2) & 1u;
        uint8_t v63 = (row >> 3) & 1u;
        uint8_t v64 = (row >> 4) & 1u;
        uint8_t v65 = (row >> 5) & 1u;
        uint8_t v66 = (row >> 6) & 1u;
        uint8_t v67 = (row >> 7) & 1u;
        cos_v67_decision_t d = cos_v67_compose_decision(
            v60, v61, v62, v63, v64, v65, v66, v67);
        uint8_t expected = v60 & v61 & v62 & v63 & v64 & v65 & v66 & v67;
        ck(d.v60_ok == v60, "compose.v60");
        ck(d.v61_ok == v61, "compose.v61");
        ck(d.v62_ok == v62, "compose.v62");
        ck(d.v63_ok == v63, "compose.v63");
        ck(d.v64_ok == v64, "compose.v64");
        ck(d.v65_ok == v65, "compose.v65");
        ck(d.v66_ok == v66, "compose.v66");
        ck(d.v67_ok == v67, "compose.v67");
        ck(d.allow  == expected, "compose.allow");
    }
}

/* ================================================================
 *  2. Top-K buffer — insertion, order preservation, overflow.
 * ================================================================ */
static void test_topk(void)
{
    cos_v67_topk_t t;
    cos_v67_topk_init(&t, 4);
    ck(t.k == 4, "topk.init.k");
    ck(t.len == 0, "topk.init.len");
    ck(cos_v67_topk_top_score(&t) == -32768, "topk.empty.top");

    /* Insert monotonically increasing. */
    cos_v67_topk_insert(&t, 100, 1000);
    cos_v67_topk_insert(&t, 101, 2000);
    cos_v67_topk_insert(&t, 102, 3000);
    cos_v67_topk_insert(&t, 103, 4000);
    ck(t.len == 4, "topk.fill.len");
    ck(t.id[0] == 103, "topk.order.0");
    ck(t.id[1] == 102, "topk.order.1");
    ck(t.id[2] == 101, "topk.order.2");
    ck(t.id[3] == 100, "topk.order.3");
    ck(t.score_q15[0] == 4000, "topk.order.s0");

    /* Overflow — worse score ignored. */
    cos_v67_topk_insert(&t, 104, 500);
    ck(t.len == 4, "topk.overflow.len");
    ck(t.id[0] == 103, "topk.overflow.keep");

    /* Better score evicts the worst. */
    cos_v67_topk_insert(&t, 105, 3500);
    ck(t.len == 4, "topk.replace.len");
    ck(t.id[0] == 103, "topk.replace.top");
    ck(t.id[1] == 105, "topk.replace.1");
    ck(t.id[2] == 102, "topk.replace.2");
    ck(t.id[3] == 101, "topk.replace.3");

    /* Confidence: single-element. */
    cos_v67_topk_t t1;
    cos_v67_topk_init(&t1, 1);
    cos_v67_topk_insert(&t1, 7, 20000);
    int16_t c = cos_v67_confidence_q15(&t1);
    ck(c > 0, "confidence.single.positive");

    /* Confidence: diverse. */
    cos_v67_topk_t td;
    cos_v67_topk_init(&td, 4);
    cos_v67_topk_insert(&td, 1, 32000);
    cos_v67_topk_insert(&td, 2, 1000);
    cos_v67_topk_insert(&td, 3, 500);
    cos_v67_topk_insert(&td, 4, 100);
    int16_t cd = cos_v67_confidence_q15(&td);
    ck(cd > 20000, "confidence.peaked.high");

    /* Confidence: flat. */
    cos_v67_topk_t tf;
    cos_v67_topk_init(&tf, 4);
    cos_v67_topk_insert(&tf, 1, 10000);
    cos_v67_topk_insert(&tf, 2, 9500);
    cos_v67_topk_insert(&tf, 3, 9000);
    cos_v67_topk_insert(&tf, 4, 8500);
    int16_t cf = cos_v67_confidence_q15(&tf);
    ck(cf < 20000, "confidence.flat.low");
}

/* ================================================================
 *  3. BM25 — tiny corpus, hand-checked scoring.
 * ================================================================ */
static void test_bm25(void)
{
    /* Corpus:
     *   doc 0: "a b c"      len=3
     *   doc 1: "a a b"      len=3
     *   doc 2: "c d"        len=2
     *   doc 3: "b"          len=1
     * terms: a=0, b=1, c=2, d=3
     */
    const uint32_t postings[] = {
        /* term 0 (a) */ 0, 1,
        /* term 1 (b) */ 0, 1, 3,
        /* term 2 (c) */ 0, 2,
        /* term 3 (d) */ 2,
    };
    const uint16_t tf[] = {
        1, 2,
        1, 1, 1,
        1, 1,
        1,
    };
    const uint32_t posting_off[] = { 0, 2, 5, 7, 8 };
    const uint16_t doc_len[] = { 3, 3, 2, 1 };

    cos_v67_bm25_t idx = {
        .posting_off = posting_off,
        .postings    = postings,
        .tf          = tf,
        .doc_len     = doc_len,
        .num_terms   = 4,
        .num_docs    = 4,
        .avgdl       = 2,
        .k1_q15      = COS_V67_BM25_K1_Q15,
        .b_q15       = COS_V67_BM25_B_Q15,
    };

    /* IDF sanity: rare term (df=1) > common term (df=3). */
    int16_t idf_d = cos_v67_bm25_idf_q15(4, 1);
    int16_t idf_b = cos_v67_bm25_idf_q15(4, 3);
    ck(idf_d > idf_b, "bm25.idf.rare>common");

    /* Search for "a b": docs 0, 1, 3 (doc 2 doesn't match). */
    uint32_t q1[] = { 0u, 1u };
    cos_v67_topk_t r;
    cos_v67_topk_init(&r, 4);
    ck(cos_v67_bm25_search(&idx, q1, 2, &r) == 0, "bm25.search.ok");
    ck(r.len >= 3, "bm25.search.len");
    /* Doc 1 has higher tf for 'a' (2) plus 'b' -> should beat doc 0. */
    int found0 = 0, found1 = 0, found2 = 0;
    for (uint32_t i = 0; i < r.len; ++i) {
        if (r.id[i] == 0u) found0 = 1;
        if (r.id[i] == 1u) found1 = 1;
        if (r.id[i] == 2u) found2 = 1;
    }
    ck(found0, "bm25.search.has0");
    ck(found1, "bm25.search.has1");
    ck(!found2, "bm25.search.no2");

    /* Search for "c": docs 0 and 2. */
    uint32_t q2[] = { 2u };
    cos_v67_topk_t r2;
    cos_v67_topk_init(&r2, 4);
    ck(cos_v67_bm25_search(&idx, q2, 1, &r2) == 0, "bm25.c.ok");
    ck(r2.len == 2, "bm25.c.len");

    /* Search for unknown term: no matches. */
    uint32_t q3[] = { 99u };
    cos_v67_topk_t r3;
    cos_v67_topk_init(&r3, 4);
    ck(cos_v67_bm25_search(&idx, q3, 1, &r3) == 0, "bm25.miss.ok");
    ck(r3.len == 0, "bm25.miss.empty");

    /* Zero-length query: no-op. */
    cos_v67_topk_t r4;
    cos_v67_topk_init(&r4, 4);
    ck(cos_v67_bm25_search(&idx, q1, 0, &r4) == 0, "bm25.empty.ok");
    ck(r4.len == 0, "bm25.empty.empty");
}

/* ================================================================
 *  4. Dense signatures — Hamming, similarity, exact-match top.
 * ================================================================ */
static void test_dense(void)
{
    cos_v67_sig_t a = { { 0xAAAAAAAAAAAAAAAAULL, 0x5555555555555555ULL,
                          0xFFFFFFFFFFFFFFFFULL, 0x0000000000000000ULL } };
    cos_v67_sig_t b = a;                         /* exact match */
    cos_v67_sig_t c = a;                         /* flip 8 bits */
    c.w[0] ^= 0xFFULL;

    ck(cos_v67_sig_hamming(&a, &b) == 0, "dense.ham.same");
    ck(cos_v67_sig_hamming(&a, &c) == 8, "dense.ham.flip8");
    ck(cos_v67_sig_sim_q15(&a, &b) == 32767 ||
       cos_v67_sig_sim_q15(&a, &b) >  30000, "dense.sim.same.high");
    ck(cos_v67_sig_sim_q15(&a, &c) < cos_v67_sig_sim_q15(&a, &b),
       "dense.sim.flip.lower");

    /* Inverse signature = maximal distance = negative similarity. */
    cos_v67_sig_t inv;
    for (uint32_t i = 0; i < COS_V67_SIG_WORDS; ++i) inv.w[i] = ~a.w[i];
    ck(cos_v67_sig_hamming(&a, &inv) == 256, "dense.ham.inv.all");
    ck(cos_v67_sig_sim_q15(&a, &inv) < 0, "dense.sim.inv.negative");

    /* Store with known correct answer. */
    cos_v67_sig_t store[8];
    uint64_t s = 42;
    for (uint32_t i = 0; i < 8; ++i)
        for (uint32_t w = 0; w < COS_V67_SIG_WORDS; ++w)
            store[i].w[w] = splitmix64(&s);

    cos_v67_topk_t r;
    cos_v67_topk_init(&r, 4);
    ck(cos_v67_dense_search(store, 8, &store[3], &r) == 0, "dense.search.ok");
    ck(r.len == 4, "dense.search.len");
    ck(r.id[0] == 3, "dense.search.self.top");
}

/* ================================================================
 *  5. Graph walker.
 * ================================================================ */
static void test_graph(void)
{
    /*   0 -1-> 1 -1-> 2
     *   |      |
     *   2->    3 -1-> 4
     *   (edge 0->2 with weight 2)
     */
    const uint32_t edges[]   = { 1, 2,  2, 3,  4,  0 };
    const int16_t  w_q15[]   = { 4096, 8192, 4096, 4096, 4096, 0 };
    const uint32_t offsets[] = { 0, 2, 4, 4, 5, 6 };

    cos_v67_graph_t g = {
        .offsets   = offsets,
        .edges     = edges,
        .w_q15     = w_q15,
        .num_nodes = 5,
        .num_edges = 6,
    };

    cos_v67_topk_t r;
    cos_v67_topk_init(&r, 16);
    uint32_t n = cos_v67_graph_walk(&g, 0, 32, &r);
    ck(n >= 4, "graph.walk.reach");   /* should visit 0,1,2,3,4 */
    ck(r.len > 0, "graph.walk.nonempty");

    /* Walk from isolated-ish node. */
    cos_v67_topk_t r2;
    cos_v67_topk_init(&r2, 16);
    uint32_t n2 = cos_v67_graph_walk(&g, 2, 32, &r2);
    ck(n2 == 1, "graph.walk.dead_end");

    /* Out of range seed. */
    cos_v67_topk_t r3;
    cos_v67_topk_init(&r3, 16);
    uint32_t n3 = cos_v67_graph_walk(&g, 99, 32, &r3);
    ck(n3 == 0, "graph.walk.oob");

    /* Tight budget. */
    cos_v67_topk_t r4;
    cos_v67_topk_init(&r4, 16);
    uint32_t n4 = cos_v67_graph_walk(&g, 0, 2, &r4);
    ck(n4 == 2, "graph.walk.budget");
}

/* ================================================================
 *  6. Hybrid rescore.
 * ================================================================ */
static void test_hybrid(void)
{
    /* Equal inputs with equal weights — result = input. */
    int16_t r = cos_v67_hybrid_score_q15(20000, 20000, 20000,
                                         10000, 10000, 10000);
    ck(r > 19000 && r < 21000, "hybrid.equal");

    /* All weight on BM25. */
    r = cos_v67_hybrid_score_q15(32000, 0, 0, 32000, 0, 0);
    ck(r > 31000, "hybrid.all_bm25");

    /* Zero weights -> zero. */
    r = cos_v67_hybrid_score_q15(32000, 32000, 32000, 0, 0, 0);
    ck(r == 0, "hybrid.zero_weights");

    /* Negative graph signal is preserved. */
    r = cos_v67_hybrid_score_q15(10000, 10000, -32000, 1, 1, 1);
    ck(r < 5000, "hybrid.negative_graph");
}

/* ================================================================
 *  7. Dual-process gate.
 * ================================================================ */
static void test_dual(void)
{
    cos_v67_topk_t t;
    cos_v67_topk_init(&t, 4);
    /* High margin: S1 suffices. */
    cos_v67_topk_insert(&t, 1, 30000);
    cos_v67_topk_insert(&t, 2, 10000);
    cos_v67_topk_insert(&t, 3, 5000);
    cos_v67_dual_t d = cos_v67_dual_gate(&t, 3277);
    ck(d.use_s2 == 0, "dual.s1.high_margin");
    ck(d.gap_q15 > 3277, "dual.gap.positive");

    /* Low margin: S2 required. */
    cos_v67_topk_t t2;
    cos_v67_topk_init(&t2, 4);
    cos_v67_topk_insert(&t2, 1, 10000);
    cos_v67_topk_insert(&t2, 2, 9900);
    cos_v67_dual_t d2 = cos_v67_dual_gate(&t2, 3277);
    ck(d2.use_s2 == 1, "dual.s2.low_margin");

    /* Empty top-k: S1 (margin = +∞). */
    cos_v67_topk_t t3;
    cos_v67_topk_init(&t3, 4);
    cos_v67_dual_t d3 = cos_v67_dual_gate(&t3, 3277);
    ck(d3.use_s2 == 0, "dual.empty.s1");
}

/* ================================================================
 *  8. Tactic library.
 * ================================================================ */
static void test_tactics(void)
{
    cos_v67_tactic_t lib[] = {
        { .precondition_mask = 0x1u, .tactic_id = 101, .witness_q15 = 10000 },
        { .precondition_mask = 0x3u, .tactic_id = 102, .witness_q15 = 25000 },
        { .precondition_mask = 0x4u, .tactic_id = 103, .witness_q15 = 30000 },
        { .precondition_mask = 0x7u, .tactic_id = 104, .witness_q15 = 32000 },
    };
    int16_t w = 0;

    /* Features 0x1: only tactic 101 applies. */
    ck(cos_v67_tactic_pick(lib, 4, 0x1u, &w) == 101, "tactic.0x1");
    ck(w == 10000, "tactic.0x1.witness");

    /* Features 0x3: tactics 101, 102 apply; 102 wins. */
    ck(cos_v67_tactic_pick(lib, 4, 0x3u, &w) == 102, "tactic.0x3");

    /* Features 0x7: all four apply; 104 wins. */
    ck(cos_v67_tactic_pick(lib, 4, 0x7u, &w) == 104, "tactic.0x7");

    /* Features 0x0: none apply. */
    ck(cos_v67_tactic_pick(lib, 4, 0x0u, &w) == 0, "tactic.none");

    /* Features 0x4 only: tactic 103 applies, not 101/102. */
    ck(cos_v67_tactic_pick(lib, 4, 0x4u, &w) == 103, "tactic.0x4");
}

/* ================================================================
 *  9. Beam deliberation.
 * ================================================================ */

/* Expansion: child state = parent + (1..max), child score = −child_state/256. */
static uint32_t bt_expand(uint32_t parent, uint32_t *child_state,
                          int16_t *child_score_q15,
                          uint32_t max_children, void *ctx)
{
    (void)ctx;
    for (uint32_t i = 0; i < max_children; ++i) {
        child_state[i] = parent + i + 1u;
        int32_t s = 32000 - (int32_t)child_state[i] * 16;
        if (s < -32768) s = -32768;
        child_score_q15[i] = (int16_t)s;
    }
    return max_children;
}

static int16_t bt_verify(uint32_t state, void *ctx)
{
    (void)ctx;
    /* Verifier: +1.0 always. */
    return 32767;
}

static void test_beam(void)
{
    cos_v67_beam_t b;
    cos_v67_beam_init(&b, 4, 0u, 32000);
    ck(b.len == 1, "beam.init.len");
    ck(b.state[0] == 0u, "beam.init.state");

    ck(cos_v67_beam_step(&b, bt_expand, bt_verify, NULL) == 0,
       "beam.step.ok");
    ck(b.depth == 1u, "beam.depth");
    ck(b.len == 4u, "beam.len.after_step");

    /* Top state should have been the child with best score, i.e.
     * child_state=1 (smallest index -> highest score). */
    ck(b.state[0] == 1u, "beam.top.smallest_state");
    ck(b.score_q15[0] >= b.score_q15[1], "beam.sorted.0");
    ck(b.score_q15[1] >= b.score_q15[2], "beam.sorted.1");

    /* Second step. */
    ck(cos_v67_beam_step(&b, bt_expand, bt_verify, NULL) == 0,
       "beam.step2.ok");
    ck(b.depth == 2u, "beam.depth2");
}

/* ================================================================
 *  10. NBL — bytecode programs.
 * ================================================================ */

static void test_nbl_basic(void)
{
    cos_v67_nbl_state_t *s = cos_v67_nbl_new();
    ck(s != NULL, "nbl.new.ok");
    ck(cos_v67_nbl_cost(s) == 0, "nbl.new.cost0");
    ck(cos_v67_nbl_v67_ok(s) == 0, "nbl.new.v67_ok_zero");

    /* Minimal program: HALT immediately -> no evidence -> gate fails. */
    cos_v67_inst_t prog1[] = {
        { .op = COS_V67_OP_HALT },
    };
    ck(cos_v67_nbl_exec(s, prog1, 1, NULL, 1000) == 0, "nbl.halt.ok");
    ck(cos_v67_nbl_v67_ok(s) == 0, "nbl.halt.no_gate");

    /* Gate without evidence: v67_ok = 0. */
    cos_v67_nbl_reset(s);
    cos_v67_inst_t prog2[] = {
        { .op = COS_V67_OP_GATE, .a = 0, .imm = 0 },
        { .op = COS_V67_OP_HALT },
    };
    ck(cos_v67_nbl_exec(s, prog2, 2, NULL, 1000) == 0, "nbl.gate.no_evidence.ok");
    ck(cos_v67_nbl_v67_ok(s) == 0, "nbl.gate.no_evidence.fail");

    cos_v67_nbl_free(s);
}

static void test_nbl_recall_gate(void)
{
    /* Set up a tiny BM25 index and a RECALL + GATE program. */
    const uint32_t postings[] = { 0, 1, 2 };
    const uint16_t tf[]       = { 1, 1, 1 };
    const uint32_t posting_off[] = { 0, 3 };
    const uint16_t doc_len[]  = { 1, 1, 1 };
    cos_v67_bm25_t idx = {
        .posting_off = posting_off,
        .postings    = postings,
        .tf          = tf,
        .doc_len     = doc_len,
        .num_terms   = 1,
        .num_docs    = 3,
        .avgdl       = 1,
        .k1_q15      = COS_V67_BM25_K1_Q15,
        .b_q15       = COS_V67_BM25_B_Q15,
    };
    uint32_t qterms[] = { 0u };
    cos_v67_topk_t scratch;
    cos_v67_nbl_ctx_t ctx = {
        .bm25 = &idx, .qterms = qterms, .nqterms = 1,
        .scratch = &scratch,
    };

    /* Program: RECALL r0 ← BM25, GATE on r0 >= 0 (should pass because
     * evidence_count now >= 1 and val is the top score). */
    cos_v67_inst_t prog[] = {
        { .op = COS_V67_OP_RECALL, .dst = 0, .a = 0, .b = 3 },
        { .op = COS_V67_OP_GATE,   .a = 0, .imm = 0 },
        { .op = COS_V67_OP_HALT },
    };

    cos_v67_nbl_state_t *s = cos_v67_nbl_new();
    ck(cos_v67_nbl_exec(s, prog, 3, &ctx, 1000) == 0, "nbl.recall.ok");
    ck(cos_v67_nbl_evidence(s) == 1, "nbl.recall.evidence");
    ck(cos_v67_nbl_v67_ok(s) == 1, "nbl.recall.gate_allow");
    cos_v67_nbl_free(s);
}

static void test_nbl_budget(void)
{
    cos_v67_nbl_state_t *s = cos_v67_nbl_new();
    /* Program with cost > budget → abstain. */
    cos_v67_inst_t prog[] = {
        { .op = COS_V67_OP_DELIBERATE, .dst = 0 },   /* cost = 16 */
        { .op = COS_V67_OP_HALT },
    };
    int rc = cos_v67_nbl_exec(s, prog, 2, NULL, 4);
    ck(rc != 0, "nbl.budget.exceeded.rc");
    ck(cos_v67_nbl_abstained(s) == 1, "nbl.budget.abstain");
    ck(cos_v67_nbl_v67_ok(s) == 0, "nbl.budget.deny");
    cos_v67_nbl_free(s);
}

static void test_nbl_cmpge(void)
{
    cos_v67_nbl_state_t *s = cos_v67_nbl_new();
    /* Load a value via VERIFY (imm), CMPGE compares, GATE acts. */
    cos_v67_inst_t prog[] = {
        { .op = COS_V67_OP_VERIFY, .dst = 0, .imm = 20000 },
        { .op = COS_V67_OP_CMPGE,  .dst = 1, .a = 0, .imm = 10000 },
        { .op = COS_V67_OP_GATE,   .a = 0, .imm = 10000 },
        { .op = COS_V67_OP_HALT },
    };
    ck(cos_v67_nbl_exec(s, prog, 4, NULL, 1000) == 0, "nbl.cmpge.ok");
    ck(cos_v67_nbl_reg_q15(s, 1) == 1, "nbl.cmpge.true");
    ck(cos_v67_nbl_v67_ok(s) == 1, "nbl.cmpge.gate");

    /* Insufficient score -> gate denies. */
    cos_v67_nbl_reset(s);
    cos_v67_inst_t prog2[] = {
        { .op = COS_V67_OP_VERIFY, .dst = 0, .imm = 5000 },
        { .op = COS_V67_OP_GATE,   .a = 0, .imm = 10000 },
        { .op = COS_V67_OP_HALT },
    };
    ck(cos_v67_nbl_exec(s, prog2, 3, NULL, 1000) == 0, "nbl.cmpge2.ok");
    ck(cos_v67_nbl_v67_ok(s) == 0, "nbl.cmpge2.deny");
    cos_v67_nbl_free(s);
}

/* ================================================================
 *  Randomised stress — compose truth table, BM25 stability,
 *  dense ordering.
 * ================================================================ */
static void test_randomised(void)
{
    /* 200 random lane vectors. */
    uint64_t s = 7ULL;
    for (uint32_t i = 0; i < 200; ++i) {
        uint64_t r = splitmix64(&s);
        uint8_t v60 = (r >> 0) & 1;
        uint8_t v61 = (r >> 1) & 1;
        uint8_t v62 = (r >> 2) & 1;
        uint8_t v63 = (r >> 3) & 1;
        uint8_t v64 = (r >> 4) & 1;
        uint8_t v65 = (r >> 5) & 1;
        uint8_t v66 = (r >> 6) & 1;
        uint8_t v67 = (r >> 7) & 1;
        cos_v67_decision_t d = cos_v67_compose_decision(
            v60, v61, v62, v63, v64, v65, v66, v67);
        uint8_t expected = v60 & v61 & v62 & v63 & v64 & v65 & v66 & v67;
        ck(d.allow == expected, "rand.compose.allow");
    }

    /* Dense top-k: inserting random signatures, verify that the top
     * slot is always the closest to the query. */
    cos_v67_sig_t q;
    for (uint32_t w = 0; w < COS_V67_SIG_WORDS; ++w) q.w[w] = splitmix64(&s);
    cos_v67_sig_t store[16];
    uint32_t best_idx = 0;
    uint32_t best_h   = 1000;
    for (uint32_t i = 0; i < 16; ++i) {
        for (uint32_t w = 0; w < COS_V67_SIG_WORDS; ++w)
            store[i].w[w] = splitmix64(&s);
        uint32_t h = cos_v67_sig_hamming(&store[i], &q);
        if (h < best_h) { best_h = h; best_idx = i; }
    }
    cos_v67_topk_t r;
    cos_v67_topk_init(&r, 1);
    cos_v67_dense_search(store, 16, &q, &r);
    ck(r.len == 1, "rand.dense.len");
    ck(r.id[0] == best_idx, "rand.dense.best");
}

/* ----------------- microbench ---------------------------------- */

static void bench_topk(void)
{
    const uint32_t N = 200000;
    uint64_t s = 11ULL;
    cos_v67_topk_t t;
    uint64_t t0 = now_ns();
    for (uint32_t i = 0; i < N; ++i) {
        cos_v67_topk_init(&t, 16);
        for (uint32_t j = 0; j < 64; ++j) {
            int16_t v = (int16_t)(splitmix64(&s) & 0x7FFF);
            cos_v67_topk_insert(&t, j, v);
        }
    }
    uint64_t dt = now_ns() - t0;
    double sec = (double)dt / 1e9;
    printf("  topk (64 inserts into k=16):%10.0f iters/s\n",
           (double)N / sec);
}

static void bench_bm25(void)
{
    /* Synthetic corpus: 1024 docs, 16 terms. Each term occurs in
     * a random 1/4 of the docs. */
    const uint32_t D = 1024, T = 16;
    uint32_t *postings = (uint32_t *)malloc((size_t)D * T * sizeof(uint32_t));
    uint16_t *tf       = (uint16_t *)malloc((size_t)D * T * sizeof(uint16_t));
    uint16_t *doc_len  = (uint16_t *)malloc((size_t)D * sizeof(uint16_t));
    uint32_t *off      = (uint32_t *)malloc((size_t)(T + 1) * sizeof(uint32_t));
    uint64_t s = 29ULL;
    uint32_t written = 0;
    off[0] = 0;
    for (uint32_t t = 0; t < T; ++t) {
        for (uint32_t d = 0; d < D; ++d) {
            if ((splitmix64(&s) & 3ULL) == 0) {
                postings[written] = d;
                tf[written] = 1 + (uint16_t)(splitmix64(&s) & 7ULL);
                written++;
            }
        }
        off[t + 1] = written;
    }
    for (uint32_t d = 0; d < D; ++d)
        doc_len[d] = 4 + (uint16_t)(splitmix64(&s) & 7ULL);

    cos_v67_bm25_t idx = {
        .posting_off = off, .postings = postings, .tf = tf,
        .doc_len = doc_len, .num_terms = T, .num_docs = D,
        .avgdl = 8, .k1_q15 = COS_V67_BM25_K1_Q15, .b_q15 = COS_V67_BM25_B_Q15,
    };
    uint32_t q[] = { 0u, 3u, 5u };
    const uint32_t N = 2000;
    cos_v67_topk_t r;
    uint64_t t0 = now_ns();
    for (uint32_t i = 0; i < N; ++i) {
        cos_v67_topk_init(&r, 16);
        cos_v67_bm25_search(&idx, q, 3, &r);
    }
    uint64_t dt = now_ns() - t0;
    double sec = (double)dt / 1e9;
    printf("  bm25 (D=1024, T=16, 3-term):%10.0f queries/s\n",
           (double)N / sec);
    free(postings); free(tf); free(doc_len); free(off);
}

static void bench_dense(void)
{
    const uint32_t N = 4096;
    cos_v67_sig_t *store = (cos_v67_sig_t *)malloc(N * sizeof(cos_v67_sig_t));
    uint64_t s = 41ULL;
    for (uint32_t i = 0; i < N; ++i)
        for (uint32_t w = 0; w < COS_V67_SIG_WORDS; ++w)
            store[i].w[w] = splitmix64(&s);
    cos_v67_sig_t q = store[0];
    cos_v67_topk_t r;
    const uint32_t REP = 10000;
    uint64_t t0 = now_ns();
    for (uint32_t i = 0; i < REP; ++i) {
        cos_v67_topk_init(&r, 16);
        cos_v67_dense_search(store, N, &q, &r);
    }
    uint64_t dt = now_ns() - t0;
    double sec = (double)dt / 1e9;
    double cmps = (double)REP * (double)N;
    printf("  dense (N=%u, Hamming):     %10.0f queries/s (%.1f M cmps/s)\n",
           N, (double)REP / sec, cmps / sec / 1e6);
    free(store);
}

static void bench_beam(void)
{
    const uint32_t N = 200000;
    cos_v67_beam_t b;
    uint64_t t0 = now_ns();
    for (uint32_t i = 0; i < N; ++i) {
        cos_v67_beam_init(&b, COS_V67_BEAM_W, 0u, 32000);
        cos_v67_beam_step(&b, bt_expand, bt_verify, NULL);
        cos_v67_beam_step(&b, bt_expand, bt_verify, NULL);
        cos_v67_beam_step(&b, bt_expand, bt_verify, NULL);
    }
    uint64_t dt = now_ns() - t0;
    double sec = (double)dt / 1e9;
    printf("  beam (w=%u, 3 steps):        %10.0f iters/s\n",
           (uint32_t)COS_V67_BEAM_W, (double)N / sec);
}

static void bench_nbl(void)
{
    const uint32_t N = 500000;
    cos_v67_nbl_state_t *s = cos_v67_nbl_new();
    cos_v67_inst_t prog[] = {
        { .op = COS_V67_OP_VERIFY, .dst = 0, .imm = 20000 },
        { .op = COS_V67_OP_CMPGE,  .dst = 1, .a = 0, .imm = 10000 },
        { .op = COS_V67_OP_CONFIDE,.dst = 2 },
        { .op = COS_V67_OP_GATE,   .a = 0, .imm = 10000 },
        { .op = COS_V67_OP_HALT },
    };
    /* CONFIDE requires a scratch; feed a one-slot one. */
    cos_v67_topk_t sc;
    cos_v67_topk_init(&sc, 4);
    cos_v67_topk_insert(&sc, 0, 30000);
    cos_v67_topk_insert(&sc, 1, 5000);
    cos_v67_topk_insert(&sc, 2, 1000);
    cos_v67_nbl_ctx_t ctx = { .scratch = &sc };

    uint64_t t0 = now_ns();
    for (uint32_t i = 0; i < N; ++i) {
        cos_v67_nbl_reset(s);
        /* VERIFY has evidence++; so gate will allow. */
        (void)cos_v67_nbl_exec(s, prog, 5, &ctx, 1u << 30);
    }
    uint64_t dt = now_ns() - t0;
    double sec = (double)dt / 1e9;
    printf("  nbl (5-op program):         %10.0f progs/s (%.1f M ops/s)\n",
           (double)N / sec, (double)N * 5.0 / sec / 1e6);
    cos_v67_nbl_free(s);
}

static int run_bench(void)
{
    printf("v67 σ-Noesis microbench:\n");
    bench_topk();
    bench_bm25();
    bench_dense();
    bench_beam();
    bench_nbl();
    return 0;
}

/* ----------------- main ---------------------------------------- */

static int run_self_test(void)
{
    g_pass = 0; g_fail = 0;
    test_compose();
    test_topk();
    test_bm25();
    test_dense();
    test_graph();
    test_hybrid();
    test_dual();
    test_tactics();
    test_beam();
    test_nbl_basic();
    test_nbl_recall_gate();
    test_nbl_budget();
    test_nbl_cmpge();
    test_randomised();
    printf("v67 self-test: %u pass, %u fail\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

static int cmd_decision(int argc, char **argv)
{
    if (argc < 10) {
        fprintf(stderr,
                "usage: %s --decision v60 v61 v62 v63 v64 v65 v66 v67\n",
                argv[0]);
        return 2;
    }
    uint8_t lanes[8];
    for (int i = 0; i < 8; ++i) {
        long v = strtol(argv[2 + i], NULL, 10);
        lanes[i] = (v != 0) ? 1u : 0u;
    }
    cos_v67_decision_t d = cos_v67_compose_decision(
        lanes[0], lanes[1], lanes[2], lanes[3],
        lanes[4], lanes[5], lanes[6], lanes[7]);
    int reason = (d.v60_ok << 0) | (d.v61_ok << 1) | (d.v62_ok << 2) |
                 (d.v63_ok << 3) | (d.v64_ok << 4) | (d.v65_ok << 5) |
                 (d.v66_ok << 6) | (d.v67_ok << 7);
    printf("{\"allow\":%u,\"reason\":%d,"
           "\"v60_ok\":%u,\"v61_ok\":%u,\"v62_ok\":%u,"
           "\"v63_ok\":%u,\"v64_ok\":%u,\"v65_ok\":%u,"
           "\"v66_ok\":%u,\"v67_ok\":%u}\n",
           d.allow, reason,
           d.v60_ok, d.v61_ok, d.v62_ok,
           d.v63_ok, d.v64_ok, d.v65_ok,
           d.v66_ok, d.v67_ok);
    return d.allow ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s\n", cos_v67_version);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--bench") == 0) return run_bench();
    if (argc >= 2 && strcmp(argv[1], "--decision") == 0)
        return cmd_decision(argc, argv);
    return run_self_test();
}
