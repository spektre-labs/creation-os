/* σ-Photonic self-test + canonical optical demo.
 *
 * Three scenarios:
 *   A. dominant     — one channel holds 90% of intensity → σ ≈ 0
 *   B. uniform      — four equal channels                → σ = 0.75
 *   C. mach_zehnder — one constructive arm + three destructive
 *                     arms (V=1)                          → σ ≪ 0.05
 */

#include "photonic.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_photonic_self_test();

    float dominant[4]  = { 0.02f, 0.03f, 0.90f, 0.05f };
    float uniform[4]   = { 0.25f, 0.25f, 0.25f, 0.25f };
    float s_d = 0.0f, s_u = 0.0f, s_m = 0.0f;
    float total_d = 0.0f, max_d = 0.0f;
    int   argmax_d = 0;
    cos_sigma_photonic_sigma_from_intensities(dominant, 4,
                                              &s_d, &total_d, &max_d, &argmax_d);
    cos_sigma_photonic_sigma_from_intensities(uniform, 4,
                                              &s_u, NULL, NULL, NULL);

    float phases[4] = { 0.0f, 3.14159265f, 3.14159265f, 3.14159265f };
    cos_photon_channel_t ch[4];
    cos_sigma_photonic_mach_zehnder_sigma(1.0f, 1.0f, phases, 4, ch, &s_m);

    printf("{\"kernel\":\"sigma_photonic\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"dominant\":{\"channels\":4,\"total\":%.4f,"
                            "\"max\":%.4f,\"argmax\":%d,\"sigma\":%.4f},"
             "\"uniform\":{\"channels\":4,\"sigma\":%.4f},"
             "\"mach_zehnder\":{\"channels\":4,\"visibility\":1.00,"
                                "\"arm0_intensity\":%.4f,"
                                "\"arm1_intensity\":%.4f,"
                                "\"sigma\":%.4f}"
             "},"
           "\"pass\":%s}\n",
           rc,
           (double)total_d, (double)max_d, argmax_d, (double)s_d,
           (double)s_u,
           (double)ch[0].intensity, (double)ch[1].intensity,
           (double)s_m,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Photonic: dominant=%.2f uniform=%.2f mzi=%.3f\n",
                (double)s_d, (double)s_u, (double)s_m);
    }
    return rc;
}
