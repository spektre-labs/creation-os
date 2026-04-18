/*
 * v177 σ-Compress — CLI.
 *
 *   creation_os_v177_compress --self-test
 *   creation_os_v177_compress               # JSON state + report
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "compress.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v177_self_test();
        if (rc == 0) puts("v177 self-test: ok");
        else         fprintf(stderr, "v177 self-test failed at stage %d\n", rc);
        return rc;
    }

    cos_v177_state_t s;
    cos_v177_init(&s, 0x177CAFEBABEULL);
    cos_v177_build_fixture(&s);
    cos_v177_prune(&s);
    cos_v177_mixed_precision(&s);
    cos_v177_merge_layers(&s);
    cos_v177_report(&s);

    char buf[32768];
    size_t n = cos_v177_to_json(&s, buf, sizeof(buf));
    if (n == 0) { fprintf(stderr, "v177: json overflow\n"); return 2; }
    fwrite(buf, 1, n, stdout); putchar('\n');
    return 0;
}
