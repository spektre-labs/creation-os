/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v61 — Σ-Citadel driver: deterministic ≥60-test self-test,
 * architecture / positioning banners, attestation demo.
 */

#include "citadel.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int g_pass = 0;
static int g_fail = 0;

#define TEST_OK(label, cond) do { \
    if (cond) { ++g_pass; } \
    else { ++g_fail; fprintf(stderr, "FAIL: %s (line %d)\n", (label), __LINE__); } \
} while (0)

static cos_v61_label_t mklab(uint8_t cl, uint8_t it, uint16_t cp)
{
    cos_v61_label_t l;
    l.clearance = cl;
    l.integrity = it;
    l.compartments = cp;
    l._reserved = 0;
    return l;
}

/* ------------------------------------------------------------------ */
/* 1. Version / policy                                                 */
/* ------------------------------------------------------------------ */

static void test_version(void)
{
    cos_v61_version_t v = cos_v61_version();
    TEST_OK("ver_major_61", v.major == 61);
    TEST_OK("ver_minor_0",  v.minor == 0);
    TEST_OK("ver_patch_ge1", v.patch >= 1);
}

static void test_policy_defaults(void)
{
    cos_v61_lattice_policy_t p;
    cos_v61_lattice_policy_default(&p);
    TEST_OK("policy_min_integrity_zero", p.min_integrity == 0);
    cos_v61_lattice_policy_default(NULL); /* must not crash */
    TEST_OK("policy_null_safe", 1);
}

/* ------------------------------------------------------------------ */
/* 2. Bell-LaPadula                                                    */
/* ------------------------------------------------------------------ */

static void test_blp_no_read_up(void)
{
    /* PUBLIC subject should not read TOP SECRET object. */
    cos_v61_label_t s = mklab(10, 128, 0xFFFF);
    cos_v61_label_t o = mklab(200, 128, 0xFFFF);
    TEST_OK("blp_no_read_up",
            cos_v61_lattice_check(s, o, COS_V61_OP_READ) == 0);
}

static void test_blp_read_down_ok(void)
{
    /* TOP SECRET subject CAN read PUBLIC object (read-down). */
    cos_v61_label_t s = mklab(200, 128, 0xFFFF);
    cos_v61_label_t o = mklab(10,  128, 0xFFFF);
    TEST_OK("blp_read_down_ok",
            cos_v61_lattice_check(s, o, COS_V61_OP_READ) == 1);
}

static void test_blp_no_write_down(void)
{
    /* TOP SECRET subject cannot write PUBLIC object (prevents leak). */
    cos_v61_label_t s = mklab(200, 128, 0xFFFF);
    cos_v61_label_t o = mklab(10,  128, 0xFFFF);
    TEST_OK("blp_no_write_down",
            cos_v61_lattice_check(s, o, COS_V61_OP_WRITE) == 0);
}

static void test_blp_write_up_ok(void)
{
    /* PUBLIC subject CAN write to TOP SECRET object (write-up). */
    cos_v61_label_t s = mklab(10,  128, 0xFFFF);
    cos_v61_label_t o = mklab(200, 128, 0xFFFF);
    TEST_OK("blp_write_up_ok",
            cos_v61_lattice_check(s, o, COS_V61_OP_WRITE) == 1);
}

static void test_blp_equal_levels(void)
{
    cos_v61_label_t s = mklab(128, 128, 0xFFFF);
    cos_v61_label_t o = mklab(128, 128, 0xFFFF);
    TEST_OK("blp_equal_read_ok",
            cos_v61_lattice_check(s, o, COS_V61_OP_READ) == 1);
    TEST_OK("blp_equal_write_ok",
            cos_v61_lattice_check(s, o, COS_V61_OP_WRITE) == 1);
}

/* ------------------------------------------------------------------ */
/* 3. Biba integrity                                                   */
/* ------------------------------------------------------------------ */

static void test_biba_no_read_down(void)
{
    /* HIGH-integrity subject cannot read LOW-integrity object
     * (untrusted data would pollute trusted subject).  With
     * clearances equal, the Biba axis decides. */
    cos_v61_label_t s = mklab(128, 200, 0xFFFF);
    cos_v61_label_t o = mklab(128, 10,  0xFFFF);
    TEST_OK("biba_no_read_down",
            cos_v61_lattice_check(s, o, COS_V61_OP_READ) == 0);
}

static void test_biba_read_up_ok(void)
{
    /* LOW-integrity subject CAN read HIGH-integrity (read-up trusts). */
    cos_v61_label_t s = mklab(128, 10,  0xFFFF);
    cos_v61_label_t o = mklab(128, 200, 0xFFFF);
    TEST_OK("biba_read_up_ok",
            cos_v61_lattice_check(s, o, COS_V61_OP_READ) == 1);
}

static void test_biba_no_write_up(void)
{
    /* LOW-integrity subject cannot write HIGH-integrity object
     * (prevents untrusted input from corrupting trusted storage). */
    cos_v61_label_t s = mklab(128, 10,  0xFFFF);
    cos_v61_label_t o = mklab(128, 200, 0xFFFF);
    TEST_OK("biba_no_write_up",
            cos_v61_lattice_check(s, o, COS_V61_OP_WRITE) == 0);
}

static void test_biba_write_down_ok(void)
{
    cos_v61_label_t s = mklab(128, 200, 0xFFFF);
    cos_v61_label_t o = mklab(128, 10,  0xFFFF);
    TEST_OK("biba_write_down_ok",
            cos_v61_lattice_check(s, o, COS_V61_OP_WRITE) == 1);
}

/* ------------------------------------------------------------------ */
/* 4. Combined BLP + Biba (the classical "contradiction" resolved)     */
/* ------------------------------------------------------------------ */

static void test_combined_strict(void)
{
    /* Classic combined BLP+Biba: read is only allowed when BOTH axes
     * agree.  PUBLIC+LOW vs TOP_SECRET+HIGH: BLP says no-read-up,
     * Biba says no-read-down — both fail, so of course both ops fail
     * here, but the point is to exercise the ∧ in lattice_core. */
    cos_v61_label_t s = mklab(10,  10,  0xFFFF);
    cos_v61_label_t o = mklab(200, 200, 0xFFFF);
    /* BLP: s < o → read = no-read-up → DENY.
     * Biba: s < o → read = read-up OK.
     * Combined: DENY (BLP fails). */
    TEST_OK("combined_blp_denies",
            cos_v61_lattice_check(s, o, COS_V61_OP_READ) == 0);
}

static void test_combined_only_equal_is_always_ok(void)
{
    cos_v61_label_t s = mklab(128, 128, 0xFFFF);
    cos_v61_label_t o = mklab(128, 128, 0xFFFF);
    TEST_OK("combined_equal_read", cos_v61_lattice_check(s, o, COS_V61_OP_READ) == 1);
    TEST_OK("combined_equal_write", cos_v61_lattice_check(s, o, COS_V61_OP_WRITE) == 1);
    TEST_OK("combined_equal_exec",  cos_v61_lattice_check(s, o, COS_V61_OP_EXEC)  == 1);
}

/* ------------------------------------------------------------------ */
/* 5. Compartments (MLS)                                               */
/* ------------------------------------------------------------------ */

static void test_compartment_match(void)
{
    cos_v61_label_t s = mklab(128, 128, 0x000F);  /* compartments 0..3 */
    cos_v61_label_t o = mklab(128, 128, 0x0003);  /* needs 0 and 1     */
    TEST_OK("compart_superset_ok",
            cos_v61_lattice_check(s, o, COS_V61_OP_READ) == 1);
}

static void test_compartment_missing(void)
{
    cos_v61_label_t s = mklab(128, 128, 0x0001);  /* only compartment 0 */
    cos_v61_label_t o = mklab(128, 128, 0x0002);  /* needs compartment 1 */
    TEST_OK("compart_missing_denies",
            cos_v61_lattice_check(s, o, COS_V61_OP_READ) == 0);
}

static void test_compartment_empty_object_ok(void)
{
    cos_v61_label_t s = mklab(128, 128, 0x0000);
    cos_v61_label_t o = mklab(128, 128, 0x0000);  /* no compartment requirement */
    TEST_OK("compart_empty_is_vacuous_ok",
            cos_v61_lattice_check(s, o, COS_V61_OP_READ) == 1);
}

/* ------------------------------------------------------------------ */
/* 6. Exec                                                             */
/* ------------------------------------------------------------------ */

static void test_exec_requires_both(void)
{
    /* Equal levels → exec OK. */
    cos_v61_label_t s = mklab(128, 128, 0x0001);
    cos_v61_label_t o = mklab(128, 128, 0x0001);
    TEST_OK("exec_equal_ok",
            cos_v61_lattice_check(s, o, COS_V61_OP_EXEC) == 1);
}

static void test_exec_denies_on_any_axis_diff(void)
{
    cos_v61_label_t s = mklab(200, 128, 0x0001);
    cos_v61_label_t o = mklab(10,  128, 0x0001);
    /* BLP: read-down OK but write-down NO; exec = read ∧ write → NO */
    TEST_OK("exec_asymmetric_denies",
            cos_v61_lattice_check(s, o, COS_V61_OP_EXEC) == 0);
}

/* ------------------------------------------------------------------ */
/* 7. Unknown op                                                       */
/* ------------------------------------------------------------------ */

static void test_unknown_op_denies(void)
{
    cos_v61_label_t s = mklab(128, 128, 0xFFFF);
    cos_v61_label_t o = mklab(128, 128, 0xFFFF);
    TEST_OK("unknown_op_denies", cos_v61_lattice_check(s, o, 999) == 0);
    TEST_OK("zero_op_denies",    cos_v61_lattice_check(s, o, 0)   == 0);
}

/* ------------------------------------------------------------------ */
/* 8. Policy overlay                                                   */
/* ------------------------------------------------------------------ */

static void test_policy_quarantine(void)
{
    cos_v61_lattice_policy_t p;
    cos_v61_lattice_policy_default(&p);
    p.min_integrity = 100;
    /* Subject with LOW integrity 10, object with integrity 50.
     * Lattice alone: clearance ok, Biba says s.integ(10) <= o.integ(50)
     * so read-up is allowed; compartments match. → allowed (1).
     * Policy overlay: obj.integrity (50) < min_integrity (100) → deny. */
    cos_v61_label_t s = mklab(128, 10,  0xFFFF);
    cos_v61_label_t o = mklab(128, 50,  0xFFFF);
    TEST_OK("policy_quarantine_denies",
            cos_v61_lattice_check_policy(s, o, COS_V61_OP_READ, &p) == 0);
    /* Without policy, Biba alone says read-up OK → allowed. */
    TEST_OK("policy_below_ok_without_overlay",
            cos_v61_lattice_check(s, o, COS_V61_OP_READ) == 1);
}

static void test_policy_null_passthrough(void)
{
    cos_v61_label_t s = mklab(128, 128, 0xFFFF);
    cos_v61_label_t o = mklab(128, 128, 0xFFFF);
    TEST_OK("policy_null_passthrough",
            cos_v61_lattice_check_policy(s, o, COS_V61_OP_READ, NULL) == 1);
}

/* ------------------------------------------------------------------ */
/* 9. Batch                                                            */
/* ------------------------------------------------------------------ */

static void test_batch_matches_scalar(void)
{
    int n = 64;
    cos_v61_label_t *subjs = (cos_v61_label_t *)aligned_alloc(64,
        ((size_t)n * sizeof(cos_v61_label_t) + 63) & ~(size_t)63);
    cos_v61_label_t *objs  = (cos_v61_label_t *)aligned_alloc(64,
        ((size_t)n * sizeof(cos_v61_label_t) + 63) & ~(size_t)63);
    uint8_t *ops = (uint8_t *)aligned_alloc(64, (size_t)((n + 63) & ~63));
    uint8_t *rb  = (uint8_t *)aligned_alloc(64, (size_t)((n + 63) & ~63));
    uint8_t *rs  = (uint8_t *)aligned_alloc(64, (size_t)((n + 63) & ~63));
    uint32_t rng = 0xCAFEBABEu;
    for (int i = 0; i < n; ++i) {
        rng = rng * 1664525u + 1013904223u;
        subjs[i] = mklab((uint8_t)(rng >> 24), (uint8_t)(rng >> 16),
                         (uint16_t)(rng & 0xFFFF));
        rng = rng * 1664525u + 1013904223u;
        objs[i]  = mklab((uint8_t)(rng >> 24), (uint8_t)(rng >> 16),
                         (uint16_t)(rng & 0xFFFF));
        ops[i] = (uint8_t)(1 + (i % 3));
    }
    cos_v61_lattice_check_batch(subjs, objs, ops, n, rb);
    for (int i = 0; i < n; ++i) {
        rs[i] = (uint8_t)cos_v61_lattice_check(subjs[i], objs[i], (int)ops[i]);
    }
    int ok = 1;
    for (int i = 0; i < n; ++i) if (rb[i] != rs[i]) { ok = 0; break; }
    TEST_OK("batch_matches_scalar", ok);
    free(subjs); free(objs); free(ops); free(rb); free(rs);
}

static void test_batch_null_safe(void)
{
    cos_v61_lattice_check_batch(NULL, NULL, NULL, 10, NULL);
    cos_v61_lattice_check_batch(NULL, NULL, NULL, 0,  NULL);
    TEST_OK("batch_null_safe", 1);
}

/* ------------------------------------------------------------------ */
/* 10. Attestation — determinism                                       */
/* ------------------------------------------------------------------ */

static cos_v61_attest_input_t mkinput(uint64_t seed)
{
    cos_v61_attest_input_t in;
    memset(&in, 0, sizeof(in));
    in.code_page_hash    = 0x1111ULL ^ seed;
    in.caps_state_hash   = 0x2222ULL ^ seed;
    in.sigma_state_hash  = 0x3333ULL ^ seed;
    in.lattice_hash      = 0x4444ULL ^ seed;
    in.nonce             = 0x5555ULL ^ seed;
    return in;
}

static void test_quote_deterministic(void)
{
    cos_v61_attest_input_t in = mkinput(42);
    cos_v61_quote256_t q1, q2;
    cos_v61_quote256(&in, &q1);
    cos_v61_quote256(&in, &q2);
    TEST_OK("quote_deterministic",
            cos_v61_ct_equal256(&q1, &q2) == 1);
}

static void test_quote_input_sensitive(void)
{
    cos_v61_attest_input_t a = mkinput(1);
    cos_v61_attest_input_t b = mkinput(2);
    cos_v61_quote256_t qa, qb;
    cos_v61_quote256(&a, &qa);
    cos_v61_quote256(&b, &qb);
    TEST_OK("quote_input_sensitive",
            cos_v61_ct_equal256(&qa, &qb) == 0);
}

static void test_quote_nonce_changes_digest(void)
{
    cos_v61_attest_input_t a = mkinput(7);
    cos_v61_attest_input_t b = a; b.nonce ^= 1ULL;
    cos_v61_quote256_t qa, qb;
    cos_v61_quote256(&a, &qa);
    cos_v61_quote256(&b, &qb);
    TEST_OK("quote_nonce_sensitive",
            cos_v61_ct_equal256(&qa, &qb) == 0);
}

static void test_quote_code_hash_changes_digest(void)
{
    cos_v61_attest_input_t a = mkinput(7);
    cos_v61_attest_input_t b = a; b.code_page_hash ^= 1ULL;
    cos_v61_quote256_t qa, qb;
    cos_v61_quote256(&a, &qa);
    cos_v61_quote256(&b, &qb);
    TEST_OK("quote_code_hash_sensitive",
            cos_v61_ct_equal256(&qa, &qb) == 0);
}

static void test_quote_each_field_sensitive(void)
{
    cos_v61_attest_input_t base = mkinput(17);
    cos_v61_quote256_t qbase; cos_v61_quote256(&base, &qbase);
    int ok = 1;

    cos_v61_attest_input_t t = base; t.caps_state_hash  ^= 1ULL;
    cos_v61_quote256_t q;  cos_v61_quote256(&t, &q);
    if (cos_v61_ct_equal256(&q, &qbase) == 1) ok = 0;
    t = base; t.sigma_state_hash ^= 1ULL; cos_v61_quote256(&t, &q);
    if (cos_v61_ct_equal256(&q, &qbase) == 1) ok = 0;
    t = base; t.lattice_hash     ^= 1ULL; cos_v61_quote256(&t, &q);
    if (cos_v61_ct_equal256(&q, &qbase) == 1) ok = 0;

    TEST_OK("quote_all_fields_sensitive", ok);
}

static void test_quote_hex_roundtrip(void)
{
    cos_v61_attest_input_t in = mkinput(99);
    cos_v61_quote256_t q;
    cos_v61_quote256(&in, &q);
    char hex[65] = {0};
    cos_v61_quote256_hex(&q, hex);
    TEST_OK("quote_hex_terminated", hex[64] == '\0');
    int all_hex = 1;
    for (int i = 0; i < 64; ++i) {
        char c = hex[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            all_hex = 0; break;
        }
    }
    TEST_OK("quote_hex_alphabet", all_hex);
}

static void test_ct_equal256_null_safe(void)
{
    TEST_OK("ct_eq256_null_a", cos_v61_ct_equal256(NULL, NULL) == 0);
}

static void test_ct_equal256_self_equal(void)
{
    cos_v61_attest_input_t in = mkinput(1);
    cos_v61_quote256_t q;
    cos_v61_quote256(&in, &q);
    TEST_OK("ct_eq256_self_equal", cos_v61_ct_equal256(&q, &q) == 1);
}

/* ------------------------------------------------------------------ */
/* 11. Composition with v60                                            */
/* ------------------------------------------------------------------ */

static void test_compose_both_allow(void)
{
    cos_v61_compose_t out;
    cos_v61_compose(1 /* v60 ALLOW */, 1 /* lattice allow */, &out);
    TEST_OK("compose_allow_allow", out.decision == COS_V61_COMPOSE_ALLOW);
}

static void test_compose_v60_denies(void)
{
    cos_v61_compose_t out;
    cos_v61_compose(2 /* v60 DENY_CAP */, 1, &out);
    TEST_OK("compose_v60_denies", out.decision == COS_V61_COMPOSE_DENY_V60);
}

static void test_compose_lattice_denies(void)
{
    cos_v61_compose_t out;
    cos_v61_compose(1 /* v60 ALLOW */, 0, &out);
    TEST_OK("compose_lattice_denies",
            out.decision == COS_V61_COMPOSE_DENY_LATTICE);
}

static void test_compose_both_deny(void)
{
    cos_v61_compose_t out;
    cos_v61_compose(3, 0, &out);
    TEST_OK("compose_both_deny", out.decision == COS_V61_COMPOSE_DENY_BOTH);
}

static void test_compose_null_safe(void)
{
    TEST_OK("compose_null_safe", cos_v61_compose(1, 1, NULL) == -1);
}

static void test_compose_tag_strings(void)
{
    TEST_OK("tag_allow",
            strcmp(cos_v61_compose_tag(COS_V61_COMPOSE_ALLOW), "ALLOW") == 0);
    TEST_OK("tag_deny_v60",
            strcmp(cos_v61_compose_tag(COS_V61_COMPOSE_DENY_V60), "DENY_V60") == 0);
    TEST_OK("tag_deny_lattice",
            strcmp(cos_v61_compose_tag(COS_V61_COMPOSE_DENY_LATTICE), "DENY_LATTICE") == 0);
    TEST_OK("tag_deny_both",
            strcmp(cos_v61_compose_tag(COS_V61_COMPOSE_DENY_BOTH), "DENY_BOTH") == 0);
    TEST_OK("tag_unknown_q",
            strcmp(cos_v61_compose_tag(0xFF), "?") == 0);
}

/* ------------------------------------------------------------------ */
/* 12. Allocator                                                       */
/* ------------------------------------------------------------------ */

static void test_alloc_attest(void)
{
    TEST_OK("alloc_zero_null", cos_v61_alloc_attest_inputs(0)  == NULL);
    TEST_OK("alloc_neg_null",  cos_v61_alloc_attest_inputs(-1) == NULL);
    cos_v61_attest_input_t *b = cos_v61_alloc_attest_inputs(7);
    TEST_OK("alloc_nonnull", b != NULL);
    int aligned = (((uintptr_t)b) & 63u) == 0;
    TEST_OK("alloc_aligned", aligned);
    int zeroed = 1;
    for (int i = 0; i < 7; ++i) {
        if (b[i].code_page_hash != 0 || b[i].nonce != 0) {
            zeroed = 0; break;
        }
    }
    TEST_OK("alloc_zeroed", zeroed);
    TEST_OK("sizeof_attest_input_64", sizeof(cos_v61_attest_input_t) == 64);
    free(b);
}

/* ------------------------------------------------------------------ */
/* 13. Adversarial scenarios                                           */
/* ------------------------------------------------------------------ */

/* ClawWorm propagation vector: low-integrity payload tries to write
 * into high-integrity code pages → Biba no-write-up. */
static void test_adversarial_clawworm_biba(void)
{
    cos_v61_label_t worm_msg = mklab(200, 10, 0xFFFF);
    cos_v61_label_t weights  = mklab(200, 250, 0xFFFF);
    TEST_OK("adversarial_clawworm_biba",
            cos_v61_lattice_check(worm_msg, weights, COS_V61_OP_WRITE) == 0);
}

/* DDIPE payload embedded in doc that reads up into secrets → BLP
 * no-read-up. */
static void test_adversarial_ddipe_blp(void)
{
    cos_v61_label_t doc_reader = mklab(10, 128, 0xFFFF);
    cos_v61_label_t secrets    = mklab(250, 128, 0x0001);
    TEST_OK("adversarial_ddipe_blp",
            cos_v61_lattice_check(doc_reader, secrets, COS_V61_OP_READ) == 0);
}

/* Confused-deputy: compartment mismatch stops lateral movement. */
static void test_adversarial_compartment_isolation(void)
{
    cos_v61_label_t billing_agent = mklab(128, 128, 0x0002);  /* billing */
    cos_v61_label_t hr_record     = mklab(128, 128, 0x0004);  /* hr      */
    TEST_OK("adversarial_compartment_isolation",
            cos_v61_lattice_check(billing_agent, hr_record,
                                  COS_V61_OP_READ) == 0);
}

/* Runtime-patch: attacker flips a bit in code_page_hash; quote differs
 * from baseline → verifier rejects. */
static void test_adversarial_runtime_patch_quote(void)
{
    cos_v61_attest_input_t good = mkinput(0xABCDEF);
    cos_v61_attest_input_t bad  = good;
    bad.code_page_hash ^= 0x1ULL;
    cos_v61_quote256_t qg, qb;
    cos_v61_quote256(&good, &qg);
    cos_v61_quote256(&bad,  &qb);
    TEST_OK("adversarial_runtime_patch_quote",
            cos_v61_ct_equal256(&qg, &qb) == 0);
}

/* ------------------------------------------------------------------ */
/* 14. Stress                                                          */
/* ------------------------------------------------------------------ */

static void test_stress_random_invariants(void)
{
    uint32_t rng = 0xC0DECAFE;
    int all_ok = 1;
    for (int i = 0; i < 2000; ++i) {
        rng = rng * 1664525u + 1013904223u;
        cos_v61_label_t s = mklab((uint8_t)(rng >> 24), (uint8_t)(rng >> 16),
                                  (uint16_t)(rng & 0xFFFF));
        rng = rng * 1664525u + 1013904223u;
        cos_v61_label_t o = mklab((uint8_t)(rng >> 24), (uint8_t)(rng >> 16),
                                  (uint16_t)(rng & 0xFFFF));
        int op = 1 + (int)(rng & 3);
        int r = cos_v61_lattice_check(s, o, op);
        if (r != 0 && r != 1) { all_ok = 0; break; }

        /* Invariant: equal labels always allow any known op when
         * compartments overlap requirement. */
        cos_v61_label_t eq = mklab(50, 50, 0x0001);
        if (cos_v61_lattice_check(eq, eq, COS_V61_OP_READ)  != 1) { all_ok = 0; break; }
        if (cos_v61_lattice_check(eq, eq, COS_V61_OP_WRITE) != 1) { all_ok = 0; break; }
    }
    TEST_OK("stress_random_invariants", all_ok);
}

/* ------------------------------------------------------------------ */
/* Harness                                                             */
/* ------------------------------------------------------------------ */

static int run_self_test(void)
{
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    test_version();
    test_policy_defaults();

    test_blp_no_read_up();
    test_blp_read_down_ok();
    test_blp_no_write_down();
    test_blp_write_up_ok();
    test_blp_equal_levels();

    test_biba_no_read_down();
    test_biba_read_up_ok();
    test_biba_no_write_up();
    test_biba_write_down_ok();

    test_combined_strict();
    test_combined_only_equal_is_always_ok();

    test_compartment_match();
    test_compartment_missing();
    test_compartment_empty_object_ok();

    test_exec_requires_both();
    test_exec_denies_on_any_axis_diff();

    test_unknown_op_denies();

    test_policy_quarantine();
    test_policy_null_passthrough();

    test_batch_matches_scalar();
    test_batch_null_safe();

    test_quote_deterministic();
    test_quote_input_sensitive();
    test_quote_nonce_changes_digest();
    test_quote_code_hash_changes_digest();
    test_quote_each_field_sensitive();
    test_quote_hex_roundtrip();
    test_ct_equal256_null_safe();
    test_ct_equal256_self_equal();

    test_compose_both_allow();
    test_compose_v60_denies();
    test_compose_lattice_denies();
    test_compose_both_deny();
    test_compose_null_safe();
    test_compose_tag_strings();

    test_alloc_attest();

    test_adversarial_clawworm_biba();
    test_adversarial_ddipe_blp();
    test_adversarial_compartment_isolation();
    test_adversarial_runtime_patch_quote();

    test_stress_random_invariants();

    fprintf(stderr, "v61 self-test: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* Attestation emit (for scripts/security/attest.sh)                   */
/* ------------------------------------------------------------------ */

static int run_attest(void)
{
    /* Deterministic attestation input: in production the caller would
     * feed real code_page_hash, caps_state_hash, σ-state hash, and
     * lattice hash.  For the demo we compute a quote over a canonical
     * zero input + the current time.  Output: JSON on stdout. */
    cos_v61_attest_input_t in;
    memset(&in, 0, sizeof(in));
    in.code_page_hash   = 0xC0DE0C0DE00C0DE0ULL;
    in.caps_state_hash  = 0xCA95CA95CA95CA95ULL;
    in.sigma_state_hash = 0x5162A5162A5162A5ULL;
    in.lattice_hash     = 0x1A771CE1A771CE00ULL;
    in.nonce            = (uint64_t)time(NULL);

    cos_v61_quote256_t q;
    cos_v61_quote256(&in, &q);
    char hex[65];
    cos_v61_quote256_hex(&q, hex);

    printf("{\n");
    printf("  \"schema\": \"creation-os/v61/attestation/1\",\n");
    printf("  \"algorithm\": \"%s\",\n",
#if defined(COS_V61_LIBSODIUM)
           "blake2b-256"
#else
           "xor-fold-256 (deterministic, not MAC)"
#endif
           );
    printf("  \"inputs\": {\n");
    printf("    \"code_page_hash\":   \"0x%016llx\",\n",
           (unsigned long long)in.code_page_hash);
    printf("    \"caps_state_hash\":  \"0x%016llx\",\n",
           (unsigned long long)in.caps_state_hash);
    printf("    \"sigma_state_hash\": \"0x%016llx\",\n",
           (unsigned long long)in.sigma_state_hash);
    printf("    \"lattice_hash\":     \"0x%016llx\",\n",
           (unsigned long long)in.lattice_hash);
    printf("    \"nonce\":            \"0x%016llx\"\n",
           (unsigned long long)in.nonce);
    printf("  },\n");
    printf("  \"quote\": \"%s\"\n", hex);
    printf("}\n");
    return 0;
}

static void print_architecture(void)
{
    printf(
"\nCreation OS v61 — Σ-Citadel architecture\n"
"========================================\n\n"
"  2026 advanced-security menu -> Creation OS surface\n\n"
"  seL4 microkernel        -> sel4/sigma_shield.camkes (M-tier when\n"
"                             the host can instantiate the component)\n"
"  Wasmtime sandbox        -> wasm/ + scripts/v61/wasm_harness.sh\n"
"  eBPF runtime policy     -> ebpf/sigma_shield.bpf.c (Linux only)\n"
"  Bell-LaPadula (conf.)   -> cos_v61_lattice_check (THIS)\n"
"  Biba (integrity)        -> cos_v61_lattice_check (THIS)\n"
"  MLS compartments        -> cos_v61_label_t.compartments\n"
"  TPM 2.0 attest          -> cos_v61_quote256 (sw fallback / libsodium)\n"
"  Sigstore / cosign       -> scripts/security/sign.sh\n"
"  libsodium (BLAKE2b)     -> -DCOS_V61_LIBSODIUM -lsodium\n"
"  Nix reproducible build  -> nix/flake.nix\n"
"  Distroless container    -> Dockerfile.distroless\n"
"  SLSA-3 provenance       -> .github/workflows/slsa.yml\n"
"  Darwin sandbox-exec     -> sandbox/darwin.sb\n"
"  OpenBSD pledge/unveil   -> sandbox/openbsd_pledge.c\n"
"  CHERI hw caps           -> (documented target; no M4 hw path)\n\n");
}

static void print_positioning(void)
{
    printf(
"\nCreation OS v61 — Σ-Citadel positioning\n"
"=======================================\n\n"
"  Layer                    Best-in-class     Creation OS v61 stance\n"
"  --------------------------------------------------------------------\n"
"  micro-kernel isolation   seL4 (formally)   compose via CAmkES spec\n"
"  userland sandbox         Wasmtime+Firecr.  compose via wasm_harness\n"
"  kernel policy            eBPF LSM          example .bpf.c (Linux)\n"
"  MLS lattice (conf+int)   Bell-LaPadula+Biba native (branchless)\n"
"  hw attestation           TPM 2.0 / SE      sw fallback; SE planned\n"
"  code signing             Sigstore + Rekor  cosign hook\n"
"  supply chain             SLSA-3            slsa-github-generator\n"
"  repro build              Nix / Bazel       flake.nix\n"
"  runtime container        Distroless        Dockerfile.distroless\n"
"  hw caps                  CHERI             doc-only (no M4 hw)\n\n"
"  v61 does NOT replace any layer; it **composes** each with a\n"
"  rehearsal target (`make chace`) that PASSes on present layers\n"
"  and SKIPs honestly on missing ones.  First open-source AI agent\n"
"  runtime to ship the full CHACE-class capability-hardening menu as a runnable gate.\n\n");
}

int main(int argc, char **argv)
{
    if (argc >= 2) {
        if (strcmp(argv[1], "--self-test")    == 0) return run_self_test();
        if (strcmp(argv[1], "--architecture") == 0) { print_architecture(); return 0; }
        if (strcmp(argv[1], "--positioning")  == 0) { print_positioning();  return 0; }
        if (strcmp(argv[1], "--attest")       == 0) return run_attest();
    }
    printf("Creation OS v61 — Σ-Citadel (BLP + Biba + attestation).\n"
           "Usage:\n"
           "  ./creation_os_v61 --self-test\n"
           "  ./creation_os_v61 --architecture\n"
           "  ./creation_os_v61 --positioning\n"
           "  ./creation_os_v61 --attest         (emits JSON quote)\n");
    return 0;
}
