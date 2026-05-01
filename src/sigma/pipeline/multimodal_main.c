/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Multimodal self-test binary. */

#include "multimodal.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_multimodal_self_test();

    float s_text  = cos_sigma_measure_text(0.42f);
    float s_code_ok  = cos_sigma_measure_code("void f(){}", 0);
    float s_code_bad = cos_sigma_measure_code("void f(){", 0);
    const char *blob = "{\"id\":1,\"name\":\"a\"}";
    const char *req1[] = {"id", "name"};
    const char *req2[] = {"id", "name", "email"};
    float s_schema_ok  = cos_sigma_measure_schema(blob, 0, req1, 2);
    float s_schema_bad = cos_sigma_measure_schema(blob, 0, req2, 3);
    float s_image_high = cos_sigma_measure_image(0.95f);
    float s_image_low  = cos_sigma_measure_image(0.10f);
    float s_audio      = cos_sigma_measure_audio(0.75f);

    printf("{\"kernel\":\"sigma_multimodal\","
           "\"self_test_rc\":%d,"
           "\"modalities\":[\"TEXT\",\"CODE\",\"STRUCTURED\","
                            "\"IMAGE\",\"AUDIO\"],"
           "\"demo\":{"
             "\"text_sigma\":%.4f,"
             "\"code_ok\":%.4f,\"code_bad\":%.4f,"
             "\"schema_ok\":%.4f,\"schema_one_missing\":%.4f,"
             "\"image_similar\":%.4f,\"image_mismatch\":%.4f,"
             "\"audio\":%.4f},"
           "\"pass\":%s}\n",
           rc,
           (double)s_text,
           (double)s_code_ok, (double)s_code_bad,
           (double)s_schema_ok, (double)s_schema_bad,
           (double)s_image_high, (double)s_image_low,
           (double)s_audio,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Multimodal demo:\n");
        fprintf(stderr, "  text σ=0.42        → %.2f\n", (double)s_text);
        fprintf(stderr, "  code OK             → %.2f\n", (double)s_code_ok);
        fprintf(stderr, "  code syntax err     → %.2f\n", (double)s_code_bad);
        fprintf(stderr, "  schema full         → %.2f\n", (double)s_schema_ok);
        fprintf(stderr, "  schema 1 missing    → %.2f\n", (double)s_schema_bad);
        fprintf(stderr, "  image CLIP=0.95     → %.2f\n", (double)s_image_high);
        fprintf(stderr, "  image CLIP=0.10     → %.2f\n", (double)s_image_low);
        fprintf(stderr, "  audio sim=0.75      → %.2f\n", (double)s_audio);
    }
    return rc;
}
