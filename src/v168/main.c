/*
 * v168 σ-Marketplace — CLI entry point.
 *
 *   creation_os_v168_marketplace --self-test
 *   creation_os_v168_marketplace                      # registry JSON
 *   creation_os_v168_marketplace --search TERM        # filtered JSON
 *   creation_os_v168_marketplace --install ID [--force]
 *   creation_os_v168_marketplace --validate-cos-skill adapter.safetensors,template.txt,...
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "marketplace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t emit_filtered(const cos_v168_registry_t *r,
                            const char *term,
                            char *buf, size_t cap) {
    bool mask[COS_V168_MAX_ARTIFACTS];
    cos_v168_search(r, term, mask);

    size_t used = 0;
    int n = snprintf(buf + used, cap - used,
                     "{\"kernel\":\"v168\",\"query\":\"%s\","
                     "\"matches\":[",
                     term ? term : "");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;

    int emitted = 0;
    for (int i = 0; i < r->n_artifacts; ++i) {
        if (!mask[i]) continue;
        const cos_v168_artifact_t *a = &r->artifacts[i];
        n = snprintf(buf + used, cap - used,
            "%s{\"id\":\"%s\",\"name\":\"%s\",\"version\":\"%s\","
            "\"kind\":\"%s\",\"sigma_reputation\":%.4f}",
            emitted == 0 ? "" : ",",
            a->id, a->name, a->version,
            cos_v168_kind_name(a->kind),
            (double)a->sigma_reputation);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
        emitted++;
    }
    n = snprintf(buf + used, cap - used, "]}");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    return used;
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v168_self_test();
        if (rc == 0) puts("v168 self-test: ok");
        else         fprintf(stderr, "v168 self-test failed at stage %d\n", rc);
        return rc;
    }

    cos_v168_registry_t r;
    cos_v168_init(&r);

    if (argc >= 3 && strcmp(argv[1], "--search") == 0) {
        char buf[8192];
        size_t n = emit_filtered(&r, argv[2], buf, sizeof(buf));
        if (n == 0) return 2;
        fwrite(buf, 1, n, stdout);
        putchar('\n');
        return 0;
    }

    if (argc >= 3 && strcmp(argv[1], "--install") == 0) {
        bool force = false;
        for (int i = 3; i < argc; ++i)
            if (strcmp(argv[i], "--force") == 0) force = true;
        cos_v168_install_receipt_t rc = cos_v168_install(&r, argv[2], force);
        char buf[1024];
        size_t n = cos_v168_install_to_json(&rc, buf, sizeof(buf));
        if (n == 0) return 2;
        fwrite(buf, 1, n, stdout);
        putchar('\n');
        switch (rc.status) {
            case COS_V168_OK:
            case COS_V168_FORCED:
            case COS_V168_ALREADY_INSTALLED: return 0;
            default:                         return 1;
        }
    }

    if (argc >= 3 && strcmp(argv[1], "--validate-cos-skill") == 0) {
        const char *csv = argv[2];
        /* build NUL-separated buffer from comma-separated list */
        char buf[512];
        size_t k = 0;
        size_t i = 0;
        while (csv[i] && k < sizeof(buf) - 1) {
            if (csv[i] == ',') buf[k++] = '\0';
            else               buf[k++] = csv[i];
            ++i;
        }
        if (k < sizeof(buf) - 1) { buf[k++] = '\0'; buf[k++] = '\0'; }
        int rc = cos_v168_validate_cos_skill(buf, k);
        printf("{\"kernel\":\"v168\",\"event\":\"validate\","
               "\"status\":\"%s\",\"missing_index\":%d}\n",
               rc == 0 ? "ok" : "invalid", rc);
        return rc == 0 ? 0 : 1;
    }

    char buf[8192];
    size_t n = cos_v168_to_json(&r, buf, sizeof(buf));
    if (n == 0) { fprintf(stderr, "v168: json overflow\n"); return 2; }
    fwrite(buf, 1, n, stdout);
    putchar('\n');
    return 0;
}
