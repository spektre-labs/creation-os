/*
 * cos embody — physical grounding loop (sim default, sovereign-gated real).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "cos_embody.h"

#include "../sigma/embodiment.h"
#include "../sigma/sovereign_limits.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct cos_embodiment G;

static int argv_skip_cmd(int argc, char **argv)
{
    if (argc > 1 && argv[1] != NULL && strcmp(argv[1], "embody") == 0)
        return 2;
    return 1;
}

static void usage(FILE *fp)
{
    fputs(
        "cos embody — physical embodiment control\n"
        "  cos embody --sim              select simulation mode (default)\n"
        "  cos embody --real             real hardware mode (sovereign-gated)\n"
        "  cos embody --state            print pose + σ metrics\n"
        "  cos embody --history          print recent actions\n"
        "  cos embody --act \"goal\"       plan → twin sim → execute\n",
        fp);
}

static int embody_boot(int simulation_mode)
{
    struct cos_sovereign_limits L;

    cos_sovereign_default_limits(&L);
    /* Allow automated checks without blocking on autonomy heuristics. */
    L.require_human_above = 1.0f;
    L.max_autonomy_level  = 1.0f;
    cos_sovereign_init(&L);

    return cos_embodiment_init(&G, simulation_mode);
}

int cos_embody_main(int argc, char **argv)
{
    int   i;
    int   base;
    int   sim_mode = 1;
    int   did_cmd  = 0;

    base = argv_skip_cmd(argc, argv);
    if (argc <= base) {
        usage(stderr);
        return 2;
    }

    if (argc > base && (strcmp(argv[base], "--help") == 0
                        || strcmp(argv[base], "-h") == 0)) {
        usage(stdout);
        return 0;
    }

    for (i = base; i < argc; ++i) {
        if (strcmp(argv[i], "--sim") == 0)
            sim_mode = 1;
        else if (strcmp(argv[i], "--real") == 0)
            sim_mode = 0;
    }

    if (embody_boot(sim_mode) != 0) {
        fputs("cos embody: init failed\n", stderr);
        return 1;
    }

    for (i = base; i < argc; ++i) {
        if (strcmp(argv[i], "--sim") == 0 || strcmp(argv[i], "--real") == 0)
            continue;

        if (strcmp(argv[i], "--state") == 0) {
            cos_embodiment_print_state(stdout, &G);
            did_cmd = 1;
            continue;
        }
        if (strcmp(argv[i], "--history") == 0) {
            cos_embodiment_print_history(stdout, &G);
            did_cmd = 1;
            continue;
        }
        if (strcmp(argv[i], "--act") == 0) {
            const char               *goal = NULL;
            struct cos_action           act;
            struct cos_physical_state   pred;
            float                       gap;
            int                         sim_rc;

            if (i + 1 >= argc) {
                fputs("cos embody: --act requires a goal string\n", stderr);
                return 2;
            }
            goal = argv[i + 1];
            i++;

            if (cos_embodiment_plan_action(&G, goal, &act) != 0) {
                printf("plan: unparsed goal (σ=%.3f)\n", (double)act.sigma);
            }
            sim_rc = cos_embodiment_simulate(&G, &act, &pred);
            if (sim_rc != 0 && sim_rc != -2) {
                fputs("cos embody: simulate failed\n", stderr);
                return 3;
            }
            printf("twin σ=%.3f  predicted pos %.3f %.3f %.3f  collision=%s\n",
                   (double)G.last_sim_sigma,
                   (double)pred.position[0], (double)pred.position[1],
                   (double)pred.position[2],
                   sim_rc == -2 ? "yes" : "no");

            if (cos_embodiment_execute(&G, &act) != 0) {
                fputs("cos embody: execute blocked or failed\n", stderr);
                return 4;
            }

            gap = cos_embodiment_sim_to_real_gap(&pred, &G.state);
            (void)cos_embodiment_update_world_model(&G, gap);
            printf("sim→real gap=%.3f  σ_wm=%.3f\n", (double)gap,
                   (double)G.sigma_sim_to_real);
            did_cmd = 1;
            continue;
        }

        fprintf(stderr, "cos embody: unknown argument: %s\n", argv[i]);
        usage(stderr);
        return 2;
    }

    if (!did_cmd)
        printf("embodiment ready (%s)\n",
               sim_mode ? "simulation" : "real");

    return 0;
}

#ifdef COS_EMBODY_MAIN
int main(int argc, char **argv)
{
    return cos_embody_main(argc, argv);
}
#endif
