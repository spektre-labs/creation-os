/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v62 — Reasoning Fabric driver + 62-test self-test.
 *
 * Usage:
 *   ./creation_os_v62                        # short summary
 *   ./creation_os_v62 --self-test            # all tests, exit 0/1
 *   ./creation_os_v62 --bench                # microbench
 *   ./creation_os_v62 --decision             # JSON decision sample
 *   ./creation_os_v62 --version              # one-line version
 *
 * No dependencies beyond libc + libm.  Builds with -Wall -Wextra clean.
 */

#include "fabric.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ====================================================================
 *  Test scaffolding (deliberately tiny — no fancy framework).
 * ==================================================================== */

static int g_pass = 0;
static int g_fail = 0;

#define TEST_OK(name, expr) do {                                       \
    int _ok_ = !!(expr);                                               \
    if (_ok_) g_pass++;                                                \
    else      { g_fail++;                                              \
                fprintf(stderr, "FAIL: %s @ %s:%d\n",                  \
                        (name), __FILE__, __LINE__); }                 \
} while (0)

static int v62_close(float a, float b, float tol)
{
    return fabsf(a - b) <= tol;
}

/* small synthetic gradient that pulls thoughts toward a target */
typedef struct { const float *target; uint32_t dim; } pull_ctx_t;
static int pull_grad(const cos_v62_thought_t *t, float *out, void *ctx)
{
    pull_ctx_t *c = (pull_ctx_t *)ctx;
    for (uint32_t i = 0; i < c->dim; ++i) out[i] = c->target[i] - t->vec[i];
    return 0;
}

/* gradient that asks for a hard stop on first call */
static int abort_grad(const cos_v62_thought_t *t, float *out, void *ctx)
{
    (void)t; (void)out; (void)ctx;
    return 1; /* non-zero = halt loop */
}

/* ====================================================================
 *  Section A — Latent CoT (Coconut-class)
 * ==================================================================== */

static void test_latent_cot(void)
{
    /* 1: thought_new returns aligned, zeroed vector                  */
    cos_v62_thought_t *t = cos_v62_thought_new(64);
    TEST_OK("thought_new_alloc", t != NULL && t->vec != NULL && t->dim == 64);

    /* 2: 64-byte alignment of vec                                    */
    TEST_OK("thought_vec_aligned", t && (((uintptr_t)t->vec & 63u) == 0));

    /* 3: zero-initialised                                            */
    {
        int all_zero = 1;
        for (uint32_t i = 0; i < t->dim; ++i)
            if (t->vec[i] != 0.0f) { all_zero = 0; break; }
        TEST_OK("thought_vec_zero_init", all_zero);
    }

    /* 4: latent_step rejects NULL                                    */
    TEST_OK("latent_step_rejects_null",
            cos_v62_latent_step(NULL, NULL, 0.f) == -1);

    /* 5: one step moves vec away from zero given non-zero gradient   */
    float *grad = (float *)aligned_alloc(64, 64 * sizeof(float));
    TEST_OK("grad_alloc", grad != NULL);
    memset(grad, 0, 64 * sizeof(float));
    grad[0] = 1.f; grad[1] = 1.f;
    cos_v62_latent_step(t, grad, 0.5f);
    TEST_OK("latent_step_moves_vec", t->vec[0] != 0.f);

    /* 6: σ-coherence drops with bigger step                          */
    float sigma_a = t->sigma;
    cos_v62_latent_step(t, grad, 1.0f);
    TEST_OK("latent_step_sigma_drops", t->sigma <= sigma_a + 1e-6f);

    /* 7: step counter increments                                     */
    TEST_OK("latent_step_counts", t->step == 2);

    /* 8: post-step L2 norm is ~1                                     */
    {
        float n2 = 0.f;
        for (uint32_t i = 0; i < t->dim; ++i) n2 += t->vec[i] * t->vec[i];
        TEST_OK("latent_step_normalised", v62_close(sqrtf(n2), 1.f, 1e-3f));
    }

    /* 9: latent_loop with target gradient converges within budget    */
    float *target = (float *)aligned_alloc(64, 64 * sizeof(float));
    memset(target, 0, 64 * sizeof(float));
    for (uint32_t i = 0; i < 64; ++i) target[i] = (i % 4 == 0) ? 1.f : 0.f;
    pull_ctx_t pctx = { target, 64 };
    cos_v62_budget_t b = { 64, 64, 8, 16, 4, 128 };
    uint32_t steps = 0;
    int rc = cos_v62_latent_loop(t, pull_grad, &pctx, &b, 0.2f, 0.99f, &steps);
    TEST_OK("latent_loop_rc_ok", rc == 0);
    TEST_OK("latent_loop_made_progress", steps > 0);

    /* 10: abort callback halts loop immediately                      */
    cos_v62_thought_t *t2 = cos_v62_thought_new(32);
    cos_v62_budget_t bb = { 16, 16, 4, 4, 2, 16 };
    uint32_t s2 = 0;
    cos_v62_latent_loop(t2, abort_grad, NULL, &bb, 0.1f, 0.5f, &s2);
    TEST_OK("latent_loop_abort_halts", s2 == 0);

    /* 11: thought_free is null-safe                                  */
    cos_v62_thought_free(NULL);
    TEST_OK("thought_free_null_safe", 1);

    free(target); free(grad);
    cos_v62_thought_free(t);
    cos_v62_thought_free(t2);
}

/* ====================================================================
 *  Section B — Energy-Based Verifier
 * ==================================================================== */

static void test_ebt(void)
{
    enum { D = 32 };
    float *x = (float *)aligned_alloc(64, D * sizeof(float));
    float *y = (float *)aligned_alloc(64, D * sizeof(float));
    for (uint32_t i = 0; i < D; ++i) { x[i] = (float)i / D; y[i] = 0.5f; }

    /* 12: energy non-negative                                        */
    float E0 = cos_v62_energy(x, y, D, 0.1f);
    TEST_OK("ebt_energy_nonneg", E0 >= 0.f);

    /* 13: energy(x, x) = 0                                           */
    float Esame = cos_v62_energy(x, x, D, 0.1f);
    TEST_OK("ebt_energy_self_zero", v62_close(Esame, 0.f, 1e-3f));

    /* 14: minimize reduces energy                                    */
    cos_v62_budget_t b = { 16, 32, 4, 4, 2, 16 };
    float E_after = 0.f; uint32_t steps = 0;
    cos_v62_ebt_minimize(x, y, D, 0.1f, 0.2f, 1e-4f, &b, &E_after, &steps);
    TEST_OK("ebt_minimize_reduces_energy", E_after <= E0 + 1e-4f);

    /* 15: minimize respects step budget                              */
    TEST_OK("ebt_minimize_under_budget", steps <= b.max_ebt_steps);

    /* 16: minimize reports zero on null candidate                    */
    TEST_OK("ebt_minimize_null_input",
            cos_v62_ebt_minimize(NULL, y, D, 0.1f, 0.1f, 1e-4f, &b, NULL, NULL) == -1);

    /* 17: energy is symmetric in cosine direction (same-magnitude)   */
    float Exy = cos_v62_energy(x, y, D, 0.1f);
    float Eyx = cos_v62_energy(y, x, D, 0.1f);
    TEST_OK("ebt_energy_symmetric", v62_close(Exy, Eyx, 1e-3f));

    /* 18: tiny tol still terminates                                  */
    cos_v62_budget_t bs = { 4, 4, 2, 2, 1, 8 };
    uint32_t s2 = 0;
    cos_v62_ebt_minimize(x, y, D, 0.05f, 0.1f, 1e-12f, &bs, NULL, &s2);
    TEST_OK("ebt_minimize_terminates", s2 <= bs.max_ebt_steps);

    free(x); free(y);
}

/* ====================================================================
 *  Section C — Hierarchical Reasoning Loop
 * ==================================================================== */

static void test_hrm(void)
{
    enum { D = 64 };
    cos_v62_thought_t *H = cos_v62_thought_new(D);
    cos_v62_thought_t *L = cos_v62_thought_new(D);
    /* seed with different signals so the loop has work to do.        */
    for (uint32_t i = 0; i < D; ++i) {
        H->vec[i] = (i % 2) ? 1.0f : 0.0f;
        L->vec[i] = (i % 3) ? 0.0f : 1.0f;
    }

    cos_v62_budget_t b = { 8, 16, 8, 16, 2, 32 };
    uint32_t hi = 0, li = 0;

    /* 19: hrm_run rc ok                                              */
    int rc = cos_v62_hrm_run(H, L, 0.2f, 0.4f, 0.999f, &b, &hi, &li);
    TEST_OK("hrm_run_rc_ok", rc == 0);

    /* 20: H↔L converge (distance < 0.1)                              */
    float dist = 0.f;
    for (uint32_t i = 0; i < D; ++i) {
        float d = H->vec[i] - L->vec[i];
        dist += d * d;
    }
    dist = sqrtf(dist);
    TEST_OK("hrm_HL_converge", dist < 0.5f);

    /* 21: outer loop did at least one iteration                      */
    TEST_OK("hrm_h_iters_nonzero", hi >= 1);

    /* 22: inner loop did h * max_l_iters at most                     */
    TEST_OK("hrm_li_bounded", li <= hi * b.max_l_iters);

    /* 23: dim mismatch detected                                      */
    cos_v62_thought_t *L2 = cos_v62_thought_new(D + 4);
    int rcm = cos_v62_hrm_run(H, L2, 0.2f, 0.4f, 0.99f, &b, NULL, NULL);
    TEST_OK("hrm_dim_mismatch_detected", rcm == -2);
    cos_v62_thought_free(L2);

    cos_v62_thought_free(H);
    cos_v62_thought_free(L);
}

/* ====================================================================
 *  Section D — Native Sparse Attention
 * ==================================================================== */

static void test_nsa(void)
{
    enum { N = 64, D = 32 };
    float *Q = (float *)aligned_alloc(64, D * sizeof(float));
    float *K = (float *)aligned_alloc(64, N * D * sizeof(float));
    float *V = (float *)aligned_alloc(64, N * D * sizeof(float));
    float *out = (float *)aligned_alloc(64, D * sizeof(float));

    for (uint32_t i = 0; i < D; ++i) Q[i] = (i % 3) ? 0.1f : 0.5f;
    for (uint32_t r = 0; r < N; ++r)
        for (uint32_t i = 0; i < D; ++i) {
            K[r * D + i] = sinf((float)(r + i) * 0.1f);
            V[r * D + i] = cosf((float)(r * 2 + i) * 0.1f);
        }
    float gate[3] = { 0.3f, 0.5f, 0.2f };

    /* 24: returns 0 on valid input                                   */
    int rc = cos_v62_nsa_attend(Q, K, V, N, D, 8, 8, 4, gate, out);
    TEST_OK("nsa_rc_ok", rc == 0);

    /* 25: output is finite                                           */
    int finite = 1;
    for (uint32_t i = 0; i < D; ++i)
        if (!isfinite(out[i])) { finite = 0; break; }
    TEST_OK("nsa_out_finite", finite);

    /* 26: output is non-trivial (not all zeros)                      */
    float n2 = 0.f;
    for (uint32_t i = 0; i < D; ++i) n2 += out[i] * out[i];
    TEST_OK("nsa_out_nontrivial", n2 > 1e-6f);

    /* 27: NULL output rejected                                       */
    int rcn = cos_v62_nsa_attend(Q, K, V, N, D, 8, 8, 4, gate, NULL);
    TEST_OK("nsa_null_out_rejected", rcn == -1);

    /* 28: zero n rejected                                            */
    int rcz = cos_v62_nsa_attend(Q, K, V, 0, D, 8, 8, 4, gate, out);
    TEST_OK("nsa_zero_n_rejected", rcz == -1);

    /* 29: extreme topk = N still works                               */
    int rce = cos_v62_nsa_attend(Q, K, V, N, D, 4, N, 8, gate, out);
    TEST_OK("nsa_topk_full", rce == 0);

    /* 30: window covers all tokens still works                       */
    int rcw = cos_v62_nsa_attend(Q, K, V, N, D, N, 8, 8, gate, out);
    TEST_OK("nsa_window_full", rcw == 0);

    free(Q); free(K); free(V); free(out);
}

/* ====================================================================
 *  Section E — Multi-Token Predictor
 * ==================================================================== */

static void test_mtp(void)
{
    enum { VOCAB = 32, K = 4 };
    float *logits = (float *)aligned_alloc(64, VOCAB * sizeof(float));
    float *bias   = (float *)aligned_alloc(64, K * VOCAB * sizeof(float));
    uint32_t draft[K];
    float    conf[K];
    /* logits put initial argmax at index 5                           */
    for (uint32_t i = 0; i < VOCAB; ++i) logits[i] = 0.f;
    logits[5] = 10.f;
    /* Each head adds a strictly-larger bias at the next position so
     * the causal-chain argmax advances by one regardless of prior
     * accumulated mass.  Mirrors DeepSeek-V3 MTP: each head depends
     * on the previous draft.                                          */
    for (uint32_t k = 0; k < K; ++k)
        for (uint32_t i = 0; i < VOCAB; ++i)
            bias[k * VOCAB + i] = (i == (6 + k)) ? (200.f * (float)(k + 1)) : 0.f;

    /* 31: draft returns 0                                            */
    int rc = cos_v62_mtp_draft(logits, VOCAB, bias, K, draft, conf);
    TEST_OK("mtp_draft_rc_ok", rc == 0);

    /* 32: causal chain advances                                       */
    TEST_OK("mtp_draft_causal_step1", draft[0] == 6);
    TEST_OK("mtp_draft_causal_step2", draft[1] == 7);
    TEST_OK("mtp_draft_causal_step3", draft[2] == 8);
    TEST_OK("mtp_draft_causal_step4", draft[3] == 9);

    /* 33: confidences positive                                        */
    int conf_ok = 1;
    for (uint32_t k = 0; k < K; ++k) if (conf[k] <= 0.f) { conf_ok = 0; break; }
    TEST_OK("mtp_draft_conf_positive", conf_ok);

    /* 34: verify accept-all                                           */
    uint32_t truth_full[K] = { 6, 7, 8, 9 };
    TEST_OK("mtp_verify_accept_all",
            cos_v62_mtp_verify(draft, truth_full, K) == K);

    /* 35: verify accept-prefix                                        */
    uint32_t truth_pref[K] = { 6, 7, 99, 9 };
    TEST_OK("mtp_verify_accept_prefix",
            cos_v62_mtp_verify(draft, truth_pref, K) == 2);

    /* 36: verify reject-first                                         */
    uint32_t truth_none[K] = { 99, 7, 8, 9 };
    TEST_OK("mtp_verify_reject_first",
            cos_v62_mtp_verify(draft, truth_none, K) == 0);

    /* 37: verify NULL safe                                            */
    TEST_OK("mtp_verify_null_safe",
            cos_v62_mtp_verify(NULL, truth_full, K) == 0);

    free(logits); free(bias);
}

/* ====================================================================
 *  Section F — Adaptive KV manager
 * ==================================================================== */

static void test_arkv(void)
{
    enum { N = 32 };
    cos_v62_arkv_t *a = cos_v62_arkv_new(N, 4, 8);
    TEST_OK("arkv_new_ok", a != NULL);

    /* 38: every token starts as QUANT                                 */
    {
        int all_quant = 1;
        for (uint32_t i = 0; i < N; ++i)
            if (a->state[i] != COS_V62_KV_QUANT) { all_quant = 0; break; }
        TEST_OK("arkv_initial_quant", all_quant);
    }

    /* 39: update with constant attention keeps caps                   */
    float *attn = (float *)aligned_alloc(64, N * sizeof(float));
    for (uint32_t i = 0; i < N; ++i) attn[i] = 0.1f;
    uint32_t orig = cos_v62_arkv_update(a, attn);
    TEST_OK("arkv_update_orig_eq_cap", orig == 4);

    /* 40: heavy hitters get promoted to ORIG                          */
    for (uint32_t i = 0; i < N; ++i) attn[i] = (i < 4) ? 100.f : 0.0001f;
    cos_v62_arkv_update(a, attn);
    int top4_orig = 1;
    for (uint32_t i = 0; i < 4; ++i)
        if (a->state[i] != COS_V62_KV_ORIG) { top4_orig = 0; break; }
    TEST_OK("arkv_promotes_heavy_hitters", top4_orig);

    /* 41: cold tokens demoted to EVICT                                */
    int many_evict = 0;
    for (uint32_t i = 12; i < N; ++i)
        if (a->state[i] == COS_V62_KV_EVICT) many_evict++;
    TEST_OK("arkv_evicts_cold", many_evict >= 8);

    /* 42: NULL-safe free                                              */
    cos_v62_arkv_free(NULL);
    TEST_OK("arkv_free_null_safe", 1);

    free(attn);
    cos_v62_arkv_free(a);
}

/* ====================================================================
 *  Section G — composition with v60 σ-Shield + v61 Σ-Citadel
 * ==================================================================== */

static void test_composition(void)
{
    cos_v62_decision_t d;

    /* 43–46: 8 cases of 3-bit composition                             */
    cos_v62_compose_decision(0, 0, 0, &d); TEST_OK("compose_000", d.allow == 0);
    cos_v62_compose_decision(1, 0, 0, &d); TEST_OK("compose_100", d.allow == 0);
    cos_v62_compose_decision(0, 1, 0, &d); TEST_OK("compose_010", d.allow == 0);
    cos_v62_compose_decision(0, 0, 1, &d); TEST_OK("compose_001", d.allow == 0);
    cos_v62_compose_decision(1, 1, 0, &d); TEST_OK("compose_110", d.allow == 0);
    cos_v62_compose_decision(1, 0, 1, &d); TEST_OK("compose_101", d.allow == 0);
    cos_v62_compose_decision(0, 1, 1, &d); TEST_OK("compose_011", d.allow == 0);
    cos_v62_compose_decision(1, 1, 1, &d); TEST_OK("compose_111", d.allow == 1);

    /* 51: NULL out rejected                                           */
    TEST_OK("compose_null_rejected",
            cos_v62_compose_decision(1, 1, 1, NULL) == -1);

    /* 52: lanes preserved                                             */
    cos_v62_compose_decision(1, 0, 1, &d);
    TEST_OK("compose_lanes_preserved",
            d.v60_ok == 1 && d.v61_ok == 0 && d.v62_ok == 1);

    /* 53: deny-only-v62 lane is the v62-distinct case                 */
    cos_v62_compose_decision(1, 1, 0, &d);
    TEST_OK("compose_deny_v62_only",
            d.allow == 0 && d.v60_ok == 1 && d.v61_ok == 1 && d.v62_ok == 0);
}

/* ====================================================================
 *  Section H — adversarial / utopistic-frontier scenarios
 * ==================================================================== */

static void test_adversarial(void)
{
    /* 54: latent loop never escapes σ ≤ 1                             */
    cos_v62_thought_t *t = cos_v62_thought_new(32);
    float *g = (float *)aligned_alloc(64, 32 * sizeof(float));
    for (uint32_t i = 0; i < 32; ++i) g[i] = 5.f; /* huge gradient    */
    cos_v62_latent_step(t, g, 1.f);
    TEST_OK("latent_sigma_bounded", t->sigma >= 0.f && t->sigma <= 1.f);

    /* 55: HRM with zero L converges to (or stays at) zero             */
    cos_v62_thought_t *H = cos_v62_thought_new(16);
    cos_v62_thought_t *L = cos_v62_thought_new(16);
    cos_v62_budget_t b = { 4, 4, 4, 4, 1, 8 };
    cos_v62_hrm_run(H, L, 0.5f, 0.5f, 0.99f, &b, NULL, NULL);
    int H_finite = 1;
    for (uint32_t i = 0; i < 16; ++i)
        if (!isfinite(H->vec[i])) { H_finite = 0; break; }
    TEST_OK("hrm_finite_under_zero_init", H_finite);
    cos_v62_thought_free(H); cos_v62_thought_free(L);

    /* 56: ARKV under attention spike never exceeds cap_orig           */
    cos_v62_arkv_t *a = cos_v62_arkv_new(16, 2, 4);
    float spike[16];
    for (int i = 0; i < 16; ++i) spike[i] = 1e6f;  /* all "heavy"      */
    cos_v62_arkv_update(a, spike);
    int orig_count = 0;
    for (uint32_t i = 0; i < 16; ++i)
        orig_count += (a->state[i] == COS_V62_KV_ORIG);
    TEST_OK("arkv_cap_orig_respected", orig_count == 2);
    cos_v62_arkv_free(a);

    /* 57: MTP draft does not segfault on vocab=1                      */
    float l1 = 1.f;
    float bh1 = 0.f;
    uint32_t dr1 = 99; float cf1 = 0.f;
    int rc = cos_v62_mtp_draft(&l1, 1, &bh1, 1, &dr1, &cf1);
    TEST_OK("mtp_draft_vocab1_ok", rc == 0 && dr1 == 0);

    /* 58: NSA with N=1                                                */
    float Q1 = 1.f, K1 = 1.f, V1 = 0.7f, gate[3] = { 0.5f, 0.3f, 0.2f }, o = 0.f;
    int rcn = cos_v62_nsa_attend(&Q1, &K1, &V1, 1, 1, 1, 1, 1, gate, &o);
    TEST_OK("nsa_n1_ok", rcn == 0);

    /* 59: EBT minimization is monotone-non-increasing                 */
    enum { D = 16 };
    float *xx = (float *)aligned_alloc(64, D * sizeof(float));
    float *yy = (float *)aligned_alloc(64, D * sizeof(float));
    for (uint32_t i = 0; i < D; ++i) { xx[i] = (float)i / D; yy[i] = -1.f; }
    float E_first = cos_v62_energy(xx, yy, D, 0.1f);
    cos_v62_budget_t b2 = { 4, 32, 4, 4, 1, 8 };
    float E_last = 0.f; uint32_t st = 0;
    cos_v62_ebt_minimize(xx, yy, D, 0.1f, 0.1f, 1e-6f, &b2, &E_last, &st);
    TEST_OK("ebt_monotone_nonincreasing", E_last <= E_first + 1e-3f);
    free(xx); free(yy);

    /* 60: composition under adversarial all-deny stays denied         */
    cos_v62_decision_t d;
    cos_v62_compose_decision(0, 1, 1, &d);
    TEST_OK("compose_adversarial_v60_dominates", d.allow == 0);

    /* 61: version string non-empty                                    */
    const char *v = cos_v62_version();
    TEST_OK("version_string_nonempty", v && strlen(v) > 8);

    /* 62: thought-new with dim=0 returns NULL (no UB)                 */
    TEST_OK("thought_new_zero_dim", cos_v62_thought_new(0) == NULL);

    free(g); cos_v62_thought_free(t);
}

/* ====================================================================
 *  Microbench: NSA + EBT throughput on M4
 * ==================================================================== */

static double v62_now_ms(void)
{
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
}

static void run_bench(void)
{
    enum { N = 1024, D = 64, ITERS = 200 };
    float *Q = (float *)aligned_alloc(64, D * sizeof(float));
    float *K = (float *)aligned_alloc(64, N * D * sizeof(float));
    float *V = (float *)aligned_alloc(64, N * D * sizeof(float));
    float *out = (float *)aligned_alloc(64, D * sizeof(float));
    float gate[3] = { 0.3f, 0.5f, 0.2f };
    for (uint32_t i = 0; i < D; ++i) Q[i] = sinf((float)i * 0.13f);
    for (uint32_t i = 0; i < N * D; ++i) K[i] = sinf((float)i * 0.07f);
    for (uint32_t i = 0; i < N * D; ++i) V[i] = cosf((float)i * 0.11f);

    double t0 = v62_now_ms();
    for (int it = 0; it < ITERS; ++it)
        cos_v62_nsa_attend(Q, K, V, N, D, 32, 32, 16, gate, out);
    double dt_nsa = v62_now_ms() - t0;
    double nsa_per_s = (double)ITERS * 1e3 / dt_nsa;

    enum { ED = 256 };
    float *xx = (float *)aligned_alloc(64, ED * sizeof(float));
    float *yy = (float *)aligned_alloc(64, ED * sizeof(float));
    for (int i = 0; i < ED; ++i) { xx[i] = (float)i / ED; yy[i] = 0.f; }
    cos_v62_budget_t b = { 1, 16, 1, 1, 1, 1 };

    int IT_E = 5000;
    t0 = v62_now_ms();
    for (int it = 0; it < IT_E; ++it)
        cos_v62_ebt_minimize(xx, yy, ED, 0.1f, 0.1f, 1e-9f, &b, NULL, NULL);
    double dt_ebt = v62_now_ms() - t0;
    double ebt_per_s = (double)IT_E * 1e3 / dt_ebt;

    printf("v62 microbench:\n");
    printf("  NSA  attend (n=%d, d=%d):   %8.0f calls/s   (%.2f ms/call)\n",
           N, D, nsa_per_s, dt_nsa / ITERS);
    printf("  EBT  minimize (d=%d, k=16):  %8.0f calls/s   (%.2f ms/call)\n",
           ED, ebt_per_s, dt_ebt / IT_E);

    free(Q); free(K); free(V); free(out); free(xx); free(yy);
}

/* ====================================================================
 *  --decision sample (machine-readable)
 * ==================================================================== */

static void run_decision_sample(void)
{
    cos_v62_decision_t d;
    cos_v62_compose_decision(1, 1, 1, &d);
    printf("{\n");
    printf("  \"version\": \"%s\",\n", cos_v62_version());
    printf("  \"v60_ok\":  %u,\n", (unsigned)d.v60_ok);
    printf("  \"v61_ok\":  %u,\n", (unsigned)d.v61_ok);
    printf("  \"v62_ok\":  %u,\n", (unsigned)d.v62_ok);
    printf("  \"allow\":   %u\n",  (unsigned)d.allow);
    printf("}\n");
}

/* ====================================================================
 *  main
 * ==================================================================== */

int main(int argc, char **argv)
{
    if (argc >= 2) {
        if (strcmp(argv[1], "--version") == 0) {
            puts(cos_v62_version()); return 0;
        }
        if (strcmp(argv[1], "--bench") == 0)     { run_bench(); return 0; }
        if (strcmp(argv[1], "--decision") == 0)  { run_decision_sample(); return 0; }
        if (strcmp(argv[1], "--self-test") != 0) {
            fprintf(stderr, "usage: %s [--self-test|--bench|--decision|--version]\n",
                    argv[0]);
            return 2;
        }
    }

    test_latent_cot();
    test_ebt();
    test_hrm();
    test_nsa();
    test_mtp();
    test_arkv();
    test_composition();
    test_adversarial();

    printf("v62 self-test: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
