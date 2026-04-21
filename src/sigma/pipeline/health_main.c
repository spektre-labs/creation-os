/* σ-Health binary (cos-health).
 *
 *   cos-health                  — human text, baked defaults
 *   cos-health --json           — stable JSON receipt, baked defaults
 *   cos-health --state PATH     — merge in engram count / cost / τ
 *                                  from a σ-Persist SQLite file
 *   cos-health --live           — DEV-7: enrich with live runtime
 *                                  metrics (engram.db, distill_pairs,
 *                                  llama-server /health probe)
 *   cos-health --no-live        — disable live (for deterministic CI)
 *   cos-health --fail-if NOT_HEALTHY
 *                               — exit 1 if status != HEALTHY
 *                                 (for CI / watchdogs)
 *
 * Determinism: the baked defaults (20 primitives, 38 checks,
 * 4 substrates, 6/6 proofs) are compile-time constants, so a run
 * without --state produces the same JSON on every host.
 */

#include "health.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(int rc) {
    puts("usage: cos-health [--json] [--state <path>] [--live] [--no-live]\n"
         "                  [--fail-if NOT_HEALTHY]");
    return rc;
}

int main(int argc, char **argv) {
    int json           = 0;
    int fail_if_degr   = 0;
    const char *state  = NULL;
    /* DEV-7: live runtime enrichment on by default for interactive use.
     * CI pins behaviour with --no-live, or COS_HEALTH_NO_LIVE=1. */
    int live           = 1;
    const char *no_live_env = getenv("COS_HEALTH_NO_LIVE");
    if (no_live_env != NULL && no_live_env[0] == '1') live = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--json") == 0)      { json = 1; continue; }
        if (strcmp(argv[i], "--state") == 0 && i + 1 < argc) {
            state = argv[++i]; continue;
        }
        if (strcmp(argv[i], "--live")    == 0) { live = 1; continue; }
        if (strcmp(argv[i], "--no-live") == 0) { live = 0; continue; }
        if (strcmp(argv[i], "--fail-if") == 0 && i + 1 < argc) {
            if (strcmp(argv[++i], "NOT_HEALTHY") == 0) fail_if_degr = 1;
            continue;
        }
        if (strcmp(argv[i], "--self-test") == 0) {
            return cos_sigma_health_self_test() == 0 ? 0 : 1;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
            return usage(0);
        /* Unknown arg → ignore quietly (keeps CI-friendly). */
    }

    cos_health_report_t r;
    cos_sigma_health_init_defaults(&r);
    if (state) cos_sigma_health_load_state(&r, state);
    if (live)  cos_sigma_health_load_live (&r);
    cos_sigma_health_grade(&r);
    cos_sigma_health_emit(&r, json);

    if (fail_if_degr && r.status != COS_HEALTH_HEALTHY) return 1;
    return 0;
}
