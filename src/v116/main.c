/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v116 CLI:
 *   --self-test   run the pure-C MCP dispatcher self-test
 *   --stdio       run the JSON-RPC 2.0 stdio loop (default when no args)
 *   --dispatch '<json>' single-shot dispatch (prints response, exits)
 */

#include "mcp_server.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int want_self = 0;
    const char *oneshot = NULL;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--self-test")) want_self = 1;
        else if (!strcmp(argv[i], "--stdio")) { /* default */ }
        else if (!strcmp(argv[i], "--dispatch") && i + 1 < argc) oneshot = argv[++i];
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            fprintf(stderr, "usage: creation_os_v116_mcp [--self-test] "
                            "[--stdio] [--dispatch '<json>']\n");
            return 0;
        }
    }

    if (want_self) {
        int rc = cos_v116_self_test();
        fprintf(stdout, rc == 0 ? "v116 self-test: PASS\n"
                                : "v116 self-test: FAIL\n");
        return rc ? 1 : 0;
    }

    cos_v116_config_t cfg;
    cos_v116_config_defaults(&cfg);

    if (oneshot) {
        char out[131072];
        int n = cos_v116_dispatch(&cfg, oneshot, strlen(oneshot),
                                   out, sizeof out);
        if (n > 0) {
            fwrite(out, 1, (size_t)n, stdout);
            fputc('\n', stdout);
        }
        return n >= 0 ? 0 : 1;
    }

    return cos_v116_run_stdio(&cfg, stdin, stdout);
}
