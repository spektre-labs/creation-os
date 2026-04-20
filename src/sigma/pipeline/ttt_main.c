/* Entry point for the σ-TTT self-test binary. */

#include "ttt.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_ttt_self_test();

    /* One demo state to report in the JSON header so the smoke script
     * can pin the canonical numbers. */
    cos_sigma_ttt_state_t st;
    float slow[8] = {1,1,1,1,1,1,1,1};
    float fast[8];
    cos_sigma_ttt_init(&st, fast, slow, 8,
                       /*lr=*/0.1f, /*tau_sigma=*/0.5f,
                       /*tau_drift=*/0.25f);
    float grad[8] = {1,1,1,1,1,1,1,1};
    cos_sigma_ttt_step(&st, 0.10f, grad);   /* skip */
    cos_sigma_ttt_step(&st, 0.90f, grad);   /* update */
    cos_sigma_ttt_step(&st, 0.90f, grad);   /* update */
    cos_sigma_ttt_step(&st, 0.90f, grad);   /* update, likely drift */
    float drift = cos_sigma_ttt_drift(&st);
    int did_reset = cos_sigma_ttt_reset_if_drift(&st);

    printf("{\"kernel\":\"sigma_ttt\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"n\":%d,\"lr\":%.4f,\"tau_sigma\":%.4f,"
             "\"tau_drift\":%.4f,"
             "\"n_steps_total\":%d,\"n_steps_updated\":%d,"
             "\"drift_pre_reset\":%.6f,"
             "\"did_reset\":%s,\"n_resets\":%d},"
           "\"pass\":%s}\n",
           rc,
           st.n, (double)st.lr, (double)st.tau_sigma,
           (double)st.tau_drift,
           st.n_steps_total, st.n_steps_updated,
           (double)drift,
           did_reset ? "true" : "false", st.n_resets,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-TTT demo:\n");
        fprintf(stderr, "  lr=%.4f  τ_σ=%.4f  τ_drift=%.4f\n",
                (double)st.lr, (double)st.tau_sigma,
                (double)st.tau_drift);
        fprintf(stderr, "  after 1 skip + 3 updates:\n");
        for (int i = 0; i < st.n; ++i)
            fprintf(stderr, "    fast[%d]=%.4f  slow[%d]=%.4f\n",
                    i, (double)st.fast[i], i, (double)st.slow[i]);
    }
    return rc;
}
