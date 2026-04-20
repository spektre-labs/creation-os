/* σ-Live self-test binary + canonical 10-observation demo. */

#include "live.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_live_self_test();

    /* Canonical demo: the "mixed curve" the self-test also covers. */
    cos_live_obs_t buf[16];
    cos_sigma_live_t l;
    cos_sigma_live_init(&l, buf, 16, 0.30f, 0.60f,
                        0.0f, 1.0f, 0.95f, 0.50f, 4);
    float sigs[10] = {0.05f, 0.10f, 0.15f, 0.20f, 0.25f,
                      0.55f, 0.60f, 0.65f, 0.70f, 0.75f};
    int   corr[10] = { 1,     1,     1,     1,     1,
                       0,     0,     0,     0,     0};
    for (int i = 0; i < 10; ++i)
        cos_sigma_live_observe(&l, sigs[i], corr[i] != 0);

    float life = cos_sigma_live_lifetime_accuracy(&l);

    printf("{\"kernel\":\"sigma_live\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"n_obs\":%u,"
             "\"ring_capacity\":%u,"
             "\"min_samples\":%u,"
             "\"tau_accept\":%.4f,"
             "\"tau_rethink\":%.4f,"
             "\"seed_tau_accept\":%.4f,"
             "\"seed_tau_rethink\":%.4f,"
             "\"accept_accuracy\":%.4f,"
             "\"rethink_accuracy\":%.4f,"
             "\"lifetime_accuracy\":%.4f,"
             "\"n_adapts\":%u},"
           "\"pass\":%s}\n",
           rc,
           (unsigned)l.count, (unsigned)l.capacity,
           (unsigned)l.min_samples,
           (double)l.tau_accept, (double)l.tau_rethink,
           (double)l.seed_tau_accept, (double)l.seed_tau_rethink,
           (double)l.accept_accuracy, (double)l.rethink_accuracy,
           (double)life, (unsigned)l.n_adapts,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Live demo (10 observations, mixed curve):\n");
        fprintf(stderr, "  seeds:  τ_accept %.2f  τ_rethink %.2f\n",
            (double)l.seed_tau_accept, (double)l.seed_tau_rethink);
        fprintf(stderr, "  adapted:τ_accept %.2f  τ_rethink %.2f\n",
            (double)l.tau_accept, (double)l.tau_rethink);
        fprintf(stderr, "  lifetime accuracy = %.1f%% (%llu/%llu)\n",
            100.0 * (double)life,
            (unsigned long long)l.correct_seen,
            (unsigned long long)l.total_seen);
    }
    return rc;
}
