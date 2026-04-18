/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v117 CLI:
 *   --self-test        run pure-C self-test
 *   --simulate N S     simulate N ingest steps at σ=S, print final JSON
 *                      (exercises the eviction path end-to-end)
 */

#include "long_context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "--self-test")) {
        int rc = cos_v117_self_test();
        printf(rc == 0 ? "v117 self-test: PASS\n" : "v117 self-test: FAIL\n");
        return rc ? 1 : 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "--simulate")) {
        int   n     = atoi(argv[2]);
        float sigma = (float)atof(argv[3]);
        cos_v117_config_t cfg;
        cos_v117_config_defaults(&cfg);
        /* Smaller config so tests complete quickly. */
        cfg.page_tokens    = 64;
        cfg.native_ctx     = 512;
        cfg.target_ctx     = 4096;
        cfg.sliding_tokens = 128;
        cfg.max_pages      = 256;
        cos_v117_ctx_t *c = cos_v117_ctx_init(&cfg, NULL, NULL);
        if (!c) return 1;
        for (int i = 0; i < n; ++i) {
            /* Vary σ: every 4th chunk is "uncertain" (0.9), the rest match
             * `sigma`.  Lets us see σ-LRU pick the 0.9 pages first. */
            float s = (i % 4 == 3) ? 0.9f : sigma;
            char preview[32];
            snprintf(preview, sizeof preview, "chunk%d", i);
            cos_v117_ingest(c, preview, 8, s);
        }
        char buf[32768];
        int w = cos_v117_to_json(c, buf, sizeof buf);
        if (w > 0) { fwrite(buf, 1, (size_t)w, stdout); fputc('\n', stdout); }
        cos_v117_ctx_free(c);
        return w > 0 ? 0 : 1;
    }
    fprintf(stderr, "usage: creation_os_v117_long_context --self-test | --simulate N SIGMA\n");
    return 0;
}
