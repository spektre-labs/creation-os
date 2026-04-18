/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v123 σ-Formal CLI.  Structural validation only; the full TLC model
 * check is driven by benchmarks/v123/check_v123_formal_tlc.sh.
 */
#include "formal.h"

#include <stdio.h>
#include <string.h>

static int usage(const char *argv0) {
    fprintf(stderr,
        "usage:\n"
        "  %s --self-test\n"
        "  %s --validate [SPEC.tla] [CFG.cfg]\n",
        argv0, argv0);
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(argv[0]);
    if (!strcmp(argv[1], "--self-test")) return cos_v123_self_test();
    if (!strcmp(argv[1], "--validate")) {
        const char *spec = argc >= 3 ? argv[2] : COS_V123_SPEC_PATH_DEFAULT;
        const char *cfg  = argc >= 4 ? argv[3] : COS_V123_CFG_PATH_DEFAULT;
        cos_v123_validation_t v;
        int rc = cos_v123_validate(spec, cfg, &v);
        char js[1024];
        cos_v123_validation_to_json(&v, js, sizeof js);
        puts(js);
        return rc == 0 ? 0 : 1;
    }
    return usage(argv[0]);
}
