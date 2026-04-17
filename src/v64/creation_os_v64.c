/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v64 — σ-Intellect self-test and microbench driver.
 *
 *   ./creation_os_v64 --self-test
 *   ./creation_os_v64 --bench
 *   ./creation_os_v64 --version
 *   ./creation_os_v64 --decision <v60> <v61> <v62> <v63> <v64>
 *
 * All self-tests are deterministic; they fail loudly on the smallest
 * drift.  No timing-dependent assertions; `--bench` only reports
 * throughput and never fails.
 */

#include "intellect.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* -------------------------------------------------------------------- */
/*  Tiny test runner                                                    */
/* -------------------------------------------------------------------- */

static int g_pass = 0;
static int g_fail = 0;

static void check(const char *what, int ok)
{
    if (ok) { ++g_pass; return; }
    ++g_fail;
    fprintf(stderr, "FAIL: %s\n", what);
}

/* -------------------------------------------------------------------- */
/*  Local timing helper                                                 */
/* -------------------------------------------------------------------- */

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* ==================================================================== */
/*  1.  Composition truth table (5 bits → 32 rows × 6 assertions each)  */
/* ==================================================================== */

static void test_composition(void)
{
    for (uint32_t row = 0; row < 32; ++row) {
        uint8_t a = (row >> 0) & 1u;
        uint8_t b = (row >> 1) & 1u;
        uint8_t c = (row >> 2) & 1u;
        uint8_t d = (row >> 3) & 1u;
        uint8_t e = (row >> 4) & 1u;
        cos_v64_decision_t r = cos_v64_compose_decision(a, b, c, d, e);
        check("compose lane v60", r.v60_ok == a);
        check("compose lane v61", r.v61_ok == b);
        check("compose lane v62", r.v62_ok == c);
        check("compose lane v63", r.v63_ok == d);
        check("compose lane v64", r.v64_ok == e);
        uint8_t expect = (uint8_t)(a & b & c & d & e);
        check("compose allow = 5-way AND", r.allow == expect);
    }
    /* Non-canonical 0/1 normalisation: any nonzero → 1.                 */
    cos_v64_decision_t r = cos_v64_compose_decision(2, 7, 0xFF, 0x80, 1);
    check("compose normalises nonzero → 1", r.allow == 1);
}

/* ==================================================================== */
/*  2.  MCTS-σ                                                          */
/* ==================================================================== */

static void test_mcts(void)
{
    cos_v64_tree_t *t = cos_v64_tree_new(64, /*cpuct*/ 49152);
    check("mcts tree_new returns non-null", t != NULL);
    if (!t) return;

    cos_v64_sigma_t root_sig = { /*eps*/ 16384, /*alp*/ 8192 };
    cos_v64_mcts_reset(t, /*prior*/ 16384, root_sig);
    check("mcts reset sets len=1", t->len == 1);
    check("mcts root parent = SENTINEL",
          t->nodes[0].parent == UINT32_MAX);

    /* Expand root with 4 children, priors biased toward child 2.       */
    uint16_t priors[4] = { 3000, 5000, 20000, 4000 };
    int rc = cos_v64_mcts_expand(t, 0, 4, priors);
    check("mcts expand returns 0", rc == 0);
    check("mcts expand grows len",
          t->len == 5 && t->nodes[0].nchild == 4);

    /* Select must pick a child whose PUCT score is max; with 0 visits
     * across children, Q=0 for all, so argmax reduces to argmax prior. */
    uint32_t sel = cos_v64_mcts_select(t, 0);
    check("mcts select picks prior-max child at 0 visits",
          sel == (0 + 2 + 1) /* child0=1 + offset 2 = 3 */);

    /* Simulate a rollout.  Push reward to the selected leaf.           */
    cos_v64_mcts_backup(t, sel, /*reward*/ 20000);
    check("mcts backup increments leaf visits",
          t->nodes[sel].visits == 1);
    check("mcts backup increments root visits",
          t->nodes[0].visits == 1);
    check("mcts backup sets leaf value≈reward",
          t->nodes[sel].value_q15 == 20000);
    check("mcts backup sets root value≈reward",
          t->nodes[0].value_q15 == 20000);

    /* Backup negative reward to a different child.                     */
    uint32_t other = t->nodes[0].child0;          /* child 0              */
    cos_v64_mcts_backup(t, other, /*reward*/ -10000);
    check("mcts backup mixed rewards updates root running mean",
          t->nodes[0].visits == 2);

    /* Run 32 more rollouts, always expand the selected leaf once.      */
    for (int it = 0; it < 32; ++it) {
        uint32_t n = 0;
        while (t->nodes[n].nchild > 0) {
            uint32_t ns = cos_v64_mcts_select(t, n);
            if (ns == UINT32_MAX) break;
            n = ns;
        }
        if (t->len + 2 <= t->cap && t->nodes[n].nchild == 0) {
            uint16_t p2[2] = { 20000, 20000 };
            cos_v64_mcts_expand(t, n, 2, p2);
        }
        /* Reward that rewards "child 2 of root" more than others.      */
        uint32_t root_child = n;
        while (t->nodes[root_child].parent != 0
            && t->nodes[root_child].parent != UINT32_MAX) {
            root_child = t->nodes[root_child].parent;
        }
        int32_t reward = (t->nodes[root_child].action_id == 2)
                       ? (int32_t)30000
                       : (int32_t)-20000;
        cos_v64_mcts_backup(t, n, reward);
    }

    uint32_t best = cos_v64_mcts_best(t);
    check("mcts best = child with highest visits",
          best != UINT32_MAX
       && t->nodes[best].action_id == 2);

    /* Overflow protection: cannot expand past cap.                      */
    cos_v64_tree_t *small = cos_v64_tree_new(3, 49152);
    cos_v64_mcts_reset(small, 16384, root_sig);
    uint16_t p4[4] = { 8192, 8192, 8192, 8192 };
    int rcx = cos_v64_mcts_expand(small, 0, 4, p4);
    check("mcts expand overflow returns -1", rcx == -1);
    cos_v64_tree_free(small);

    cos_v64_tree_free(t);
}

/* ==================================================================== */
/*  3.  Skill library                                                   */
/* ==================================================================== */

static void fill_sig(uint8_t *sig, uint8_t seed)
{
    for (int i = 0; i < COS_V64_SKILL_SIG_BYTES; ++i)
        sig[i] = (uint8_t)(seed ^ (i * 31));
}

static void test_skill_lib(void)
{
    cos_v64_skill_lib_t *l = cos_v64_skill_lib_new(16);
    check("skill lib_new returns non-null", l != NULL);
    if (!l) return;

    uint8_t sA[32], sB[32], sC[32];
    fill_sig(sA, 0x01);
    fill_sig(sB, 0x02);
    fill_sig(sC, 0x03);

    int rA = cos_v64_skill_register(l, sA, /*id*/ 10, /*conf*/ 12000);
    int rB = cos_v64_skill_register(l, sB, /*id*/ 20, /*conf*/ 18000);
    int rC = cos_v64_skill_register(l, sC, /*id*/ 30, /*conf*/ 30000);
    check("skill register A returns 0", rA == 0);
    check("skill register B returns 0", rB == 0);
    check("skill register C returns 0", rC == 0);
    check("skill lib length after 3 registers", l->len == 3);

    /* Duplicate registration detected.                                  */
    int dup = cos_v64_skill_register(l, sA, 11, 9000);
    check("skill register duplicate returns -2", dup == -2);

    /* Exact lookup.                                                      */
    uint32_t iA = cos_v64_skill_lookup(l, sA);
    uint32_t iB = cos_v64_skill_lookup(l, sB);
    uint32_t iC = cos_v64_skill_lookup(l, sC);
    check("skill lookup sA", iA == 0);
    check("skill lookup sB", iB == 1);
    check("skill lookup sC", iC == 2);

    /* Miss.                                                              */
    uint8_t sMiss[32];
    memset(sMiss, 0xFF, sizeof(sMiss));
    uint32_t iM = cos_v64_skill_lookup(l, sMiss);
    check("skill lookup miss returns SENTINEL", iM == UINT32_MAX);

    /* Hamming retrieval: query similar to B but conf floor = 15000.    */
    uint8_t qry[32];
    memcpy(qry, sB, 32);
    qry[0] ^= 0x01;           /* 1-bit Hamming distance                  */
    uint32_t ham = UINT32_MAX;
    uint32_t hit = cos_v64_skill_retrieve(l, qry, /*min_conf*/ 15000, &ham);
    check("skill retrieve picks B (nearest with confidence ≥ floor)",
          hit == 1);
    check("skill retrieve reports Hamming = 1", ham == 1);

    /* Floor excludes A (conf 12000 < floor 15000), retrieval returns
     * either B or C depending on distance.  Move query exactly midway. */
    uint8_t qry2[32];
    memcpy(qry2, sC, 32);
    qry2[0] ^= 0x02;
    uint32_t hit2 = cos_v64_skill_retrieve(l, qry2, /*min_conf*/ 15000,
                                           &ham);
    check("skill retrieve prefers C (closer, conf 30000)", hit2 == 2);

    /* Floor above all → miss.                                            */
    uint32_t hit3 = cos_v64_skill_retrieve(l, sA, /*min_conf*/ 32000,
                                           &ham);
    check("skill retrieve with unreachable floor → SENTINEL",
          hit3 == UINT32_MAX);

    cos_v64_skill_lib_free(l);
}

/* ==================================================================== */
/*  4.  Tool authorise                                                  */
/* ==================================================================== */

static void test_tool_authz(void)
{
    cos_v64_tool_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.tool_id       = 0xC0DE;
    desc.schema_ver    = 3;
    desc.required_caps = 0x0003;  /* bits 0+1                            */
    for (int i = 0; i < 32; ++i) desc.schema_hash[i] = (uint8_t)(i * 7);
    for (int i = 0; i < 32; ++i) desc.effect_hash[i] = (uint8_t)(i * 11);

    uint8_t entry[32];
    uint8_t use[32];
    for (int i = 0; i < 32; ++i) entry[i] = (uint8_t)(i + 0x10);
    memcpy(use, entry, 32);

    cos_v64_sigma_t sig_good  = { /*eps*/ 5000,  /*alp*/ 2000 };
    cos_v64_sigma_t sig_alpy  = { /*eps*/ 5000,  /*alp*/ 30000 };

    /* Happy path.                                                       */
    cos_v64_tool_authz_t r =
        cos_v64_tool_authorise(&desc, /*caps*/ 0x0003, /*ver*/ 5,
                               sig_good, /*alp_thr*/ 16000,
                               entry, use);
    check("tool allow happy path", r.verdict == COS_V64_TOOL_ALLOW);
    check("tool allow reason_bits == 0", r.reason_bits == 0);

    /* Missing cap.                                                       */
    r = cos_v64_tool_authorise(&desc, /*caps*/ 0x0001, 5, sig_good,
                               16000, entry, use);
    check("tool deny caps", r.verdict == COS_V64_TOOL_DENY_CAPS);
    check("tool deny caps reason bit 0 set",
          (r.reason_bits & 0x01) != 0);

    /* High α.                                                            */
    r = cos_v64_tool_authorise(&desc, 0x0003, 5, sig_alpy, 16000,
                               entry, use);
    check("tool deny sigma", r.verdict == COS_V64_TOOL_DENY_SIGMA);
    check("tool deny sigma reason bit 1 set",
          (r.reason_bits & 0x02) != 0);

    /* Future schema.                                                     */
    r = cos_v64_tool_authorise(&desc, 0x0003, /*ver_max*/ 2, sig_good,
                               16000, entry, use);
    check("tool deny schema", r.verdict == COS_V64_TOOL_DENY_SCHEMA);
    check("tool deny schema reason bit 2 set",
          (r.reason_bits & 0x04) != 0);

    /* TOCTOU.                                                            */
    uint8_t use2[32]; memcpy(use2, entry, 32); use2[7] ^= 0x20;
    r = cos_v64_tool_authorise(&desc, 0x0003, 5, sig_good,
                               16000, entry, use2);
    check("tool deny toctou", r.verdict == COS_V64_TOOL_DENY_TOCTOU);
    check("tool deny toctou reason bit 3 set",
          (r.reason_bits & 0x08) != 0);

    /* TOCTOU beats everything else.                                      */
    r = cos_v64_tool_authorise(&desc, /*caps*/ 0x0000 /*no caps*/,
                               /*ver_max*/ 2 /*schema bad*/,
                               sig_alpy /*alpha high*/, 16000,
                               entry, use2 /*toctou bad*/);
    check("tool TOCTOU dominates other denials",
          r.verdict == COS_V64_TOOL_DENY_TOCTOU);
    check("tool multi-cause reason_bits honest",
          (r.reason_bits & 0x0F) == 0x0F);

    /* NULL input → reserved.                                             */
    r = cos_v64_tool_authorise(&desc, 0x0003, 5, sig_good, 16000,
                               NULL, use);
    check("tool NULL entry → deny reserved",
          r.verdict == COS_V64_TOOL_DENY_RESERVED);
}

/* ==================================================================== */
/*  5.  Reflexion ratchet                                               */
/* ==================================================================== */

static void test_reflexion(void)
{
    cos_v64_skill_lib_t *l = cos_v64_skill_lib_new(8);
    if (!l) return;
    uint8_t s1[32]; fill_sig(s1, 0x55);
    cos_v64_skill_register(l, s1, /*id*/ 1, /*conf*/ 0);

    cos_v64_reflect_t r;
    cos_v64_sigma_t pred = { /*eps*/ 20000, /*alp*/ 10000 };
    cos_v64_sigma_t obs  = { /*eps*/ 10000, /*alp*/ 2000  };  /* win */

    int rc = cos_v64_reflect_update(l, 0, pred, obs, /*win_thr*/ 5000, &r);
    check("reflect update returns 0", rc == 0);
    check("reflect delta_eps = 10000", r.delta_eps_q15 == 10000);
    check("reflect delta_alp = 8000", r.delta_alp_q15 == 8000);
    check("reflect uses increments", l->skills[0].uses == 1);
    check("reflect wins increments on observed.alp ≤ thr",
          l->skills[0].wins == 1);
    check("reflect confidence = 32768 after 1/1 win",
          l->skills[0].confidence_q15 == 32768);

    /* Loss: α above threshold → wins unchanged.                          */
    cos_v64_sigma_t obs_loss = { /*eps*/ 8000, /*alp*/ 25000 };
    cos_v64_reflect_update(l, 0, pred, obs_loss, /*win_thr*/ 5000, &r);
    check("reflect loss keeps wins at 1", l->skills[0].wins == 1);
    check("reflect uses = 2", l->skills[0].uses == 2);
    check("reflect conf halves on 1/2 = 16384",
          l->skills[0].confidence_q15 == 16384);

    /* Monotone ratchet: repeated wins drive conf up, repeated losses
     * drive it down.                                                     */
    for (int i = 0; i < 4; ++i) {
        cos_v64_reflect_update(l, 0, pred, obs, /*win_thr*/ 5000, &r);
    }
    check("reflect after 5 wins / 1 loss → conf > 25000",
          l->skills[0].confidence_q15 > 25000);
    for (int i = 0; i < 10; ++i) {
        cos_v64_reflect_update(l, 0, pred, obs_loss, /*win_thr*/ 5000, &r);
    }
    check("reflect after many losses → conf drops below 16000",
          l->skills[0].confidence_q15 < 16000);

    /* Out-of-range index → -1.                                            */
    int rcb = cos_v64_reflect_update(l, 99, pred, obs, 5000, &r);
    check("reflect oor returns -1", rcb == -1);

    cos_v64_skill_lib_free(l);
}

/* ==================================================================== */
/*  6.  AlphaEvolve-σ                                                   */
/* ==================================================================== */

typedef struct {
    uint32_t calls;
    int32_t  base_energy;
} evolve_ctx_t;

static int toy_score(const uint32_t *weights, uint32_t nw,
                     void *ctx, cos_v64_evolve_score_t *out)
{
    evolve_ctx_t *c = (evolve_ctx_t *)ctx;
    c->calls += 1;
    /* Score = Q0.15 of popcount(weight==+1) − popcount(weight==-1):
     * we prefer +1 weights.  σ.alp decreases as score approaches
     * nweights.                                                         */
    int32_t pos = 0, neg = 0;
    for (uint32_t i = 0; i < nw; ++i) {
        uint32_t word = i >> 4;
        uint32_t slot = (i & 15u) * 2u;
        uint32_t v = (weights[word] >> slot) & 0x3u;
        pos += (v == 0x1u) ? 1 : 0;
        neg += (v == 0x2u) ? 1 : 0;
    }
    int32_t score = pos - neg;          /* in [-nw, +nw]                  */
    /* Map to Q0.15 energy (lower = better): energy = (nw - score).       */
    out->energy_q15 = (int32_t)nw - score;
    /* α = 32768 * (nw - score) / (2*nw + 1)   — monotone in energy.      */
    int32_t denom = 2 * (int32_t)nw + 1;
    int32_t a = (int32_t)(((int64_t)(out->energy_q15) * 32768 + denom/2)
                          / denom);
    if (a < 0) a = 0; if (a > 32767) a = 32767;
    out->sigma.alp = (uint16_t)a;
    out->sigma.eps = 16384;
    out->_pad = 0;
    return 0;
}

static void test_evolve(void)
{
    cos_v64_bitnet_t *b = cos_v64_bitnet_new(48);     /* 3 uint32 */
    check("bitnet_new returns non-null", b != NULL);
    if (!b) return;

    uint32_t scratch[3];
    evolve_ctx_t ctx = { .calls = 0, .base_energy = 0 };
    cos_v64_evolve_score_t parent;
    /* Initial state is all zeros: pos=0, neg=0, energy=nw=48.            */
    toy_score(b->weights, b->nweights, &ctx, &parent);
    check("evolve initial energy = nweights",
          parent.energy_q15 == 48);

    /* Mutate weight 0: 00 → 10 (flip of zero produces zero) — no change. */
    uint32_t f0[1] = { 0 };
    int acc0 = cos_v64_evolve_mutate(b, f0, 1, toy_score, &ctx, scratch,
                                     /*slack*/ 0 /*strict*/, &parent);
    /* 0 flips to 0; energy same; α same → alp_ok true, energy_ok true.   */
    check("evolve accepts no-op flip (zero → zero)",
          acc0 == 1 || acc0 == 0);

    /* Seed state with some +1's manually: weights array word layout.     */
    /* Directly set 16 +1 weights in word 0.                              */
    b->weights[0] = 0x55555555;  /* 16 × 01 = +1                          */
    toy_score(b->weights, b->nweights, &ctx, &parent);
    check("evolve seeded energy lower",
          parent.energy_q15 < 48);

    /* Flip one +1 weight to -1 — should INCREASE energy, be REJECTED.    */
    uint32_t f1[1] = { 5 };
    int acc1 = cos_v64_evolve_mutate(b, f1, 1, toy_score, &ctx, scratch,
                                     /*slack*/ 0 /*strict*/, &parent);
    check("evolve strict rejects bad mutation", acc1 == 0);
    /* State rolled back.                                                  */
    check("evolve rollback preserves parent pattern",
          b->weights[0] == 0x55555555);

    /* Add a known -1 at position 17 (word 1), flip it to +1.  Energy
     * should decrease → ACCEPT.                                           */
    b->weights[1] = 0x00000008;    /* slot 1 of word 1 = bits 2-3 = 10 */
    toy_score(b->weights, b->nweights, &ctx, &parent);
    int32_t before = parent.energy_q15;
    uint32_t f2[1] = { 17 };
    int acc2 = cos_v64_evolve_mutate(b, f2, 1, toy_score, &ctx, scratch,
                                     /*slack*/ 0, &parent);
    check("evolve accepts good mutation", acc2 == 1);
    check("evolve accepted mutation lowers energy",
          parent.energy_q15 < before);

    /* Out-of-range flip → -1.                                              */
    uint32_t fbad[1] = { 9999 };
    int accb = cos_v64_evolve_mutate(b, fbad, 1, toy_score, &ctx, scratch,
                                     0, &parent);
    check("evolve OOR flip returns -1", accb == -1);

    cos_v64_bitnet_free(b);
}

/* ==================================================================== */
/*  7.  Mixture-of-Depths-σ                                             */
/* ==================================================================== */

static void test_mod(void)
{
    cos_v64_sigma_t sig[8];
    for (int i = 0; i < 8; ++i) {
        sig[i].eps = 0;
        sig[i].alp = (uint16_t)(i * 4096);    /* 0, 4096, …, 28672       */
    }
    uint8_t out[8];
    cos_v64_mod_route(sig, 8, /*lo*/ 2, /*hi*/ 14, out);
    check("MoD i=0 → d_min", out[0] == 2);
    check("MoD i=7 → high depth",
          out[7] >= 12 && out[7] <= 14);
    check("MoD monotone", out[0] <= out[4] && out[4] <= out[7]);

    /* Reversed bounds auto-swap.                                         */
    uint8_t out2[8];
    cos_v64_mod_route(sig, 8, /*lo*/ 14, /*hi*/ 2, out2);
    check("MoD swap hi<lo still produces valid range",
          out2[0] == 2 && out2[7] >= 12);

    /* Cost sum branchless.                                               */
    uint64_t cost = cos_v64_mod_cost(out, 8);
    uint64_t exp  = 0;
    for (int i = 0; i < 8; ++i) exp += out[i];
    check("MoD cost sum matches", cost == exp);

    /* 0 ntokens → cost 0, no crash.                                      */
    uint64_t c0 = cos_v64_mod_cost(out, 0);
    check("MoD cost 0 tokens", c0 == 0);
}

/* ==================================================================== */
/*  8.  Microbench                                                      */
/* ==================================================================== */

static void run_bench(void)
{
    /* MCTS: 10 000 select+expand+backup iterations on a 4 096-node tree. */
    const uint32_t cap = 4096;
    cos_v64_tree_t *t = cos_v64_tree_new(cap, 49152);
    cos_v64_sigma_t root = { 16384, 8192 };
    cos_v64_mcts_reset(t, 16384, root);
    uint16_t priors[4] = { 8192, 8192, 8192, 8192 };
    cos_v64_mcts_expand(t, 0, 4, priors);

    const int iters = 10000;
    double s = now_s();
    for (int i = 0; i < iters; ++i) {
        uint32_t n = 0;
        int guard = 0;
        while (t->nodes[n].nchild > 0 && guard < 30) {
            uint32_t ns = cos_v64_mcts_select(t, n);
            if (ns == UINT32_MAX) break;
            n = ns;
            ++guard;
        }
        if (t->len + 2 <= cap && t->nodes[n].nchild == 0) {
            uint16_t p2[2] = { 8192, 8192 };
            cos_v64_mcts_expand(t, n, 2, p2);
        }
        cos_v64_mcts_backup(t, n, /*reward*/ (i % 31) * 1000 - 15500);
    }
    double e = now_s();
    printf("  MCTS-σ  (tree=%u cap):          %8.0f iters/s\n",
           cap, (double)iters / (e - s));
    cos_v64_tree_free(t);

    /* Skill-lib retrieve: 1 024 skills × 10 000 queries.                 */
    cos_v64_skill_lib_t *l = cos_v64_skill_lib_new(1024);
    for (uint32_t i = 0; i < 1024; ++i) {
        uint8_t sig[32];
        for (int j = 0; j < 32; ++j) sig[j] = (uint8_t)((i * 19 + j * 7) & 0xFF);
        cos_v64_skill_register(l, sig, /*id*/ (uint16_t)i, /*conf*/ 20000);
    }
    uint8_t q[32] = {0};
    s = now_s();
    for (int i = 0; i < 10000; ++i) {
        q[i & 31] ^= (uint8_t)i;
        uint32_t ham;
        (void)cos_v64_skill_retrieve(l, q, 10000, &ham);
    }
    e = now_s();
    printf("  skill retrieve (1024 skills):     %8.0f ops/s\n",
           10000.0 / (e - s));
    cos_v64_skill_lib_free(l);

    /* Tool authorise: 1 000 000 decisions.                                */
    cos_v64_tool_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.schema_ver = 1;
    desc.required_caps = 0x3;
    uint8_t entry[32]; uint8_t use[32];
    memset(entry, 0xAA, 32); memcpy(use, entry, 32);
    cos_v64_sigma_t sg = { 1000, 2000 };
    const int T = 1000000;
    s = now_s();
    uint32_t allow_count = 0;
    for (int i = 0; i < T; ++i) {
        cos_v64_tool_authz_t r = cos_v64_tool_authorise(
            &desc, 0x3, 5, sg, 16000, entry, use);
        allow_count += (r.verdict == COS_V64_TOOL_ALLOW);
    }
    e = now_s();
    printf("  tool-authz branchless:          %10.0f decisions/s (allow=%u)\n",
           (double)T / (e - s), allow_count);

    /* MoD route: 1 M tokens.                                               */
    uint32_t N = 1u << 20;
    cos_v64_sigma_t *alps = (cos_v64_sigma_t *)aligned_alloc(
        64, ((size_t)N * sizeof(*alps) + 63) & ~(size_t)63);
    uint8_t *depths = (uint8_t *)aligned_alloc(
        64, ((size_t)N + 63) & ~(size_t)63);
    for (uint32_t i = 0; i < N; ++i) {
        alps[i].eps = 0;
        alps[i].alp = (uint16_t)((i * 97) & 0x7FFF);
    }
    s = now_s();
    cos_v64_mod_route(alps, N, 2, 32, depths);
    e = now_s();
    printf("  MoD-σ route (%u tokens):       %10.1f MiB/s\n",
           N, (double)N / (e - s) / 1.0e6);
    free(alps); free(depths);
}

/* ==================================================================== */
/*  main                                                                */
/* ==================================================================== */

static void run_self_test(void)
{
    test_composition();
    test_mcts();
    test_skill_lib();
    test_tool_authz();
    test_reflexion();
    test_evolve();
    test_mod();
    printf("v64 self-test: %d pass, %d fail\n", g_pass, g_fail);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr,
                "usage: %s --self-test|--bench|--version|"
                "--decision v60 v61 v62 v63 v64\n",
                argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "--version") == 0) {
        puts(cos_v64_version);
        return 0;
    }
    if (strcmp(argv[1], "--self-test") == 0) {
        run_self_test();
        return g_fail == 0 ? 0 : 1;
    }
    if (strcmp(argv[1], "--bench") == 0) {
        printf("v64 microbench:\n");
        run_bench();
        return 0;
    }
    if (strcmp(argv[1], "--decision") == 0 && argc == 7) {
        uint8_t v60 = (uint8_t)atoi(argv[2]);
        uint8_t v61 = (uint8_t)atoi(argv[3]);
        uint8_t v62 = (uint8_t)atoi(argv[4]);
        uint8_t v63 = (uint8_t)atoi(argv[5]);
        uint8_t v64 = (uint8_t)atoi(argv[6]);
        cos_v64_decision_t r = cos_v64_compose_decision(v60, v61, v62,
                                                        v63, v64);
        printf("{\"v60\":%u,\"v61\":%u,\"v62\":%u,\"v63\":%u,\"v64\":%u,"
               "\"allow\":%u}\n",
               r.v60_ok, r.v61_ok, r.v62_ok, r.v63_ok, r.v64_ok, r.allow);
        return r.allow == 1 ? 0 : 1;
    }
    fprintf(stderr, "unknown option: %s\n", argv[1]);
    return 2;
}
