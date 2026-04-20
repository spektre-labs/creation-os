/* σ-Curriculum self-test + canonical ladder demo. */

#include "curriculum.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_curriculum_self_test();

    /* Canonical demo: 5-level ladder, min_rounds=2, τ=0.30.
     * Deterministic σ stream designed to:
     *   1) promote 1→2   (two successes)
     *   2) fail once     (RESET)
     *   3) promote 2→3   (two successes)
     *   4) promote 3→4   (two successes)
     *   5) fail once     (RESET)
     *   6) promote 4→5   (two successes)
     *   7) attempt 5→6   (MAXED, stay at 5)
     */
    cos_curriculum_state_t s;
    cos_sigma_curriculum_init(&s, 5, 2, 0.30f);

    float stream[] = {
        0.10f, 0.15f,       /* → PROMOTED level 2 (streak 2)    */
        0.10f, 0.70f,       /* → RESET (success then fail)      */
        0.10f, 0.20f,       /* → PROMOTED level 3               */
        0.05f, 0.25f,       /* → PROMOTED level 4               */
        0.10f, 0.65f,       /* → RESET                          */
        0.10f, 0.10f,       /* → PROMOTED level 5               */
        0.05f, 0.05f,       /* → MAXED on the second step       */
    };
    const int n = (int)(sizeof stream / sizeof stream[0]);
    int n_promoted = 0, n_maxed = 0, n_reset = 0;
    for (int i = 0; i < n; ++i) {
        cos_curriculum_event_t ev = cos_sigma_curriculum_step(&s, stream[i]);
        if (ev == COS_CUR_PROMOTED) n_promoted++;
        if (ev == COS_CUR_MAXED)    n_maxed++;
        if (ev == COS_CUR_RESET)    n_reset++;
    }
    float rate = cos_sigma_curriculum_acceptance_rate(&s);

    printf("{\"kernel\":\"sigma_curriculum\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"max_level\":%d,\"min_rounds\":%d,\"sigma_threshold\":%.2f,"
             "\"rounds\":%d,\"promotions\":%d,\"resets\":%d,\"maxed\":%d,"
             "\"final_level\":%d,\"final_level_name\":\"%s\","
             "\"total_success\":%d,\"acceptance_rate\":%.4f,"
             "\"last_promotion_at\":%d"
             "},"
           "\"pass\":%s}\n",
           rc,
           s.max_level, s.min_rounds, (double)s.sigma_threshold,
           s.total_rounds, s.total_promotions, n_reset, n_maxed,
           s.level, cos_sigma_curriculum_level_name(s.level),
           s.total_success, (double)rate, s.last_promotion_at,
           (rc == 0) ? "true" : "false");
    (void)n_promoted;

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Curriculum demo: final level %d (%s), "
                        "promotions=%d resets=%d acceptance=%.2f\n",
                s.level, cos_sigma_curriculum_level_name(s.level),
                s.total_promotions, n_reset, (double)rate);
    }
    return rc;
}
