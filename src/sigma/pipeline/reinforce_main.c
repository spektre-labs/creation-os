/* Entry point for the σ-reinforce self-test binary.
 *
 * Emits a one-line JSON summary on stdout so a CI smoke script can
 * grep it.  Exit code 0 on full PASS. */

#include "reinforce.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_reinforce_self_test();
    printf("{\"kernel\":\"sigma_reinforce\","
           "\"self_test_rc\":%d,"
           "\"max_rounds\":%d,"
           "\"canonical_actions\":[\"ACCEPT\",\"RETHINK\",\"ABSTAIN\"],"
           "\"rethink_suffix_len\":%zu,"
           "\"pass\":%s}\n",
           rc,
           cos_sigma_reinforce_max_rounds(),
           strlen(cos_sigma_reinforce_rethink_suffix()),
           (rc == 0) ? "true" : "false");
    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        const float grid[] = {0.10f, 0.35f, 0.55f, 0.85f};
        for (size_t i = 0; i < sizeof grid / sizeof grid[0]; ++i) {
            cos_sigma_action_t a =
                cos_sigma_reinforce(grid[i], 0.30f, 0.70f);
            fprintf(stderr, "  σ=%.2f  τa=0.30  τr=0.70  →  %s\n",
                    (double)grid[i], cos_sigma_action_label(a));
        }
    }
    return rc;
}
