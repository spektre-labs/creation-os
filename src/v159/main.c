/*
 * v159 σ-Heal — CLI entry point.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "heal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fputs(
      "usage: creation_os_v159_heal --self-test\n"
      "       creation_os_v159_heal --scenario N [--seed S]\n"
      "       creation_os_v159_heal --predict {http|sigma|memory|kv|adapter|"
                                              "gate|bridge|weights} [--seed S]\n"
      "       creation_os_v159_heal --cycle [--seed S]\n",
      stderr);
    return 2;
}

static int parse_component(const char *s, cos_v159_component_t *out) {
    if (!s || !out) return -1;
    if (!strcmp(s, "http"))    { *out = COS_V159_COMP_HTTP_SERVER;     return 0; }
    if (!strcmp(s, "sigma"))   { *out = COS_V159_COMP_SIGMA_CHANNELS;  return 0; }
    if (!strcmp(s, "memory"))  { *out = COS_V159_COMP_MEMORY_SQLITE;   return 0; }
    if (!strcmp(s, "kv"))      { *out = COS_V159_COMP_KV_CACHE;        return 0; }
    if (!strcmp(s, "adapter")) { *out = COS_V159_COMP_ADAPTER_VERSION; return 0; }
    if (!strcmp(s, "gate"))    { *out = COS_V159_COMP_MERGE_GATE;      return 0; }
    if (!strcmp(s, "bridge"))  { *out = COS_V159_COMP_BRIDGE;          return 0; }
    if (!strcmp(s, "weights")) { *out = COS_V159_COMP_WEIGHTS;         return 0; }
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (!strcmp(argv[1], "--self-test")) {
        int rc = cos_v159_self_test();
        printf("{\"kernel\":\"v159\",\"self_test\":\"%s\",\"rc\":%d}\n",
               rc == 0 ? "pass" : "fail", rc);
        return rc == 0 ? 0 : 1;
    }

    uint64_t seed = 159;
    int      scenario = -1;
    const char *predict_comp = NULL;
    int      do_cycle = 0;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--scenario") && i + 1 < argc) {
            scenario = (int)strtol(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
            seed = strtoull(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--predict") && i + 1 < argc) {
            predict_comp = argv[++i];
        } else if (!strcmp(argv[i], "--cycle")) {
            do_cycle = 1;
        } else {
            return usage();
        }
    }

    cos_v159_daemon_t d;
    cos_v159_daemon_init(&d, seed);

    if (do_cycle) {
        /* Run every scenario in turn — proves the full self-heal
         * loop can handle back-to-back failures without manual
         * re-init. */
        for (int s = 1; s <= 5; ++s) {
            cos_v159_daemon_tick(&d, s);
        }
    } else if (predict_comp) {
        cos_v159_component_t c;
        if (parse_component(predict_comp, &c) != 0) return usage();
        cos_v159_daemon_predict(&d, c);
    } else if (scenario >= 0) {
        cos_v159_daemon_tick(&d, scenario);
    } else {
        return usage();
    }

    char buf[16384];
    cos_v159_daemon_to_json(&d, buf, sizeof(buf));
    printf("%s\n", buf);
    return 0;
}
