/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v73 — σ-Omnimodal (Creator) — standalone driver.
 *
 * Modes:
 *    --self-test       deterministic self-tests (target ≥ 200 000 pass)
 *    --bench           microbenchmarks
 *    --decision v60 v61 ... v73     one-shot 14-bit composed decision
 *
 * Branchless integer-only hot path.  Libc-only.  Target: Apple M4 /
 * aarch64; runs anywhere clang / GCC with C11 exists.
 */

#include "omnimodal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ====================================================================
 *  Accounting.
 * ==================================================================== */

static uint64_t g_pass = 0;
static uint64_t g_fail = 0;

#define CHECK(cond, label) do { \
    if (cond) { g_pass++; } \
    else      { g_fail++; fprintf(stderr, "FAIL: %s (%s:%d)\n", label, __FILE__, __LINE__); } \
} while (0)

/* ====================================================================
 *  Timing.
 * ==================================================================== */

static double now_sec(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

/* ====================================================================
 *  Self-tests.
 * ==================================================================== */

static void t_hv_primitives(void) {
    cos_v73_hv_t a, b, c;
    cos_v73_hv_from_seed(&a, 1);
    cos_v73_hv_from_seed(&b, 2);
    cos_v73_hv_xor(&c, &a, &b);
    CHECK(c.w[0] == (a.w[0] ^ b.w[0]), "hv_xor word0");
    CHECK(c.w[1] == (a.w[1] ^ b.w[1]), "hv_xor word1");
    CHECK(c.w[2] == (a.w[2] ^ b.w[2]), "hv_xor word2");
    CHECK(c.w[3] == (a.w[3] ^ b.w[3]), "hv_xor word3");

    /* Self-inverse */
    cos_v73_hv_t rr;
    cos_v73_hv_xor(&rr, &c, &b);
    CHECK(rr.w[0] == a.w[0] && rr.w[3] == a.w[3], "hv_xor involution");

    /* Hamming(x, x) = 0 */
    CHECK(cos_v73_hv_hamming(&a, &a) == 0, "hamming self-zero");
    /* Hamming(x, y) ≤ 256 */
    CHECK(cos_v73_hv_hamming(&a, &b) <= 256u, "hamming bounded");

    /* Permute determinism */
    cos_v73_hv_t p1, p2;
    cos_v73_hv_permute(&p1, &a, COS_V73_MOD_VIDEO);
    cos_v73_hv_permute(&p2, &a, COS_V73_MOD_VIDEO);
    CHECK(p1.w[0] == p2.w[0] && p1.w[3] == p2.w[3], "permute deterministic");
    cos_v73_hv_t p3;
    cos_v73_hv_permute(&p3, &a, COS_V73_MOD_AUDIO);
    CHECK(p1.w[0] != p3.w[0] || p1.w[1] != p3.w[1], "permute modality-sensitive");
}

static void t_tokenizer(void) {
    cos_v73_codebook_t *cb = cos_v73_codebook_new(256, COS_V73_MOD_IMAGE, 0xDEADBEEF);
    CHECK(cb != NULL, "codebook_new ok");
    CHECK(cb->n == 256, "codebook n");
    /* Tokenise a known codeword → should return itself. */
    cos_v73_hv_t recon;
    uint32_t idx = cos_v73_tokenize(cb, &cb->book[42], &recon);
    CHECK(idx == 42, "tokenize exact-hit idx");
    CHECK(recon.w[0] == cb->book[42].w[0], "tokenize exact-hit recon");

    /* RVQ on a codeword gives zero residual. */
    uint32_t rvq = 999;
    cos_v73_hv_t r2;
    idx = cos_v73_tokenize_rvq(cb, &cb->book[7], &rvq, &r2);
    CHECK(idx == 7, "rvq stage1 idx");
    /* rvq stage2 should index the zero-vector's nearest codeword deterministically */
    CHECK(rvq < 256, "rvq stage2 bounded");

    cos_v73_codebook_free(cb);
}

static void t_flow(void) {
    cos_v73_flow_t f;
    cos_v73_flow_init(&f, 16, 16, 0xA5A5);
    cos_v73_hv_t cond, out1, out2;
    cos_v73_hv_from_seed(&cond, 42);
    uint32_t u1 = 0, u2 = 0;
    uint8_t ok1 = cos_v73_flow_run(&f, &cond, &out1, &u1);
    uint8_t ok2 = cos_v73_flow_run(&f, &cond, &out2, &u2);
    CHECK(ok1 == 1, "flow under budget");
    CHECK(ok2 == 1, "flow under budget (2)");
    CHECK(u1 == 16 && u2 == 16, "flow units == steps");
    CHECK(out1.w[0] == out2.w[0] && out1.w[3] == out2.w[3], "flow deterministic");

    /* Budget too tight */
    cos_v73_flow_t ft;
    cos_v73_flow_init(&ft, 32, 16, 0xA5A5);
    uint32_t ut = 0;
    cos_v73_hv_t outt;
    uint8_t okt = cos_v73_flow_run(&ft, &cond, &outt, &ut);
    CHECK(okt == 0, "flow over budget");
}

static void t_bind(void) {
    cos_v73_hv_t h_text, h_img, h_vid;
    cos_v73_hv_from_seed(&h_text, 10);
    cos_v73_hv_from_seed(&h_img,  20);
    cos_v73_hv_from_seed(&h_vid,  30);
    cos_v73_mod_input_t ins[3] = {
        { &h_text, COS_V73_MOD_TEXT  },
        { &h_img,  COS_V73_MOD_IMAGE },
        { &h_vid,  COS_V73_MOD_VIDEO },
    };
    cos_v73_hv_t claim;
    cos_v73_bind(&claim, ins, 3);
    CHECK(cos_v73_bind_verify(ins, 3, &claim, 0) == 1, "bind exact verify");

    /* Small perturbation: flip one bit of claim, should still verify with tol. */
    cos_v73_hv_t claim2 = claim;
    claim2.w[0] ^= 0x1ULL;
    CHECK(cos_v73_bind_verify(ins, 3, &claim2, 4) == 1, "bind tol=4 verify");
    CHECK(cos_v73_bind_verify(ins, 3, &claim2, 0) == 0, "bind tol=0 rejects");
}

static void t_av_lock(void) {
    const uint32_t L = 8;
    cos_v73_hv_t video[8], audio[8];
    for (uint32_t i = 0; i < L; ++i) {
        cos_v73_hv_from_seed(&video[i], 100 + i);
        /* Audio = video XOR tiny noise to keep Hamming low. */
        cos_v73_hv_copy(&audio[i], &video[i]);
        audio[i].w[0] ^= (uint64_t)0x1u;
    }
    cos_v73_av_lock_t lock = {
        .video = video, .audio = audio, .reel_len = L,
        .lip_lag_max = 256, .fx_thresh = 64, .tempo_err_max = 64,
    };
    uint32_t lip, fx, tempo;
    uint8_t ok = cos_v73_av_lock_verify(&lock, &lip, &fx, &tempo);
    CHECK(ok == 1, "av_lock close pair");
    CHECK(fx <= 8, "av_lock fx low");

    /* Very loose audio: should fail fx threshold. */
    for (uint32_t i = 0; i < L; ++i) cos_v73_hv_from_seed(&audio[i], 999 + i);
    ok = cos_v73_av_lock_verify(&lock, NULL, NULL, NULL);
    CHECK(ok == 0, "av_lock rejects uncorrelated");
}

static void t_world(void) {
    cos_v73_world_t *w = cos_v73_world_new(64, 0xBADC0DE);
    CHECK(w != NULL, "world_new ok");
    cos_v73_hv_t action;
    cos_v73_hv_from_seed(&action, 77);
    /* Retrieve an existing tile returns itself with distance 0. */
    uint32_t dist = 999;
    uint32_t idx = cos_v73_world_retrieve(w, &w->tiles[5], &dist);
    CHECK(idx == 5, "world_retrieve self");
    CHECK(dist == 0, "world_retrieve distance 0");

    /* Step changes the target tile. */
    cos_v73_hv_t before;
    cos_v73_hv_copy(&before, &w->tiles[3]);
    cos_v73_world_step(w, &action, 3);
    CHECK(w->camera_idx == 3, "world_step camera");
    CHECK(w->tiles[3].w[0] != before.w[0] || w->tiles[3].w[1] != before.w[1], "world_step mutates tile");

    cos_v73_world_free(w);
}

static void t_physics(void) {
    CHECK(cos_v73_physics_gate(10, 100, 50, 100) == 1, "physics allow");
    CHECK(cos_v73_physics_gate(-50, 100, 50, 100) == 1, "physics allow (neg)");
    CHECK(cos_v73_physics_gate(200, 100, 50, 100) == 0, "physics reject momentum");
    CHECK(cos_v73_physics_gate(10, 100, 500, 100) == 0, "physics reject collision");
}

static void t_workflow(void) {
    /* Build a 6-node DAG: 0 → 1 → 3, 0 → 2 → 3, 3 → 4 → 5. */
    cos_v73_workflow_t *wf = cos_v73_workflow_new(6);
    CHECK(wf != NULL, "workflow_new");
    cos_v73_workflow_add_edge(wf, 0, 1);
    cos_v73_workflow_add_edge(wf, 0, 2);
    cos_v73_workflow_add_edge(wf, 1, 3);
    cos_v73_workflow_add_edge(wf, 2, 3);
    cos_v73_workflow_add_edge(wf, 3, 4);
    cos_v73_workflow_add_edge(wf, 4, 5);

    uint32_t score[6] = {0};
    uint8_t ok = cos_v73_workflow_execute(wf, 16, 0, score);
    CHECK(ok == 1, "workflow_execute ok");
    CHECK(wf->faults == 0, "workflow zero faults");
    /* score[3] should be ≥ 2 (had 2 predecessors). */
    CHECK(score[3] >= 2, "workflow judgeflow score");

    /* Add a cycle: 5 → 0 → bad: can't complete. */
    cos_v73_workflow_add_edge(wf, 5, 0);
    ok = cos_v73_workflow_execute(wf, 16, 0, NULL);
    CHECK(ok == 0, "workflow cycle rejected");

    cos_v73_workflow_free(wf);
}

static void t_plan(void) {
    cos_v73_plan_graph_t *g = cos_v73_plan_new(8);
    CHECK(g != NULL, "plan_new");
    cos_v73_plan_add_dep(g, 0, 1);
    cos_v73_plan_add_dep(g, 1, 2);
    cos_v73_plan_add_dep(g, 3, 4);

    cos_v73_plan_step_t steps[3];
    for (uint32_t i = 0; i < 3; ++i) {
        steps[i].file_idx = i;
        steps[i].repair_count = 0;
        cos_v73_hv_from_seed(&steps[i].diff_tag, 200 + i);
    }
    cos_v73_hv_t manifest;
    cos_v73_hv_zero(&manifest);
    for (uint32_t i = 0; i < 3; ++i)
        cos_v73_hv_xor(&manifest, &manifest, &steps[i].diff_tag);

    CHECK(cos_v73_plan_verify(g, steps, 3, 0, &manifest, 0) == 1, "plan exact manifest");

    /* Out-of-bounds file rejects. */
    steps[0].file_idx = 999;
    CHECK(cos_v73_plan_verify(g, steps, 3, 0, &manifest, 0) == 0, "plan OOB rejects");
    steps[0].file_idx = 0;

    /* Repair over budget rejects. */
    steps[1].repair_count = 10;
    CHECK(cos_v73_plan_verify(g, steps, 3, 5, &manifest, 0) == 0, "plan repair-over rejects");
    steps[1].repair_count = 0;

    /* Cycle rejects. */
    cos_v73_plan_add_dep(g, 2, 0);
    CHECK(cos_v73_plan_verify(g, steps, 3, 0, &manifest, 0) == 0, "plan cycle rejects");

    cos_v73_plan_free(g);
}

static void t_asset_nav(void) {
    cos_v73_world_t *w = cos_v73_world_new(64, 0x1234);
    cos_v73_hv_t anchor, target;
    cos_v73_hv_copy(&anchor, &w->tiles[0]);
    cos_v73_hv_copy(&target, &w->tiles[33]);
    cos_v73_hv_t path;
    uint32_t hops;
    uint8_t ok = cos_v73_asset_navigate(w, &anchor, &target, 8, &path, &hops);
    CHECK(ok == 1, "asset_navigate arrive");
    CHECK(hops <= 8, "asset_navigate bounded");
    cos_v73_world_free(w);
}

static void t_oml(void) {
    cos_v73_codebook_t *cb = cos_v73_codebook_new(64, COS_V73_MOD_CODE, 42);
    cos_v73_flow_t flow;
    cos_v73_flow_init(&flow, 8, 16, 0xBEEF);

    cos_v73_hv_t h0, h1;
    cos_v73_hv_from_seed(&h0, 1);
    cos_v73_hv_from_seed(&h1, 2);
    cos_v73_mod_input_t bi[2] = {
        { &h0, COS_V73_MOD_TEXT },
        { &h1, COS_V73_MOD_CODE },
    };
    cos_v73_hv_t claim;
    cos_v73_bind(&claim, bi, 2);

    const uint32_t L = 4;
    cos_v73_hv_t video[4], audio[4];
    for (uint32_t i = 0; i < L; ++i) {
        cos_v73_hv_from_seed(&video[i], 111 + i);
        cos_v73_hv_copy(&audio[i], &video[i]);
    }
    cos_v73_av_lock_t lock = {
        .video = video, .audio = audio, .reel_len = L,
        .lip_lag_max = 256, .fx_thresh = 4, .tempo_err_max = 4,
    };

    cos_v73_world_t *w = cos_v73_world_new(16, 0xC0FFEE);
    cos_v73_hv_t action;
    cos_v73_hv_from_seed(&action, 99);

    cos_v73_workflow_t *wf = cos_v73_workflow_new(4);
    cos_v73_workflow_add_edge(wf, 0, 1);
    cos_v73_workflow_add_edge(wf, 1, 2);
    cos_v73_workflow_add_edge(wf, 2, 3);

    cos_v73_plan_graph_t *pg = cos_v73_plan_new(4);
    cos_v73_plan_step_t steps[2] = {
        { 0, 0, { { 0, 0, 0, 0 } } },
        { 1, 0, { { 0, 0, 0, 0 } } },
    };
    cos_v73_hv_from_seed(&steps[0].diff_tag, 500);
    cos_v73_hv_from_seed(&steps[1].diff_tag, 501);
    cos_v73_hv_t pclaim;
    cos_v73_hv_zero(&pclaim);
    cos_v73_hv_xor(&pclaim, &pclaim, &steps[0].diff_tag);
    cos_v73_hv_xor(&pclaim, &pclaim, &steps[1].diff_tag);

    cos_v73_oml_ctx_t ctx = {0};
    ctx.cb            = cb;
    ctx.tok_input     = &h0;
    ctx.flow          = &flow;
    ctx.flow_cond     = &h0;
    ctx.bind_inputs   = bi;
    ctx.bind_n        = 2;
    ctx.bind_claim    = &claim;
    ctx.bind_tol      = 0;
    ctx.lock          = &lock;
    ctx.world         = w;
    ctx.world_action  = &action;
    ctx.world_next_cam = 3;
    ctx.phys_dmom     = 1;
    ctx.phys_dmom_tol = 100;
    ctx.phys_ce       = 1;
    ctx.phys_ce_tol   = 100;
    ctx.workflow      = wf;
    ctx.workflow_rounds = 16;
    ctx.workflow_faults = 0;
    ctx.plan_graph    = pg;
    ctx.plan_steps    = steps;
    ctx.plan_n        = 2;
    ctx.plan_repair   = 0;
    ctx.plan_claim    = &pclaim;
    ctx.plan_tol      = 0;
    ctx.abstain       = 0;

    cos_v73_oml_inst_t prog[] = {
        { COS_V73_OP_TOKENIZE, 0, 0, 0, 0, 0 },
        { COS_V73_OP_FLOW,     0, 0, 0, 0, 0 },
        { COS_V73_OP_BIND,     0, 0, 0, 0, 0 },
        { COS_V73_OP_LOCK,     0, 0, 0, 0, 0 },
        { COS_V73_OP_WORLD,    0, 0, 0, 0, 0 },
        { COS_V73_OP_PHYSICS,  0, 0, 0, 0, 0 },
        { COS_V73_OP_WORKFLOW, 0, 0, 0, 0, 0 },
        { COS_V73_OP_PLAN,     0, 0, 0, 0, 0 },
        { COS_V73_OP_GATE,     0, 0, 0, 0, 0 },
        { COS_V73_OP_HALT,     0, 0, 0, 0, 0 },
    };

    cos_v73_oml_state_t *s = cos_v73_oml_new();
    int rc = cos_v73_oml_exec(s, prog, sizeof(prog)/sizeof(prog[0]), &ctx, 1024);
    CHECK(rc == 0, "oml exec ok");
    CHECK(cos_v73_oml_flow_ok(s)     == 1, "oml flow_ok");
    CHECK(cos_v73_oml_bind_ok(s)     == 1, "oml bind_ok");
    CHECK(cos_v73_oml_lock_ok(s)     == 1, "oml lock_ok");
    CHECK(cos_v73_oml_physics_ok(s)  == 1, "oml physics_ok");
    CHECK(cos_v73_oml_plan_ok(s)     == 1, "oml plan_ok");
    CHECK(cos_v73_oml_workflow_ok(s) == 1, "oml workflow_ok");
    CHECK(cos_v73_oml_abstained(s)   == 0, "oml not abstained");
    CHECK(cos_v73_oml_v73_ok(s)      == 1, "oml v73_ok");

    /* Abstain kills the gate. */
    ctx.abstain = 1;
    cos_v73_oml_exec(s, prog, sizeof(prog)/sizeof(prog[0]), &ctx, 1024);
    CHECK(cos_v73_oml_v73_ok(s) == 0, "oml abstain -> 0");

    /* Over-budget. */
    ctx.abstain = 0;
    rc = cos_v73_oml_exec(s, prog, sizeof(prog)/sizeof(prog[0]), &ctx, 4);
    CHECK(rc == -2, "oml budget exceeded");

    cos_v73_oml_free(s);
    cos_v73_plan_free(pg);
    cos_v73_workflow_free(wf);
    cos_v73_world_free(w);
    cos_v73_codebook_free(cb);
}

/* The composed-decision 14-bit truth table: 2^14 = 16 384 rows ×
 * 15 CHECKs per row ≈ 245 760 pass counts. */
static void t_compose_truth_table(void) {
    for (uint32_t mask = 0; mask < (1u << 14); ++mask) {
        uint8_t b[14];
        for (uint32_t i = 0; i < 14; ++i) b[i] = (mask >> i) & 0x1u;
        cos_v73_decision_t d = cos_v73_compose_decision(
            b[0], b[1], b[2], b[3], b[4], b[5], b[6],
            b[7], b[8], b[9], b[10], b[11], b[12], b[13]);
        CHECK(d.v60_ok == b[0], "truth v60");
        CHECK(d.v61_ok == b[1], "truth v61");
        CHECK(d.v62_ok == b[2], "truth v62");
        CHECK(d.v63_ok == b[3], "truth v63");
        CHECK(d.v64_ok == b[4], "truth v64");
        CHECK(d.v65_ok == b[5], "truth v65");
        CHECK(d.v66_ok == b[6], "truth v66");
        CHECK(d.v67_ok == b[7], "truth v67");
        CHECK(d.v68_ok == b[8], "truth v68");
        CHECK(d.v69_ok == b[9], "truth v69");
        CHECK(d.v70_ok == b[10], "truth v70");
        CHECK(d.v71_ok == b[11], "truth v71");
        CHECK(d.v72_ok == b[12], "truth v72");
        CHECK(d.v73_ok == b[13], "truth v73");
        uint8_t expected = (uint8_t)(b[0] & b[1] & b[2] & b[3] & b[4] & b[5] & b[6]
                                   & b[7] & b[8] & b[9] & b[10] & b[11] & b[12] & b[13]);
        CHECK(d.allow == expected, "truth allow");
    }
}

/* ====================================================================
 *  Benchmarks.
 * ==================================================================== */

static void bench_all(void) {
    printf("v73 σ-Omnimodal microbench\n");
    printf("==========================\n");

    /* Hamming throughput */
    cos_v73_hv_t a, b;
    cos_v73_hv_from_seed(&a, 1);
    cos_v73_hv_from_seed(&b, 2);
    const uint64_t N_HAM = 5000000ULL;
    double t0 = now_sec();
    uint64_t acc = 0;
    for (uint64_t i = 0; i < N_HAM; ++i) {
        acc += cos_v73_hv_hamming(&a, &b);
        a.w[0]++;
    }
    double t1 = now_sec();
    double gbs = (double)(N_HAM * 32 * 2) / (t1 - t0) / 1e9;
    printf("  hamming(256b):         %10.2f M ops/s (acc=%llu, %.2f GB/s)\n",
           (double)N_HAM / (t1 - t0) / 1e6, (unsigned long long)acc, gbs);

    /* Tokenise */
    cos_v73_codebook_t *cb = cos_v73_codebook_new(512, COS_V73_MOD_IMAGE, 7);
    cos_v73_hv_t q; cos_v73_hv_from_seed(&q, 123);
    const uint64_t N_TOK = 200000ULL;
    t0 = now_sec();
    uint64_t sum = 0;
    for (uint64_t i = 0; i < N_TOK; ++i) {
        sum += cos_v73_tokenize(cb, &q, NULL);
        q.w[1]++;
    }
    t1 = now_sec();
    printf("  tokenize (N=512):      %10.2f K ops/s (sum=%llu)\n",
           (double)N_TOK / (t1 - t0) / 1e3, (unsigned long long)sum);
    cos_v73_codebook_free(cb);

    /* Flow run */
    cos_v73_flow_t f; cos_v73_flow_init(&f, 32, 32, 7);
    const uint64_t N_FLOW = 500000ULL;
    t0 = now_sec();
    uint64_t cnt = 0;
    cos_v73_hv_t cond; cos_v73_hv_from_seed(&cond, 999);
    for (uint64_t i = 0; i < N_FLOW; ++i) {
        uint32_t u = 0; cos_v73_hv_t out;
        cnt += cos_v73_flow_run(&f, &cond, &out, &u);
        cond.w[2]++;
    }
    t1 = now_sec();
    printf("  flow (32 steps):       %10.2f K runs/s (cnt=%llu)\n",
           (double)N_FLOW / (t1 - t0) / 1e3, (unsigned long long)cnt);

    /* Composed decision */
    const uint64_t N_DEC = 10000000ULL;
    t0 = now_sec();
    uint64_t allowed = 0;
    for (uint64_t i = 0; i < N_DEC; ++i) {
        cos_v73_decision_t d = cos_v73_compose_decision(
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, (uint8_t)(i & 1u));
        allowed += d.allow;
    }
    t1 = now_sec();
    printf("  14-bit composed:       %10.2f M ops/s (allow=%llu)\n",
           (double)N_DEC / (t1 - t0) / 1e6, (unsigned long long)allowed);

    /* Workflow execute */
    cos_v73_workflow_t *wf = cos_v73_workflow_new(32);
    for (uint32_t i = 0; i < 31; ++i) cos_v73_workflow_add_edge(wf, i, i + 1);
    const uint64_t N_WF = 10000ULL;
    t0 = now_sec();
    uint64_t okc = 0;
    for (uint64_t i = 0; i < N_WF; ++i) {
        okc += cos_v73_workflow_execute(wf, 64, 0, NULL);
    }
    t1 = now_sec();
    printf("  workflow (32 nodes):   %10.2f K runs/s (ok=%llu)\n",
           (double)N_WF / (t1 - t0) / 1e3, (unsigned long long)okc);
    cos_v73_workflow_free(wf);
}

/* ====================================================================
 *  Entry.
 * ==================================================================== */

static int self_test(void) {
    t_hv_primitives();
    t_tokenizer();
    t_flow();
    t_bind();
    t_av_lock();
    t_world();
    t_physics();
    t_workflow();
    t_plan();
    t_asset_nav();
    t_oml();
    t_compose_truth_table();

    printf("v73 σ-Omnimodal self-test: %llu pass, %llu fail\n",
           (unsigned long long)g_pass, (unsigned long long)g_fail);
    return g_fail == 0 ? 0 : 1;
}

static int decision_mode(int argc, char **argv) {
    if (argc < 16) {
        fprintf(stderr, "usage: %s --decision v60 v61 v62 v63 v64 v65 v66 v67 v68 v69 v70 v71 v72 v73\n", argv[0]);
        return 2;
    }
    uint8_t b[14];
    for (int i = 0; i < 14; ++i) b[i] = (uint8_t)(atoi(argv[2 + i]) != 0);
    cos_v73_decision_t d = cos_v73_compose_decision(
        b[0], b[1], b[2], b[3], b[4], b[5], b[6],
        b[7], b[8], b[9], b[10], b[11], b[12], b[13]);
    printf("{ \"v60\":%u,\"v61\":%u,\"v62\":%u,\"v63\":%u,\"v64\":%u,\"v65\":%u,"
           "\"v66\":%u,\"v67\":%u,\"v68\":%u,\"v69\":%u,\"v70\":%u,\"v71\":%u,"
           "\"v72\":%u,\"v73\":%u,\"allow\":%u }\n",
           d.v60_ok, d.v61_ok, d.v62_ok, d.v63_ok, d.v64_ok, d.v65_ok,
           d.v66_ok, d.v67_ok, d.v68_ok, d.v69_ok, d.v70_ok, d.v71_ok,
           d.v72_ok, d.v73_ok, d.allow);
    return d.allow ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) return self_test();
    if (argc >= 2 && strcmp(argv[1], "--bench")     == 0) { bench_all(); return 0; }
    if (argc >= 2 && strcmp(argv[1], "--decision")  == 0) return decision_mode(argc, argv);

    printf("Creation OS %s\n", cos_v73_version);
    printf("  --self-test    run deterministic self-tests\n");
    printf("  --bench        run microbenchmarks\n");
    printf("  --decision v60 v61 ... v73   14-bit composed decision (JSON)\n");
    return 0;
}
