/*
 * σ-team demo: spin up a five-role team, plan "Build a REST API for
 * todo list", run it, and emit a deterministic JSON envelope.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "team.h"

#include <stdio.h>
#include <string.h>

static void escape_and_print(const char *s) {
    for (; *s; s++) {
        switch (*s) {
            case '"':  fputs("\\\"", stdout); break;
            case '\\': fputs("\\\\", stdout); break;
            case '\n': fputs("\\n",  stdout); break;
            default:   putchar(*s);
        }
    }
}

int main(void) {
    int st = cos_team_self_test();

    cos_team_t team;
    cos_team_init(&team, "coordinator");
    cos_team_add_agent(&team, "alice",  "coordinator", 0.85f);
    cos_team_add_agent(&team, "rosa",   "researcher",  0.90f);
    cos_team_add_agent(&team, "rami",   "researcher",  0.60f);
    cos_team_add_agent(&team, "cody",   "coder",       0.88f);
    cos_team_add_agent(&team, "cole",   "coder",       0.70f);
    cos_team_add_agent(&team, "rex",    "reviewer",    0.92f);
    cos_team_add_agent(&team, "willow", "writer",      0.80f);

    cos_team_plan_t plan;
    cos_team_plan(&team, "Build a REST API for todo list", &plan);
    int rc = cos_team_run(&team, &plan, NULL, NULL);

    printf("{\n");
    printf("  \"kernel\": \"sigma_team\",\n");
    printf("  \"self_test\": %d,\n", st);
    printf("  \"run_rc\": %d,\n", rc);
    printf("  \"coordinator\": \"%s\",\n", team.coordinator_id);
    printf("  \"tau_retry\": %.4f,\n", (double)team.tau_retry);
    printf("  \"tau_abort\": %.4f,\n", (double)team.tau_abort);
    printf("  \"goal\": \""); escape_and_print(plan.goal); printf("\",\n");
    printf("  \"sigma_goal\": %.4f,\n", (double)plan.sigma_goal);
    printf("  \"escalations\": %d,\n", plan.escalations);
    printf("  \"steps\": [\n");
    for (int i = 0; i < plan.n_steps; i++) {
        const cos_team_step_t *s = &plan.steps[i];
        printf("    { \"index\": %d, \"role\": \"%s\", \"assigned\": \"%s\","
               " \"attempts\": %d, \"sigma\": %.4f, \"escalated\": %s,"
               " \"description\": \"",
               s->index, s->required_role, s->assigned,
               s->attempts, (double)s->sigma,
               s->escalated ? "true" : "false");
        escape_and_print(s->description);
        printf("\" }%s\n", i + 1 == plan.n_steps ? "" : ",");
    }
    printf("  ],\n");
    printf("  \"roster\": [\n");
    for (int i = 0; i < team.n_agents; i++) {
        const cos_team_agent_t *a = &team.agents[i];
        printf("    { \"id\": \"%s\", \"role\": \"%s\", \"capability\": %.4f,"
               " \"uses\": %d }%s\n",
               a->id, a->role, (double)a->capability, a->uses,
               i + 1 == team.n_agents ? "" : ",");
    }
    printf("  ]\n");
    printf("}\n");

    return st == 0 ? 0 : 1;
}
