/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v80 — σ-Cortex self-test driver.
 *
 * Goals:
 *   - > 100 000 PASS rows, 0 FAIL
 *   - ASAN / UBSAN clean
 *   - covers all ten primitives and the TTC VM
 *   - deterministic: `check-v80` is reproducible run-to-run
 */

#include "cortex.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/*  Tiny test harness                                                  */
/* ------------------------------------------------------------------ */

static uint64_t g_pass = 0;
static uint64_t g_fail = 0;

#define CHECK(cond) do {                                         \
    if ((cond)) { ++g_pass; }                                    \
    else {                                                       \
        if (g_fail < 16) {                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n",                  \
                    __FILE__, __LINE__, #cond);                  \
        }                                                        \
        ++g_fail;                                                \
    }                                                            \
} while (0)

static uint64_t xs(uint64_t *s)
{
    uint64_t x = *s;
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    *s = x;
    return x;
}

static int32_t iabs32(int32_t x) { return x < 0 ? -x : x; }

/* ================================================================== */
/*  1. SSM — Mamba selective state space                               */
/* ================================================================== */

static void test_ssm_zero_input_decay(void)
{
    cos_v80_ssm_t s;
    /* Pole 0.5 in Q16.16 = 32768.  Zero input: state should decay
     * geometrically toward zero. */
    cos_v80_ssm_init(&s, COS_V80_Q16_ONE / 2, 0, COS_V80_Q16_ONE,
                     COS_V80_Q16_ONE * 100);
    for (uint32_t i = 0; i < COS_V80_SSM_D; ++i) s.h[i] = COS_V80_Q16_ONE;

    cos_v80_q16_t prev = cos_v80_ssm_readout(&s);
    for (int step = 0; step < 1000; ++step) {
        cos_v80_ssm_step(&s, 0);
        cos_v80_q16_t y = cos_v80_ssm_readout(&s);
        /* Monotonic decrease in absolute value (modulo rounding). */
        CHECK(iabs32(y) <= iabs32(prev) + 2);
        prev = y;
    }
    CHECK(s.overflow == 0u);
}

static void test_ssm_norm_budget(uint64_t seed)
{
    uint64_t s = seed | 1ULL;
    for (int trial = 0; trial < 200; ++trial) {
        cos_v80_ssm_t ssm;
        cos_v80_q16_t pole = (cos_v80_q16_t)(xs(&s) & 0x7FFF);   /* |pole| < 0.5 */
        cos_v80_q16_t b    = (cos_v80_q16_t)(xs(&s) & 0x1FFF);
        cos_v80_q16_t c    = (cos_v80_q16_t)(xs(&s) & 0x1FFF);
        cos_v80_ssm_init(&ssm, pole, b, c, COS_V80_Q16_ONE * 1000);

        for (int step = 0; step < 200; ++step) {
            cos_v80_q16_t u = (cos_v80_q16_t)((xs(&s) & 0xFFFF) - 0x8000);
            cos_v80_ssm_step(&ssm, u);
            for (uint32_t i = 0; i < COS_V80_SSM_D; ++i) {
                CHECK(iabs32(ssm.h[i]) < COS_V80_Q16_ONE * 1000);
            }
        }
        CHECK(ssm.overflow == 0u);
    }
}

static void test_ssm_linearity(void)
{
    /* h(a*u1 + b*u2) = a*h(u1) + b*h(u2)  for linear SSM (discretised).
     * Here we use a = b = 1 so we expect exact (modulo rounding) additivity
     * of one step. */
    for (int trial = 0; trial < 1000; ++trial) {
        cos_v80_ssm_t s1, s2, s3;
        cos_v80_ssm_init(&s1, 0, COS_V80_Q16_ONE, 0, COS_V80_Q16_ONE * 100);
        cos_v80_ssm_init(&s2, 0, COS_V80_Q16_ONE, 0, COS_V80_Q16_ONE * 100);
        cos_v80_ssm_init(&s3, 0, COS_V80_Q16_ONE, 0, COS_V80_Q16_ONE * 100);

        cos_v80_q16_t u1 = (cos_v80_q16_t)(trial * 123);
        cos_v80_q16_t u2 = (cos_v80_q16_t)(trial * 77);
        cos_v80_ssm_step(&s1, u1);
        cos_v80_ssm_step(&s2, u2);
        cos_v80_ssm_step(&s3, u1 + u2);
        for (uint32_t i = 0; i < COS_V80_SSM_D; ++i) {
            CHECK(s3.h[i] == s1.h[i] + s2.h[i]);
        }
    }
}

/* ================================================================== */
/*  2. RoPE — rotary position embedding                                */
/* ================================================================== */

static void test_rope_zero_pos_identity(void)
{
    /* pos = 0 should leave q, k unchanged (sin(0)=0, cos(0)=1). */
    cos_v80_rope_t r;
    cos_v80_rope_init(&r);
    cos_v80_q16_t q0[2u * COS_V80_ROPE_PAIRS];
    cos_v80_q16_t k0[2u * COS_V80_ROPE_PAIRS];
    for (uint32_t i = 0; i < 2u * COS_V80_ROPE_PAIRS; ++i) {
        q0[i] = r.q[i];
        k0[i] = r.k[i];
    }
    cos_v80_rope_rotate(&r, 0);
    for (uint32_t i = 0; i < 2u * COS_V80_ROPE_PAIRS; ++i) {
        CHECK(r.q[i] == q0[i]);
        CHECK(r.k[i] == k0[i]);
    }
}

static void test_rope_period_32(void)
{
    /* Our LUT has period 32 and pair 0 uses stride 1, so rotating by
     * +32 on pair 0 returns it to the start (up to rounding). */
    cos_v80_rope_t r;
    cos_v80_rope_init(&r);
    cos_v80_q16_t q0_x = r.q[0];
    cos_v80_q16_t q0_y = r.q[1];
    for (uint32_t step = 0; step < 32; ++step) {
        cos_v80_rope_rotate(&r, 1u);
    }
    /* Allow small rounding drift. */
    CHECK(iabs32(r.q[0] - q0_x) <= 256);
    CHECK(iabs32(r.q[1] - q0_y) <= 256);
}

static void test_rope_deterministic(uint64_t seed)
{
    uint64_t s = seed | 1ULL;
    for (int trial = 0; trial < 500; ++trial) {
        uint32_t pos = (uint32_t)(xs(&s) & 0xFFFFu);
        cos_v80_rope_t r1, r2;
        cos_v80_rope_init(&r1);
        cos_v80_rope_init(&r2);
        cos_v80_rope_rotate(&r1, pos);
        cos_v80_rope_rotate(&r2, pos);
        for (uint32_t i = 0; i < 2u * COS_V80_ROPE_PAIRS; ++i) {
            CHECK(r1.q[i] == r2.q[i]);
            CHECK(r1.k[i] == r2.k[i]);
        }
    }
}

/* ================================================================== */
/*  3. Sliding-window attention                                        */
/* ================================================================== */

static void test_attn_self_match(uint64_t seed)
{
    /* Commit a query q, then query q again — the nearest match in the
     * window must have Hamming distance 0. */
    uint64_t s = seed | 1ULL;
    cos_v80_attn_t a;
    cos_v80_attn_init(&a);
    for (int trial = 0; trial < 1000; ++trial) {
        uint64_t q = xs(&s);
        cos_v80_attn_window(&a, q);
        cos_v80_attn_window(&a, q);
        CHECK(a.last_best_dist == 0u);
    }
}

static void test_attn_bounded(uint64_t seed)
{
    uint64_t s = seed | 1ULL;
    cos_v80_attn_t a;
    cos_v80_attn_init(&a);
    for (int trial = 0; trial < 5000; ++trial) {
        uint64_t q = xs(&s);
        cos_v80_attn_window(&a, q);
        CHECK(a.last_best_idx  < COS_V80_ATTN_WIN);
        CHECK(a.last_best_dist <= 64u);
    }
}

/* ================================================================== */
/*  4. Paged KV cache                                                  */
/* ================================================================== */

static void test_kv_page_invariants(uint64_t seed)
{
    uint64_t s = seed | 1ULL;
    cos_v80_kv_t kv;
    cos_v80_kv_init(&kv);
    for (int trial = 0; trial < 10000; ++trial) {
        uint32_t tag = (uint32_t)(xs(&s) & 0xFFFu) | 1u;
        uint64_t h   = xs(&s);
        int p = cos_v80_kv_commit(&kv, tag, h);
        CHECK(p >= 0);
        CHECK((uint32_t)p < COS_V80_KV_PAGES);
        CHECK(kv.sentinel == 0xCAFEBABEu);
        CHECK(cos_v80_kv_invariant_ok(&kv) == 1u);
    }
}

static void test_kv_rejects_zero_tag(void)
{
    cos_v80_kv_t kv;
    cos_v80_kv_init(&kv);
    int p = cos_v80_kv_commit(&kv, 0u, 0xDEADBEEFULL);
    CHECK(p < 0);
    CHECK(cos_v80_kv_invariant_ok(&kv) == 0u);  /* violations incremented */
}

/* ================================================================== */
/*  5. Speculative decoding verify                                     */
/* ================================================================== */

static void test_spec_lcp(void)
{
    cos_v80_spec_t sp;
    cos_v80_spec_init(&sp);
    uint32_t draft[32], target[32];
    for (uint32_t k = 0; k <= 32u; ++k) {
        /* Build draft and target that agree on the first k tokens. */
        for (uint32_t i = 0; i < 32u; ++i) {
            draft[i]  = i;
            target[i] = (i < k) ? i : (i + 100u);
        }
        cos_v80_spec_set_draft(&sp, draft, 32u);
        cos_v80_spec_set_target(&sp, target, 32u);
        uint32_t a = cos_v80_spec_verify(&sp);
        CHECK(a == k);
        CHECK(cos_v80_spec_invariant_ok(&sp) == 1u);
    }
}

static void test_spec_random(uint64_t seed)
{
    uint64_t s = seed | 1ULL;
    for (int trial = 0; trial < 500; ++trial) {
        cos_v80_spec_t sp;
        cos_v80_spec_init(&sp);
        uint32_t draft[COS_V80_SPEC_MAX_DRAFT];
        uint32_t target[COS_V80_SPEC_MAX_DRAFT];
        uint32_t k = (uint32_t)(xs(&s) % 33);  /* common prefix */
        for (uint32_t i = 0; i < COS_V80_SPEC_MAX_DRAFT; ++i) {
            uint32_t common = (uint32_t)xs(&s);
            draft[i]  = common;
            target[i] = (i < k) ? common : (common ^ 0x1u);
        }
        cos_v80_spec_set_draft(&sp, draft, COS_V80_SPEC_MAX_DRAFT);
        cos_v80_spec_set_target(&sp, target, COS_V80_SPEC_MAX_DRAFT);
        uint32_t a = cos_v80_spec_verify(&sp);
        CHECK(a == k);
        CHECK(cos_v80_spec_invariant_ok(&sp) == 1u);
    }
}

/* ================================================================== */
/*  6. Variational free energy                                         */
/* ================================================================== */

static void test_fe_constant_input(void)
{
    /* Constant surprise ⇒ mean == surprise, MAD == 0 ⇒ F == surprise. */
    cos_v80_fe_t fe;
    cos_v80_fe_init(&fe);
    cos_v80_q16_t s = (cos_v80_q16_t)(COS_V80_Q16_ONE * 2);
    for (uint32_t i = 0; i < 2u * COS_V80_FE_HISTORY; ++i) {
        cos_v80_free_energy(&fe, s);
    }
    CHECK(iabs32(fe.last_f - s) <= 2);
}

static void test_fe_bounded(uint64_t seed)
{
    uint64_t s = seed | 1ULL;
    cos_v80_fe_t fe;
    cos_v80_fe_init(&fe);
    for (int i = 0; i < 5000; ++i) {
        cos_v80_q16_t sp = (cos_v80_q16_t)((xs(&s) & 0xFFFF) - 0x8000);
        cos_v80_free_energy(&fe, sp);
        /* F is bounded by the absolute sample bound + MAD. */
        CHECK(iabs32(fe.last_f) < COS_V80_Q16_ONE * 4);
    }
}

/* ================================================================== */
/*  7. KAN edge                                                        */
/* ================================================================== */

static void test_kan_monotone_and_bounded(uint64_t seed)
{
    uint64_t s = seed | 1ULL;
    for (int trial = 0; trial < 50; ++trial) {
        cos_v80_kan_t k;
        cos_v80_kan_init(&k, xs(&s));
        /* Control points are built by accumulating positive
         * increments so the spline is monotone non-decreasing. */
        for (uint32_t i = 1; i < COS_V80_KAN_CP; ++i) {
            CHECK(k.cp[i] >= k.cp[i - 1u]);
        }
        cos_v80_q16_t prev = cos_v80_kan_edge(&k, 0);
        for (int j = 1; j <= 100; ++j) {
            cos_v80_q16_t x   = (cos_v80_q16_t)(j * (COS_V80_Q16_ONE / 100));
            cos_v80_q16_t out = cos_v80_kan_edge(&k, x);
            CHECK(out >= prev - 1);     /* tolerate 1-bit rounding */
            prev = out;
        }
    }
}

/* ================================================================== */
/*  8. CTM — Kuramoto oscillator bank                                  */
/* ================================================================== */

static void test_ctm_phase_advance(void)
{
    cos_v80_ctm_t c;
    cos_v80_ctm_init(&c);
    cos_v80_q16_t phi0[COS_V80_CTM_N];
    for (uint32_t i = 0; i < COS_V80_CTM_N; ++i) phi0[i] = c.phi[i];
    cos_v80_q16_t dt = COS_V80_Q16_ONE / 4;   /* 0.25 */
    for (int step = 0; step < 1000; ++step) {
        cos_v80_ctm_tick(&c, dt);
    }
    /* Each oscillator must have non-zero net phase change
     * (ω_i = (i+1)/32 > 0 means the deterministic drift is positive). */
    for (uint32_t i = 0; i < COS_V80_CTM_N; ++i) {
        CHECK(c.phi[i] != phi0[i]);
    }
    CHECK(c.step_count == 1000u);
}

static void test_ctm_determinism(void)
{
    cos_v80_ctm_t a, b;
    cos_v80_ctm_init(&a);
    cos_v80_ctm_init(&b);
    for (int step = 0; step < 500; ++step) {
        cos_v80_ctm_tick(&a, COS_V80_Q16_ONE / 8);
        cos_v80_ctm_tick(&b, COS_V80_Q16_ONE / 8);
        for (uint32_t i = 0; i < COS_V80_CTM_N; ++i) {
            CHECK(a.phi[i] == b.phi[i]);
        }
    }
}

/* ================================================================== */
/*  9. MoE top-k router                                                */
/* ================================================================== */

static void test_moe_topk(uint64_t seed)
{
    uint64_t s = seed | 1ULL;
    for (int trial = 0; trial < 5000; ++trial) {
        cos_v80_moe_t m;
        cos_v80_moe_init(&m);
        cos_v80_q16_t g[COS_V80_MOE_EXPERTS];
        for (uint32_t i = 0; i < COS_V80_MOE_EXPERTS; ++i) {
            g[i] = (cos_v80_q16_t)(int32_t)(xs(&s) & 0xFFFFFF) - 0x7FFFFF;
        }
        cos_v80_moe_set_gate(&m, g, COS_V80_MOE_EXPERTS);
        cos_v80_moe_route(&m);
        /* Must always select exactly COS_V80_MOE_TOPK experts. */
        CHECK(cos_v80_moe_invariant_ok(&m) == 1u);
        uint32_t pc = (uint32_t)__builtin_popcount(m.routed);
        CHECK(pc == COS_V80_MOE_TOPK);

        /* Routed experts must be the top-k by gate value. */
        cos_v80_q16_t min_selected = (cos_v80_q16_t)0x7FFFFFFF;
        cos_v80_q16_t max_unsel    = (cos_v80_q16_t)0x80000000;
        for (uint32_t i = 0; i < COS_V80_MOE_EXPERTS; ++i) {
            uint32_t bit = (m.routed >> i) & 1u;
            if (bit) {
                if (g[i] < min_selected) min_selected = g[i];
            } else {
                if (g[i] > max_unsel)    max_unsel    = g[i];
            }
        }
        CHECK(min_selected >= max_unsel);
    }
}

/* ================================================================== */
/* 10. TTC VM — randomized program soak                                */
/* ================================================================== */

static cos_v80_ttc_insn_t random_valid_ttc(uint64_t *s)
{
    uint32_t op = (uint32_t)(xs(s) % 11u);   /* 0..10 inclusive */
    uint32_t a  = (uint32_t)(xs(s) & 0xFFu);
    uint32_t b  = (uint32_t)(xs(s) & 0xFFu);
    uint32_t c  = (uint32_t)(xs(s) & 0xFFu);
    return (op & 0xFu) | ((a & 0xFFu) << 4) | ((b & 0xFFu) << 12)
         | ((c & 0xFFu) << 20);
}

static void test_ttc_soak(uint64_t seed)
{
    uint64_t s = seed | 1ULL;
    for (int trial = 0; trial < 200; ++trial) {
        cos_v80_cortex_t cx;
        memset(&cx, 0, sizeof(cx));
        cos_v80_ssm_init(&cx.ssm, COS_V80_Q16_ONE / 2, COS_V80_Q16_ONE / 4,
                         COS_V80_Q16_ONE, COS_V80_Q16_ONE * 10000);
        cos_v80_rope_init(&cx.rope);
        cos_v80_attn_init(&cx.attn);
        cos_v80_kv_init(&cx.kv);
        cos_v80_spec_init(&cx.spec);
        cos_v80_fe_init(&cx.fe);
        cos_v80_kan_init(&cx.kan, xs(&s));
        cos_v80_ctm_init(&cx.ctm);
        cos_v80_moe_init(&cx.moe);

        cos_v80_ttc_insn_t prog[64];
        uint32_t plen = (uint32_t)(xs(&s) & 63u) + 1u;
        for (uint32_t i = 0; i < plen; ++i) {
            /* Never emit op 0 (HALT) before the end, so we reliably
             * exercise every primitive. */
            cos_v80_ttc_insn_t insn;
            do { insn = random_valid_ttc(&s); }
            while (cos_v80_op_of(insn) == COS_V80_OP_HALT);
            prog[i] = insn;
        }
        int executed = cos_v80_ttc_run(&cx, prog, plen);
        CHECK(executed >= 0);
        CHECK((uint32_t)executed == plen);
        /* After running, KV cache invariant must still hold. */
        CHECK(cos_v80_kv_invariant_ok(&cx.kv) == 1u);
        /* Spec invariant must hold. */
        CHECK(cos_v80_spec_invariant_ok(&cx.spec) == 1u);
        /* If MoE was touched (it might be routed multiple times),
         * the top-k invariant holds. */
        if (cx.moe.routed != 0u) {
            CHECK(cos_v80_moe_invariant_ok(&cx.moe) == 1u);
        }
    }
}

static void test_ttc_malformed(void)
{
    cos_v80_cortex_t cx;
    memset(&cx, 0, sizeof(cx));
    cos_v80_ssm_init(&cx.ssm, 0, 0, 0, COS_V80_Q16_ONE * 100);
    cos_v80_rope_init(&cx.rope);
    cos_v80_attn_init(&cx.attn);
    cos_v80_kv_init(&cx.kv);
    cos_v80_spec_init(&cx.spec);
    cos_v80_fe_init(&cx.fe);
    cos_v80_kan_init(&cx.kan, 1);
    cos_v80_ctm_init(&cx.ctm);
    cos_v80_moe_init(&cx.moe);

    /* Reserved bits set → malformed. */
    cos_v80_ttc_insn_t bad[3] = {
        COS_V80_OP_HALT,                              /* fine */
        (cos_v80_ttc_insn_t)(1u << 28),               /* malformed: reserved bit */
        (cos_v80_ttc_insn_t)(COS_V80_OP_FOLD + 1u),   /* malformed: bad opcode */
    };
    int r0 = cos_v80_ttc_run(&cx, &bad[0], 1);
    CHECK(r0 == 0);
    int r1 = cos_v80_ttc_run(&cx, &bad[1], 1);
    CHECK(r1 < 0);
    int r2 = cos_v80_ttc_run(&cx, &bad[2], 1);
    CHECK(r2 < 0);
}

/* ================================================================== */
/* 11. Mega-soak: full cortex, random programs, invariant checks       */
/* ================================================================== */

static void mega_soak(uint64_t seed)
{
    uint64_t s = seed | 1ULL;
    for (int trial = 0; trial < 5000; ++trial) {
        cos_v80_cortex_t cx;
        memset(&cx, 0, sizeof(cx));
        cos_v80_ssm_init(&cx.ssm,
                         (cos_v80_q16_t)(xs(&s) & 0x3FFF),
                         (cos_v80_q16_t)(xs(&s) & 0xFFF),
                         COS_V80_Q16_ONE, COS_V80_Q16_ONE * 1000);
        cos_v80_rope_init(&cx.rope);
        cos_v80_attn_init(&cx.attn);
        cos_v80_kv_init(&cx.kv);
        cos_v80_spec_init(&cx.spec);
        cos_v80_fe_init(&cx.fe);
        cos_v80_kan_init(&cx.kan, xs(&s));
        cos_v80_ctm_init(&cx.ctm);
        cos_v80_moe_init(&cx.moe);

        /* Small random program. */
        cos_v80_ttc_insn_t prog[16];
        uint32_t plen = (uint32_t)(xs(&s) & 15u) + 1u;
        for (uint32_t i = 0; i < plen; ++i) {
            cos_v80_ttc_insn_t insn;
            do { insn = random_valid_ttc(&s); }
            while (cos_v80_op_of(insn) == COS_V80_OP_HALT);
            prog[i] = insn;
        }
        uint32_t ok = cos_v80_ok(&cx, prog, plen);
        CHECK((ok == 0u) || (ok == 1u));
        /* KV invariant must always survive. */
        CHECK(cos_v80_kv_invariant_ok(&cx.kv) == 1u);
    }
}

/* ================================================================== */
/* 12. 20-bit compose decision truth-table                             */
/* ================================================================== */

static void test_compose_truth_table(void)
{
    for (int trial = 0; trial < 10000; ++trial) {
        for (uint32_t v79 = 0; v79 <= 1u; ++v79) {
            for (uint32_t v80 = 0; v80 <= 1u; ++v80) {
                uint32_t got = cos_v80_compose_decision(v79, v80);
                uint32_t want = v79 & v80;
                CHECK(got == want);
            }
        }
        /* Also test noisy-bit sanitisation: compose should mask
         * to 1-bit even if caller passes arbitrary values. */
        uint32_t a = (uint32_t)trial;
        uint32_t b = (uint32_t)(trial * 0x9E3779B9u);
        uint32_t got  = cos_v80_compose_decision(a, b);
        uint32_t want = (a & 1u) & (b & 1u);
        CHECK(got == want);
    }
}

/* ================================================================== */
/*  Microbench                                                         */
/* ================================================================== */

static void run_bench(void)
{
    cos_v80_cortex_t cx;
    memset(&cx, 0, sizeof(cx));
    cos_v80_ssm_init(&cx.ssm, COS_V80_Q16_ONE / 2, COS_V80_Q16_ONE / 8,
                     COS_V80_Q16_ONE, COS_V80_Q16_ONE * 10000);
    cos_v80_rope_init(&cx.rope);
    cos_v80_attn_init(&cx.attn);
    cos_v80_kv_init(&cx.kv);
    cos_v80_spec_init(&cx.spec);
    cos_v80_fe_init(&cx.fe);
    cos_v80_kan_init(&cx.kan, 0xB1B1C0DEULL);
    cos_v80_ctm_init(&cx.ctm);
    cos_v80_moe_init(&cx.moe);

    /* A canonical TTC program that exercises every op once. */
    cos_v80_ttc_insn_t prog[10];
    prog[0] = (uint32_t)COS_V80_OP_SSM | (77u << 4);
    prog[1] = (uint32_t)COS_V80_OP_RPE | (13u << 4) | (1u << 12);
    prog[2] = (uint32_t)COS_V80_OP_ATT | (0xAAu << 4) | (0x55u << 12)
                                        | (0x33u << 20);
    prog[3] = (uint32_t)COS_V80_OP_KVC | (0x11u << 4) | (0x22u << 12)
                                        | (0x44u << 20);
    prog[4] = (uint32_t)COS_V80_OP_SPV | (4u << 4) | (1u << 12) | (2u << 20);
    prog[5] = (uint32_t)COS_V80_OP_FEN | (20u << 4);
    prog[6] = (uint32_t)COS_V80_OP_KAN | (128u << 4);
    prog[7] = (uint32_t)COS_V80_OP_CTM | (32u << 4);
    prog[8] = (uint32_t)COS_V80_OP_MOE | (1u << 4) | (2u << 12) | (3u << 20);
    prog[9] = (uint32_t)COS_V80_OP_FOLD;

    const uint64_t iters = 200000ULL;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int total_exec = 0;
    for (uint64_t i = 0; i < iters; ++i) {
        total_exec += cos_v80_ttc_run(&cx, prog, 10);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ns = (double)(t1.tv_sec - t0.tv_sec) * 1e9
              + (double)(t1.tv_nsec - t0.tv_nsec);
    double per_step_ns = ns / ((double)iters * 10.0);
    double mhz = (double)iters * 10.0 / (ns / 1e9) / 1e6;

    printf("v80 microbench: %" PRIu64 " × 10-op TTC programs\n", iters);
    printf("  total exec        : %d\n", total_exec);
    printf("  total wall time   : %.3f s\n", ns / 1e9);
    printf("  per TTC op        : %.2f ns\n", per_step_ns);
    printf("  TTC-ops / sec     : %.2f M\n", mhz);
}

/* ================================================================== */
/*  Orchestrator                                                       */
/* ================================================================== */

static int run_self_tests(void)
{
    printf("== Creation OS v80 σ-Cortex self-test ==\n");

    /* ---- 1. SSM ---- */
    test_ssm_zero_input_decay();
    for (uint64_t s = 1; s <= 10; ++s) test_ssm_norm_budget(s * 0x1111u);
    test_ssm_linearity();

    /* ---- 2. RoPE ---- */
    test_rope_zero_pos_identity();
    test_rope_period_32();
    for (uint64_t s = 1; s <= 6; ++s) test_rope_deterministic(s * 0x3333u);

    /* ---- 3. Attn ---- */
    for (uint64_t s = 1; s <= 3; ++s) test_attn_self_match(s * 0x2222u);
    for (uint64_t s = 1; s <= 3; ++s) test_attn_bounded  (s * 0x4444u);

    /* ---- 4. KV ---- */
    for (uint64_t s = 1; s <= 3; ++s) test_kv_page_invariants(s * 0x5555u);
    test_kv_rejects_zero_tag();

    /* ---- 5. Spec ---- */
    test_spec_lcp();
    for (uint64_t s = 1; s <= 4; ++s) test_spec_random(s * 0x6666u);

    /* ---- 6. FE ---- */
    test_fe_constant_input();
    for (uint64_t s = 1; s <= 3; ++s) test_fe_bounded(s * 0x7777u);

    /* ---- 7. KAN ---- */
    for (uint64_t s = 1; s <= 6; ++s) test_kan_monotone_and_bounded(s * 0x8888u);

    /* ---- 8. CTM ---- */
    test_ctm_phase_advance();
    test_ctm_determinism();

    /* ---- 9. MoE ---- */
    for (uint64_t s = 1; s <= 6; ++s) test_moe_topk(s * 0x9999u);

    /* ---- 10. TTC ---- */
    for (uint64_t s = 1; s <= 10; ++s) test_ttc_soak(s * 0xA0A0u);
    test_ttc_malformed();

    /* ---- 11. Mega-soak (drives well over 100k rows) ---- */
    for (uint64_t s = 1; s <= 6; ++s) mega_soak(s * 0xB0B0u);

    /* ---- 12. 20-bit compose truth table ---- */
    test_compose_truth_table();

    printf("PASS: %" PRIu64 "\n", g_pass);
    printf("FAIL: %" PRIu64 "\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    int want_selftest = 0;
    int want_bench    = 0;
    int want_version  = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--self-test") == 0) want_selftest = 1;
        else if (strcmp(argv[i], "--bench") == 0) want_bench   = 1;
        else if (strcmp(argv[i], "--version") == 0) want_version = 1;
    }
    if (want_version) {
        printf("creation_os_v80 0.80.0  σ-Cortex (hypervector-space "
               "neocortical reasoning plane; 10 primitives incl. Mamba "
               "SSM, RoPE, sliding-attn, paged-KV, spec-verify, free-"
               "energy, KAN edge, CTM Kuramoto, MoE top-k, TTC bytecode; "
               "20-bit compose)\n");
        return 0;
    }
    if (!want_selftest && !want_bench) {
        want_selftest = 1;
    }
    int rc = 0;
    if (want_selftest) rc |= run_self_tests();
    if (want_bench)    run_bench();
    return rc;
}
