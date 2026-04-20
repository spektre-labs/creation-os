/* σ-TinyML self-test binary + canonical 8-sample demo. */

#include "tinyml.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_tinyml_self_test();

    /* Canonical demo: 6 stable temperature readings then 2 spikes. */
    cos_sigma_tinyml_state_t s;
    cos_sigma_tinyml_init(&s);
    float xs[8] = {20.0f, 20.1f, 19.9f, 20.0f, 20.0f, 20.05f,
                   /* spike 1: 10 °C above baseline  */
                   30.0f,
                   /* back to baseline              */
                   20.1f};
    float sig[8];
    for (int i = 0; i < 8; ++i) sig[i] = cos_sigma_tinyml_observe(&s, xs[i]);
    bool anomalous_at_spike = (sig[6] > 0.9f);

    printf("{\"kernel\":\"sigma_tinyml\","
           "\"self_test_rc\":%d,"
           "\"state_bytes\":%zu,"
           "\"z_alert\":%.2f,"
           "\"demo\":["
             "{\"x\":%.2f,\"sigma\":%.4f},"
             "{\"x\":%.2f,\"sigma\":%.4f},"
             "{\"x\":%.2f,\"sigma\":%.4f},"
             "{\"x\":%.2f,\"sigma\":%.4f},"
             "{\"x\":%.2f,\"sigma\":%.4f},"
             "{\"x\":%.2f,\"sigma\":%.4f},"
             "{\"x\":%.2f,\"sigma\":%.4f},"
             "{\"x\":%.2f,\"sigma\":%.4f}],"
           "\"anomaly_at_spike\":%s,"
           "\"final_mean\":%.4f,"
           "\"final_count\":%u,"
           "\"pass\":%s}\n",
           rc, sizeof(cos_sigma_tinyml_state_t),
           (double)COS_SIGMA_TINYML_Z_ALERT,
           (double)xs[0], (double)sig[0],
           (double)xs[1], (double)sig[1],
           (double)xs[2], (double)sig[2],
           (double)xs[3], (double)sig[3],
           (double)xs[4], (double)sig[4],
           (double)xs[5], (double)sig[5],
           (double)xs[6], (double)sig[6],
           (double)xs[7], (double)sig[7],
           anomalous_at_spike ? "true" : "false",
           (double)s.mean, (unsigned)s.count,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-TinyML demo (12-byte state):\n");
        for (int i = 0; i < 8; ++i)
            fprintf(stderr, "  x = %5.2f °C  σ = %.3f%s\n",
                    (double)xs[i], (double)sig[i],
                    (i == 6) ? "   ← anomaly" : "");
    }
    return rc;
}
