/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v69 — σ-Constellation: standalone self-test + microbench
 * driver.  Exits non-zero on any assertion failure.
 *
 * 1 = 1.
 */

#include "constellation.h"

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

static void sketch_random(cos_v69_sketch_t *s, uint64_t *st)
{
    for (uint32_t i = 0; i < COS_V69_DEDUP_WORDS; ++i)
        s->w[i] = splitmix64(st);
}

static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* ==================================================================
 *  1. Tree speculative decoding tests.
 * ================================================================== */

static void test_tree_basic(void)
{
    cos_v69_draft_tree_t t;
    cos_v69_tree_init(&t);
    CHECK(t.n == 0);

    /* root: token 10, accept 0.9. */
    uint32_t r = cos_v69_tree_push(&t, 0, 10, 29491);
    CHECK(r == 0);
    CHECK(t.n == 1);

    /* child1: token 20, depth 1. */
    uint32_t c1 = cos_v69_tree_push(&t, r, 20, 29491);
    CHECK(c1 == 1);
    CHECK(t.nodes[c1].depth == 1);

    /* child2 under c1: token 30, depth 2. */
    uint32_t c2 = cos_v69_tree_push(&t, c1, 30, 29491);
    CHECK(c2 == 2);
    CHECK(t.nodes[c2].depth == 2);

    /* target path [10, 20, 30] → full branch accepted. */
    int32_t target[] = {10, 20, 30};
    uint32_t path[COS_V69_TREE_MAX];
    uint32_t len = cos_v69_draft_tree_verify(&t, target, 3,
                                             16384, path);
    CHECK(len == 3);
    CHECK(path[0] == 0);
    CHECK(path[1] == c1);
    CHECK(path[2] == c2);

    /* Target mismatch at depth 2 → length 2. */
    int32_t bad[] = {10, 20, 99};
    len = cos_v69_draft_tree_verify(&t, bad, 3, 16384, NULL);
    CHECK(len == 2);

    /* Accept threshold too high → length 0. */
    len = cos_v69_draft_tree_verify(&t, target, 3, 32000, NULL);
    CHECK(len == 0);
}

static void test_tree_fanout(void)
{
    cos_v69_draft_tree_t t;
    cos_v69_tree_init(&t);
    /* Root token 1 */
    uint32_t r = cos_v69_tree_push(&t, 0, 1, 32000);
    /* Two children: token 2 and token 3 */
    uint32_t a = cos_v69_tree_push(&t, r, 2, 32000);
    uint32_t b = cos_v69_tree_push(&t, r, 3, 32000);
    /* Grandchildren: 4 under a, 5 under b */
    uint32_t a4 = cos_v69_tree_push(&t, a, 4, 32000);
    uint32_t b5 = cos_v69_tree_push(&t, b, 5, 32000);
    (void)a4; (void)b5;

    /* Target [1, 3, 5] selects branch b → length 3. */
    int32_t tgt1[] = {1, 3, 5};
    uint32_t path[16];
    uint32_t len = cos_v69_draft_tree_verify(&t, tgt1, 3, 16384, path);
    CHECK(len == 3);
    CHECK(path[1] == b);
    CHECK(path[2] == b5);

    /* Target [1, 2, 4] selects branch a. */
    int32_t tgt2[] = {1, 2, 4};
    len = cos_v69_draft_tree_verify(&t, tgt2, 3, 16384, path);
    CHECK(len == 3);
    CHECK(path[1] == a);
    CHECK(path[2] == a4);
}

static void test_tree_prune(void)
{
    cos_v69_draft_tree_t t;
    cos_v69_tree_init(&t);
    uint32_t r = cos_v69_tree_push(&t, 0, 1, 32000);
    uint32_t c = cos_v69_tree_push(&t, r, 2, 1000); /* low path */
    (void)c;
    /* Path(r→c) ≈ 32000 * 1000 / 32768 ≈ 977 Q0.15. */
    uint32_t pruned = cos_v69_tree_prune(&t, 5000);
    CHECK(pruned == 1);
    CHECK(t.nodes[c].depth == 0xFFFF);
}

/* ==================================================================
 *  2. Multi-agent debate tests.
 * ================================================================== */

static void test_debate_basic(void)
{
    cos_v69_agent_vote_t votes[] = {
        { 42, 30000, 0 },
        { 42, 28000, 0 },
        { 42, 30000, 0 },
        {  7, 20000, 0 },
    };
    cos_v69_debate_result_t out;
    cos_v69_debate_score(votes, 4, 4, 1000, 0, &out);
    CHECK(out.len >= 2);
    CHECK(out.rank[0].proposal == 42);
    CHECK(out.abstained == 0);
    CHECK(out.margin_q15 > 0);
}

static void test_debate_abstain(void)
{
    /* Tie: abstain because margin is 0. */
    cos_v69_agent_vote_t tie[] = {
        { 1, 20000, 0 },
        { 2, 20000, 0 },
    };
    cos_v69_debate_result_t out;
    cos_v69_debate_score(tie, 2, 4, 100, 0, &out);
    CHECK(out.abstained == 1);
}

static void test_debate_anti_conformity(void)
{
    /* Without anti-conformity: 42 wins by 3 × 20000. */
    cos_v69_agent_vote_t v[] = {
        { 42, 20000, 0 }, { 42, 20000, 0 }, { 42, 20000, 0 },
        {  7, 19000, 0 },
    };
    cos_v69_debate_result_t out_no_ac, out_ac;
    cos_v69_debate_score(v, 4, 4, 100, 0, &out_no_ac);
    cos_v69_debate_score(v, 4, 4, 100, 32000, &out_ac);
    CHECK(out_no_ac.rank[0].proposal == 42);
    /* With anti-conformity penalty scaled by 3/4 → 42 loses more.
     * Still may win but margin shrinks. */
    CHECK(out_ac.margin_q15 <= out_no_ac.margin_q15);
}

/* ==================================================================
 *  3. Byzantine vote tests.
 * ================================================================== */

static void test_byzantine(void)
{
    /* N=7, f=2 → quorum = 5. */
    uint8_t v5[] = {1,1,1,1,1,0,0}; uint8_t ok = 0;
    uint32_t c = cos_v69_byzantine_vote(v5, 7, 2, &ok);
    CHECK(c == 5);
    CHECK(ok == 1);

    uint8_t v4[] = {1,1,1,1,0,0,0};
    c = cos_v69_byzantine_vote(v4, 7, 2, &ok);
    CHECK(c == 4);
    CHECK(ok == 0);

    /* N=4, f=1 → quorum = 3. */
    uint8_t v3[] = {1,1,1,0};
    c = cos_v69_byzantine_vote(v3, 4, 1, &ok);
    CHECK(c == 3);
    CHECK(ok == 1);
}

/* ==================================================================
 *  4. MoE routing tests.
 * ================================================================== */

static void test_route_topk(void)
{
    int16_t scores[] = {100, 500, 200, 800, 300, 50, 1000, 400};
    cos_v69_route_t r;
    cos_v69_route_init(&r, 3);
    uint32_t load[8] = {0};
    cos_v69_route_topk(scores, 8, &r, load);
    CHECK(r.len == 3);
    /* top-3 should be 1000 (idx 6), 800 (idx 3), 500 (idx 1) */
    CHECK(r.expert[0] == 6);
    CHECK(r.expert[1] == 3);
    CHECK(r.expert[2] == 1);
    CHECK(load[6] == 1);
    CHECK(load[3] == 1);
    CHECK(load[1] == 1);
    CHECK(load[0] == 0);
}

/* ==================================================================
 *  6. Vector clock tests.
 * ================================================================== */

static void test_vector_clock(void)
{
    cos_v69_clock_t a, b;
    cos_v69_clock_init(&a, 3);
    cos_v69_clock_init(&b, 3);
    cos_v69_clock_tick(&a, 0);
    cos_v69_clock_tick(&a, 0);
    cos_v69_clock_tick(&a, 1);
    /* a = [2,1,0] */
    CHECK(a.c[0] == 2);
    CHECK(a.c[1] == 1);

    /* b = [2,1,0] (copy) → a !< b and b !< a */
    b = a;
    CHECK(cos_v69_clock_happens_before(&a, &b) == 0);

    /* b ticks node 2 → b = [2,1,1]. Now a happens-before b. */
    cos_v69_clock_tick(&b, 2);
    CHECK(cos_v69_clock_happens_before(&a, &b) == 1);
    CHECK(cos_v69_clock_happens_before(&b, &a) == 0);

    /* Merge a with b → a becomes [2,1,1] */
    cos_v69_clock_merge(&a, &b);
    CHECK(a.c[2] == 1);
}

/* ==================================================================
 *  7. Chunked dot test.
 * ================================================================== */

static void test_chunked_dot(void)
{
    int16_t q[16], k[16];
    for (int i = 0; i < 16; ++i) { q[i] = 1000; k[i] = 1000; }
    int16_t run_max = 0;
    int16_t s = cos_v69_chunked_dot(q, k, 16, 4, &run_max);
    /* Each per-element product ≈ 1000*1000/32768 ≈ 30.
     * 16 × 30 = 480. */
    CHECK(s > 400 && s < 560);
    CHECK(run_max > 25 && run_max < 40);

    /* Zeros. */
    int16_t z[4] = {0};
    s = cos_v69_chunked_dot(z, z, 4, 4, &run_max);
    CHECK(s == 0);
}

/* ==================================================================
 *  8. Elo + UCB tests.
 * ================================================================== */

static void test_elo_ucb(void)
{
    cos_v69_bandit_t bd;
    cos_v69_bandit_init(&bd, 4);
    CHECK(bd.arms[0].elo_q15 == COS_V69_ELO_INIT_Q15);

    /* Arm 2 consistently wins. */
    for (int i = 0; i < 50; ++i) {
        cos_v69_elo_update(&bd, 2, 1, COS_V69_ELO_K_Q15);
    }
    CHECK(bd.arms[2].elo_q15 > COS_V69_ELO_INIT_Q15);
    CHECK(bd.arms[2].pulls == 50);

    /* Arm 0 loses. */
    for (int i = 0; i < 50; ++i)
        cos_v69_elo_update(&bd, 0, 0, COS_V69_ELO_K_Q15);
    CHECK(bd.arms[0].elo_q15 < COS_V69_ELO_INIT_Q15);

    /* UCB: with tiny c, picks the best-mean arm (= arm 2). */
    uint32_t pick = cos_v69_ucb_select(&bd, 0);
    CHECK(pick == 2);
}

/* ==================================================================
 *  9. Dedup tests.
 * ================================================================== */

static void test_dedup(void)
{
    cos_v69_dedup_t d;
    cos_v69_dedup_init(&d);

    cos_v69_sketch_t a, b, c;
    memset(&a, 0, sizeof(a));
    a.w[0] = 0x1234567890ABCDEFULL;
    memset(&b, 0, sizeof(b));
    b.w[0] = 0x1234567890ABCDEFULL;   /* identical */
    memset(&c, 0, sizeof(c));
    for (uint32_t i = 0; i < COS_V69_DEDUP_WORDS; ++i)
        c.w[i] = 0xFFFFFFFFFFFFFFFFULL ^ ((uint64_t)i * 7ULL); /* far */

    uint8_t collapsed = 99;
    uint32_t s0 = cos_v69_dedup_insert(&d, &a, 64, &collapsed);
    CHECK(s0 == 0);
    CHECK(collapsed == 0);
    CHECK(d.len == 1);

    uint32_t s1 = cos_v69_dedup_insert(&d, &b, 64, &collapsed);
    CHECK(s1 == 0);         /* collapsed into a */
    CHECK(collapsed == 1);
    CHECK(d.len == 1);
    CHECK(d.collapses == 1);

    uint32_t s2 = cos_v69_dedup_insert(&d, &c, 64, &collapsed);
    CHECK(s2 == 1);
    CHECK(collapsed == 0);
    CHECK(d.len == 2);
}

/* ==================================================================
 * 10. CL bytecode tests.
 * ================================================================== */

static void test_cl_end_to_end(void)
{
    cos_v69_cl_state_t *s = cos_v69_cl_new();
    CHECK(s != NULL);

    /* Tree. */
    cos_v69_draft_tree_t t;
    cos_v69_tree_init(&t);
    cos_v69_tree_push(&t, 0, 10, 32000);
    cos_v69_tree_push(&t, 0, 20, 32000);
    int32_t target[] = {10, 20};

    /* Agent votes. */
    cos_v69_agent_vote_t votes[] = {
        { 1, 30000, 0 },
        { 1, 29000, 0 },
        { 1, 30000, 0 },
        { 2, 15000, 0 },
    };
    cos_v69_debate_result_t dres;

    /* Byzantine votes (N=7, f=2 → need 5). */
    uint8_t byz[] = {1,1,1,1,1,0,0};

    /* Experts. */
    int16_t scores[] = {100, 500, 200, 800, 300, 50, 1000, 400};
    cos_v69_route_t rr;
    cos_v69_route_init(&rr, 3);
    uint32_t load[8] = {0};

    /* Clocks. */
    cos_v69_clock_t my, peer;
    cos_v69_clock_init(&my, 3);
    cos_v69_clock_init(&peer, 3);
    cos_v69_clock_tick(&peer, 1);

    /* Bandit. */
    cos_v69_bandit_t bd;
    cos_v69_bandit_init(&bd, 4);

    /* Dedup. */
    cos_v69_dedup_t dd;
    cos_v69_dedup_init(&dd);
    cos_v69_sketch_t sk = { {0x1111,0,0,0,0,0,0,0} };

    cos_v69_cl_ctx_t ctx = {0};
    ctx.tree            = &t;
    ctx.target_tokens   = target;
    ctx.target_len      = 2;
    ctx.min_accept_q15  = 16384;
    ctx.agent_votes     = votes;
    ctx.n_agent_votes   = 4;
    ctx.min_margin_q15  = 1000;
    ctx.anti_conformity_q15 = 0;
    ctx.debate_out      = &dres;
    ctx.byz_votes       = byz;
    ctx.n_byz_votes     = 7;
    ctx.byzantine_budget = 2;
    ctx.expert_scores   = scores;
    ctx.n_experts       = 8;
    ctx.route_out       = &rr;
    ctx.load_counter    = load;
    ctx.clock           = &my;
    ctx.peer_clock      = &peer;
    ctx.clock_node      = 0;
    ctx.bandit          = &bd;
    ctx.elo_k_q15       = COS_V69_ELO_K_Q15;
    ctx.ucb_c_q15       = COS_V69_UCB_C_Q15;
    ctx.dedup           = &dd;
    ctx.dedup_sk        = &sk;
    ctx.dedup_thresh    = 64;

    cos_v69_inst_t prog[] = {
        { COS_V69_OP_DRAFT,  0, 0, 0, 0, 0 },
        { COS_V69_OP_VERIFY, 1, 2, 0, 0, 0 },  /* reg_q15[2] = coverage */
        { COS_V69_OP_DEBATE, 3, 4, 0, 0, 0 },  /* reg_q15[3] = margin  */
        { COS_V69_OP_VOTE,   5, 6, 0, 0, 0 },
        { COS_V69_OP_ROUTE,  7, 8, 0, 0, 0 },
        { COS_V69_OP_GOSSIP, 9, 0, 0, 0, 0 },
        { COS_V69_OP_ELO,   10, 2, 0, 1, 0 },  /* arm=2 outcome=win */
        { COS_V69_OP_DEDUP, 11,12, 0, 0, 0 },
        { COS_V69_OP_GATE,   0, 3, 0, 1000, 0 },  /* margin ≥ 1000 */
    };
    int rc = cos_v69_cl_exec(s, prog, sizeof(prog)/sizeof(prog[0]),
                             &ctx, 1024);
    CHECK(rc == 0);
    CHECK(cos_v69_cl_v69_ok(s) == 1);
    CHECK(cos_v69_cl_abstained(s) == 0);
    cos_v69_cl_free(s);
}

static void test_cl_budget_abstain(void)
{
    cos_v69_cl_state_t *s = cos_v69_cl_new();
    cos_v69_cl_ctx_t ctx = {0};

    cos_v69_inst_t prog[] = {
        { COS_V69_OP_HALT, 0, 0, 0, 0, 0 },
    };
    (void)prog;
    /* Single HALT costs 1; budget 0 → abstain. */
    cos_v69_inst_t one = { COS_V69_OP_HALT, 0, 0, 0, 0, 0 };
    int rc = cos_v69_cl_exec(s, &one, 1, &ctx, 0);
    CHECK(rc == -3);
    CHECK(cos_v69_cl_abstained(s) == 1);
    CHECK(cos_v69_cl_v69_ok(s) == 0);
    cos_v69_cl_free(s);
}

/* ==================================================================
 *  Composed decision truth table (2^10 = 1024 entries).
 * ================================================================== */

static void test_compose_truth_table(void)
{
    for (uint32_t mask = 0; mask < (1u << 10); ++mask) {
        uint8_t v60 = (mask >> 0) & 1u;
        uint8_t v61 = (mask >> 1) & 1u;
        uint8_t v62 = (mask >> 2) & 1u;
        uint8_t v63 = (mask >> 3) & 1u;
        uint8_t v64 = (mask >> 4) & 1u;
        uint8_t v65 = (mask >> 5) & 1u;
        uint8_t v66 = (mask >> 6) & 1u;
        uint8_t v67 = (mask >> 7) & 1u;
        uint8_t v68 = (mask >> 8) & 1u;
        uint8_t v69 = (mask >> 9) & 1u;
        cos_v69_decision_t d = cos_v69_compose_decision(
            v60,v61,v62,v63,v64,v65,v66,v67,v68,v69);
        uint8_t expected = (uint8_t)(v60 & v61 & v62 & v63 & v64 &
                                     v65 & v66 & v67 & v68 & v69);
        CHECK(d.allow == expected);
    }
}

/* ==================================================================
 *  Randomised fuzz of tree verify (sanity).
 * ================================================================== */

static void test_tree_fuzz(void)
{
    uint64_t st = 0xC0FFEE;
    for (int trial = 0; trial < 128; ++trial) {
        cos_v69_draft_tree_t t;
        cos_v69_tree_init(&t);
        uint32_t r = cos_v69_tree_push(&t, 0, (int32_t)(splitmix64(&st) & 0xFFFF),
                                       (int16_t)(20000 + (splitmix64(&st) & 0x0FFF)));
        int32_t target[16];
        target[0] = t.nodes[r].token;
        /* Build a linear chain of depth 8 with known tokens. */
        uint32_t prev = r;
        for (int d = 1; d < 8; ++d) {
            int32_t tok = (int32_t)(splitmix64(&st) & 0xFFFF);
            target[d] = tok;
            uint32_t idx = cos_v69_tree_push(&t, prev, tok,
                                             (int16_t)(20000 + (splitmix64(&st) & 0x0FFF)));
            prev = idx;
        }
        /* Verify full path. */
        uint32_t len = cos_v69_draft_tree_verify(&t, target, 8, 16384, NULL);
        CHECK(len == 8);
    }
}

/* ==================================================================
 *  Microbenchmark.
 * ================================================================== */

static void run_bench(void)
{
    printf("v69 σ-Constellation microbench:\n");

    /* 1. Tree verify: depth-8 chain × 10 000 verifies. */
    {
        cos_v69_draft_tree_t t;
        cos_v69_tree_init(&t);
        uint32_t prev = cos_v69_tree_push(&t, 0, 0, 32000);
        int32_t target[8]; target[0] = 0;
        for (int i = 1; i < 8; ++i) {
            target[i] = i;
            prev = cos_v69_tree_push(&t, prev, i, 32000);
        }
        const int N = 10000;
        double t0 = now_sec();
        uint32_t sum = 0;
        for (int i = 0; i < N; ++i) {
            sum += cos_v69_draft_tree_verify(&t, target, 8, 16384, NULL);
        }
        double dt = now_sec() - t0;
        printf("  tree_verify depth=8       : %.0f verifies/s (sum=%u)\n",
               (double)N / (dt > 0 ? dt : 1e-9), sum);
    }

    /* 2. Debate aggregation: N=32 agents × 20 000 passes. */
    {
        cos_v69_agent_vote_t votes[32];
        uint64_t st = 42;
        for (int i = 0; i < 32; ++i) {
            votes[i].proposal = (int32_t)(splitmix64(&st) & 0x3);
            votes[i].confidence_q15 = (int16_t)(splitmix64(&st) & 0x7FFF);
        }
        const int N = 20000;
        cos_v69_debate_result_t out;
        double t0 = now_sec();
        for (int i = 0; i < N; ++i) {
            cos_v69_debate_score(votes, 32, 4, 100, 0, &out);
        }
        double dt = now_sec() - t0;
        printf("  debate_score N=32         : %.0f aggs/s (top=%d)\n",
               (double)N / (dt > 0 ? dt : 1e-9), out.rank[0].proposal);
    }

    /* 3. Byzantine vote N=64 × 1 000 000. */
    {
        uint8_t votes[64];
        for (int i = 0; i < 64; ++i) votes[i] = (i < 44) ? 1 : 0; /* 44/64 */
        const int N = 1000000;
        uint8_t ok; uint32_t c = 0;
        double t0 = now_sec();
        for (int i = 0; i < N; ++i)
            c = cos_v69_byzantine_vote(votes, 64, 21, &ok);
        double dt = now_sec() - t0;
        printf("  byzantine_vote N=64       : %.2f M votes/s (count=%u)\n",
               (double)N / (dt > 0 ? dt : 1e-9) / 1e6, c);
    }

    /* 4. Route top-K N=64 K=8 × 500 000. */
    {
        int16_t scores[64];
        uint64_t st = 7;
        for (int i = 0; i < 64; ++i)
            scores[i] = (int16_t)(splitmix64(&st) & 0x7FFF);
        cos_v69_route_t r;
        cos_v69_route_init(&r, 8);
        const int N = 500000;
        double t0 = now_sec();
        for (int i = 0; i < N; ++i)
            cos_v69_route_topk(scores, 64, &r, NULL);
        double dt = now_sec() - t0;
        printf("  route_topk N=64 K=8       : %.0f routes/s (top=%u)\n",
               (double)N / (dt > 0 ? dt : 1e-9), r.expert[0]);
    }

    /* 5. Dedup: insert 1 M sketches into 64-slot table. */
    {
        cos_v69_dedup_t d;
        cos_v69_dedup_init(&d);
        uint64_t st = 99;
        const int N = 1000000;
        double t0 = now_sec();
        cos_v69_sketch_t sk;
        for (int i = 0; i < N; ++i) {
            sketch_random(&sk, &st);
            uint8_t col = 0;
            (void)cos_v69_dedup_insert(&d, &sk,
                                       COS_V69_DEDUP_HAMMING_DEFAULT, &col);
        }
        double dt = now_sec() - t0;
        printf("  dedup_insert Hamming<64   : %.2f M inserts/s (len=%u coll=%u)\n",
               (double)N / (dt > 0 ? dt : 1e-9) / 1e6,
               d.len, d.collapses);
    }

    /* 6. Chunked dot N=1024 × 100 000. */
    {
        int16_t q[1024], k[1024];
        for (int i = 0; i < 1024; ++i) { q[i] = 1000; k[i] = 1000; }
        const int N = 100000;
        int16_t rm = 0;
        double t0 = now_sec();
        int32_t acc = 0;
        for (int i = 0; i < N; ++i)
            acc += cos_v69_chunked_dot(q, k, 1024, 64, &rm);
        double dt = now_sec() - t0;
        printf("  chunked_dot N=1024 c=64   : %.0f dots/s (rm=%d acc=%d)\n",
               (double)N / (dt > 0 ? dt : 1e-9), rm, acc);
    }

    /* 7. Elo + UCB on 16 arms × 500 000. */
    {
        cos_v69_bandit_t bd;
        cos_v69_bandit_init(&bd, 16);
        const int N = 500000;
        double t0 = now_sec();
        uint32_t pick = 0;
        for (int i = 0; i < N; ++i) {
            cos_v69_elo_update(&bd, (uint32_t)(i & 15),
                               (uint8_t)((i * 17) & 1),
                               COS_V69_ELO_K_Q15);
            pick = cos_v69_ucb_select(&bd, COS_V69_UCB_C_Q15);
        }
        double dt = now_sec() - t0;
        printf("  elo+ucb 16 arms           : %.0f updates/s (last pick=%u)\n",
               (double)N / (dt > 0 ? dt : 1e-9), pick);
    }

    /* 8. CL program: 9-instruction end-to-end × 200 000. */
    {
        cos_v69_cl_state_t *s = cos_v69_cl_new();
        cos_v69_draft_tree_t t;
        cos_v69_tree_init(&t);
        cos_v69_tree_push(&t, 0, 10, 32000);
        cos_v69_tree_push(&t, 0, 20, 32000);
        int32_t target[] = {10, 20};
        cos_v69_agent_vote_t votes[] = {
            { 1, 30000, 0 }, { 1, 29000, 0 }, { 1, 30000, 0 }, { 2, 15000, 0 },
        };
        cos_v69_debate_result_t dres;
        uint8_t byz[] = {1,1,1,1,1,0,0};
        int16_t scores[] = {100, 500, 200, 800, 300, 50, 1000, 400};
        cos_v69_route_t rr; cos_v69_route_init(&rr, 3);
        uint32_t load[8] = {0};
        cos_v69_clock_t my, peer;
        cos_v69_clock_init(&my, 3); cos_v69_clock_init(&peer, 3);
        cos_v69_bandit_t bd; cos_v69_bandit_init(&bd, 4);
        cos_v69_dedup_t dd;  cos_v69_dedup_init(&dd);
        cos_v69_sketch_t sk = { {0x11,0,0,0,0,0,0,0} };
        cos_v69_cl_ctx_t ctx = {
            &t, target, 2, 16384,
            votes, 4, 1000, 0, &dres,
            byz, 7, 2,
            scores, 8, &rr, load,
            &my, &peer, 0,
            &bd, COS_V69_ELO_K_Q15, COS_V69_UCB_C_Q15,
            &dd, &sk, 64,
        };
        cos_v69_inst_t prog[] = {
            { COS_V69_OP_DRAFT,  0, 0, 0,    0, 0 },
            { COS_V69_OP_VERIFY, 1, 2, 0,    0, 0 },
            { COS_V69_OP_DEBATE, 3, 4, 0,    0, 0 },
            { COS_V69_OP_VOTE,   5, 6, 0,    0, 0 },
            { COS_V69_OP_ROUTE,  7, 8, 0,    0, 0 },
            { COS_V69_OP_GOSSIP, 9, 0, 0,    0, 0 },
            { COS_V69_OP_ELO,   10, 2, 0,    1, 0 },
            { COS_V69_OP_DEDUP, 11,12, 0,    0, 0 },
            { COS_V69_OP_GATE,   0, 3, 0, 1000, 0 },
        };
        const int N = 200000;
        double t0 = now_sec();
        int ok_sum = 0;
        for (int i = 0; i < N; ++i) {
            (void)cos_v69_cl_exec(s, prog, 9, &ctx, 1024);
            ok_sum += cos_v69_cl_v69_ok(s);
        }
        double dt = now_sec() - t0;
        printf("  cl 9-inst end-to-end      : %.0f progs/s (ok=%d)\n",
               (double)N / (dt > 0 ? dt : 1e-9), ok_sum);
        cos_v69_cl_free(s);
    }

    /* 9. Compose 10-bit × 10 000 000. */
    {
        const int N = 10000000;
        uint32_t acc = 0;
        double t0 = now_sec();
        for (int i = 0; i < N; ++i) {
            cos_v69_decision_t d = cos_v69_compose_decision(
                (i & 1), ((i >> 1) & 1), ((i >> 2) & 1),
                ((i >> 3) & 1), ((i >> 4) & 1), ((i >> 5) & 1),
                ((i >> 6) & 1), ((i >> 7) & 1), ((i >> 8) & 1),
                ((i >> 9) & 1));
            acc += d.allow;
        }
        double dt = now_sec() - t0;
        printf("  compose_decision 10-bit   : %.0f decisions/s (acc=%u)\n",
               (double)N / (dt > 0 ? dt : 1e-9), acc);
    }
}

/* ==================================================================
 *  Decision CLI: print JSON for cos_v69_compose_decision.
 * ================================================================== */

static int run_decision(int argc, char **argv)
{
    if (argc != 10) {
        fprintf(stderr,
                "usage: --decision v60 v61 v62 v63 v64 v65 v66 v67 v68 v69\n");
        return 2;
    }
    uint8_t v[10];
    for (int i = 0; i < 10; ++i) {
        v[i] = (uint8_t)(atoi(argv[i]) ? 1 : 0);
    }
    cos_v69_decision_t d = cos_v69_compose_decision(
        v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9]);
    printf("{\"v60_ok\":%u,\"v61_ok\":%u,\"v62_ok\":%u,\"v63_ok\":%u,"
           "\"v64_ok\":%u,\"v65_ok\":%u,\"v66_ok\":%u,\"v67_ok\":%u,"
           "\"v68_ok\":%u,\"v69_ok\":%u,\"allow\":%u}\n",
           d.v60_ok, d.v61_ok, d.v62_ok, d.v63_ok, d.v64_ok,
           d.v65_ok, d.v66_ok, d.v67_ok, d.v68_ok, d.v69_ok, d.allow);
    return d.allow ? 0 : 1;
}

/* ==================================================================
 *  Entry.
 * ================================================================== */

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s\n", cos_v69_version);
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
                "--decision v60 v61 v62 v63 v64 v65 v66 v67 v68 v69\n",
                argv[0]);
        return 2;
    }

    test_tree_basic();
    test_tree_fanout();
    test_tree_prune();
    test_debate_basic();
    test_debate_abstain();
    test_debate_anti_conformity();
    test_byzantine();
    test_route_topk();
    test_vector_clock();
    test_chunked_dot();
    test_elo_ucb();
    test_dedup();
    test_cl_end_to_end();
    test_cl_budget_abstain();
    test_compose_truth_table();
    test_tree_fuzz();

    /* Large deterministic fuzz: 1 000 byzantine votes × varying f. */
    for (uint32_t trial = 0; trial < 1000; ++trial) {
        uint8_t votes[16];
        uint32_t ones = 0;
        for (int i = 0; i < 16; ++i) {
            votes[i] = (uint8_t)((trial >> i) & 1u);
            ones += votes[i];
        }
        uint8_t ok = 0;
        uint32_t c = cos_v69_byzantine_vote(votes, 16, 3, &ok);
        CHECK(c == ones);
        CHECK(ok == (ones >= 7 ? 1 : 0));
    }

    /* 512 dedup random inserts — table saturates (collapses counted). */
    {
        cos_v69_dedup_t d; cos_v69_dedup_init(&d);
        uint64_t st = 0xABCD1234u;
        for (int i = 0; i < 512; ++i) {
            cos_v69_sketch_t sk;
            sketch_random(&sk, &st);
            uint8_t col = 0;
            (void)cos_v69_dedup_insert(&d, &sk, 64, &col);
        }
        CHECK(d.len <= COS_V69_DEDUP_MAX);
    }

    printf("v69 σ-Constellation self-test: %u pass, %u fail\n",
           g_pass, g_fail);
    return g_fail ? 1 : 0;
}
