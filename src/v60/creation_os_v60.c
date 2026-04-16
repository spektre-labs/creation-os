/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v60 — σ-Shield driver: deterministic ≥60-test self-test,
 * architecture / positioning banners, microbench harness.
 */

#include "sigma_shield.h"

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

static void *v60_alloc64(size_t bytes)
{
    if (bytes == 0) bytes = 64;
    size_t aligned = (bytes + 63u) & ~(size_t)63u;
    return aligned_alloc(64, aligned);
}

static uint32_t v60_lcg_next(uint32_t *state)
{
    *state = (*state) * 1664525u + 1013904223u;
    return *state;
}

/* ------------------------------------------------------------------ */
/* 1. Version / defaults                                               */
/* ------------------------------------------------------------------ */

static void test_version(void)
{
    cos_v60_version_t v = cos_v60_version();
    TEST_OK("ver_major_60", v.major == 60);
    TEST_OK("ver_minor_0",  v.minor == 0);
    TEST_OK("ver_patch_ge1", v.patch >= 1);
}

static void test_defaults(void)
{
    cos_v60_policy_t p;
    cos_v60_policy_default(&p);
    TEST_OK("def_sigma_high_pos", p.sigma_high > 0.0f);
    TEST_OK("def_alpha_dom_unit", p.alpha_dominance > 0.0f
                               && p.alpha_dominance < 1.0f);
    TEST_OK("def_sticky_has_dlsym",
            (p.sticky_deny_mask & COS_V60_CAP_DLSYM) != 0);
    TEST_OK("def_sticky_has_mmap_exec",
            (p.sticky_deny_mask & COS_V60_CAP_MMAP_EXEC) != 0);
    TEST_OK("def_sticky_has_self_modify",
            (p.sticky_deny_mask & COS_V60_CAP_SELF_MODIFY) != 0);
    TEST_OK("def_always_caps_zero", p.always_caps == 0);
    TEST_OK("def_enforce_integrity_on", p.enforce_integrity == 1);
}

static void test_defaults_null_safe(void) {
    cos_v60_policy_default(NULL); TEST_OK("def_null_safe", 1);
}

/* ------------------------------------------------------------------ */
/* 2. Hash fold + constant-time equality                               */
/* ------------------------------------------------------------------ */

static void test_hash_deterministic(void)
{
    const char *msg = "creation os sigma shield";
    uint64_t a = cos_v60_hash_fold(msg, strlen(msg), 0xDEADBEEFu);
    uint64_t b = cos_v60_hash_fold(msg, strlen(msg), 0xDEADBEEFu);
    TEST_OK("hash_deterministic", a == b);
}

static void test_hash_seed_sensitive(void)
{
    const char *m = "payload";
    uint64_t a = cos_v60_hash_fold(m, strlen(m), 1u);
    uint64_t b = cos_v60_hash_fold(m, strlen(m), 2u);
    TEST_OK("hash_seed_differs", a != b);
}

static void test_hash_data_sensitive(void)
{
    uint64_t a = cos_v60_hash_fold("aaa", 3, 42u);
    uint64_t b = cos_v60_hash_fold("aaA", 3, 42u);
    TEST_OK("hash_data_differs", a != b);
}

static void test_hash_length_sensitive(void)
{
    uint64_t a = cos_v60_hash_fold("aa",  2, 42u);
    uint64_t b = cos_v60_hash_fold("aaa", 3, 42u);
    TEST_OK("hash_len_differs", a != b);
}

static void test_hash_null_safe(void)
{
    uint64_t h = cos_v60_hash_fold(NULL, 0, 7u);
    /* Must not crash; returns a seed-derived value deterministically. */
    TEST_OK("hash_null_safe", h == cos_v60_hash_fold(NULL, 0, 7u));
}

static void test_hash_empty_differs_from_nonempty(void)
{
    uint64_t a = cos_v60_hash_fold(NULL, 0, 7u);
    uint64_t b = cos_v60_hash_fold("x", 1, 7u);
    TEST_OK("hash_empty_vs_nonempty", a != b);
}

static void test_ct_equal_true(void)
{
    TEST_OK("ct_equal_match", cos_v60_ct_equal64(0x123456789ABCDEF0ULL,
                                                 0x123456789ABCDEF0ULL) == 1);
    TEST_OK("ct_equal_zero_zero", cos_v60_ct_equal64(0, 0) == 1);
}

static void test_ct_equal_false(void)
{
    TEST_OK("ct_equal_low_bit",  cos_v60_ct_equal64(0, 1) == 0);
    TEST_OK("ct_equal_high_bit",
            cos_v60_ct_equal64(0, 1ULL << 63) == 0);
    TEST_OK("ct_equal_mixed",
            cos_v60_ct_equal64(0x123456789ABCDEF0ULL,
                               0x123456789ABCDEF1ULL) == 0);
}

static void test_ct_equal_has_no_branch_on_input(void)
{
    /* Indirect: all-ones and all-zeros must both yield 0 vs each other
     * and 1 vs themselves; running many values should never diverge. */
    uint32_t rng = 0xC0FFEE;
    int ok = 1;
    for (int i = 0; i < 2048; ++i) {
        uint64_t a = ((uint64_t)v60_lcg_next(&rng) << 32) | v60_lcg_next(&rng);
        uint64_t b = ((uint64_t)v60_lcg_next(&rng) << 32) | v60_lcg_next(&rng);
        int same_expected = (a == b);
        int ct = cos_v60_ct_equal64(a, b);
        if ((ct != 0 && ct != 1) || (ct == 1) != same_expected) { ok = 0; break; }
    }
    TEST_OK("ct_equal_matches_semantic_eq_over_random", ok);
}

/* ------------------------------------------------------------------ */
/* 3. Authorise — decision surfaces                                    */
/* ------------------------------------------------------------------ */

static void zero_req(cos_v60_request_t *r)
{
    memset(r, 0, sizeof(*r));
    r->arg_hash_at_entry = 0x1111222233334444ULL;
    r->arg_hash_at_use   = 0x1111222233334444ULL;
    r->epistemic = 0.10f;
    r->aleatoric = 0.05f;
}

static void test_allow_basic(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.action_id = 1;
    r.required_caps = COS_V60_CAP_FS_READ;
    uint64_t code_hash = 0xAAAAAAAAAAAAAAAAULL;
    cos_v60_result_t out;
    int rc = cos_v60_authorize(&r, COS_V60_CAP_FS_READ,
                               code_hash, code_hash, &p, &out);
    TEST_OK("auth_rc0", rc == 0);
    TEST_OK("auth_allow", out.decision == COS_V60_ALLOW);
    TEST_OK("auth_missing_zero", out.missing_caps == 0);
    TEST_OK("auth_reason_clear", out.reason_bits == 0);
}

static void test_deny_cap_missing(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_FS_READ | COS_V60_CAP_NETWORK;
    uint64_t h = 0x42ULL;
    cos_v60_result_t out;
    cos_v60_authorize(&r, COS_V60_CAP_FS_READ, h, h, &p, &out);
    TEST_OK("deny_cap_missing", out.decision == COS_V60_DENY_CAP);
    TEST_OK("deny_cap_has_reason",
            (out.reason_bits & COS_V60_REASON_CAP) != 0);
    TEST_OK("deny_cap_missing_bits",
            out.missing_caps == COS_V60_CAP_NETWORK);
}

static void test_deny_cap_sticky(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_DLSYM;  /* sticky by default */
    uint64_t h = 0x42ULL;
    cos_v60_result_t out;
    cos_v60_authorize(&r, (uint64_t)-1 /* ALL caps held */, h, h, &p, &out);
    TEST_OK("deny_sticky_wins", out.decision == COS_V60_DENY_CAP);
    TEST_OK("deny_sticky_reason_sticky",
            (out.reason_bits & COS_V60_REASON_STICKY) != 0);
    TEST_OK("deny_sticky_reason_cap",
            (out.reason_bits & COS_V60_REASON_CAP) != 0);
}

static void test_deny_toctou(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_FS_READ;
    r.arg_hash_at_entry = 0xAAAAULL;
    r.arg_hash_at_use   = 0xBBBBULL;
    uint64_t h = 0x42ULL;
    cos_v60_result_t out;
    cos_v60_authorize(&r, COS_V60_CAP_FS_READ, h, h, &p, &out);
    TEST_OK("deny_toctou", out.decision == COS_V60_DENY_TOCTOU);
    TEST_OK("deny_toctou_reason",
            (out.reason_bits & COS_V60_REASON_TOCTOU) != 0);
}

static void test_deny_intent(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_FS_READ;
    /* σ_total = 1.5 ≥ 1.0 (high), α/(ε+α) = 1.2/1.5 = 0.8 ≥ 0.65 */
    r.epistemic = 0.30f;
    r.aleatoric = 1.20f;
    uint64_t h = 0x42ULL;
    cos_v60_result_t out;
    cos_v60_authorize(&r, COS_V60_CAP_FS_READ, h, h, &p, &out);
    TEST_OK("deny_intent", out.decision == COS_V60_DENY_INTENT);
    TEST_OK("deny_intent_reason",
            (out.reason_bits & COS_V60_REASON_INTENT) != 0);
}

static void test_deny_integrity(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_FS_READ;
    cos_v60_result_t out;
    cos_v60_authorize(&r, COS_V60_CAP_FS_READ, 0xAAAAu, 0xBBBBu, &p, &out);
    TEST_OK("deny_integrity", out.decision == COS_V60_DENY_INTEGRITY);
    TEST_OK("deny_integrity_reason",
            (out.reason_bits & COS_V60_REASON_INTEGRITY) != 0);
}

static void test_integrity_bypass_when_disabled(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    p.enforce_integrity = 0;
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_FS_READ;
    cos_v60_result_t out;
    cos_v60_authorize(&r, COS_V60_CAP_FS_READ, 0xAAAAu, 0xBBBBu, &p, &out);
    TEST_OK("integrity_bypass_allows_on_drift",
            out.decision == COS_V60_ALLOW);
    TEST_OK("integrity_bypass_clear_reason",
            (out.reason_bits & COS_V60_REASON_INTEGRITY) == 0);
}

static void test_authorize_null_safe(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    cos_v60_result_t out;
    TEST_OK("auth_null_req",
            cos_v60_authorize(NULL, 0, 0, 0, &p, &out) == -1);
    TEST_OK("auth_null_policy",
            cos_v60_authorize(&r, 0, 0, 0, NULL, &out) == -1);
    TEST_OK("auth_null_out",
            cos_v60_authorize(&r, 0, 0, 0, &p, NULL) == -1);
}

/* ------------------------------------------------------------------ */
/* 4. Priority cascade                                                 */
/* ------------------------------------------------------------------ */

static void test_priority_integrity_beats_cap(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_FS_READ | COS_V60_CAP_NETWORK;
    cos_v60_result_t out;
    cos_v60_authorize(&r, 0, 0xAAu, 0xBBu, &p, &out);
    TEST_OK("priority_integrity_wins",
            out.decision == COS_V60_DENY_INTEGRITY);
    /* But reason_bits should still record CAP too. */
    TEST_OK("priority_integrity_reason_has_cap",
            (out.reason_bits & COS_V60_REASON_CAP) != 0);
}

static void test_priority_sticky_beats_toctou(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_DLSYM;
    r.arg_hash_at_use = r.arg_hash_at_entry + 1;
    cos_v60_result_t out;
    cos_v60_authorize(&r, (uint64_t)-1, 0x42u, 0x42u, &p, &out);
    TEST_OK("priority_sticky_wins", out.decision == COS_V60_DENY_CAP);
    TEST_OK("priority_sticky_reason_union",
            (out.reason_bits & COS_V60_REASON_STICKY)
         && (out.reason_bits & COS_V60_REASON_TOCTOU));
}

static void test_priority_cap_beats_intent(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_NETWORK;
    r.epistemic = 0.30f; r.aleatoric = 1.20f;
    cos_v60_result_t out;
    cos_v60_authorize(&r, 0, 0x42u, 0x42u, &p, &out);
    TEST_OK("priority_cap_wins", out.decision == COS_V60_DENY_CAP);
    TEST_OK("priority_cap_reason_has_both",
            (out.reason_bits & COS_V60_REASON_CAP)
         && (out.reason_bits & COS_V60_REASON_INTENT));
}

static void test_priority_toctou_beats_intent(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_FS_READ;
    r.arg_hash_at_use = r.arg_hash_at_entry + 1;
    r.epistemic = 0.30f; r.aleatoric = 1.20f;
    cos_v60_result_t out;
    cos_v60_authorize(&r, COS_V60_CAP_FS_READ, 0x42u, 0x42u, &p, &out);
    TEST_OK("priority_toctou_wins", out.decision == COS_V60_DENY_TOCTOU);
}

/* ------------------------------------------------------------------ */
/* 5. Always-caps bootstrap                                            */
/* ------------------------------------------------------------------ */

static void test_always_caps(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    p.always_caps = COS_V60_CAP_FS_READ;  /* all holders can read */
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_FS_READ;
    cos_v60_result_t out;
    cos_v60_authorize(&r, 0 /* no explicit caps */, 0x42u, 0x42u, &p, &out);
    TEST_OK("always_caps_grants", out.decision == COS_V60_ALLOW);
}

/* ------------------------------------------------------------------ */
/* 6. Intent signal                                                    */
/* ------------------------------------------------------------------ */

static void test_intent_pass_when_epsilon_dominant(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_FS_READ;
    /* σ_total high, but ε ≫ α → NOT α-dominant, so intent OK. */
    r.epistemic = 1.20f;
    r.aleatoric = 0.10f;
    cos_v60_result_t out;
    cos_v60_authorize(&r, COS_V60_CAP_FS_READ, 0x42u, 0x42u, &p, &out);
    TEST_OK("intent_pass_high_sigma_eps_dom",
            out.decision == COS_V60_ALLOW);
}

static void test_intent_pass_when_sigma_low(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_FS_READ;
    /* σ low: even 100% α-dominant is OK. */
    r.epistemic = 0.00f;
    r.aleatoric = 0.10f;
    cos_v60_result_t out;
    cos_v60_authorize(&r, COS_V60_CAP_FS_READ, 0x42u, 0x42u, &p, &out);
    TEST_OK("intent_pass_low_sigma", out.decision == COS_V60_ALLOW);
}

static void test_intent_threshold_boundary(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    /* Exactly at α_dominance threshold: should still trigger (≥). */
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_FS_READ;
    r.epistemic = 0.35f;
    r.aleatoric = 0.65f;  /* σ=1.00, α_frac = 0.65 exactly */
    cos_v60_result_t out;
    cos_v60_authorize(&r, COS_V60_CAP_FS_READ, 0x42u, 0x42u, &p, &out);
    TEST_OK("intent_threshold_inclusive",
            out.decision == COS_V60_DENY_INTENT);
}

/* ------------------------------------------------------------------ */
/* 7. Batch                                                            */
/* ------------------------------------------------------------------ */

static void test_batch_matches_scalar(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    int n = 32;
    cos_v60_request_t *reqs = cos_v60_alloc_requests(n);
    cos_v60_result_t  *b    = (cos_v60_result_t *)v60_alloc64(
                                    (size_t)n * sizeof(cos_v60_result_t));
    cos_v60_result_t  *s    = (cos_v60_result_t *)v60_alloc64(
                                    (size_t)n * sizeof(cos_v60_result_t));
    uint32_t rng = 0xFEEDFACEu;
    for (int i = 0; i < n; ++i) {
        zero_req(&reqs[i]);
        reqs[i].required_caps = COS_V60_CAP_FS_READ;
        reqs[i].epistemic     = (float)(v60_lcg_next(&rng) & 0xFF) / 512.0f;
        reqs[i].aleatoric     = (float)(v60_lcg_next(&rng) & 0xFF) / 512.0f;
    }
    cos_v60_authorize_batch(reqs, n, COS_V60_CAP_FS_READ,
                            0x42u, 0x42u, &p, b);
    for (int i = 0; i < n; ++i) {
        cos_v60_authorize(&reqs[i], COS_V60_CAP_FS_READ,
                          0x42u, 0x42u, &p, &s[i]);
    }
    int ok = 1;
    for (int i = 0; i < n; ++i) {
        if (b[i].decision     != s[i].decision
         || b[i].missing_caps != s[i].missing_caps
         || b[i].reason_bits  != s[i].reason_bits) { ok = 0; break; }
    }
    TEST_OK("batch_matches_scalar", ok);
    free(reqs); free(b); free(s);
}

static void test_batch_null_safe(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_authorize_batch(NULL, 10, 0, 0, 0, &p, NULL);
    cos_v60_authorize_batch(NULL, 0, 0, 0, 0, &p, NULL);
    TEST_OK("batch_null_safe", 1);
}

/* ------------------------------------------------------------------ */
/* 8. Tags / allocator                                                 */
/* ------------------------------------------------------------------ */

static void test_tags(void)
{
    TEST_OK("tag_allow",
            strcmp(cos_v60_decision_tag(COS_V60_ALLOW), "ALLOW") == 0);
    TEST_OK("tag_deny_cap",
            strcmp(cos_v60_decision_tag(COS_V60_DENY_CAP), "DENY_CAP") == 0);
    TEST_OK("tag_deny_intent",
            strcmp(cos_v60_decision_tag(COS_V60_DENY_INTENT), "DENY_INTENT") == 0);
    TEST_OK("tag_deny_toctou",
            strcmp(cos_v60_decision_tag(COS_V60_DENY_TOCTOU), "DENY_TOCTOU") == 0);
    TEST_OK("tag_deny_integrity",
            strcmp(cos_v60_decision_tag(COS_V60_DENY_INTEGRITY), "DENY_INTEGRITY") == 0);
    TEST_OK("tag_unknown_q",
            strcmp(cos_v60_decision_tag(0xFF), "?") == 0);
}

static void test_alloc(void)
{
    TEST_OK("alloc_zero_null", cos_v60_alloc_requests(0)  == NULL);
    TEST_OK("alloc_neg_null",  cos_v60_alloc_requests(-1) == NULL);
    cos_v60_request_t *r = cos_v60_alloc_requests(11);
    TEST_OK("alloc_nonnull", r != NULL);
    int aligned = (((uintptr_t)r) & 63u) == 0;
    TEST_OK("alloc_aligned", aligned);
    int zeroed = 1;
    for (int i = 0; i < 11; ++i) {
        if (r[i].required_caps != 0 || r[i].arg_hash_at_entry != 0) {
            zeroed = 0; break;
        }
    }
    TEST_OK("alloc_zeroed", zeroed);
    TEST_OK("sizeof_request_64", sizeof(cos_v60_request_t) == 64);
    free(r);
}

/* ------------------------------------------------------------------ */
/* 9. Adversarial scenarios                                            */
/* ------------------------------------------------------------------ */

/* DDIPE-style: payload embedded in a doc-example.  The caller holds
 * CAP_FS_READ but the intent signal (from an upstream σ producer) is
 * α-dominated because a document-driven attack looks like legitimate
 * usage — σ-Shield refuses regardless of caps. */
static void test_adversarial_ddipe_refused(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_FS_READ | COS_V60_CAP_TOOL_CALL;
    r.epistemic = 0.15f;
    r.aleatoric = 1.40f;  /* high α → DDIPE-shaped intent */
    cos_v60_result_t out;
    cos_v60_authorize(&r,
                      COS_V60_CAP_FS_READ | COS_V60_CAP_TOOL_CALL,
                      0x42u, 0x42u, &p, &out);
    TEST_OK("adversarial_ddipe_refused",
            out.decision == COS_V60_DENY_INTENT);
}

/* ClawWorm-style: self-propagating worm asks for NETWORK + EXEC on
 * content that looks legitimate.  Without σ-intent, caps would allow
 * it.  With σ α-dominated, refused. */
static void test_adversarial_clawworm_refused(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_NETWORK | COS_V60_CAP_EXEC;
    r.epistemic = 0.10f;
    r.aleatoric = 1.50f;
    cos_v60_result_t out;
    cos_v60_authorize(&r,
                      COS_V60_CAP_NETWORK | COS_V60_CAP_EXEC,
                      0x42u, 0x42u, &p, &out);
    TEST_OK("adversarial_clawworm_refused",
            out.decision == COS_V60_DENY_INTENT);
}

/* Malicious intermediary attacks the argument AFTER caps checked.
 * arg_hash_at_use differs from arg_hash_at_entry → TOCTOU firing. */
static void test_adversarial_intermediary_toctou(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_NETWORK;
    r.arg_hash_at_entry = cos_v60_hash_fold("GET /api/key", 12, 0x1234u);
    r.arg_hash_at_use   = cos_v60_hash_fold("GET /api/keyX", 13, 0x1234u);
    cos_v60_result_t out;
    cos_v60_authorize(&r, COS_V60_CAP_NETWORK, 0x42u, 0x42u, &p, &out);
    TEST_OK("adversarial_intermediary_toctou",
            out.decision == COS_V60_DENY_TOCTOU);
}

/* Confused-deputy: attacker tries to escalate via DLSYM (sticky). */
static void test_adversarial_confused_deputy_dlsym(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_DLSYM;  /* always sticky-deny */
    cos_v60_result_t out;
    cos_v60_authorize(&r, (uint64_t)-1, 0x42u, 0x42u, &p, &out);
    TEST_OK("adversarial_confused_deputy_dlsym",
            out.decision == COS_V60_DENY_CAP
         && (out.reason_bits & COS_V60_REASON_STICKY));
}

/* Code-injection post-load: baseline_hash was computed at boot; then
 * attacker rewrites a code page.  v60 sees drift → DENY_INTEGRITY. */
static void test_adversarial_runtime_patch(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_MODEL_CALL;
    uint64_t baseline = 0xCAFE1234u;
    uint64_t current  = 0xDEADBEEFu;  /* drifted */
    cos_v60_result_t out;
    cos_v60_authorize(&r, COS_V60_CAP_MODEL_CALL, current, baseline, &p, &out);
    TEST_OK("adversarial_runtime_patch_caught",
            out.decision == COS_V60_DENY_INTEGRITY);
}

/* v58 / v59 composition: a CAP_KV_EVICT attempt with α-dom intent.
 * σ-Shield protects σ-Cache from ambiguous-intent mutations. */
static void test_composition_v58_evict_guarded(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_KV_EVICT;
    r.epistemic = 0.20f;
    r.aleatoric = 1.30f;
    cos_v60_result_t out;
    cos_v60_authorize(&r, COS_V60_CAP_KV_EVICT, 0x42u, 0x42u, &p, &out);
    TEST_OK("composition_v58_evict_refused_intent",
            out.decision == COS_V60_DENY_INTENT);
}

/* v59 σ-Budget EXPAND: capability gates excess compute.  Without
 * CAP_BUDGET_EXT held, σ-Shield refuses the EXPAND. */
static void test_composition_v59_expand_gated(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_BUDGET_EXT;
    cos_v60_result_t out;
    cos_v60_authorize(&r, 0 /* no expand cap held */,
                      0x42u, 0x42u, &p, &out);
    TEST_OK("composition_v59_expand_gated_by_cap",
            out.decision == COS_V60_DENY_CAP);
}

/* ------------------------------------------------------------------ */
/* 10. Determinism / idempotency                                       */
/* ------------------------------------------------------------------ */

static void test_deterministic_under_same_inputs(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.required_caps = COS_V60_CAP_FS_READ;
    r.epistemic = 0.3f; r.aleatoric = 0.5f;
    cos_v60_result_t a, b;
    cos_v60_authorize(&r, COS_V60_CAP_FS_READ, 0x42u, 0x42u, &p, &a);
    cos_v60_authorize(&r, COS_V60_CAP_FS_READ, 0x42u, 0x42u, &p, &b);
    TEST_OK("deterministic_auth",
            a.decision == b.decision
         && a.missing_caps == b.missing_caps
         && a.reason_bits  == b.reason_bits);
}

static void test_result_summary_fields(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t r; zero_req(&r);
    r.action_id = 0xAABBCCDDu;
    r.required_caps = COS_V60_CAP_FS_READ;
    r.epistemic = 0.40f; r.aleatoric = 0.20f;
    cos_v60_result_t out;
    cos_v60_authorize(&r, COS_V60_CAP_FS_READ, 0x42u, 0x42u, &p, &out);
    TEST_OK("summary_action_id", out.action_id == 0xAABBCCDDu);
    TEST_OK("summary_sigma_tot",
            out.sigma_total > 0.59f && out.sigma_total < 0.61f);
    TEST_OK("summary_alpha_frac",
            out.alpha_fraction > 0.32f && out.alpha_fraction < 0.35f);
}

static void test_stress_random_invariants(void)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    uint32_t rng = 0x1337u;
    int all_ok = 1;
    for (int i = 0; i < 2000; ++i) {
        cos_v60_request_t r; zero_req(&r);
        r.required_caps = v60_lcg_next(&rng) & 0xFFFu;
        r.epistemic = (float)(v60_lcg_next(&rng) & 0x3FF) / 256.0f;
        r.aleatoric = (float)(v60_lcg_next(&rng) & 0x3FF) / 256.0f;
        if (v60_lcg_next(&rng) & 1u) r.arg_hash_at_use ^= 1ULL;
        uint64_t holder = (uint64_t)v60_lcg_next(&rng);
        uint64_t cur    = v60_lcg_next(&rng) & 1u ? 0x42u : 0xDEADu;
        cos_v60_result_t out;
        cos_v60_authorize(&r, holder, cur, 0x42u, &p, &out);
        /* Invariant: decision in valid range. */
        if (out.decision < COS_V60_ALLOW || out.decision > COS_V60_DENY_INTEGRITY) {
            all_ok = 0; break;
        }
        /* Invariant: if ALLOW, no reason bits set. */
        if (out.decision == COS_V60_ALLOW && out.reason_bits != 0) {
            all_ok = 0; break;
        }
        /* Invariant: if not ALLOW, at least one reason bit. */
        if (out.decision != COS_V60_ALLOW && out.reason_bits == 0) {
            all_ok = 0; break;
        }
    }
    TEST_OK("stress_random_invariants", all_ok);
}

/* ------------------------------------------------------------------ */
/* Top-level harness                                                   */
/* ------------------------------------------------------------------ */

static int run_self_test(void)
{
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    test_version();
    test_defaults();
    test_defaults_null_safe();

    test_hash_deterministic();
    test_hash_seed_sensitive();
    test_hash_data_sensitive();
    test_hash_length_sensitive();
    test_hash_null_safe();
    test_hash_empty_differs_from_nonempty();
    test_ct_equal_true();
    test_ct_equal_false();
    test_ct_equal_has_no_branch_on_input();

    test_allow_basic();
    test_deny_cap_missing();
    test_deny_cap_sticky();
    test_deny_toctou();
    test_deny_intent();
    test_deny_integrity();
    test_integrity_bypass_when_disabled();
    test_authorize_null_safe();

    test_priority_integrity_beats_cap();
    test_priority_sticky_beats_toctou();
    test_priority_cap_beats_intent();
    test_priority_toctou_beats_intent();

    test_always_caps();
    test_intent_pass_when_epsilon_dominant();
    test_intent_pass_when_sigma_low();
    test_intent_threshold_boundary();

    test_batch_matches_scalar();
    test_batch_null_safe();

    test_tags();
    test_alloc();

    test_adversarial_ddipe_refused();
    test_adversarial_clawworm_refused();
    test_adversarial_intermediary_toctou();
    test_adversarial_confused_deputy_dlsym();
    test_adversarial_runtime_patch();
    test_composition_v58_evict_guarded();
    test_composition_v59_expand_gated();

    test_deterministic_under_same_inputs();
    test_result_summary_fields();
    test_stress_random_invariants();

    fprintf(stderr, "v60 self-test: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* Banners                                                             */
/* ------------------------------------------------------------------ */

static void print_architecture(void)
{
    printf(
"\nCreation OS v60 — σ-Shield architecture\n"
"======================================\n\n"
"  caller (agent)\n"
"     │ { action_id, required_caps, arg_hash_at_entry,\n"
"     │   arg_hash_at_use, ε, α }\n"
"     ▼\n"
"  cos_v60_authorize (branchless, constant-time)\n"
"     ├─ integrity  : ct_equal64(code_hash, baseline_hash)\n"
"     ├─ sticky     : !(required & sticky_deny)\n"
"     ├─ cap subset : required & ~(holder | always) == 0\n"
"     ├─ TOCTOU     : ct_equal64(hash_entry, hash_use)\n"
"     └─ intent     : !(ε+α ≥ σ_high ∧ α/(ε+α) ≥ α_dom)\n"
"     │\n"
"     ▼  priority cascade (mask AND-NOT)\n"
"  decision ∈ {ALLOW, DENY_CAP, DENY_INTENT, DENY_TOCTOU, DENY_INTEGRITY}\n\n"
"  .cursorrules:  aligned_alloc(64)  |  prefetch(16)\n"
"                 no allocation on hot path  |  no `if` on hot path\n"
"                 constant-time hash equality (no early-exit)\n"
"  dispatch:      P-cores, zero syscalls, zero network\n\n");
}

static void print_positioning(void)
{
    printf(
"\nCreation OS v60 — σ-Shield positioning (Q2 2026)\n"
"================================================\n\n"
"  Attack class (2026)        Defense used today        v60 stance\n"
"  ───────────────────────────────────────────────────────────────\n"
"  DDIPE (2604.03081)         signature/heuristic       σ-intent DENY\n"
"  ClawWorm (2603.15727)      anomaly propagation       σ-intent DENY\n"
"  Mal.Intermediary (2604.08407) allowlist/audit        TOCTOU DENY\n"
"  ShieldNet (2604.04426)     network MITM + events     (composes)\n"
"  Confused deputy            capability (seL4 / WASM)  sticky-deny\n"
"  Code-injection at runtime  AMT / CFI                 integrity DENY\n\n"
"  v60 is the first capability kernel to gate on a σ=(ε,α) intent\n"
"  signal.  ALLOW requires caps AND intent-not-α-dominated AND\n"
"  TOCTOU-clean AND code-integrity-holds.  All five checks run\n"
"  every authorize call; priority cascade picks the first failure\n"
"  but `reason_bits` records them all.\n\n");
}

/* ------------------------------------------------------------------ */
/* Microbench                                                          */
/* ------------------------------------------------------------------ */

static double v60_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

static void microbench_one(int n, int iters)
{
    cos_v60_policy_t p; cos_v60_policy_default(&p);
    cos_v60_request_t *reqs = cos_v60_alloc_requests(n);
    cos_v60_result_t  *out  = (cos_v60_result_t *)v60_alloc64(
                                    (size_t)n * sizeof(cos_v60_result_t));
    uint32_t rng = 0xA5A5A5A5u;
    for (int i = 0; i < n; ++i) {
        zero_req(&reqs[i]);
        reqs[i].required_caps = v60_lcg_next(&rng) & 0xFFFu;
        reqs[i].epistemic = (float)(v60_lcg_next(&rng) & 0xFF) / 256.0f;
        reqs[i].aleatoric = (float)(v60_lcg_next(&rng) & 0xFF) / 256.0f;
    }
    for (int k = 0; k < 2; ++k)
        cos_v60_authorize_batch(reqs, n, (uint64_t)-1, 0x42u, 0x42u, &p, out);
    double t0 = v60_now_ms();
    for (int it = 0; it < iters; ++it)
        cos_v60_authorize_batch(reqs, n, (uint64_t)-1, 0x42u, 0x42u, &p, out);
    double t1 = v60_now_ms();
    double ms = (t1 - t0) / (double)iters;
    double dps = ((double)n / (t1 - t0)) * (double)iters * 1000.0;
    int allow=0, dc=0, di=0, dt=0, dg=0;
    for (int i = 0; i < n; ++i) {
        switch (out[i].decision) {
            case COS_V60_ALLOW:          ++allow; break;
            case COS_V60_DENY_CAP:       ++dc;    break;
            case COS_V60_DENY_INTENT:    ++di;    break;
            case COS_V60_DENY_TOCTOU:    ++dt;    break;
            case COS_V60_DENY_INTEGRITY: ++dg;    break;
        }
    }
    printf("v60 microbench (deterministic synthetic):\n"
           "  requests / iter    : %d\n"
           "  iterations         : %d\n"
           "  ms / iter (avg)    : %.4f\n"
           "  decisions / s      : %.2e\n"
           "  mix: allow=%d deny_cap=%d deny_intent=%d deny_toctou=%d deny_integrity=%d\n\n",
           n, iters, ms, dps, allow, dc, di, dt, dg);
    free(reqs); free(out);
}

static int run_microbench(void)
{
    printf("----- N=128 iters=5000 -----\n");  microbench_one(128, 5000);
    printf("----- N=1024 iters=1000 -----\n"); microbench_one(1024, 1000);
    printf("----- N=8192 iters=200 -----\n");  microbench_one(8192, 200);
    return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    if (argc >= 2) {
        if (strcmp(argv[1], "--self-test")    == 0) return run_self_test();
        if (strcmp(argv[1], "--architecture") == 0) { print_architecture(); return 0; }
        if (strcmp(argv[1], "--positioning")  == 0) { print_positioning();  return 0; }
        if (strcmp(argv[1], "--microbench")   == 0) return run_microbench();
    }
    printf("Creation OS v60 — σ-Shield (runtime security kernel).\n"
           "Usage:\n"
           "  ./creation_os_v60 --self-test\n"
           "  ./creation_os_v60 --architecture\n"
           "  ./creation_os_v60 --positioning\n"
           "  ./creation_os_v60 --microbench\n");
    return 0;
}
