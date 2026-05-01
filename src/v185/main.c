/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v185 σ-Multimodal-Fusion — CLI entry.
 *
 *   ./creation_os_v185_fusion --self-test
 *   ./creation_os_v185_fusion            # full fusion JSON
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "fusion.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v185_self_test();
        if (rc == 0) puts("v185 self-test: OK");
        else         fprintf(stderr, "v185 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v185_state_t s;
    cos_v185_init(&s, 0x185FEEDULL);
    cos_v185_register(&s, "vision", "siglip",       768,  "sigma_vis");
    cos_v185_register(&s, "audio",  "whisper",      512,  "sigma_aud");
    cos_v185_register(&s, "text",   "bitnet",      2560,  "sigma_txt");
    cos_v185_register(&s, "action", "policy_head",  128,  "sigma_act");
    cos_v185_build_samples(&s);
    cos_v185_run_fusion(&s);

    char out[16384];
    size_t n = cos_v185_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
