/*
 * σ-arxiv demo — build manifest, print it, validate.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "arxiv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    int st = cos_arxiv_self_test();

    cos_arxiv_manifest_t m;
    cos_arxiv_manifest_build(&m);
    char *buf = (char *)malloc(16384);
    if (!buf) return 2;
    int n = cos_arxiv_manifest_write_json(&m, buf, 16384);
    if (n <= 0) { free(buf); return 3; }

    /* Insert self_test ahead of metadata by re-emitting. */
    printf("{\n");
    printf("  \"kernel\": \"sigma_arxiv\",\n");
    printf("  \"self_test\": %d,\n", st);
    /* Skip kernel key in the inner manifest by starting at its
     * metadata section so the outer envelope stays canonical. */
    const char *p = buf;
    const char *meta = strstr(p, "\"metadata\":");
    if (!meta) { free(buf); return 4; }
    fputs(meta, stdout);
    free(buf);
    return st == 0 ? 0 : 1;
}
