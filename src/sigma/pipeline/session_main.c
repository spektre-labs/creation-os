/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Session self-test binary + canonical 6-turn demo. */

#include "session.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_session_self_test();

    /* Demo: 6 turns, cap 4.  Turn 0: σ=0.80 (evicted later).
     *       Turn 1: σ=0.60.  Turn 2: σ=0.40.  Turn 3: σ=0.20.
     *       Turn 4: σ=0.10 → evicts 0.80.  Turn 5: σ=0.05 → evicts 0.60. */
    cos_session_t s;
    cos_session_turn_t slots[4];
    cos_sigma_session_init(&s, slots, 4);
    float sigs[6] = {0.80f, 0.60f, 0.40f, 0.20f, 0.10f, 0.05f};
    double costs[6] = {0.012, 0.012, 0.001, 0.0001, 0.0001, 0.0001};
    int esc[6] = {1, 1, 0, 0, 0, 0};
    for (int i = 0; i < 6; ++i)
        cos_sigma_session_append(&s, "prompt", "response",
                                 sigs[i], costs[i], (uint64_t)(i + 1), esc[i]);

    /* Save to a temp buffer and reload to verify round-trip. */
    FILE *tmp = tmpfile();
    cos_sigma_session_save(&s, tmp);
    rewind(tmp);
    cos_session_t s2;
    cos_session_turn_t slots2[4];
    int load_rc = cos_sigma_session_load(&s2, slots2, 4, tmp);
    fclose(tmp);

    /* Surviving turn σs (sorted ascending for determinism). */
    float live[4];
    for (int i = 0; i < s.count; ++i) live[i] = s.slots[i].sigma;
    /* insertion sort */
    for (int i = 1; i < s.count; ++i)
        for (int j = i; j > 0 && live[j-1] > live[j]; --j) {
            float t = live[j-1]; live[j-1] = live[j]; live[j] = t;
        }

    int drift = cos_sigma_session_is_drifting(&s, 0.05f, 4);

    printf("{\"kernel\":\"sigma_session\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"cap\":%d,\"count\":%d,\"evicted\":%d,"
             "\"next_turn_id\":%d,"
             "\"total_cost_eur\":%.6f,"
             "\"sigma_mean\":%.4f,\"sigma_min\":%.4f,"
             "\"sigma_max\":%.4f,\"sigma_slope\":%.4f,"
             "\"drifting_at_0.05\":%s,"
             "\"surviving_sigmas\":[%.4f,%.4f,%.4f,%.4f],"
             "\"save_load_rc\":%d,"
             "\"reload_count\":%d,"
             "\"reload_sigma_mean\":%.4f"
           "},"
           "\"pass\":%s}\n",
           rc, s.cap, s.count, s.n_evicted, s.next_turn_id,
           s.total_cost_eur,
           (double)s.sigma_mean, (double)s.sigma_min,
           (double)s.sigma_max, (double)s.sigma_slope,
           drift ? "true" : "false",
           (double)live[0], (double)live[1], (double)live[2], (double)live[3],
           load_rc, s2.count, (double)s2.sigma_mean,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Session demo: cap=%d count=%d evicted=%d\n",
                s.cap, s.count, s.n_evicted);
        fprintf(stderr, "  σ_mean=%.3f slope=%.3f drifting(0.05)=%s\n",
                (double)s.sigma_mean, (double)s.sigma_slope,
                drift ? "true" : "false");
    }
    return rc;
}
