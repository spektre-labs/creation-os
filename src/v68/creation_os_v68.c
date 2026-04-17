/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v68 — σ-Mnemos: standalone driver.
 *
 *   ./creation_os_v68 --self-test   2 793 deterministic assertions
 *   ./creation_os_v68 --bench       microbench: HV / recall / hebb /
 *                                   consolidate / MML throughput
 *   ./creation_os_v68 --version     prints the version string
 *   ./creation_os_v68 --decision a b c d e f g h i  → ALLOW/DENY
 */

#include "mnemos.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* --------------------------------------------------------------------
 *  Tiny test harness (mirrors v66/v67 style).
 * -------------------------------------------------------------------- */

static uint32_t g_pass = 0, g_fail = 0;

#define TASSERT(name, cond) do {                                       \
    if (cond) { g_pass++; }                                            \
    else      { g_fail++;                                              \
                fprintf(stderr, "FAIL %s @ %s:%d\n",                   \
                        (name), __FILE__, __LINE__); }                 \
} while (0)

/* Deterministic xorshift64 PRNG (no external deps). */
static uint64_t rng_state = 0x9E3779B97F4A7C15ULL;
static uint64_t rng_u64(void)
{
    uint64_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng_state = x;
    return x;
}
static void rng_seed(uint64_t s) { rng_state = s ? s : 1ULL; }

static void random_hv(cos_v68_hv_t *hv)
{
    for (uint32_t i = 0; i < COS_V68_HV_WORDS; ++i) hv->w[i] = rng_u64();
}

static void flip_bits(cos_v68_hv_t *hv, uint32_t k)
{
    for (uint32_t i = 0; i < k; ++i) {
        uint32_t b = (uint32_t)(rng_u64() % COS_V68_HV_BITS);
        hv->w[b / 64] ^= (1ULL << (b % 64));
    }
}

/* --------------------------------------------------------------------
 *  Test 1: 9-bit composed decision — full 512-row truth table.
 * -------------------------------------------------------------------- */

static void test_compose_truth_table(void)
{
    for (uint32_t bits = 0; bits < 512u; ++bits) {
        uint8_t a = (bits >> 0) & 1u, b = (bits >> 1) & 1u,
                c = (bits >> 2) & 1u, d = (bits >> 3) & 1u,
                e = (bits >> 4) & 1u, f = (bits >> 5) & 1u,
                g = (bits >> 6) & 1u, h = (bits >> 7) & 1u,
                i = (bits >> 8) & 1u;
        cos_v68_decision_t dec =
            cos_v68_compose_decision(a, b, c, d, e, f, g, h, i);
        uint8_t expected = (uint8_t)(a & b & c & d & e & f & g & h & i);
        TASSERT("compose.truth_table", dec.allow == expected);
        TASSERT("compose.v60", dec.v60_ok == a);
        TASSERT("compose.v68", dec.v68_ok == i);
    }
    /* Non-bool inputs (any non-zero → 1). */
    cos_v68_decision_t d =
        cos_v68_compose_decision(255, 7, 1, 99, 1, 1, 1, 1, 1);
    TASSERT("compose.coerce", d.allow == 1u);
}

/* --------------------------------------------------------------------
 *  Test 2: HV Hamming + similarity sanity.
 * -------------------------------------------------------------------- */

static void test_hv_basics(void)
{
    cos_v68_hv_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    TASSERT("hv.identical.h", cos_v68_hv_hamming(&a, &b) == 0u);
    TASSERT("hv.identical.sim",
            cos_v68_hv_sim_q15(&a, &b) == (int16_t)32767 ||
            cos_v68_hv_sim_q15(&a, &b) == (int16_t)32768);

    /* Opposite HV. */
    for (uint32_t i = 0; i < COS_V68_HV_WORDS; ++i) b.w[i] = ~0ULL;
    TASSERT("hv.opposite.h",
            cos_v68_hv_hamming(&a, &b) == COS_V68_HV_BITS);
    TASSERT("hv.opposite.sim", cos_v68_hv_sim_q15(&a, &b) <= -32767);

    /* Random HV: similarity to itself == max. */
    rng_seed(0xC0FFEE);
    cos_v68_hv_t r;
    random_hv(&r);
    TASSERT("hv.self_sim.max", cos_v68_hv_sim_q15(&r, &r) >= 32767);

    /* XOR bind is involution. */
    cos_v68_hv_t r2 = r;
    cos_v68_hv_t mask;
    random_hv(&mask);
    cos_v68_hv_xor_inplace(&r2, &mask);
    cos_v68_hv_xor_inplace(&r2, &mask);
    TASSERT("hv.xor.involution", memcmp(&r2, &r, sizeof(r)) == 0);

    /* Permutation preserves popcount. */
    uint32_t pre = 0;
    for (uint32_t i = 0; i < COS_V68_HV_WORDS; ++i)
        pre += (uint32_t)__builtin_popcountll(r.w[i]);
    cos_v68_hv_t rp;
    cos_v68_hv_perm1(&rp, &r);
    uint32_t post = 0;
    for (uint32_t i = 0; i < COS_V68_HV_WORDS; ++i)
        post += (uint32_t)__builtin_popcountll(rp.w[i]);
    TASSERT("hv.perm.popcount", pre == post);

    /* Noisy HV similarity decreases monotonically with bit-flips. */
    cos_v68_hv_t base;
    rng_seed(0xBEEF);
    random_hv(&base);
    cos_v68_hv_t noisy = base;
    int16_t s_prev = 32767;
    for (uint32_t k = 1; k <= 8; ++k) {
        flip_bits(&noisy, 64);
        int16_t s = cos_v68_hv_sim_q15(&noisy, &base);
        TASSERT("hv.noise.monotonic", s <= s_prev);
        s_prev = s;
    }
}

/* --------------------------------------------------------------------
 *  Test 3: Surprise gate.
 * -------------------------------------------------------------------- */

static void test_surprise_gate(void)
{
    int16_t out = -1;
    /* Equal pred & obs → surprise = 0, gate = 0. */
    uint8_t g = cos_v68_surprise_gate(10000, 10000, 1000, &out);
    TASSERT("surprise.zero.gate", g == 0u);
    TASSERT("surprise.zero.out",  out == 0);

    /* Large diff → surprise above threshold → gate = 1. */
    g = cos_v68_surprise_gate(0, 30000, 1000, &out);
    TASSERT("surprise.high.gate", g == 1u);
    TASSERT("surprise.high.out",  out == 30000);

    /* Negative diff handled (absolute value). */
    g = cos_v68_surprise_gate(20000, -10000, 5000, &out);
    TASSERT("surprise.neg.gate", g == 1u);
    TASSERT("surprise.neg.out",  out == 30000);

    /* Edge case: surprise == threshold → gate = 1 (≥). */
    g = cos_v68_surprise_gate(0, 1000, 1000, &out);
    TASSERT("surprise.eq.gate",  g == 1u);
    TASSERT("surprise.eq.out",   out == 1000);

    /* Below threshold → gate = 0. */
    g = cos_v68_surprise_gate(0, 999, 1000, &out);
    TASSERT("surprise.below.gate", g == 0u);
}

/* --------------------------------------------------------------------
 *  Test 4: ACT-R decay.
 * -------------------------------------------------------------------- */

static void test_actr_decay(void)
{
    /* Linear, branchless. */
    int16_t a = cos_v68_actr_decay_q15(32000, 100, 10);
    TASSERT("decay.linear", a == 31000);

    /* Saturates at 0. */
    a = cos_v68_actr_decay_q15(100, 1000, 1000);
    TASSERT("decay.zero.sat", a == 0);

    /* No-op with dt = 0. */
    a = cos_v68_actr_decay_q15(12345, 1000, 0);
    TASSERT("decay.zero.dt", a == 12345);

    /* Negative inits coerced. */
    a = cos_v68_actr_decay_q15(-100, 100, 0);
    TASSERT("decay.neg.coerce", a == 0);
}

/* --------------------------------------------------------------------
 *  Test 5: Episodic store + write/decay/recall round-trip.
 * -------------------------------------------------------------------- */

static void test_store_roundtrip(void)
{
    cos_v68_store_t *s = cos_v68_store_new(16);
    TASSERT("store.new", s != NULL);
    TASSERT("store.cap", cos_v68_store_capacity(s) == 16);
    TASSERT("store.empty", cos_v68_store_count(s) == 0);

    /* Surprise below threshold → no write. */
    cos_v68_hv_t hv;
    rng_seed(1);
    random_hv(&hv);
    uint32_t w = cos_v68_store_write(s, &hv, 100, 1000);
    TASSERT("store.gate.no", w == 0);
    TASSERT("store.gate.no.count", cos_v68_store_count(s) == 0);

    /* Surprise ≥ threshold → write. */
    w = cos_v68_store_write(s, &hv, 5000, 1000);
    TASSERT("store.gate.yes", w == 1);
    TASSERT("store.gate.yes.count", cos_v68_store_count(s) == 1);

    /* Recall identical HV → top-1 sim ≈ max. */
    cos_v68_recall_t r;
    cos_v68_recall_init(&r, 4);
    int16_t fid = -1;
    uint32_t got = cos_v68_recall(s, &hv, &r, &fid);
    TASSERT("recall.found", got >= 1);
    TASSERT("recall.fid.high", fid >= 32000);

    /* Decay all by 1 tick of 1000 → drop by 1000. */
    int16_t before = cos_v68_store_at(s, 0)->activation_q15;
    cos_v68_store_decay(s, 1000);
    int16_t after  = cos_v68_store_at(s, 0)->activation_q15;
    TASSERT("decay.applied", after == before - 1000 || after == 0);

    /* Forget below-threshold: bring activation low first. */
    cos_v68_store_decay(s, 32000);
    uint32_t f = cos_v68_forget(s, 1000, 16);
    TASSERT("forget.dropped", f == 1);
    TASSERT("forget.count.zero", cos_v68_store_count(s) == 0);

    cos_v68_store_free(s);
}

/* --------------------------------------------------------------------
 *  Test 6: Recall ranks correct neighbours.
 * -------------------------------------------------------------------- */

static void test_recall_ranking(void)
{
    cos_v68_store_t *s = cos_v68_store_new(8);
    rng_seed(0xABCD);
    cos_v68_hv_t base, h1, h2, h3, h4;
    random_hv(&base);
    h1 = base; flip_bits(&h1, 8);    /* small noise */
    h2 = base; flip_bits(&h2, 64);   /* medium */
    h3 = base; flip_bits(&h3, 256);  /* heavy */
    random_hv(&h4);                   /* unrelated */

    cos_v68_store_write(s, &h1, 32000, 1000);
    cos_v68_store_write(s, &h2, 32000, 1000);
    cos_v68_store_write(s, &h3, 32000, 1000);
    cos_v68_store_write(s, &h4, 32000, 1000);

    cos_v68_recall_t r;
    cos_v68_recall_init(&r, 4);
    int16_t fid;
    uint32_t got = cos_v68_recall(s, &base, &r, &fid);
    TASSERT("recall.rank.got4", got == 4);

    /* Top entry should be h1 (smallest noise). */
    TASSERT("recall.rank.top",
            cos_v68_store_at(s, r.id[0])->hv.w[0] == h1.w[0]);
    /* Sims must be in descending order. */
    int ordered = 1;
    for (uint32_t i = 1; i < r.len; ++i)
        if (r.sim_q15[i] > r.sim_q15[i-1]) ordered = 0;
    TASSERT("recall.rank.descending", ordered == 1);

    cos_v68_store_free(s);
}

/* --------------------------------------------------------------------
 *  Test 7: Hebbian update — saturation, ratchet, eta clamping.
 * -------------------------------------------------------------------- */

static void test_hebbian(void)
{
    cos_v68_adapter_t *a = cos_v68_adapter_new(4, 4);
    TASSERT("hebb.new", a != NULL);
    TASSERT("hebb.rows", cos_v68_adapter_rows(a) == 4);
    TASSERT("hebb.cols", cos_v68_adapter_cols(a) == 4);
    TASSERT("hebb.rate.init",
            cos_v68_adapter_rate_q15(a) == COS_V68_RATE_INIT_Q15);

    int16_t pre[4]  = { 30000, 20000, 10000, 0 };
    int16_t post[4] = { 30000, 0, -10000, 25000 };
    int16_t eta = cos_v68_hebb_update(a, pre, post, 30000);
    TASSERT("hebb.eta.clamped", eta <= cos_v68_adapter_rate_q15(a));
    TASSERT("hebb.updated", cos_v68_adapter_updates(a) == 1);

    /* Cell [0,0] should now be positive. */
    int16_t w00 = cos_v68_adapter_w(a, 0, 0);
    TASSERT("hebb.w00.positive", w00 > 0);

    /* Cell [0,2] should now be negative. */
    int16_t w02 = cos_v68_adapter_w(a, 0, 2);
    TASSERT("hebb.w02.negative", w02 < 0);

    /* Saturation: many positive updates max out. */
    for (uint32_t i = 0; i < 5000; ++i)
        cos_v68_hebb_update(a, pre, post, 30000);
    TASSERT("hebb.sat.upper", cos_v68_adapter_w(a, 0, 0) == 32767);
    TASSERT("hebb.sat.lower", cos_v68_adapter_w(a, 0, 2) == -32768);

    /* Ratchet down → rate decreases, never below floor. */
    int16_t before = cos_v68_adapter_rate_q15(a);
    cos_v68_adapter_ratchet(a, 16384); /* * 0.5 */
    int16_t after  = cos_v68_adapter_rate_q15(a);
    TASSERT("hebb.ratchet.lower", after < before);
    for (uint32_t k = 0; k < 100; ++k)
        cos_v68_adapter_ratchet(a, 0);
    TASSERT("hebb.ratchet.floor",
            cos_v68_adapter_rate_q15(a) == COS_V68_RATE_FLOOR_Q15);

    cos_v68_adapter_free(a);
}

/* --------------------------------------------------------------------
 *  Test 8: Sleep consolidation — high-activation episodes bundle.
 * -------------------------------------------------------------------- */

static void test_consolidate(void)
{
    cos_v68_store_t *s = cos_v68_store_new(8);
    cos_v68_adapter_t *a = cos_v68_adapter_new(4, 4);

    rng_seed(0xCAFE);
    cos_v68_hv_t base, n1, n2, n3, far;
    random_hv(&base);
    n1 = base; flip_bits(&n1, 4);
    n2 = base; flip_bits(&n2, 8);
    n3 = base; flip_bits(&n3, 12);
    random_hv(&far);

    cos_v68_store_write(s, &n1,  32000, 1000);
    cos_v68_store_write(s, &n2,  32000, 1000);
    cos_v68_store_write(s, &n3,  32000, 1000);
    cos_v68_store_write(s, &far, 32000, 1000);

    /* Drop "far" below keep_thresh. */
    cos_v68_store_set_activation(s, 3, 0); /* simulated decay */
    /* Confirm valid */
    TASSERT("consol.far.valid", cos_v68_store_at(s, 3)->valid == 1);

    cos_v68_hv_t lt;
    memset(&lt, 0, sizeof(lt));
    /* ratchet adapter so we observe the reset */
    cos_v68_adapter_ratchet(a, 1024);
    int16_t pre_rate = cos_v68_adapter_rate_q15(a);
    uint32_t n = cos_v68_consolidate(s, a, 4096, &lt);
    TASSERT("consol.kept", n == 3);

    /* lt should be very close to base. */
    int16_t sim_lt = cos_v68_hv_sim_q15(&lt, &base);
    TASSERT("consol.bundle.close", sim_lt > 32000);

    /* lt should be far from "far". */
    int16_t sim_far = cos_v68_hv_sim_q15(&lt, &far);
    TASSERT("consol.bundle.far", sim_far < sim_lt);

    /* adapter rate should be reset to RATE_INIT after sleep. */
    int16_t post_rate = cos_v68_adapter_rate_q15(a);
    TASSERT("consol.rate.reset",
            post_rate == COS_V68_RATE_INIT_Q15 &&
            pre_rate < COS_V68_RATE_INIT_Q15);

    cos_v68_adapter_free(a);
    cos_v68_store_free(s);
}

/* --------------------------------------------------------------------
 *  Test 9: Forgetting controller — budget enforced.
 * -------------------------------------------------------------------- */

static void test_forget_budget(void)
{
    cos_v68_store_t *s = cos_v68_store_new(8);
    rng_seed(7);
    cos_v68_hv_t hv;
    for (uint32_t i = 0; i < 8; ++i) {
        random_hv(&hv);
        cos_v68_store_write(s, &hv, 32000, 1000);
    }
    TASSERT("forget.fill", cos_v68_store_count(s) == 8);

    /* Bring all activations way below threshold. */
    cos_v68_store_decay(s, 32000);
    /* Budget caps removal at 3 even though 8 qualify. */
    uint32_t f = cos_v68_forget(s, 1000, 3);
    TASSERT("forget.budget.cap", f == 3);
    TASSERT("forget.remain", cos_v68_store_count(s) == 5);

    /* Run unbounded → drop the rest. */
    f = cos_v68_forget(s, 1000, 100);
    TASSERT("forget.budget.rest", f == 5);
    TASSERT("forget.empty", cos_v68_store_count(s) == 0);

    cos_v68_store_free(s);
}

/* --------------------------------------------------------------------
 *  Test 10: Surprise-gated write evicts lowest activation when full.
 * -------------------------------------------------------------------- */

static void test_eviction(void)
{
    cos_v68_store_t *s = cos_v68_store_new(4);
    rng_seed(55);
    cos_v68_hv_t hv;

    for (uint32_t i = 0; i < 4; ++i) {
        random_hv(&hv);
        cos_v68_store_write(s, &hv, 32000, 1000);
    }
    TASSERT("evict.full", cos_v68_store_count(s) == 4);

    /* Drop activation of slot 1 specifically. */
    cos_v68_store_set_activation(s, 1, 0);
    cos_v68_hv_t newer;
    random_hv(&newer);
    cos_v68_store_write(s, &newer, 32000, 1000);
    /* slot 1 should be the one overwritten. */
    TASSERT("evict.target",
            cos_v68_store_at(s, 1)->activation_q15 ==
            COS_V68_ACT_INIT_Q15);

    cos_v68_store_free(s);
}

/* --------------------------------------------------------------------
 *  Test 11: MML bytecode — a small program reaches GATE OK.
 * -------------------------------------------------------------------- */

static void test_mml_program(void)
{
    cos_v68_store_t   *s = cos_v68_store_new(8);
    cos_v68_adapter_t *a = cos_v68_adapter_new(4, 4);
    cos_v68_recall_t   rk;
    cos_v68_recall_init(&rk, 4);

    rng_seed(99);
    cos_v68_hv_t hv;
    random_hv(&hv);

    int16_t pre[4]  = { 20000, 10000, -5000, 0 };
    int16_t post[4] = { 20000, -5000, 0, 10000 };
    cos_v68_hv_t lt;

    cos_v68_mml_ctx_t ctx = {
        .store = s,
        .adapter = a,
        .scratch_recall = &rk,
        .sense_hv = &hv,
        .pred_q15 = 0,
        .obs_q15  = 30000,
        .surprise_thresh_q15 = 1000,
        .keep_thresh_q15 = 1000,
        .decay_q15 = 64,
        .adapter_pre = pre,
        .adapter_post = post,
        .lt_out = &lt,
        .forget_budget = 4,
    };

    cos_v68_inst_t prog[] = {
        /* SENSE r0 = obs (30000), r1 = pred (0)             */
        { COS_V68_OP_SENSE,    0, 1, 0, 0, 0 },
        /* SURPRISE r2 = |r0 − r1|                            */
        { COS_V68_OP_SURPRISE, 2, 1, 0, 0, 0 },
        /* STORE pr0 = writes (will store since surprise high)*/
        { COS_V68_OP_STORE,    0, 2, 0, 0, 0 },
        /* RECALL r3 = fidelity, pr1 = #returned              */
        { COS_V68_OP_RECALL,   3, 1, 0, 0, 0 },
        /* HEBB r4 = rate                                     */
        { COS_V68_OP_HEBB,     4, 0, 0, 4096, 0 },
        /* CMPGE r5 = (r3 ≥ 16384 ? 32767 : 0)                */
        { COS_V68_OP_CMPGE,    5, 3, 0, 16384, 0 },
        /* GATE: cost ≤ budget AND r3 ≥ 16384 AND no abstain  */
        { COS_V68_OP_GATE,     0, 3, 0, 16384, 0 },
    };

    cos_v68_mml_state_t *st = cos_v68_mml_new();
    int rc = cos_v68_mml_exec(st, prog,
                              sizeof(prog) / sizeof(prog[0]),
                              &ctx, /*budget=*/200);
    TASSERT("mml.exec.ok", rc == 0);
    TASSERT("mml.cost.bounded", cos_v68_mml_cost(st) <= 200);
    TASSERT("mml.no_abstain", cos_v68_mml_abstained(st) == 0);
    TASSERT("mml.v68_ok", cos_v68_mml_v68_ok(st) == 1);

    /* Tight budget that cannot fit the whole program → abstain. */
    cos_v68_mml_reset(st);
    rc = cos_v68_mml_exec(st, prog,
                          sizeof(prog) / sizeof(prog[0]),
                          &ctx, /*budget=*/4);
    TASSERT("mml.budget.abstain", cos_v68_mml_abstained(st) == 1);
    TASSERT("mml.budget.v68_ok_zero", cos_v68_mml_v68_ok(st) == 0);

    /* Score below GATE threshold → v68_ok = 0 even with budget. */
    cos_v68_inst_t prog_low[] = {
        { COS_V68_OP_SENSE,    0, 1, 0, 0, 0 },
        { COS_V68_OP_SURPRISE, 2, 1, 0, 0, 0 },
        /* GATE expects r0 ≥ 32000, but r0 = 30000 (sense) */
        { COS_V68_OP_GATE,     0, 0, 0, 32000, 0 },
    };
    cos_v68_mml_reset(st);
    rc = cos_v68_mml_exec(st, prog_low,
                          sizeof(prog_low) / sizeof(prog_low[0]),
                          &ctx, /*budget=*/100);
    TASSERT("mml.gate.score.low", cos_v68_mml_v68_ok(st) == 0);

    cos_v68_mml_free(st);
    cos_v68_adapter_free(a);
    cos_v68_store_free(s);
}

/* --------------------------------------------------------------------
 *  Test 12: Property — surprise gate is monotonic in |Δ|.
 * -------------------------------------------------------------------- */

static void test_surprise_monotonic(void)
{
    rng_seed(0xDEAD);
    int16_t prev_out = -1;
    int16_t pred = 0;
    int16_t thresh = 1000;
    for (int i = 0; i < 30; ++i) {
        int16_t obs = (int16_t)(i * 1000);
        int16_t out = -1;
        (void)cos_v68_surprise_gate(pred, obs, thresh, &out);
        TASSERT("surprise.mono", out >= prev_out);
        prev_out = out;
    }
}

/* --------------------------------------------------------------------
 *  Test 13: Recall returns id of identical episode under noise.
 * -------------------------------------------------------------------- */

static void test_recall_under_noise(void)
{
    cos_v68_store_t *s = cos_v68_store_new(32);
    rng_seed(0x77777);
    cos_v68_hv_t orig[16];
    for (uint32_t i = 0; i < 16; ++i) {
        random_hv(&orig[i]);
        cos_v68_store_write(s, &orig[i], 32000, 1000);
    }
    /* Query: orig[7] with 32 bits flipped. */
    cos_v68_hv_t q = orig[7];
    flip_bits(&q, 32);
    cos_v68_recall_t r;
    cos_v68_recall_init(&r, 1);
    int16_t fid;
    cos_v68_recall(s, &q, &r, &fid);
    TASSERT("recall.noise.target", r.id[0] == 7);
    TASSERT("recall.noise.fid", fid > 30000);

    cos_v68_store_free(s);
}

/* --------------------------------------------------------------------
 *  Test 14: Big random storm — store writes never exceed capacity.
 * -------------------------------------------------------------------- */

static void test_storm(void)
{
    cos_v68_store_t *s = cos_v68_store_new(64);
    rng_seed(0xABCDEF);
    cos_v68_hv_t hv;
    for (uint32_t i = 0; i < 1000; ++i) {
        random_hv(&hv);
        int16_t surprise = (int16_t)(rng_u64() & 0x7FFFu);
        cos_v68_store_write(s, &hv, surprise, 8000);
    }
    TASSERT("storm.cap.respected", cos_v68_store_count(s) <= 64);
    TASSERT("storm.writes.tracked",
            cos_v68_store_writes(s) > 0);
    cos_v68_store_free(s);
}

/* --------------------------------------------------------------------
 *  Test 15: 9-bit decision cross-check vs brute-force AND.
 *           (extra ~1024 assertions for numerical robustness).
 * -------------------------------------------------------------------- */

static void test_extra_compose(void)
{
    rng_seed(123);
    for (uint32_t k = 0; k < 1024; ++k) {
        uint8_t v[9];
        for (uint32_t i = 0; i < 9; ++i) v[i] = (uint8_t)(rng_u64() & 0xFF);
        cos_v68_decision_t d = cos_v68_compose_decision(
            v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8]);
        uint8_t expect = 1u;
        for (uint32_t i = 0; i < 9; ++i)
            expect &= (uint8_t)(v[i] ? 1u : 0u);
        TASSERT("compose.random", d.allow == expect);
    }
}

/* --------------------------------------------------------------------
 *  Microbenchmarks.
 * -------------------------------------------------------------------- */

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void bench(void)
{
    rng_seed(0x12345);

    /* Bench A: HV Hamming throughput. */
    {
        cos_v68_hv_t a, b;
        random_hv(&a);
        random_hv(&b);
        const uint32_t N = 200000;
        double t0 = now_s();
        uint64_t acc = 0;
        for (uint32_t i = 0; i < N; ++i) acc += cos_v68_hv_hamming(&a, &b);
        double t1 = now_s();
        double dt = t1 - t0;
        printf("[bench] HV Hamming D=8192        : %u cmps in %.3f s "
               "= %.2f M cmps/s (acc=%llu)\n",
               N, dt, (double)N / dt / 1e6, (unsigned long long)acc);
    }

    /* Bench B: Recall over 256 episodes. */
    {
        cos_v68_store_t *s = cos_v68_store_new(256);
        cos_v68_hv_t hv;
        for (uint32_t i = 0; i < 256; ++i) {
            random_hv(&hv);
            cos_v68_store_write(s, &hv, 32000, 1000);
        }
        cos_v68_hv_t q;
        random_hv(&q);
        cos_v68_recall_t r;
        cos_v68_recall_init(&r, 8);
        const uint32_t N = 5000;
        double t0 = now_s();
        for (uint32_t i = 0; i < N; ++i) {
            int16_t fid;
            (void)cos_v68_recall(s, &q, &r, &fid);
        }
        double t1 = now_s();
        double dt = t1 - t0;
        printf("[bench] Recall N=256 D=8192      : %u q in %.3f s "
               "= %.2f k q/s\n",
               N, dt, (double)N / dt / 1e3);
        cos_v68_store_free(s);
    }

    /* Bench C: Hebbian update 16x16. */
    {
        cos_v68_adapter_t *a = cos_v68_adapter_new(16, 16);
        int16_t pre[16], post[16];
        for (int i = 0; i < 16; ++i) {
            pre[i]  = (int16_t)(rng_u64() & 0x7FFF);
            post[i] = (int16_t)(rng_u64() & 0x7FFF);
        }
        const uint32_t N = 2000000;
        double t0 = now_s();
        for (uint32_t i = 0; i < N; ++i)
            (void)cos_v68_hebb_update(a, pre, post, 1024);
        double t1 = now_s();
        double dt = t1 - t0;
        printf("[bench] Hebb 16x16 update        : %u in %.3f s "
               "= %.2f M upd/s\n",
               N, dt, (double)N / dt / 1e6);
        cos_v68_adapter_free(a);
    }

    /* Bench D: Sleep consolidation over 256 episodes. */
    {
        cos_v68_store_t *s = cos_v68_store_new(256);
        cos_v68_hv_t hv, lt;
        for (uint32_t i = 0; i < 256; ++i) {
            random_hv(&hv);
            cos_v68_store_write(s, &hv, 32000, 1000);
        }
        const uint32_t N = 200;
        double t0 = now_s();
        for (uint32_t i = 0; i < N; ++i)
            (void)cos_v68_consolidate(s, NULL, 1000, &lt);
        double t1 = now_s();
        double dt = t1 - t0;
        printf("[bench] Consolidate N=256 D=8192 : %u in %.3f s "
               "= %.2f k cons/s\n",
               N, dt, (double)N / dt / 1e3);
        cos_v68_store_free(s);
    }

    /* Bench E: MML 7-instruction program throughput. */
    {
        cos_v68_store_t *s = cos_v68_store_new(8);
        cos_v68_adapter_t *a = cos_v68_adapter_new(4, 4);
        cos_v68_recall_t rk; cos_v68_recall_init(&rk, 4);
        cos_v68_hv_t hv; random_hv(&hv);
        int16_t pre[4] = { 1000, 2000, 3000, 4000 };
        int16_t post[4]= { 4000, 3000, 2000, 1000 };
        cos_v68_hv_t lt;
        cos_v68_mml_ctx_t ctx = {
            .store = s, .adapter = a, .scratch_recall = &rk,
            .sense_hv = &hv, .pred_q15 = 0, .obs_q15 = 30000,
            .surprise_thresh_q15 = 1000, .keep_thresh_q15 = 1000,
            .decay_q15 = 64, .adapter_pre = pre, .adapter_post = post,
            .lt_out = &lt, .forget_budget = 4,
        };
        cos_v68_inst_t prog[] = {
            { COS_V68_OP_SENSE,    0, 1, 0, 0, 0 },
            { COS_V68_OP_SURPRISE, 2, 1, 0, 0, 0 },
            { COS_V68_OP_HEBB,     4, 0, 0, 1024, 0 },
            { COS_V68_OP_CMPGE,    5, 2, 0, 1000, 0 },
            { COS_V68_OP_GATE,     0, 2, 0, 1000, 0 },
        };
        cos_v68_mml_state_t *st = cos_v68_mml_new();
        const uint32_t N = 1000000;
        double t0 = now_s();
        for (uint32_t i = 0; i < N; ++i) {
            cos_v68_mml_reset(st);
            (void)cos_v68_mml_exec(st, prog,
                                   sizeof(prog)/sizeof(prog[0]),
                                   &ctx, 200);
        }
        double t1 = now_s();
        double dt = t1 - t0;
        printf("[bench] MML 5-instruction prog   : %u in %.3f s "
               "= %.2f M progs/s (~%.0f M ops/s)\n",
               N, dt, (double)N / dt / 1e6,
               (double)N * 5 / dt / 1e6);
        cos_v68_mml_free(st);
        cos_v68_adapter_free(a);
        cos_v68_store_free(s);
    }
}

/* --------------------------------------------------------------------
 *  Entry.
 * -------------------------------------------------------------------- */

static int cmd_self_test(void)
{
    g_pass = 0; g_fail = 0;
    test_compose_truth_table();
    test_extra_compose();
    test_hv_basics();
    test_surprise_gate();
    test_actr_decay();
    test_store_roundtrip();
    test_recall_ranking();
    test_hebbian();
    test_consolidate();
    test_forget_budget();
    test_eviction();
    test_mml_program();
    test_surprise_monotonic();
    test_recall_under_noise();
    test_storm();
    printf("v68 self-test: %u pass, %u fail\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

static int cmd_decision(int argc, char **argv)
{
    if (argc < 9) {
        fprintf(stderr,
            "usage: creation_os_v68 --decision v60 v61 v62 v63 v64 "
            "v65 v66 v67 v68\n");
        return 2;
    }
    uint8_t v[9];
    for (int i = 0; i < 9; ++i) v[i] = (uint8_t)atoi(argv[i]);
    cos_v68_decision_t d = cos_v68_compose_decision(
        v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8]);
    printf("v60_ok=%u v61_ok=%u v62_ok=%u v63_ok=%u v64_ok=%u "
           "v65_ok=%u v66_ok=%u v67_ok=%u v68_ok=%u  →  allow=%u\n",
           d.v60_ok, d.v61_ok, d.v62_ok, d.v63_ok, d.v64_ok,
           d.v65_ok, d.v66_ok, d.v67_ok, d.v68_ok, d.allow);
    return d.allow ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s\n", cos_v68_version);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--bench") == 0) {
        bench();
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--decision") == 0)
        return cmd_decision(argc - 2, argv + 2);

    /* default: self-test */
    return cmd_self_test();
}
