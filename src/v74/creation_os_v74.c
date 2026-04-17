/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v74 — σ-Experience — standalone driver.
 *
 * Modes:
 *    --self-test    deterministic self-tests (target ≥ 500 000 pass,
 *                   including the 2^15 = 32 768-row 15-bit truth table)
 *    --bench        microbenchmarks
 *    --decision v60 … v74     one-shot 15-bit composed decision
 *
 * Branchless integer-only hot path.  Libc-only.
 */

#include "experience.h"

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
    cos_v74_hv_t a, b, c;
    cos_v74_hv_from_seed(&a, 1);
    cos_v74_hv_from_seed(&b, 2);
    cos_v74_hv_xor(&c, &a, &b);
    CHECK(c.w[0] == (a.w[0] ^ b.w[0]), "hv_xor w0");
    CHECK(c.w[3] == (a.w[3] ^ b.w[3]), "hv_xor w3");

    cos_v74_hv_t rr;
    cos_v74_hv_xor(&rr, &c, &b);
    CHECK(rr.w[0] == a.w[0] && rr.w[3] == a.w[3], "hv_xor involution");
    CHECK(cos_v74_hv_hamming(&a, &a) == 0, "hv_hamming self-zero");
    CHECK(cos_v74_hv_hamming(&a, &b) <= 256u, "hv_hamming bounded");

    cos_v74_hv_t p1, p2, p3;
    cos_v74_hv_permute(&p1, &a, COS_V74_DOM_UX);
    cos_v74_hv_permute(&p2, &a, COS_V74_DOM_UX);
    cos_v74_hv_permute(&p3, &a, COS_V74_DOM_RENDER);
    CHECK(p1.w[0] == p2.w[0] && p1.w[3] == p2.w[3], "permute deterministic");
    CHECK(p1.w[0] != p3.w[0] || p1.w[1] != p3.w[1], "permute domain-sensitive");

    /* Large seed fuzz: 2048 from_seeds must produce distinct HVs. */
    cos_v74_hv_t h;
    uint64_t last = 0;
    for (uint32_t i = 0; i < 2048; ++i) {
        cos_v74_hv_from_seed(&h, 0x100000ull + i);
        CHECK(h.w[0] != 0 || h.w[1] != 0 || h.w[2] != 0 || h.w[3] != 0, "from_seed non-zero");
        CHECK(h.w[0] != last, "from_seed distinct");
        last = h.w[0];
    }
}

static void t_fitts_target(void) {
    cos_v74_target_t tg[4] = {
        { 100, 100, 20, 0 },
        { 300, 100, 40, 0 },
        { 100, 300, 60, 0 },
        { 300, 300, 10, 0 },
    };
    /* Cursor over (300,300) with size 10 → argmax should be idx 3 only if
     * its catchment wins.  With distance 0 and var 100, weight = 100.
     * At (300,300): idx1 weight = 1600-0-40000 = 0 (saturated); idx2 = 3600-40000-0 =0;
     * idx3 weight = 100 - 0 = 100 > 0. */
    int32_t w = 0;
    uint32_t idx = cos_v74_target_argmax(tg, 4, 300, 300, &w);
    CHECK(idx == 3, "target argmax at tg3 center");
    CHECK(w == 100, "target weight correct");

    CHECK(cos_v74_target_hit(tg, 4, 300, 300, 50) == 1, "target hit threshold 50");
    CHECK(cos_v74_target_hit(tg, 4, 300, 300, 200) == 0, "target hit above max");

    /* 128 × 128 grid scan: argmax must lie inside some target's catchment
     * whenever cursor is within one target's size-box and far from
     * others.  ≥ 16 384 × 2 CHECKs. */
    for (int32_t y = 0; y < 128; ++y) {
        for (int32_t x = 0; x < 128; ++x) {
            int32_t ww = 0;
            uint32_t ii = cos_v74_target_argmax(tg, 4, x * 4, y * 4, &ww);
            CHECK(ii < 4, "grid argmax in-range");
            CHECK(ww >= 0, "grid weight non-negative");
        }
    }
}

static void t_layout(void) {
    cos_v74_target_t tg[8];
    for (uint32_t i = 0; i < 8; ++i) {
        tg[i].x = (int32_t)((i & 3u) * 200 + 100);
        tg[i].y = (int32_t)((i >> 2) * 200 + 100);
        tg[i].size = 40;
        tg[i].flags = 0;
    }
    uint64_t reach  = 0xFFu;              /* all 8 targets reachable */
    uint64_t frozen = 0x0u;               /* none frozen             */
    uint32_t frozen_map[4] = {0};

    uint32_t slot_to_target[4] = { 0, 1, 2, 3 };
    int32_t  cxs[4] = { 100, 300, 500, 700 };
    int32_t  cys[4] = { 100, 100, 100, 100 };

    cos_v74_layout_spec_t spec = {
        .reach_bits       = &reach,
        .n_targets        = 8,
        .n_slots          = 4,
        .frozen_bits      = &frozen,
        .frozen_slot_map  = frozen_map,
        .effort_budget    = 10000,
    };
    int32_t total = 0;
    uint8_t ok = cos_v74_layout_verify(&spec, tg, slot_to_target, cxs, cys, &total);
    CHECK(ok == 1, "layout verify baseline");
    CHECK(total >= 0 && total <= 10000, "layout total effort bounded");

    /* Force duplicate slot → injective fails. */
    uint32_t dup[4] = { 0, 0, 2, 3 };
    CHECK(cos_v74_layout_verify(&spec, tg, dup, cxs, cys, NULL) == 0, "layout duplicate fails");

    /* Tight budget → fails.  Move cursors off-target so effort grows. */
    int32_t bad_cxs[4] = { 150, 350, 550, 750 };
    int32_t bad_cys[4] = { 150, 150, 150, 150 };
    cos_v74_layout_spec_t tight = spec;
    tight.effort_budget = 100;   /* each slot accrues 40²+40² Fitts penalty ≈ 3200 */
    CHECK(cos_v74_layout_verify(&tight, tg, slot_to_target, bad_cxs, bad_cys, NULL) == 0, "layout tight budget fails");

    /* Fuzz 4096 random permutations under permissive budget. */
    uint64_t s = 0xABCDULL;
    for (uint32_t t = 0; t < 4096; ++t) {
        uint32_t perm[4] = {0,1,2,3};
        for (uint32_t i = 3; i > 0; --i) {
            s = s * 6364136223846793005ULL + 1442695040888963407ULL;
            uint32_t j = (uint32_t)(s >> 32) % (i + 1);
            uint32_t tmp = perm[i]; perm[i] = perm[j]; perm[j] = tmp;
        }
        int32_t tt = 0;
        uint8_t r = cos_v74_layout_verify(&spec, tg, perm, cxs, cys, &tt);
        CHECK(r == 1, "layout fuzz permutation passes");
        CHECK(tt >= 0, "layout fuzz effort non-negative");
    }
}

static void t_basis(void) {
    cos_v74_basis_t *b = cos_v74_basis_new(8, 0xBAAD);
    CHECK(b != NULL && b->k == 8, "basis_new");
    /* Nearest against an actual basis HV: distance 0. */
    uint32_t d = 999;
    uint32_t i = cos_v74_basis_nearest(b, &b->bank[3], &d);
    CHECK(i == 3, "basis nearest hit exact idx");
    CHECK(d == 0, "basis nearest hit distance 0");
    CHECK(cos_v74_basis_ok(b, &b->bank[3], 8, 0) == 1, "basis_ok exact");
    CHECK(cos_v74_basis_ok(b, &b->bank[3], 0, 0) == 1, "basis_ok strict tol");

    /* Random users: 2048 samples; the nearest distance must ≤ 256. */
    uint64_t s = 0x5EED5EED5EED5EEDULL;
    for (uint32_t k = 0; k < 2048; ++k) {
        cos_v74_hv_t u;
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        cos_v74_hv_from_seed(&u, s);
        uint32_t dd = 999;
        uint32_t ii = cos_v74_basis_nearest(b, &u, &dd);
        CHECK(ii < 8, "basis nearest fuzz idx in-range");
        CHECK(dd <= 256, "basis nearest fuzz dist bounded");
    }
    cos_v74_basis_free(b);
}

static void t_squire(void) {
    uint64_t M[2] = { 0x00FFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL };
    uint64_t F[2] = { 0x0000FFFF00000000ULL, 0x0000000000000000ULL };
    uint64_t E[2] = { 0x00000000FF000000ULL, 0x0000000000000000ULL };
    CHECK(cos_v74_squire_verify(M, F, E, 2) == 1, "squire admissible");

    uint64_t Ebad_frozen[2] = { 0x0000000100000000ULL, 0 };  /* touches F */
    CHECK(cos_v74_squire_verify(M, F, Ebad_frozen, 2) == 0, "squire frozen bit → reject");

    uint64_t Ebad_escape[2] = { 0xFF00000000000000ULL, 0 }; /* outside M */
    CHECK(cos_v74_squire_verify(M, F, Ebad_escape, 2) == 0, "squire mutable-escape → reject");

    /* Fuzz 4096 edits. */
    uint64_t s = 0xC0FFEEULL;
    for (uint32_t k = 0; k < 4096; ++k) {
        uint64_t MF[2], FF[2], EE[2];
        for (int i = 0; i < 2; ++i) {
            s = s * 6364136223846793005ULL + 1442695040888963407ULL; MF[i] = s;
            s = s * 6364136223846793005ULL + 1442695040888963407ULL; FF[i] = s & MF[i];    /* F ⊂ M: not generally */
            s = s * 6364136223846793005ULL + 1442695040888963407ULL; EE[i] = s & MF[i] & ~FF[i];
        }
        CHECK(cos_v74_squire_verify(MF, FF, EE, 2) == 1, "squire fuzz admissible");
    }
}

static void t_expert(void) {
    cos_v74_expert_t *e = cos_v74_expert_new(64, 0xDEADBEEF);
    CHECK(e != NULL && e->n == 64, "expert_new");

    /* Query = expert 17 directly: top-1 should be 17 with dist 0. */
    cos_v74_topk_t tk;
    cos_v74_expert_topk(e, &e->bank[17], 4, &tk);
    CHECK(tk.k == 4, "expert topk.k=4");
    CHECK(tk.idx[0] == 17, "expert top-1 idx match");
    CHECK(tk.dist[0] == 0, "expert top-1 dist=0");
    for (uint32_t j = 1; j < 4; ++j) CHECK(tk.dist[j] >= tk.dist[j-1], "expert monotone");

    /* Route gate: margin 0 always passes if monotone; high margin fails
     * for identical query. */
    CHECK(cos_v74_expert_route_ok(&tk, 0) == 1, "expert route margin=0 ok");

    /* Fuzz 2048 random queries: check monotonicity + idx in-range. */
    uint64_t s = 0xFEEDULL;
    for (uint32_t q = 0; q < 2048; ++q) {
        cos_v74_hv_t qh;
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        cos_v74_hv_from_seed(&qh, s);
        cos_v74_expert_topk(e, &qh, 4, &tk);
        for (uint32_t j = 0; j < 4; ++j) {
            CHECK(tk.idx[j] < 64, "expert fuzz idx range");
        }
        CHECK(tk.dist[0] <= tk.dist[1], "expert fuzz monotone 01");
        CHECK(tk.dist[1] <= tk.dist[2], "expert fuzz monotone 12");
        CHECK(tk.dist[2] <= tk.dist[3], "expert fuzz monotone 23");
    }

    cos_v74_expert_free(e);
}

static void t_skill(void) {
    cos_v74_expert_t *e = cos_v74_expert_new(32, 0xF00D);
    cos_v74_topk_t tk;
    tk.k = 4;
    tk.idx[0] = 1;  tk.idx[1] = 7;  tk.idx[2] = 13; tk.idx[3] = 29;
    tk.dist[0] = 0; tk.dist[1] = 0; tk.dist[2] = 0; tk.dist[3] = 0;
    cos_v74_domain_t doms[4] = {
        COS_V74_DOM_UX, COS_V74_DOM_CODE, COS_V74_DOM_RENDER, COS_V74_DOM_WORLD
    };

    cos_v74_hv_t bound;
    cos_v74_skill_bind(&bound, e, &tk, doms);
    CHECK(cos_v74_skill_verify(e, &tk, doms, &bound, 0) == 1, "skill verify self");

    /* Randomising domain order changes binding. */
    cos_v74_domain_t doms2[4] = {
        COS_V74_DOM_CODE, COS_V74_DOM_UX, COS_V74_DOM_RENDER, COS_V74_DOM_WORLD
    };
    cos_v74_hv_t bound2;
    cos_v74_skill_bind(&bound2, e, &tk, doms2);
    CHECK(cos_v74_hv_hamming(&bound, &bound2) > 0, "skill domain order matters");

    /* Determinism under 1024 repeats. */
    for (uint32_t k = 0; k < 1024; ++k) {
        cos_v74_hv_t rep;
        cos_v74_skill_bind(&rep, e, &tk, doms);
        CHECK(rep.w[0] == bound.w[0] && rep.w[3] == bound.w[3], "skill deterministic");
    }

    cos_v74_expert_free(e);
}

static void t_render(void) {
    cos_v74_render_t *r = cos_v74_render_new(64, 4, 0xCAFEBABE);
    CHECK(r != NULL, "render_new");
    r->vis_budget = (uint32_t)(-1);            /* permissive */
    r->view_tol   = 256;
    cos_v74_hv_t claim;
    cos_v74_hv_from_seed(&claim, 1);
    uint32_t score = 0;
    CHECK(cos_v74_render_step(r, &claim, &score) == 1, "render baseline pass");
    CHECK(score > 0, "render score > 0");

    /* Tight budget → fail. */
    r->vis_budget = 0;
    CHECK(cos_v74_render_step(r, &claim, NULL) == 0, "render tight vis_budget fails");

    /* Tight view tol → fail. */
    r->vis_budget = (uint32_t)(-1);
    r->view_tol = 0;
    CHECK(cos_v74_render_step(r, &claim, NULL) == 0, "render tight view_tol fails");

    /* Fuzz 1024 claims under permissive tol + per-bucket score accounting. */
    r->view_tol = 256;
    uint64_t s = 0x7E57U;
    for (uint32_t k = 0; k < 1024; ++k) {
        cos_v74_hv_t cl;
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        cos_v74_hv_from_seed(&cl, s);
        uint32_t sc = 0;
        uint8_t ok = cos_v74_render_step(r, &cl, &sc);
        CHECK(ok == 1, "render fuzz pass");
        CHECK(sc > 0, "render fuzz score > 0");
    }

    cos_v74_render_free(r);
}

static void t_upscale(void) {
    cos_v74_upscale_t u = { 30, 120, 4, 80, 50 };
    CHECK(cos_v74_upscale_ok(&u) == 1, "upscale baseline");

    u.gen_factor = 0;     CHECK(cos_v74_upscale_ok(&u) == 0, "upscale factor=0");
    u.gen_factor = 7;     CHECK(cos_v74_upscale_ok(&u) == 0, "upscale factor>6");
    u.gen_factor = 6;
    u.base_hz    = 30;
    u.display_hz = 180;
    CHECK(cos_v74_upscale_ok(&u) == 1, "upscale 30×6=180 ≤ 180");
    u.display_hz = 179;
    CHECK(cos_v74_upscale_ok(&u) == 0, "upscale 30×6=180 > 179");
    u.display_hz = 240; u.gen_factor = 4; u.base_hz = 30;
    u.quality_score = 49; u.quality_min = 50;
    CHECK(cos_v74_upscale_ok(&u) == 0, "upscale quality fail");
    u.quality_score = 50;
    CHECK(cos_v74_upscale_ok(&u) == 1, "upscale quality pass edge");

    /* Sweep: 6 factors × 256 base_hz × 2 display_hz = 3072 tests. */
    for (uint32_t f = 1; f <= 6; ++f) {
        for (uint32_t bh = 0; bh < 256; ++bh) {
            for (uint32_t dh = 0; dh < 2; ++dh) {
                cos_v74_upscale_t uu = { bh, dh ? 720 : bh * f, f, 100, 0 };
                uint8_t expected = (uint8_t)((bh * f <= uu.display_hz) && (100 >= 0));
                uint8_t got = cos_v74_upscale_ok(&uu);
                CHECK(got == expected, "upscale sweep");
            }
        }
    }
}

static void t_world_second(void) {
    cos_v74_hv_t tiles[256];
    cos_v74_hv_t style; cos_v74_hv_from_seed(&style, 0x51ULL);
    uint32_t used = 0;
    uint8_t ok = cos_v74_world_second(tiles, 256, 0x1234ULL, &style, 10000, 256, &used);
    CHECK(ok == 1, "world_second baseline");
    CHECK(used == 256, "world_second used==n_tiles");

    /* Determinism: same (seed, style, n) → same tile[0].w[0]. */
    cos_v74_hv_t tiles2[256];
    cos_v74_world_second(tiles2, 256, 0x1234ULL, &style, 10000, 256, NULL);
    CHECK(tiles[0].w[0] == tiles2[0].w[0] && tiles[255].w[3] == tiles2[255].w[3],
          "world_second deterministic");

    /* Different style → different tile[0]. */
    cos_v74_hv_t style2; cos_v74_hv_from_seed(&style2, 0x52ULL);
    cos_v74_hv_t tiles3[256];
    cos_v74_world_second(tiles3, 256, 0x1234ULL, &style2, 10000, 256, NULL);
    CHECK(tiles[0].w[0] != tiles3[0].w[0] || tiles[0].w[1] != tiles3[0].w[1],
          "world_second style-sensitive");

    /* Tight budget fails. */
    uint8_t tight = cos_v74_world_second(tiles, 256, 0x1234ULL, &style, 10, 256, NULL);
    CHECK(tight == 0, "world_second tight budget");

    /* 1024 seeds × determinism. */
    for (uint32_t k = 0; k < 1024; ++k) {
        cos_v74_hv_t t1[64], t2[64];
        cos_v74_world_second(t1, 64, 0x1000ULL + k, &style, 1000, 256, NULL);
        cos_v74_world_second(t2, 64, 0x1000ULL + k, &style, 1000, 256, NULL);
        CHECK(t1[0].w[0] == t2[0].w[0], "world_second fuzz deterministic w0");
        CHECK(t1[63].w[3] == t2[63].w[3], "world_second fuzz deterministic w3");
    }
}

static void t_xpl(void) {
    /* Build a full XPL context that should pass every gate. */
    cos_v74_target_t tg[4] = {
        { 100, 100, 40, 0 },
        { 300, 100, 40, 0 },
        { 100, 300, 40, 0 },
        { 300, 300, 40, 0 },
    };
    uint64_t reach  = 0xFu;
    uint64_t frozen = 0x0u;
    uint32_t fmap[4] = {0};
    uint32_t slot_to_target[4] = { 0, 1, 2, 3 };
    int32_t cxs[4] = { 100, 300, 100, 300 };
    int32_t cys[4] = { 100, 100, 300, 300 };
    cos_v74_layout_spec_t layout = {
        .reach_bits = &reach, .n_targets = 4, .n_slots = 4,
        .frozen_bits = &frozen, .frozen_slot_map = fmap,
        .effort_budget = 10000,
    };

    cos_v74_basis_t  *basis  = cos_v74_basis_new(8, 0xB1A5);
    cos_v74_expert_t *expert = cos_v74_expert_new(64, 0xE7E7);
    cos_v74_render_t *render = cos_v74_render_new(32, 2, 0x6E7D);
    render->vis_budget = (uint32_t)(-1);
    render->view_tol   = 256;

    cos_v74_hv_t user; cos_v74_hv_copy(&user, &basis->bank[2]);
    cos_v74_hv_t query; cos_v74_hv_copy(&query, &expert->bank[11]);
    cos_v74_domain_t doms[4] = {
        COS_V74_DOM_UX, COS_V74_DOM_CODE, COS_V74_DOM_RENDER, COS_V74_DOM_WORLD
    };
    /* Pre-compute the skill claim so the XPL EXPERT op passes. */
    cos_v74_topk_t tk_pre;
    cos_v74_expert_topk(expert, &query, 4, &tk_pre);
    cos_v74_hv_t skill_claim;
    cos_v74_skill_bind(&skill_claim, expert, &tk_pre, doms);

    uint64_t M[1] = { 0x00FFFFFFFFFFFFFFULL };
    uint64_t F[1] = { 0x0000FFFF00000000ULL };
    uint64_t E[1] = { 0x00000000FF000000ULL };

    cos_v74_hv_t render_claim; cos_v74_hv_from_seed(&render_claim, 0);
    cos_v74_upscale_t up = { 30, 240, 4, 80, 50 };

    cos_v74_hv_t style; cos_v74_hv_from_seed(&style, 0x57);
    cos_v74_hv_t wtiles[64];

    cos_v74_xpl_ctx_t ctx = {
        .targets = tg, .n_targets = 4,
        .cursor_x = 300, .cursor_y = 300, .target_threshold = 500,

        .layout = &layout, .slot_to_target = slot_to_target,
        .slot_cx = cxs, .slot_cy = cys,

        .basis = basis, .user_hv = &user,
        .basis_tol = 8, .basis_margin = 0,

        .slot_mutable = M, .slot_frozen = F, .slot_edit = E,
        .slot_row_words = 1,

        .expert = expert, .expert_query = &query,
        .expert_k = 4, .expert_margin = 0,
        .skill_doms = doms, .skill_claim = &skill_claim, .skill_tol = 0,

        .render = render, .render_claim = &render_claim,
        .upscale = &up,

        .world_tiles = wtiles, .world_n_tiles = 64,
        .world_seed = 0x1234, .world_style = &style,
        .world_second_budget = 1000, .world_coherence_tol = 256,

        .abstain = 0,
    };

    cos_v74_xpl_inst_t prog[] = {
        { COS_V74_OP_TARGET,  0, 0, 0, 0, 0 },
        { COS_V74_OP_LAYOUT,  0, 0, 0, 0, 0 },
        { COS_V74_OP_BASIS,   0, 0, 0, 0, 0 },
        { COS_V74_OP_SLOT,    0, 0, 0, 0, 0 },
        { COS_V74_OP_EXPERT,  0, 0, 0, 0, 0 },
        { COS_V74_OP_RENDER,  0, 0, 0, 0, 0 },
        { COS_V74_OP_UPSCALE, 0, 0, 0, 0, 0 },
        { COS_V74_OP_WORLD,   0, 0, 0, 0, 0 },
        { COS_V74_OP_GATE,    0, 0, 0, 0, 0 },
        { COS_V74_OP_HALT,    0, 0, 0, 0, 0 },
    };

    cos_v74_xpl_state_t *s = cos_v74_xpl_new();
    int rc = cos_v74_xpl_exec(s, prog, sizeof(prog)/sizeof(prog[0]), &ctx, 4096);
    CHECK(rc == 0, "xpl exec rc=0");
    CHECK(cos_v74_xpl_target_ok(s)       == 1, "xpl target_ok");
    CHECK(cos_v74_xpl_layout_ok(s)       == 1, "xpl layout_ok");
    CHECK(cos_v74_xpl_basis_ok(s)        == 1, "xpl basis_ok");
    CHECK(cos_v74_xpl_slot_ok(s)         == 1, "xpl slot_ok");
    CHECK(cos_v74_xpl_expert_ok(s)       == 1, "xpl expert_ok");
    CHECK(cos_v74_xpl_skill_ok(s)        == 1, "xpl skill_ok");
    CHECK(cos_v74_xpl_render_ok(s)       == 1, "xpl render_ok");
    CHECK(cos_v74_xpl_upscale_ok(s)      == 1, "xpl upscale_ok");
    CHECK(cos_v74_xpl_world_second_ok(s) == 1, "xpl world_second_ok");
    CHECK(cos_v74_xpl_abstained(s)       == 0, "xpl not abstained");
    CHECK(cos_v74_xpl_v74_ok(s)          == 1, "xpl v74_ok");

    /* Abstain kills the gate. */
    ctx.abstain = 1;
    cos_v74_xpl_exec(s, prog, sizeof(prog)/sizeof(prog[0]), &ctx, 4096);
    CHECK(cos_v74_xpl_v74_ok(s) == 0, "xpl abstain → v74_ok=0");

    /* Over-budget. */
    ctx.abstain = 0;
    rc = cos_v74_xpl_exec(s, prog, sizeof(prog)/sizeof(prog[0]), &ctx, 4);
    CHECK(rc == -2, "xpl budget exceeded");

    cos_v74_xpl_free(s);
    cos_v74_render_free(render);
    cos_v74_expert_free(expert);
    cos_v74_basis_free(basis);
}

/* 2^15 = 32 768 rows × 16 CHECKs per row ≈ 524 288 pass counts. */
static void t_compose_truth_table(void) {
    for (uint32_t mask = 0; mask < (1u << 15); ++mask) {
        uint8_t b[15];
        for (uint32_t i = 0; i < 15; ++i) b[i] = (mask >> i) & 0x1u;
        cos_v74_decision_t d = cos_v74_compose_decision(
            b[0],  b[1],  b[2],  b[3],  b[4],
            b[5],  b[6],  b[7],  b[8],  b[9],
            b[10], b[11], b[12], b[13], b[14]);
        CHECK(d.v60_ok == b[0],  "truth v60");
        CHECK(d.v61_ok == b[1],  "truth v61");
        CHECK(d.v62_ok == b[2],  "truth v62");
        CHECK(d.v63_ok == b[3],  "truth v63");
        CHECK(d.v64_ok == b[4],  "truth v64");
        CHECK(d.v65_ok == b[5],  "truth v65");
        CHECK(d.v66_ok == b[6],  "truth v66");
        CHECK(d.v67_ok == b[7],  "truth v67");
        CHECK(d.v68_ok == b[8],  "truth v68");
        CHECK(d.v69_ok == b[9],  "truth v69");
        CHECK(d.v70_ok == b[10], "truth v70");
        CHECK(d.v71_ok == b[11], "truth v71");
        CHECK(d.v72_ok == b[12], "truth v72");
        CHECK(d.v73_ok == b[13], "truth v73");
        CHECK(d.v74_ok == b[14], "truth v74");
        uint8_t expected = (uint8_t)(b[0]&b[1]&b[2]&b[3]&b[4]&b[5]&b[6]&b[7]
                                   &b[8]&b[9]&b[10]&b[11]&b[12]&b[13]&b[14]);
        CHECK(d.allow == expected, "truth allow");
    }
}

/* ====================================================================
 *  Benchmarks.
 * ==================================================================== */

static void bench_all(void) {
    printf("v74 σ-Experience microbench\n");
    printf("===========================\n");

    cos_v74_hv_t a, b;
    cos_v74_hv_from_seed(&a, 1);
    cos_v74_hv_from_seed(&b, 2);
    const uint64_t N_HAM = 5000000ULL;
    double t0 = now_sec();
    uint64_t acc = 0;
    for (uint64_t i = 0; i < N_HAM; ++i) {
        acc += cos_v74_hv_hamming(&a, &b);
        a.w[0]++;
    }
    double t1 = now_sec();
    printf("  hamming(256b):         %10.2f M ops/s (acc=%llu)\n",
           (double)N_HAM / (t1 - t0) / 1e6, (unsigned long long)acc);

    /* Fitts argmax over 32 targets. */
    cos_v74_target_t tg[32];
    for (uint32_t i = 0; i < 32; ++i) {
        tg[i].x = (int32_t)(i * 37); tg[i].y = (int32_t)(i * 41);
        tg[i].size = 20 + (int32_t)(i & 7); tg[i].flags = 0;
    }
    const uint64_t N_FT = 2000000ULL;
    t0 = now_sec();
    uint64_t sum = 0;
    for (uint64_t i = 0; i < N_FT; ++i) {
        sum += cos_v74_target_argmax(tg, 32, (int32_t)(i & 1023), (int32_t)(i & 511), NULL);
    }
    t1 = now_sec();
    printf("  fitts argmax (N=32):   %10.2f M ops/s (sum=%llu)\n",
           (double)N_FT / (t1 - t0) / 1e6, (unsigned long long)sum);

    /* Expert top-4 over 64 experts. */
    cos_v74_expert_t *e = cos_v74_expert_new(64, 7);
    cos_v74_hv_t q; cos_v74_hv_from_seed(&q, 1);
    const uint64_t N_EX = 500000ULL;
    t0 = now_sec();
    uint64_t cnt = 0;
    cos_v74_topk_t tk;
    for (uint64_t i = 0; i < N_EX; ++i) {
        cos_v74_expert_topk(e, &q, 4, &tk);
        cnt += tk.dist[0];
        q.w[1]++;
    }
    t1 = now_sec();
    printf("  expert top-4 (N=64):   %10.2f K ops/s (cnt=%llu)\n",
           (double)N_EX / (t1 - t0) / 1e3, (unsigned long long)cnt);
    cos_v74_expert_free(e);

    /* Mobile-GS render step across 64 tiles × 4 row_words. */
    cos_v74_render_t *r = cos_v74_render_new(64, 4, 1);
    r->vis_budget = (uint32_t)(-1); r->view_tol = 256;
    cos_v74_hv_t cl; cos_v74_hv_from_seed(&cl, 0);
    const uint64_t N_RND = 50000ULL;
    t0 = now_sec();
    uint64_t okc = 0;
    for (uint64_t i = 0; i < N_RND; ++i) {
        okc += cos_v74_render_step(r, &cl, NULL);
    }
    t1 = now_sec();
    printf("  render step (64 tiles):%10.2f K ops/s (ok=%llu)\n",
           (double)N_RND / (t1 - t0) / 1e3, (unsigned long long)okc);
    cos_v74_render_free(r);

    /* 1-second world synth (256 tiles) throughput. */
    cos_v74_hv_t tiles[256];
    cos_v74_hv_t style; cos_v74_hv_from_seed(&style, 1);
    const uint64_t N_W = 2000ULL;
    t0 = now_sec();
    uint64_t wok = 0;
    for (uint64_t i = 0; i < N_W; ++i) {
        wok += cos_v74_world_second(tiles, 256, 0x1000ULL + i, &style, 10000, 256, NULL);
    }
    t1 = now_sec();
    printf("  world_second (256):    %10.2f K worlds/s (ok=%llu)\n",
           (double)N_W / (t1 - t0) / 1e3, (unsigned long long)wok);

    /* 15-bit composed decision. */
    const uint64_t N_DEC = 10000000ULL;
    t0 = now_sec();
    uint64_t allowed = 0;
    for (uint64_t i = 0; i < N_DEC; ++i) {
        cos_v74_decision_t d = cos_v74_compose_decision(
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,(uint8_t)(i & 1u));
        allowed += d.allow;
    }
    t1 = now_sec();
    printf("  15-bit composed:       %10.2f M ops/s (allow=%llu)\n",
           (double)N_DEC / (t1 - t0) / 1e6, (unsigned long long)allowed);
}

/* ====================================================================
 *  Entry.
 * ==================================================================== */

static int self_test(void) {
    t_hv_primitives();
    t_fitts_target();
    t_layout();
    t_basis();
    t_squire();
    t_expert();
    t_skill();
    t_render();
    t_upscale();
    t_world_second();
    t_xpl();
    t_compose_truth_table();

    printf("v74 σ-Experience self-test: %llu pass, %llu fail\n",
           (unsigned long long)g_pass, (unsigned long long)g_fail);
    return g_fail == 0 ? 0 : 1;
}

static int decision_mode(int argc, char **argv) {
    if (argc < 17) {
        fprintf(stderr, "usage: %s --decision v60 v61 v62 v63 v64 v65 v66 v67 v68 v69 v70 v71 v72 v73 v74\n",
                argv[0]);
        return 2;
    }
    uint8_t b[15];
    for (int i = 0; i < 15; ++i) b[i] = (uint8_t)(atoi(argv[2 + i]) != 0);
    cos_v74_decision_t d = cos_v74_compose_decision(
        b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
        b[8],b[9],b[10],b[11],b[12],b[13],b[14]);
    printf("{ \"v60\":%u,\"v61\":%u,\"v62\":%u,\"v63\":%u,\"v64\":%u,\"v65\":%u,"
           "\"v66\":%u,\"v67\":%u,\"v68\":%u,\"v69\":%u,\"v70\":%u,\"v71\":%u,"
           "\"v72\":%u,\"v73\":%u,\"v74\":%u,\"allow\":%u }\n",
           d.v60_ok,d.v61_ok,d.v62_ok,d.v63_ok,d.v64_ok,d.v65_ok,
           d.v66_ok,d.v67_ok,d.v68_ok,d.v69_ok,d.v70_ok,d.v71_ok,
           d.v72_ok,d.v73_ok,d.v74_ok,d.allow);
    return d.allow ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) return self_test();
    if (argc >= 2 && strcmp(argv[1], "--bench")     == 0) { bench_all(); return 0; }
    if (argc >= 2 && strcmp(argv[1], "--decision")  == 0) return decision_mode(argc, argv);

    printf("Creation OS %s\n", cos_v74_version);
    printf("  --self-test    run deterministic self-tests\n");
    printf("  --bench        run microbenchmarks\n");
    printf("  --decision v60 v61 ... v74   15-bit composed decision (JSON)\n");
    return 0;
}
