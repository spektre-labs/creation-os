/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v76 — σ-Surface — standalone driver.
 *
 * Modes:
 *    --self-test    deterministic self-tests (target ≥ 100 000 pass,
 *                   including the full 2^16 = 65 536-row 16-bit
 *                   composed-decision truth table)
 *    --bench        microbenchmarks
 *    --decision v60 … v74 v76   one-shot 16-bit composed decision
 *    --version      one-line version
 *
 * Branchless integer-only hot path.  Libc-only.
 */

#include "surface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t g_pass = 0;
static uint64_t g_fail = 0;

#define CHECK(cond, label) do { \
    if (cond) { g_pass++; } \
    else      { g_fail++; fprintf(stderr, "FAIL: %s (%s:%d)\n", label, __FILE__, __LINE__); } \
} while (0)

static double now_sec(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

/* ====================================================================
 *  Self-tests.
 * ==================================================================== */

static void t_hv_primitives(void) {
    cos_v76_hv_t a, b, c;
    cos_v76_hv_from_seed(&a, 1);
    cos_v76_hv_from_seed(&b, 2);
    cos_v76_hv_xor(&c, &a, &b);
    CHECK(c.w[0] == (a.w[0] ^ b.w[0]), "hv_xor w0");
    CHECK(c.w[3] == (a.w[3] ^ b.w[3]), "hv_xor w3");
    cos_v76_hv_t rr;
    cos_v76_hv_xor(&rr, &c, &b);
    CHECK(rr.w[0] == a.w[0] && rr.w[3] == a.w[3], "hv_xor involution");
    CHECK(cos_v76_hv_hamming(&a, &a) == 0, "hv_hamming self-zero");
    CHECK(cos_v76_hv_hamming(&a, &b) <= 256u, "hv_hamming bounded");

    cos_v76_hv_t p1, p2, p3;
    cos_v76_hv_permute(&p1, &a, 0);
    cos_v76_hv_permute(&p2, &a, 0);
    cos_v76_hv_permute(&p3, &a, 1);
    CHECK(p1.w[0] == p2.w[0] && p1.w[3] == p2.w[3], "permute deterministic");
    CHECK(p1.w[0] != p3.w[0] || p1.w[1] != p3.w[1], "permute key-sensitive");

    /* from_seed fuzz: 4096 distinct outputs. */
    uint64_t last = 0;
    for (uint32_t i = 0; i < 4096; ++i) {
        cos_v76_hv_t h;
        cos_v76_hv_from_seed(&h, 0x200000ull + i);
        CHECK(h.w[0] != last, "from_seed distinct");
        last = h.w[0];
    }
}

static void t_touch(void) {
    cos_v76_touch_t t = { 12345, 6789, 16384, 1, 1000 };
    CHECK(cos_v76_touch_ok(&t) == 1, "touch_ok valid");

    cos_v76_touch_t bad1 = { 40000, 0, 0, 0, 0 };
    CHECK(cos_v76_touch_ok(&bad1) == 0, "touch_ok x too big");
    cos_v76_touch_t bad2 = { 0, 40000, 0, 0, 0 };
    CHECK(cos_v76_touch_ok(&bad2) == 0, "touch_ok y too big");
    cos_v76_touch_t bad3 = { 0, 0, 40000, 0, 0 };
    CHECK(cos_v76_touch_ok(&bad3) == 0, "touch_ok pressure too big");
    cos_v76_touch_t bad4 = { 0, 0, 0, 4, 0 };
    CHECK(cos_v76_touch_ok(&bad4) == 0, "touch_ok phase out of range");

    cos_v76_hv_t h1, h2, h3;
    cos_v76_touch_decode(&h1, &t);
    cos_v76_touch_decode(&h2, &t);
    CHECK(memcmp(&h1, &h2, sizeof h1) == 0, "touch_decode deterministic");
    cos_v76_touch_t t2 = t; t2.phase = 2;
    cos_v76_touch_decode(&h3, &t2);
    CHECK(memcmp(&h1, &h3, sizeof h1) != 0, "touch_decode phase-sensitive");

    /* Fuzz: 8192 random-ish events all decode with bounded hamming distance. */
    for (uint32_t i = 0; i < 8192; ++i) {
        cos_v76_touch_t tt;
        tt.x_q15        = (uint16_t)(i * 37 & 0x7FFF);
        tt.y_q15        = (uint16_t)(i * 53 & 0x7FFF);
        tt.pressure_q15 = (uint16_t)(i * 19 & 0x7FFF);
        tt.phase        = (uint16_t)(i & 3);
        tt.timestamp_ms = i * 7u;
        CHECK(cos_v76_touch_ok(&tt) == 1, "touch_ok fuzz");
        cos_v76_hv_t h;
        cos_v76_touch_decode(&h, &tt);
        CHECK(h.w[0] != 0 || h.w[1] != 0 || h.w[2] != 0 || h.w[3] != 0,
              "touch_decode nonzero");
    }
}

static void t_gesture(void) {
    cos_v76_gesture_bank_t g;
    cos_v76_gesture_bank_init(&g, 0x1234);

    /* Feeding a template back in must classify it to its own id with
     * distance 0 (exact self-match). */
    for (uint32_t i = 0; i < COS_V76_GESTURE_TEMPLATES; ++i) {
        cos_v76_gesture_result_t r;
        cos_v76_gesture_classify(&g, &g.bank[i], &r);
        CHECK(r.idx  == i,  "gesture self-match id");
        CHECK(r.dist == 0u, "gesture self-match dist zero");
        CHECK(r.margin > 0u, "gesture self-match margin positive");
        CHECK(cos_v76_gesture_ok(&r, 0u, 1u) == 1, "gesture_ok self-match");
    }

    /* Feeding an unrelated HV: ok must be 0 under a tight margin. */
    cos_v76_hv_t far;
    cos_v76_hv_from_seed(&far, 0xdeadbeef);
    cos_v76_gesture_result_t r;
    cos_v76_gesture_classify(&g, &far, &r);
    CHECK(r.dist <= 256u, "gesture far dist bounded");
    CHECK(cos_v76_gesture_ok(&r, 0u, 1u) == 0, "gesture_ok unrelated strict fails");
}

static void t_haptic(void) {
    int16_t *wave = (int16_t *)aligned_alloc(64, COS_V76_HAPTIC_SAMPLES_MAX * sizeof(int16_t));
    CHECK(wave != NULL, "haptic wave alloc");

    /* Sine: total energy > 0, peak ≤ amp. */
    int64_t e = cos_v76_haptic_generate(wave, 64, 0, 32767, 64);
    CHECK(e > 0, "haptic sine energy positive");
    CHECK(cos_v76_haptic_ok(wave, 64, 32767, e, 1LL << 40) == 1, "haptic sine ok");

    /* Ramp. */
    e = cos_v76_haptic_generate(wave, 64, 1, 16384, 64);
    CHECK(cos_v76_haptic_ok(wave, 64, 16384, e, 1LL << 40) == 1, "haptic ramp ok");

    /* Impulse: only wave[0] non-zero. */
    e = cos_v76_haptic_generate(wave, 64, 2, 32767, 64);
    uint32_t nz = 0;
    for (uint32_t i = 0; i < 64; ++i) nz += (wave[i] != 0);
    CHECK(nz == 1, "haptic impulse single sample");
    CHECK(cos_v76_haptic_ok(wave, 64, 32767, e, 1LL << 40) == 1, "haptic impulse ok");

    /* Energy cap failure. */
    e = cos_v76_haptic_generate(wave, 256, 0, 32767, 64);
    CHECK(cos_v76_haptic_ok(wave, 256, 32767, e, 1024) == 0, "haptic energy cap fails");

    /* Amp cap failure. */
    e = cos_v76_haptic_generate(wave, 64, 0, 32767, 64);
    CHECK(cos_v76_haptic_ok(wave, 64, 100, e, 1LL << 40) == 0, "haptic amp cap fails");

    free(wave);
}

static void t_msg(void) {
    cos_v76_hv_t payload, recipient, sender;
    cos_v76_hv_from_seed(&payload,   0xAAAA);
    cos_v76_hv_from_seed(&recipient, 0xBBBB);
    cos_v76_hv_from_seed(&sender,    0xCCCC);

    for (uint32_t p = 0; p < COS_V76_PROTOCOLS; ++p) {
        cos_v76_msg_t m = {
            .protocol_id = (uint8_t)p,
            .payload_hv  = &payload,
            .recipient_hv= &recipient,
            .sender_hv   = &sender,
        };
        CHECK(cos_v76_msg_ok(&m) == 1, "msg_ok valid");
        cos_v76_hv_t env;
        cos_v76_msg_encode(&env, &m);
        CHECK(cos_v76_msg_protocol(&env) == (uint8_t)p, "msg protocol recoverable");
    }

    /* Unknown protocol id fails msg_ok. */
    cos_v76_msg_t bad = {
        .protocol_id  = 99,
        .payload_hv   = &payload,
        .recipient_hv = &recipient,
        .sender_hv    = &sender,
    };
    CHECK(cos_v76_msg_ok(&bad) == 0, "msg_ok unknown protocol fails");

    cos_v76_msg_t nil = { 0, NULL, NULL, NULL };
    CHECK(cos_v76_msg_ok(&nil) == 0, "msg_ok null ptrs fails");
}

static void t_ratchet(void) {
    uint8_t seed[32];
    for (uint32_t i = 0; i < 32; ++i) seed[i] = (uint8_t)(i * 7 + 3);

    cos_v76_ratchet_t r;
    cos_v76_ratchet_init(&r, seed);
    CHECK(cos_v76_ratchet_ok(&r) == 1, "ratchet init ok");

    uint8_t k1[32], k2[32], k3[32];
    cos_v76_ratchet_step(&r, k1);
    cos_v76_ratchet_step(&r, k2);
    cos_v76_ratchet_step(&r, k3);
    CHECK(memcmp(k1, k2, 32) != 0, "ratchet k1 != k2");
    CHECK(memcmp(k2, k3, 32) != 0, "ratchet k2 != k3");
    CHECK(memcmp(k1, k3, 32) != 0, "ratchet k1 != k3");
    CHECK(r.counter == 3, "ratchet counter advances");

    /* Determinism: rebuild ratchet, re-step, keys must match. */
    cos_v76_ratchet_t r2;
    cos_v76_ratchet_init(&r2, seed);
    uint8_t k1b[32], k2b[32], k3b[32];
    cos_v76_ratchet_step(&r2, k1b);
    cos_v76_ratchet_step(&r2, k2b);
    cos_v76_ratchet_step(&r2, k3b);
    CHECK(memcmp(k1, k1b, 32) == 0, "ratchet deterministic k1");
    CHECK(memcmp(k2, k2b, 32) == 0, "ratchet deterministic k2");
    CHECK(memcmp(k3, k3b, 32) == 0, "ratchet deterministic k3");

    /* Mixing changes chain_key so next step diverges. */
    uint8_t dh_a[32], dh_b[32];
    for (uint32_t i = 0; i < 32; ++i) { dh_a[i] = (uint8_t)(i ^ 0x5A); dh_b[i] = (uint8_t)(i ^ 0xA5); }
    cos_v76_ratchet_mix(&r, dh_a, dh_b);
    CHECK(r.counter == 0, "ratchet mix resets counter");
    uint8_t k4[32];
    cos_v76_ratchet_step(&r, k4);
    CHECK(memcmp(k4, k1, 32) != 0, "ratchet mix diverges");
}

static void t_a11y(void) {
    cos_v76_a11y_t good = { 20000, 1, 1, 48, 1, {0,0,0} };
    CHECK(cos_v76_a11y_ok(&good) == 1, "a11y good");

    cos_v76_a11y_t low_contrast = good; low_contrast.contrast_q15 = 1000;
    CHECK(cos_v76_a11y_ok(&low_contrast) == 0, "a11y low contrast fails");
    cos_v76_a11y_t no_focus = good; no_focus.focus_order_ok = 0;
    CHECK(cos_v76_a11y_ok(&no_focus) == 0, "a11y no focus fails");
    cos_v76_a11y_t motion = good; motion.motion_safe = 0;
    CHECK(cos_v76_a11y_ok(&motion) == 0, "a11y motion unsafe fails");
    cos_v76_a11y_t tiny = good; tiny.touch_target_px = 20;
    CHECK(cos_v76_a11y_ok(&tiny) == 0, "a11y tiny target fails");
    cos_v76_a11y_t no_label = good; no_label.label_present = 0;
    CHECK(cos_v76_a11y_ok(&no_label) == 0, "a11y no label fails");
}

static void t_crdt(void) {
    cos_v76_crdt_t a = { 10, 1, 0, 100, { { 0x1, 0x2, 0x4, 0x8 } } };
    cos_v76_crdt_t b = { 20, 2, 0, 200, { { 0x10, 0x20, 0x40, 0x80 } } };
    cos_v76_crdt_t c1, c2;
    cos_v76_crdt_merge(&c1, &a, &b);
    cos_v76_crdt_merge(&c2, &b, &a);
    CHECK(c1.timestamp == c2.timestamp, "crdt commutative ts");
    CHECK(c1.replica_id == c2.replica_id, "crdt commutative rep");
    CHECK(c1.value == c2.value, "crdt commutative val");
    CHECK(c1.or_set.w[0] == c2.or_set.w[0], "crdt commutative orset");
    CHECK(c1.timestamp == 20, "crdt picks later");
    CHECK(c1.value == 200, "crdt picks later value");
    CHECK(c1.or_set.w[0] == 0x11, "crdt or-set union");

    /* Tie-break by replica id. */
    cos_v76_crdt_t t1 = { 10, 5, 0, 111, { { 0, 0, 0, 0 } } };
    cos_v76_crdt_t t2 = { 10, 3, 0, 222, { { 0, 0, 0, 0 } } };
    cos_v76_crdt_t tc;
    cos_v76_crdt_merge(&tc, &t1, &t2);
    CHECK(tc.replica_id == 5, "crdt tie-break by replica");
    CHECK(tc.value == 111, "crdt tie-break value");
}

static void t_legacy(void) {
    cos_v76_legacy_t L;
    cos_v76_legacy_init(&L);

    for (uint32_t i = 0; i < COS_V76_LEGACY_APPS; ++i) {
        cos_v76_legacy_result_t r;
        cos_v76_legacy_match(&L, &L.bank[i], &r);
        CHECK(r.app_id == i, "legacy self-match id");
        CHECK(r.dist == 0u, "legacy self-match dist");
        CHECK(r.margin > 0u, "legacy self-match margin");
        CHECK(cos_v76_legacy_ok(&r, 0u, 1u) == 1, "legacy_ok self-match");
    }

    /* Spot-check category assignment. */
    CHECK(L.category[COS_V76_APP_WORD]       == COS_V76_CAT_OFFICE,  "cat WORD office");
    CHECK(L.category[COS_V76_APP_PHOTOSHOP]  == COS_V76_CAT_DESIGN,  "cat PS design");
    CHECK(L.category[COS_V76_APP_AUTOCAD]    == COS_V76_CAT_CAD,     "cat AutoCAD cad");
    CHECK(L.category[COS_V76_APP_SAP]        == COS_V76_CAT_ERP_CRM, "cat SAP erp");
    CHECK(L.category[COS_V76_APP_FIGMA]      == COS_V76_CAT_COLLAB,  "cat Figma collab");
    CHECK(L.category[COS_V76_APP_XCODE]      == COS_V76_CAT_DEV,     "cat Xcode dev");
    CHECK(L.category[COS_V76_APP_POSTGRES]   == COS_V76_CAT_DB,      "cat Postgres db");
    CHECK(L.category[COS_V76_APP_AWS]        == COS_V76_CAT_CLOUD,   "cat AWS cloud");
    CHECK(L.category[COS_V76_APP_CHROME]     == COS_V76_CAT_BROWSER, "cat Chrome browser");
}

static void t_format(void) {
    cos_v76_format_bank_t F;
    cos_v76_format_bank_init(&F);
    for (uint32_t i = 0; i < COS_V76_FORMATS; ++i) {
        cos_v76_format_result_t r;
        cos_v76_format_classify(&F, &F.bank[i], &r);
        CHECK(r.fmt_id == i, "format self-match id");
        CHECK(r.dist == 0u, "format self-match dist");
        CHECK(r.margin > 0u, "format self-match margin");
    }
}

static void t_sbl(void) {
    /* Build a full 9-opcode + GATE program exercising every primitive. */
    cos_v76_sbl_state_t *s = cos_v76_sbl_new();
    CHECK(s != NULL, "sbl alloc");

    cos_v76_gesture_bank_t g;
    cos_v76_gesture_bank_init(&g, 0xBEEF);
    cos_v76_legacy_t L;
    cos_v76_legacy_init(&L);

    cos_v76_touch_t t = { 12345, 6789, 16384, 1, 1000 };

    int16_t *wave = (int16_t *)aligned_alloc(64, COS_V76_HAPTIC_SAMPLES_MAX * sizeof(int16_t));

    cos_v76_hv_t payload, recipient, sender;
    cos_v76_hv_from_seed(&payload,   0xA1);
    cos_v76_hv_from_seed(&recipient, 0xB2);
    cos_v76_hv_from_seed(&sender,    0xC3);
    cos_v76_msg_t m = {
        .protocol_id = COS_V76_P_SIGNAL,
        .payload_hv  = &payload,
        .recipient_hv= &recipient,
        .sender_hv   = &sender,
    };

    uint8_t seed[32]; for (uint32_t i = 0; i < 32; ++i) seed[i] = (uint8_t)(i ^ 0x33);
    cos_v76_ratchet_t rt;
    cos_v76_ratchet_init(&rt, seed);
    uint8_t mk[32];

    cos_v76_a11y_t a = { 20000, 1, 1, 48, 1, {0,0,0} };

    cos_v76_crdt_t ca = { 10, 1, 0, 100, { { 1, 2, 4, 8 } } };
    cos_v76_crdt_t cb = { 20, 2, 0, 200, { { 16, 32, 64, 128 } } };
    cos_v76_crdt_t cdst;

    cos_v76_sbl_ctx_t ctx = {0};
    ctx.touch = &t;
    ctx.gest = &g;
    ctx.gest_q = &g.bank[COS_V76_G_TAP];  /* exact match */
    ctx.gest_tol = 0; ctx.gest_margin = 1;
    ctx.haptic_wave = wave; ctx.haptic_n = 64;
    ctx.haptic_kind = 0; ctx.haptic_amp_q15 = 16384; ctx.haptic_period = 64;
    ctx.haptic_amp_cap_q15 = 32767; ctx.haptic_energy_cap = 1LL << 40;
    ctx.msg = &m;
    ctx.ratchet = &rt; ctx.out_message_key = mk;
    ctx.a11y = &a;
    ctx.crdt_dst = &cdst; ctx.crdt_a = &ca; ctx.crdt_b = &cb;
    ctx.legacy = &L; ctx.legacy_query = &L.bank[COS_V76_APP_WORD];
    ctx.legacy_tol = 0; ctx.legacy_margin = 1;
    ctx.abstain = 0;

    cos_v76_sbl_inst_t prog[] = {
        { COS_V76_OP_TOUCH,   0, 0, 0, 0, 0 },
        { COS_V76_OP_GESTURE, 0, 0, 0, 0, 0 },
        { COS_V76_OP_HAPTIC,  0, 0, 0, 0, 0 },
        { COS_V76_OP_MSG,     0, 0, 0, 0, 0 },
        { COS_V76_OP_E2E,     0, 0, 0, 0, 0 },
        { COS_V76_OP_A11Y,    0, 0, 0, 0, 0 },
        { COS_V76_OP_SYNC,    0, 0, 0, 0, 0 },
        { COS_V76_OP_LEGACY,  0, 0, 0, 0, 0 },
        { COS_V76_OP_GATE,    0, 0, 0, 0, 0 },
    };
    int rc = cos_v76_sbl_exec(s, prog, sizeof prog / sizeof prog[0], &ctx, 1024);
    CHECK(rc == 0, "sbl full program returns 0");
    CHECK(cos_v76_sbl_touch_ok(s)   == 1, "sbl touch_ok");
    CHECK(cos_v76_sbl_gesture_ok(s) == 1, "sbl gesture_ok");
    CHECK(cos_v76_sbl_haptic_ok(s)  == 1, "sbl haptic_ok");
    CHECK(cos_v76_sbl_msg_ok(s)     == 1, "sbl msg_ok");
    CHECK(cos_v76_sbl_e2e_ok(s)     == 1, "sbl e2e_ok");
    CHECK(cos_v76_sbl_a11y_ok(s)    == 1, "sbl a11y_ok");
    CHECK(cos_v76_sbl_sync_ok(s)    == 1, "sbl sync_ok");
    CHECK(cos_v76_sbl_legacy_ok(s)  == 1, "sbl legacy_ok");
    CHECK(cos_v76_sbl_v76_ok(s)     == 1, "sbl v76_ok");
    CHECK(cos_v76_sbl_units(s)      <= 1024u, "sbl within budget");

    /* Abstain must zero v76_ok. */
    ctx.abstain = 1;
    rc = cos_v76_sbl_exec(s, prog, sizeof prog / sizeof prog[0], &ctx, 1024);
    CHECK(rc == 0, "sbl abstain exec");
    CHECK(cos_v76_sbl_v76_ok(s) == 0, "sbl abstain zeros v76_ok");

    /* Budget exhaustion. */
    ctx.abstain = 0;
    rc = cos_v76_sbl_exec(s, prog, sizeof prog / sizeof prog[0], &ctx, 5);
    CHECK(rc == -2, "sbl budget exceeded");

    free(wave);
    cos_v76_sbl_free(s);
}

static void t_compose_16bit_truth_table(void) {
    /* Full 2^16 = 65 536-row truth table. */
    for (uint32_t mask = 0; mask < (1u << 16); ++mask) {
        uint8_t v[16];
        for (uint32_t i = 0; i < 16; ++i) v[i] = (uint8_t)((mask >> i) & 1u);
        cos_v76_decision_t d = cos_v76_compose_decision(
            v[0],  v[1],  v[2],  v[3],  v[4],  v[5],  v[6],  v[7],
            v[8],  v[9],  v[10], v[11], v[12], v[13], v[14], v[15]);
        uint8_t expected = (mask == 0xFFFFu) ? 1u : 0u;
        if (d.allow != expected) { g_fail++;
            if (g_fail < 4) fprintf(stderr, "FAIL compose mask=%04x got=%u want=%u\n",
                                     mask, d.allow, expected);
        } else { g_pass++; }
    }
}

/* ====================================================================
 *  Microbenchmarks.
 * ==================================================================== */

static void bench(void) {
    printf("=== v76 σ-Surface microbenchmarks ===\n");

    /* Hamming. */
    cos_v76_hv_t a, b;
    cos_v76_hv_from_seed(&a, 1); cos_v76_hv_from_seed(&b, 2);
    uint32_t acc = 0;
    double t0 = now_sec();
    for (uint32_t i = 0; i < 10000000u; ++i) { acc += cos_v76_hv_hamming(&a, &b); b.w[0] += acc; }
    double dt = now_sec() - t0;
    printf("  hamming            : %.2f M ops/s  (acc=%u)\n", 10.0 / dt, acc);

    /* Gesture classify. */
    cos_v76_gesture_bank_t g; cos_v76_gesture_bank_init(&g, 0xABCD);
    cos_v76_gesture_result_t r;
    t0 = now_sec();
    for (uint32_t i = 0; i < 2000000u; ++i) {
        cos_v76_hv_t q; cos_v76_hv_from_seed(&q, i);
        cos_v76_gesture_classify(&g, &q, &r);
    }
    dt = now_sec() - t0;
    printf("  gesture classify   : %.2f M ops/s  (last idx=%u)\n", 2.0 / dt, r.idx);

    /* Legacy match (64-bank). */
    cos_v76_legacy_t L; cos_v76_legacy_init(&L);
    cos_v76_legacy_result_t lr;
    t0 = now_sec();
    for (uint32_t i = 0; i < 500000u; ++i) {
        cos_v76_hv_t q; cos_v76_hv_from_seed(&q, i);
        cos_v76_legacy_match(&L, &q, &lr);
    }
    dt = now_sec() - t0;
    printf("  legacy match (64)  : %.2f M ops/s  (last app=%u)\n", 0.5 / dt, lr.app_id);

    /* Ratchet. */
    uint8_t seed[32] = {0}; cos_v76_ratchet_t rt; cos_v76_ratchet_init(&rt, seed);
    uint8_t mk[32];
    t0 = now_sec();
    for (uint32_t i = 0; i < 200000u; ++i) cos_v76_ratchet_step(&rt, mk);
    dt = now_sec() - t0;
    printf("  ratchet step       : %.2f k steps/s\n", 200.0 / dt);

    /* 16-bit compose. */
    volatile uint32_t sink = 0;
    t0 = now_sec();
    for (uint32_t i = 0; i < 20000000u; ++i) {
        cos_v76_decision_t d = cos_v76_compose_decision(
            (i>>0)&1,(i>>1)&1,(i>>2)&1,(i>>3)&1,(i>>4)&1,(i>>5)&1,(i>>6)&1,(i>>7)&1,
            (i>>8)&1,(i>>9)&1,(i>>10)&1,(i>>11)&1,(i>>12)&1,(i>>13)&1,(i>>14)&1,(i>>15)&1);
        sink += d.allow;
    }
    dt = now_sec() - t0;
    printf("  compose (16-bit)   : %.2f M ops/s  (sink=%u)\n", 20.0 / dt, sink);
}

/* ====================================================================
 *  main.
 * ==================================================================== */

static int mode_decision(int argc, char **argv) {
    if (argc != 17) {
        fprintf(stderr, "usage: %s --decision v60 v61 v62 v63 v64 v65 v66 v67 v68 v69 v70 v71 v72 v73 v74 v76\n", argv[0]);
        return 64;
    }
    uint8_t v[16];
    for (uint32_t i = 0; i < 16; ++i) v[i] = (uint8_t)(atoi(argv[i + 1]) & 1);
    cos_v76_decision_t d = cos_v76_compose_decision(
        v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7],
        v[8], v[9], v[10], v[11], v[12], v[13], v[14], v[15]);
    printf("{\"allow\":%u,\"v60\":%u,\"v61\":%u,\"v62\":%u,\"v63\":%u,\"v64\":%u,"
           "\"v65\":%u,\"v66\":%u,\"v67\":%u,\"v68\":%u,\"v69\":%u,\"v70\":%u,"
           "\"v71\":%u,\"v72\":%u,\"v73\":%u,\"v74\":%u,\"v76\":%u}\n",
           d.allow, d.v60_ok, d.v61_ok, d.v62_ok, d.v63_ok, d.v64_ok,
           d.v65_ok, d.v66_ok, d.v67_ok, d.v68_ok, d.v69_ok, d.v70_ok,
           d.v71_ok, d.v72_ok, d.v73_ok, d.v74_ok, d.v76_ok);
    return d.allow ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s\n", cos_v76_version);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--decision") == 0) {
        return mode_decision(argc - 1, argv + 1);
    }
    if (argc >= 2 && strcmp(argv[1], "--bench") == 0) {
        bench();
        return 0;
    }
    if (argc < 2 || strcmp(argv[1], "--self-test") == 0) {
        t_hv_primitives();
        t_touch();
        t_gesture();
        t_haptic();
        t_msg();
        t_ratchet();
        t_a11y();
        t_crdt();
        t_legacy();
        t_format();
        t_sbl();
        t_compose_16bit_truth_table();
        printf("v76 self-test: pass=%llu fail=%llu\n",
               (unsigned long long)g_pass, (unsigned long long)g_fail);
        return (g_fail == 0) ? 0 : 1;
    }
    fprintf(stderr, "usage: %s [--self-test | --bench | --decision v60..v76 | --version]\n", argv[0]);
    return 64;
}
