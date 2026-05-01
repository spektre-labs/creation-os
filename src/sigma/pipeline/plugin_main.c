/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Plugin demo + pinned JSON receipt.
 *
 * Exercises:
 *   1. static self-test (registry bounds, duplicate rejection,
 *      ABI check, full-registry check).
 *   2. dlopen of libcos_plugin_demo (if present alongside the
 *      binary) — verifies the shared-library ABI path.
 *   3. canonical JSON listing for `cos plugin list`.
 *
 * Pinned by benchmarks/sigma_pipeline/check_sigma_plugin.sh.
 */

#include "plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int file_exists(const char *p) {
    struct stat s; return (stat(p, &s) == 0);
}

static void json_escape(const char *s, char *dst, size_t cap) {
    if (!dst || cap == 0) return;
    size_t j = 0;
    if (!s) { dst[0] = '\0'; return; }
    for (size_t i = 0; s[i] && j + 7 < cap; i++) {
        unsigned char c = (unsigned char)s[i];
        if      (c == '"' || c == '\\') { dst[j++] = '\\'; dst[j++] = c; }
        else if (c == '\n')              { dst[j++] = '\\'; dst[j++] = 'n'; }
        else if (c == '\r')              { dst[j++] = '\\'; dst[j++] = 'r'; }
        else if (c == '\t')              { dst[j++] = '\\'; dst[j++] = 't'; }
        else if (c < 0x20)               { j += (size_t)snprintf(dst+j, cap-j, "\\u%04x", c); }
        else                              { dst[j++] = (char)c; }
    }
    dst[j] = '\0';
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int self = cos_sigma_plugin_self_test();

    /* 1. Registry is clean after self-test. */
    cos_sigma_plugin_registry_reset();

    /* 2. Try dlopen of the demo plugin, either libcos_plugin_demo
     *    .dylib (macOS) or .so (Linux). */
    const char *candidates[] = {
        "./libcos_plugin_demo.dylib",
        "./libcos_plugin_demo.so",
        NULL,
    };
    int    dlopen_rc  = -99;
    const char *which = NULL;
    for (int i = 0; candidates[i]; i++) {
        if (file_exists(candidates[i])) {
            dlopen_rc = cos_sigma_plugin_load(candidates[i]);
            which = candidates[i];
            break;
        }
    }

    /* 3. Tool invocation round-trip: call demo_echo through the
     *    plugin's registered function pointer. */
    int tool_rc = -99;
    char *captured = NULL;
    const cos_sigma_plugin_t *p = NULL;
    if (dlopen_rc == COS_PLUGIN_OK) {
        p = cos_sigma_plugin_at(0);
        if (p && p->n_tools > 0 && p->tools[0].run) {
            const char *argv_[] = {"demo_echo", "prometheus", NULL};
            tool_rc = p->tools[0].run(argv_, &captured, NULL);
        }
    }

    float sigma_probe = -1.0f;
    if (p && p->custom_sigma) sigma_probe = p->custom_sigma(NULL);

    char list[4096];
    int  list_n = cos_sigma_plugin_list_json(list, sizeof list);
    char escaped_stdout[512];
    json_escape(captured, escaped_stdout, sizeof escaped_stdout);

    printf("{\"kernel\":\"sigma_plugin\","
           "\"self_test_rc\":%d,"
           "\"abi_version\":%d,"
           "\"dlopen\":{\"tried\":\"%s\",\"rc\":%d},"
           "\"tool_rc\":%d,"
           "\"tool_stdout\":\"%s\","
           "\"custom_sigma\":%.4f,"
           "\"list_bytes\":%d,"
           "\"list\":%s,"
           "\"pass\":%s}\n",
           self,
           COS_SIGMA_PLUGIN_ABI,
           which ? which : "(none)",
           dlopen_rc,
           tool_rc,
           escaped_stdout,
           (double)sigma_probe,
           list_n,
           list_n > 0 ? list : "{}",
           self == 0 ? "true" : "false");

    free(captured);
    cos_sigma_plugin_registry_reset();
    return self == 0 ? 0 : 1;
}
