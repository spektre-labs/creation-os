/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v176 σ-Simulator — CLI.
 *
 *   creation_os_v176_simulator --self-test
 *   creation_os_v176_simulator            # full JSON
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "simulator.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v176_self_test();
        if (rc == 0) puts("v176 self-test: ok");
        else         fprintf(stderr, "v176 self-test failed at stage %d\n", rc);
        return rc;
    }

    cos_v176_state_t s;
    cos_v176_init(&s, 0x1760ABCDEF176ULL);
    cos_v176_generate_worlds(&s);
    cos_v176_build_curriculum(&s);
    cos_v176_measure_transfer(&s);
    cos_v176_dream_train(&s);

    char buf[16384];
    size_t n = cos_v176_to_json(&s, buf, sizeof(buf));
    if (n == 0) { fprintf(stderr, "v176: json overflow\n"); return 2; }
    fwrite(buf, 1, n, stdout); putchar('\n');
    return 0;
}
