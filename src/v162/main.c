/*
 * v162 σ-Compose — CLI.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "compose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fputs(
      "usage: creation_os_v162_compose --self-test\n"
      "       creation_os_v162_compose --profile {lean|researcher|sovereign}\n"
      "       creation_os_v162_compose --profile custom --kernels v1,v2,...\n"
      "       creation_os_v162_compose --profile lean --enable v159\n"
      "       creation_os_v162_compose --profile sovereign --disable v150\n",
      stderr);
    return 2;
}

static int parse_profile(const char *s, cos_v162_profile_kind_t *out) {
    if (!strcmp(s, "lean"))       { *out = COS_V162_PROFILE_LEAN;       return 0; }
    if (!strcmp(s, "researcher")) { *out = COS_V162_PROFILE_RESEARCHER; return 0; }
    if (!strcmp(s, "sovereign"))  { *out = COS_V162_PROFILE_SOVEREIGN;  return 0; }
    if (!strcmp(s, "custom"))     { *out = COS_V162_PROFILE_CUSTOM;     return 0; }
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (!strcmp(argv[1], "--self-test")) {
        int rc = cos_v162_self_test();
        printf("{\"kernel\":\"v162\",\"self_test\":\"%s\",\"rc\":%d}\n",
               rc == 0 ? "pass" : "fail", rc);
        return rc == 0 ? 0 : 1;
    }

    cos_v162_profile_kind_t kind = COS_V162_PROFILE_LEAN;
    const char *custom_kernels = NULL;
    const char *enable  = NULL;
    const char *disable = NULL;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--profile") && i + 1 < argc) {
            if (parse_profile(argv[++i], &kind) != 0) return usage();
        } else if (!strcmp(argv[i], "--kernels") && i + 1 < argc) {
            custom_kernels = argv[++i];
        } else if (!strcmp(argv[i], "--enable") && i + 1 < argc) {
            enable = argv[++i];
        } else if (!strcmp(argv[i], "--disable") && i + 1 < argc) {
            disable = argv[++i];
        } else {
            return usage();
        }
    }

    cos_v162_composer_t c;
    cos_v162_composer_init(&c);
    if (kind == COS_V162_PROFILE_CUSTOM) {
        if (!custom_kernels) return usage();
        cos_v162_composer_set_custom(&c, custom_kernels);
    }
    if (cos_v162_composer_select(&c, kind) < 0) {
        char buf[16384];
        cos_v162_composer_to_json(&c, buf, sizeof(buf));
        printf("%s\n", buf);
        return 1;
    }
    if (enable)  cos_v162_composer_enable(&c, enable);
    if (disable) cos_v162_composer_disable(&c, disable);

    char buf[32768];
    cos_v162_composer_to_json(&c, buf, sizeof(buf));
    printf("%s\n", buf);
    return 0;
}
