/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v164 σ-Plugin — CLI entry point.
 *
 *   creation_os_v164_plugin                 # JSON registry report
 *   creation_os_v164_plugin --list          # same JSON
 *   creation_os_v164_plugin --self-test     # 0 on pass
 *   creation_os_v164_plugin --run NAME PROMPT CAPS
 *                                            # run a plugin with a granted cap mask
 *   creation_os_v164_plugin --disable NAME  # hot-swap off, print JSON
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    cos_v164_registry_t reg;
    cos_v164_registry_init(&reg, 0x16401964ABCDEF01ULL);

    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v164_self_test();
        if (rc == 0) puts("v164 self-test: ok");
        else         fprintf(stderr, "v164 self-test failed at stage %d\n", rc);
        return rc;
    }

    if (argc >= 3 && strcmp(argv[1], "--disable") == 0) {
        if (cos_v164_registry_set_enabled(&reg, argv[2], false) != 0) {
            fprintf(stderr, "unknown plugin: %s\n", argv[2]);
            return 1;
        }
        char buf[4096];
        size_t n = cos_v164_registry_to_json(&reg, buf, sizeof(buf));
        if (n == 0) { fprintf(stderr, "v164: json overflow\n"); return 2; }
        fwrite(buf, 1, n, stdout);
        putchar('\n');
        return 0;
    }

    if (argc >= 4 && strcmp(argv[1], "--run") == 0) {
        const char *name    = argv[2];
        const char *prompt  = argv[3];
        uint32_t granted    = argc >= 5 ? (uint32_t)strtoul(argv[4], NULL, 0) : 0u;
        cos_v164_input_t in;
        memset(&in, 0, sizeof(in));
        strncpy(in.prompt, prompt, sizeof(in.prompt) - 1);
        in.granted_caps = granted;
        cos_v164_output_t o = cos_v164_registry_run(&reg, name, &in);
        printf("{\"kernel\":\"v164\",\"event\":\"run\",\"plugin\":\"%s\","
               "\"status\":\"%s\",\"sigma_plugin\":%.4f,"
               "\"sigma_override\":%s,\"runtime_ms\":%d,"
               "\"response\":\"%s\",\"note\":\"%s\"}\n",
               name, cos_v164_status_name(o.status),
               (double)o.sigma_plugin,
               o.sigma_override ? "true" : "false",
               o.runtime_ms, o.response, o.note);
        return o.status == COS_V164_OK ? 0 : 1;
    }

    char buf[4096];
    size_t n = cos_v164_registry_to_json(&reg, buf, sizeof(buf));
    if (n == 0) { fprintf(stderr, "v164: json overflow\n"); return 2; }
    fwrite(buf, 1, n, stdout);
    putchar('\n');
    return 0;
}
