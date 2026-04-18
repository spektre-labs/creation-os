/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v122 σ-Red-Team CLI.  Drives the built-in mock harness by default;
 * a future --endpoint flag in v122.1 will target a live v106 server.
 */
#include "red_team.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(const char *argv0) {
    fprintf(stderr,
        "usage:\n"
        "  %s --self-test\n"
        "  %s --run [--json|--md] [--report PATH]\n",
        argv0, argv0);
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(argv[0]);
    if (!strcmp(argv[1], "--self-test")) return cos_v122_self_test();
    if (!strcmp(argv[1], "--run")) {
        int emit_json = 1;  /* default JSON */
        int emit_md   = 0;
        const char *report_path = NULL;
        for (int i = 2; i < argc; ++i) {
            if      (!strcmp(argv[i], "--json"))   { emit_json = 1; emit_md = 0; }
            else if (!strcmp(argv[i], "--md"))     { emit_json = 0; emit_md = 1; }
            else if (!strcmp(argv[i], "--report") && i + 1 < argc) {
                report_path = argv[++i];
            }
            else return usage(argv[0]);
        }
        cos_v122_config_t c;
        cos_v122_config_defaults(&c);
        cos_v122_stats_t s;
        if (cos_v122_run(&c, NULL, NULL, &s) != 0) return 1;

        if (emit_json) {
            char js[2048];
            cos_v122_stats_to_json(&c, &s, js, sizeof js);
            puts(js);
        }
        if (emit_md) {
            char md[4096];
            cos_v122_stats_to_markdown(&c, &s, md, sizeof md);
            fputs(md, stdout);
        }
        if (report_path) {
            FILE *f = fopen(report_path, "w");
            if (!f) { perror(report_path); return 1; }
            char md[4096];
            cos_v122_stats_to_markdown(&c, &s, md, sizeof md);
            fputs(md, f);
            fclose(f);
        }
        /* Exit code reflects gate pass/fail to feed make. */
        int gate =
            cos_v122_defense_rate(&s, COS_V122_CAT_INJECTION)     >= c.defense_gate &&
            cos_v122_defense_rate(&s, COS_V122_CAT_JAILBREAK)     >= c.defense_gate &&
            cos_v122_defense_rate(&s, COS_V122_CAT_HALLUCINATION) >= c.defense_gate;
        return gate ? 0 : 2;
    }
    return usage(argv[0]);
}
