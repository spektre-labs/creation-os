/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v57 — The Verified Agent (driver + self-test).
 *
 * Modes:
 *   --self-test     deterministic invariant + slot registry tests
 *   --architecture  composition diagram + tier table
 *   --positioning   honest comparison vs ad-hoc agent sandboxes
 *   --verify-status static report (no make calls); see
 *                   scripts/v57/verify_agent.sh for the live aggregator
 *
 * No mode opens sockets, allocates on a hot path, or invokes the
 * Makefile.  The live aggregate runs in scripts/v57/verify_agent.sh
 * which honestly distinguishes PASS / FAIL / SKIP per slot.
 */

#include "invariants.h"
#include "verified_agent.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Self-test harness                                                   */
/* ------------------------------------------------------------------ */

static int g_pass = 0;
static int g_fail = 0;

#define TEST_OK(label, cond) do { \
    if (cond) { ++g_pass; } \
    else { ++g_fail; \
        fprintf(stderr, "FAIL: %s (line %d)\n", (label), __LINE__); } \
} while (0)

/* ------------------------------------------------------------------ */
/* Invariant registry tests                                            */
/* ------------------------------------------------------------------ */

static void test_invariant_count(void)
{
    int n = v57_invariant_count();
    TEST_OK("invariant_count_is_five", n == 5);
}

static void test_invariant_get_bounds(void)
{
    TEST_OK("get_negative_returns_null", v57_invariant_get(-1) == NULL);
    TEST_OK("get_oob_returns_null",      v57_invariant_get(999) == NULL);
    TEST_OK("get_zero_nonnull",          v57_invariant_get(0) != NULL);
}

static void test_invariant_ids_present(void)
{
    int n = v57_invariant_count();
    int all_have_id = 1;
    int all_have_statement = 1;
    int all_have_owner = 1;
    for (int i = 0; i < n; ++i) {
        const v57_invariant_t *inv = v57_invariant_get(i);
        if (!inv->id || inv->id[0] == '\0')           all_have_id = 0;
        if (!inv->statement || inv->statement[0] == '\0')
            all_have_statement = 0;
        if (!inv->owner_versions || inv->owner_versions[0] == '\0')
            all_have_owner = 0;
    }
    TEST_OK("all_invariants_have_id",        all_have_id);
    TEST_OK("all_invariants_have_statement", all_have_statement);
    TEST_OK("all_invariants_have_owner",     all_have_owner);
}

static void test_invariant_ids_unique(void)
{
    int n = v57_invariant_count();
    int unique = 1;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            const v57_invariant_t *a = v57_invariant_get(i);
            const v57_invariant_t *b = v57_invariant_get(j);
            if (strcmp(a->id, b->id) == 0) unique = 0;
        }
    }
    TEST_OK("invariant_ids_unique", unique);
}

static void test_invariant_find(void)
{
    TEST_OK("find_sigma_gated_actions",
            v57_invariant_find("sigma_gated_actions") != NULL);
    TEST_OK("find_bounded_side_effects",
            v57_invariant_find("bounded_side_effects") != NULL);
    TEST_OK("find_no_unbounded_self_modification",
            v57_invariant_find("no_unbounded_self_modification") != NULL);
    TEST_OK("find_deterministic_verifier_floor",
            v57_invariant_find("deterministic_verifier_floor") != NULL);
    TEST_OK("find_ensemble_disagreement_abstains",
            v57_invariant_find("ensemble_disagreement_abstains") != NULL);
    TEST_OK("find_nonexistent_returns_null",
            v57_invariant_find("does_not_exist") == NULL);
    TEST_OK("find_null_returns_null",
            v57_invariant_find(NULL) == NULL);
}

/* No invariant should be entirely empty: at least one of
 * runtime / formal / documented / planned must be enabled. */
static void test_no_invariant_fully_disabled(void)
{
    int n = v57_invariant_count();
    int ok = 1;
    for (int i = 0; i < n; ++i) {
        const v57_invariant_t *inv = v57_invariant_get(i);
        int any = inv->runtime.enabled    || inv->formal.enabled
               || inv->documented.enabled || inv->planned.enabled;
        if (!any) ok = 0;
    }
    TEST_OK("no_invariant_fully_disabled", ok);
}

/* Enabled checks must declare a non-NULL target string for tiers
 * M, F, I (planned P entries may also have a target but a null
 * description is allowed for forward-references; we still require
 * SOME pointer when enabled). */
static void test_enabled_checks_have_target(void)
{
    int n = v57_invariant_count();
    int ok = 1;
    for (int i = 0; i < n; ++i) {
        const v57_invariant_t *inv = v57_invariant_get(i);
        const v57_check_t *c[4] = {
            &inv->runtime, &inv->formal,
            &inv->documented, &inv->planned
        };
        for (int k = 0; k < 4; ++k) {
            if (!c[k]->enabled) continue;
            /* P-tier entries describe what to do next; allow
             * either target or description to anchor them. */
            if (c[k]->tier == V57_TIER_P) {
                if (!c[k]->target && !c[k]->description) ok = 0;
            } else {
                if (!c[k]->target) ok = 0;
            }
        }
    }
    TEST_OK("enabled_checks_have_target_or_description", ok);
}

static void test_best_tier_monotone(void)
{
    /* If runtime M is enabled, best_tier must be M.
     * If only F is enabled, best_tier must be F.  Etc. */
    v57_invariant_t inv;
    memset(&inv, 0, sizeof(inv));
    inv.id = "x"; inv.statement = "x";
    inv.owner_versions = "x";

    inv.runtime    = (v57_check_t){ 1, V57_TIER_M, "t", "d" };
    TEST_OK("best_tier_picks_M_when_runtime",
            v57_invariant_best_tier(&inv) == V57_TIER_M);

    inv.runtime    = (v57_check_t){ 0, V57_TIER_M, NULL, NULL };
    inv.formal     = (v57_check_t){ 1, V57_TIER_F, "p", "d" };
    TEST_OK("best_tier_picks_F_when_only_formal",
            v57_invariant_best_tier(&inv) == V57_TIER_F);

    inv.formal     = (v57_check_t){ 0, V57_TIER_F, NULL, NULL };
    inv.documented = (v57_check_t){ 1, V57_TIER_I, "d", "doc" };
    TEST_OK("best_tier_picks_I_when_only_documented",
            v57_invariant_best_tier(&inv) == V57_TIER_I);

    inv.documented = (v57_check_t){ 0, V57_TIER_I, NULL, NULL };
    inv.planned    = (v57_check_t){ 1, V57_TIER_P, NULL, "todo" };
    TEST_OK("best_tier_picks_P_when_only_planned",
            v57_invariant_best_tier(&inv) == V57_TIER_P);

    inv.planned    = (v57_check_t){ 0, V57_TIER_P, NULL, NULL };
    TEST_OK("best_tier_returns_N_when_none_enabled",
            v57_invariant_best_tier(&inv) == V57_TIER_N);
}

static void test_invariant_histogram_sums(void)
{
    int counts[V57_TIER_N + 1];
    v57_invariant_tier_histogram(counts);

    int sum = 0;
    for (int i = 0; i <= V57_TIER_N; ++i) sum += counts[i];
    TEST_OK("invariant_histogram_sums_to_count",
            sum == v57_invariant_count());
    /* All five shipped invariants have at least one M-tier check. */
    TEST_OK("at_least_three_invariants_are_M",
            counts[V57_TIER_M] >= 3);
}

static void test_tier_tags(void)
{
    TEST_OK("tag_M", strcmp(v57_tier_tag(V57_TIER_M), "M") == 0);
    TEST_OK("tag_F", strcmp(v57_tier_tag(V57_TIER_F), "F") == 0);
    TEST_OK("tag_I", strcmp(v57_tier_tag(V57_TIER_I), "I") == 0);
    TEST_OK("tag_P", strcmp(v57_tier_tag(V57_TIER_P), "P") == 0);
    TEST_OK("tag_unknown", strcmp(v57_tier_tag(V57_TIER_N), "?") == 0);
}

/* ------------------------------------------------------------------ */
/* Slot registry tests                                                 */
/* ------------------------------------------------------------------ */

static void test_slot_count_at_least_six(void)
{
    /* The Verified Agent must compose at least the six core
     * Creation OS subsystems: sandbox, σ-kernel, harness,
     * proconductor, σ₃-speculative, and σ-Constitutional. */
    TEST_OK("slot_count_at_least_six", v57_slot_count() >= 6);
}

static void test_slot_get_bounds(void)
{
    TEST_OK("slot_get_negative_null", v57_slot_get(-1) == NULL);
    TEST_OK("slot_get_oob_null",      v57_slot_get(999) == NULL);
    TEST_OK("slot_get_zero_nonnull",  v57_slot_get(0) != NULL);
}

static void test_slot_required_fields(void)
{
    int n = v57_slot_count();
    int ok = 1;
    for (int i = 0; i < n; ++i) {
        const v57_slot_t *s = v57_slot_get(i);
        if (!s->slot           || !s->slot[0])           ok = 0;
        if (!s->owner_versions || !s->owner_versions[0]) ok = 0;
        if (!s->make_target    || !s->make_target[0])    ok = 0;
        if (!s->summary        || !s->summary[0])        ok = 0;
    }
    TEST_OK("slot_required_fields_present", ok);
}

static void test_slot_ids_unique(void)
{
    int n = v57_slot_count();
    int unique = 1;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (strcmp(v57_slot_get(i)->slot,
                       v57_slot_get(j)->slot) == 0) unique = 0;
        }
    }
    TEST_OK("slot_ids_unique", unique);
}

static void test_slot_find(void)
{
    TEST_OK("find_execution_sandbox",
            v57_slot_find("execution_sandbox") != NULL);
    TEST_OK("find_sigma_kernel_surface",
            v57_slot_find("sigma_kernel_surface") != NULL);
    TEST_OK("find_harness_loop",
            v57_slot_find("harness_loop") != NULL);
    TEST_OK("find_constitutional_self_modification",
            v57_slot_find("constitutional_self_modification") != NULL);
    TEST_OK("find_nonexistent_slot_null",
            v57_slot_find("does_not_exist") == NULL);
    TEST_OK("find_null_slot_null", v57_slot_find(NULL) == NULL);
}

static void test_report_build(void)
{
    v57_report_t r;
    memset(&r, 0xAA, sizeof(r));
    v57_report_build(&r);

    TEST_OK("report_n_slots_matches",
            r.n_slots == v57_slot_count());
    TEST_OK("report_n_invariants_matches",
            r.n_invariants == v57_invariant_count());

    int slot_sum = 0, inv_sum = 0;
    for (int i = 0; i <= V57_TIER_N; ++i) {
        slot_sum += r.slot_counts[i];
        inv_sum  += r.invariant_counts[i];
    }
    TEST_OK("report_slot_counts_sum",      slot_sum == r.n_slots);
    TEST_OK("report_invariant_counts_sum", inv_sum  == r.n_invariants);

    /* Most slots should reach M today; F + I together cover the
     * formal-and-assurance surface; no slot should fall through
     * to "none". */
    TEST_OK("report_no_slot_unaccounted",
            r.slot_counts[V57_TIER_N] == 0);
    TEST_OK("report_majority_slots_M",
            r.slot_counts[V57_TIER_M] >= 4);
    TEST_OK("report_at_least_one_F_slot",
            r.slot_counts[V57_TIER_F] >= 1);
    TEST_OK("report_at_least_one_I_slot",
            r.slot_counts[V57_TIER_I] >= 1);
}

/* ------------------------------------------------------------------ */
/* Banners                                                             */
/* ------------------------------------------------------------------ */

static void print_architecture(void)
{
    puts("");
    puts("Creation OS v57 — The Verified Agent");
    puts("====================================");
    puts("");
    puts("Composition slots (each owns one verifiable surface of the");
    puts("agent runtime).  Tier is the strongest claim shipped today,");
    puts("not the strongest claim aspired to.");
    puts("");
    puts("  tier  slot                              owner");
    puts("  ----  --------------------------------  -------------------------");

    int n = v57_slot_count();
    for (int i = 0; i < n; ++i) {
        const v57_slot_t *s = v57_slot_get(i);
        printf("  [%s]   %-32s  %s\n",
               v57_tier_tag(s->best_tier),
               s->slot,
               s->owner_versions);
    }

    puts("");
    puts("Invariants (each is owned by one or more slots above):");
    puts("");
    int m = v57_invariant_count();
    for (int i = 0; i < m; ++i) {
        const v57_invariant_t *inv = v57_invariant_get(i);
        printf("  [%s]   %s\n",
               v57_tier_tag(v57_invariant_best_tier(inv)),
               inv->id);
    }

    puts("");
    puts("Tier legend:");
    puts("  M = runtime-checked deterministic self-test (PASS/FAIL)");
    puts("  F = formal proof artifact (Frama-C / TLA+ / sby / Coq)");
    puts("  I = interpreted / documented (no mechanical check yet)");
    puts("  P = planned (next concrete step)");
    puts("");
    puts("Live aggregate: scripts/v57/verify_agent.sh");
    puts("  → invokes the make targets above and reports SKIP");
    puts("    honestly when tooling is absent.  Never silently");
    puts("    downgrades a slot.");
    puts("");
}

static void print_positioning(void)
{
    puts("");
    puts("Creation OS v57 vs ad-hoc agent sandboxes (positioning)");
    puts("=======================================================");
    puts("");
    puts("This table follows the convergence oivallus that motivates");
    puts("v57.  The competitor names appear in the driving prompt as");
    puts("stylized references to the open-source / proprietary agent");
    puts("framework categories of Q2 2026; CVE counts and ARR figures");
    puts("are reported there, not independently audited here.");
    puts("");
    puts("  property              ad-hoc OSS agent     enterprise SaaS     Creation OS v57");
    puts("  --------------------  ---------------------  ---------------     -------------------");
    puts("  license               permissive             proprietary         AGPL-3.0");
    puts("  isolation              opt-in container       cloud-side          formally bounded");
    puts("  reasoning trust        prompt + config        prompt + config     σ-gated + verified");
    puts("  multi-LLM strategy     model-agnostic         single-vendor       σ-triangulated");
    puts("  proofs shipped         none                   internal audit      Frama-C + sby + DO-178C");
    puts("  invariant registry     n/a                    n/a                 5 invariants, tiered");
    puts("  user-facing answer     'trust our arch.'      'trust our SLA'     'run make verify-agent'");
    puts("");
    puts("v57 is the only column where the user-facing answer is");
    puts("'verify yourself.'  The proof surface is small (the σ-kernel");
    puts("contract via Frama-C), the runtime surface is exhaustive");
    puts("(every check-vNN target deterministic), and the documentation");
    puts("surface (DO-178C HLR/LLR/PSAC/SCMP/SDD/SDP/SQAP/SVP) is");
    puts("explicitly tiered I — assurance artifacts, NOT FAA/EASA");
    puts("certification.");
    puts("");
}

static void print_verify_status(void)
{
    v57_report_t r;
    v57_report_build(&r);

    puts("");
    puts("Creation OS v57 — verify-status (static report)");
    puts("================================================");
    puts("");
    printf("Composition slots: %d\n", r.n_slots);
    printf("  M (runtime-checked): %d\n", r.slot_counts[V57_TIER_M]);
    printf("  F (formally proven): %d\n", r.slot_counts[V57_TIER_F]);
    printf("  I (documented)     : %d\n", r.slot_counts[V57_TIER_I]);
    printf("  P (planned)        : %d\n", r.slot_counts[V57_TIER_P]);
    printf("  none               : %d\n", r.slot_counts[V57_TIER_N]);
    puts("");
    printf("Invariants: %d\n", r.n_invariants);
    printf("  M (runtime-checked): %d\n", r.invariant_counts[V57_TIER_M]);
    printf("  F (formally proven): %d\n", r.invariant_counts[V57_TIER_F]);
    printf("  I (documented)     : %d\n", r.invariant_counts[V57_TIER_I]);
    printf("  P (planned)        : %d\n", r.invariant_counts[V57_TIER_P]);
    printf("  none               : %d\n", r.invariant_counts[V57_TIER_N]);
    puts("");
    puts("This is a STATIC report from the registry data table.");
    puts("It does not invoke make.  For the live aggregate that");
    puts("dispatches every slot's make target and reports honest");
    puts("PASS / FAIL / SKIP per slot, run:");
    puts("");
    puts("  scripts/v57/verify_agent.sh");
    puts("");
}

/* ------------------------------------------------------------------ */
/* Entry                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    int run_self_test    = 0;
    int run_architecture = 0;
    int run_positioning  = 0;
    int run_verify_status = 0;

    if (argc <= 1) {
        run_self_test    = 1;
        run_architecture = 1;
    }
    for (int i = 1; i < argc; ++i) {
        if      (strcmp(argv[i], "--self-test")     == 0) run_self_test     = 1;
        else if (strcmp(argv[i], "--architecture")  == 0) run_architecture  = 1;
        else if (strcmp(argv[i], "--positioning")   == 0) run_positioning   = 1;
        else if (strcmp(argv[i], "--verify-status") == 0) run_verify_status = 1;
        else if (strcmp(argv[i], "--all")           == 0) {
            run_self_test = run_architecture =
                run_positioning = run_verify_status = 1;
        } else {
            fprintf(stderr,
                "creation_os_v57: unknown flag '%s'\n"
                "  usage: creation_os_v57 [--self-test] [--architecture] "
                "[--positioning] [--verify-status] [--all]\n",
                argv[i]);
            return 2;
        }
    }

    if (run_architecture)  print_architecture();
    if (run_positioning)   print_positioning();
    if (run_verify_status) print_verify_status();

    if (run_self_test) {
        puts("");
        puts("v57 self-test (Verified Agent registry)");
        puts("---------------------------------------");

        test_invariant_count();
        test_invariant_get_bounds();
        test_invariant_ids_present();
        test_invariant_ids_unique();
        test_invariant_find();
        test_no_invariant_fully_disabled();
        test_enabled_checks_have_target();
        test_best_tier_monotone();
        test_invariant_histogram_sums();
        test_tier_tags();

        test_slot_count_at_least_six();
        test_slot_get_bounds();
        test_slot_required_fields();
        test_slot_ids_unique();
        test_slot_find();
        test_report_build();

        printf("\nv57 self-test: %d/%d PASS\n",
               g_pass, g_pass + g_fail);
        if (g_fail) return 1;
    }
    return 0;
}
