/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v178 σ-Consensus — CLI.
 *
 *   creation_os_v178_consensus --self-test
 *   creation_os_v178_consensus           # full JSON
 *   creation_os_v178_consensus --root    # print merkle root hex
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "consensus.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v178_self_test();
        if (rc == 0) puts("v178 self-test: ok");
        else         fprintf(stderr, "v178 self-test failed at stage %d\n", rc);
        return rc;
    }

    cos_v178_state_t s;
    cos_v178_init(&s, 0x178BEEFE0178ULL);
    cos_v178_run(&s);

    if (argc >= 2 && strcmp(argv[1], "--root") == 0) {
        char hex[2 * COS_V178_HASH_LEN + 1];
        cos_v178_merkle_root_hex(&s, hex);
        puts(hex);
        return 0;
    }

    char buf[16384];
    size_t n = cos_v178_to_json(&s, buf, sizeof(buf));
    if (n == 0) { fprintf(stderr, "v178: json overflow\n"); return 2; }
    fwrite(buf, 1, n, stdout); putchar('\n');
    return 0;
}
