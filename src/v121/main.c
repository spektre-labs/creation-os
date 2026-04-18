/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v121 σ-Planning CLI.  Runs the synthetic happy planner so the
 * merge-gate can inspect the /v1/plan JSON contract without wiring
 * v112 / v113 / v114 backends.
 */
#include "planning.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Duplicate of happy_source in planning.c so the CLI is a single
 * translation unit alongside planning.c.  (planning.c's happy_source
 * is file-static; exposing a CLI source avoids making it public.) */
static int cli_source(const char *goal, int step_index,
                      cos_v121_candidate_t *out, int cap, void *user) {
    (void)goal; (void)user;
    if (cap < 3) return 0;
    switch (step_index) {
    case 0:
        strncpy(out[0].action, "read_file",            COS_V121_MAX_STR-1);
        out[0].tool = COS_V121_TOOL_SANDBOX;
        out[0].sigma_decompose = 0.50f; out[0].sigma_tool = 0.50f;
        out[0].sigma_result_on_execute = 0.99f;
        strncpy(out[1].action, "read_file_safe",       COS_V121_MAX_STR-1);
        out[1].tool = COS_V121_TOOL_SANDBOX;
        out[1].sigma_decompose = 0.48f; out[1].sigma_tool = 0.48f;
        out[1].sigma_result_on_execute = 0.99f;
        strncpy(out[2].action, "read_file_via_memory", COS_V121_MAX_STR-1);
        out[2].tool = COS_V121_TOOL_MEMORY;
        out[2].sigma_decompose = 0.55f; out[2].sigma_tool = 0.55f;
        out[2].sigma_result_on_execute = 0.30f;
        return 3;
    case 1:
        strncpy(out[0].action, "analyze_data", COS_V121_MAX_STR-1);
        out[0].tool = COS_V121_TOOL_SANDBOX;
        out[0].sigma_decompose = 0.30f; out[0].sigma_tool = 0.40f;
        out[0].sigma_result_on_execute = 0.40f;
        return 1;
    case 2:
        strncpy(out[0].action, "generate_report", COS_V121_MAX_STR-1);
        out[0].tool = COS_V121_TOOL_CHAT;
        out[0].sigma_decompose = 0.30f; out[0].sigma_tool = 0.30f;
        out[0].sigma_result_on_execute = 0.50f;
        return 1;
    default: return 0;
    }
}

static int usage(const char *argv0) {
    fprintf(stderr,
        "usage:\n"
        "  %s --self-test\n"
        "  %s --plan \"GOAL\"           # emit /v1/plan-shaped JSON\n",
        argv0, argv0);
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(argv[0]);
    if (!strcmp(argv[1], "--self-test")) return cos_v121_self_test();
    if (!strcmp(argv[1], "--plan") && argc == 3) {
        cos_v121_plan_t plan;
        int rc = cos_v121_run(NULL, argv[2], cli_source, NULL, &plan);
        char js[4096];
        cos_v121_plan_to_json(&plan, js, sizeof js);
        puts(js);
        return rc == 0 ? 0 : 1;
    }
    return usage(argv[0]);
}
