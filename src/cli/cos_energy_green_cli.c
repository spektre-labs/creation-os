/*
 * cos energy / cos green — session and lifetime accounting display.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "cos_energy_green_cli.h"

#include "../sigma/energy_accounting.h"
#include "../sigma/green_score.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int argv_skip_energy_cmd(int argc, char **argv)
{
    if (argc > 1 && argv[1] != NULL && strcmp(argv[1], "energy") == 0)
        return 2;
    return 1;
}

static int argv_skip_green_cmd(int argc, char **argv)
{
    if (argc > 1 && argv[1] != NULL && strcmp(argv[1], "green") == 0)
        return 2;
    return 1;
}

static int usage_energy(FILE *fp)
{
    fputs(
        "cos energy — CPU-time-based energy receipt (session vs lifetime)\n"
        "  cos energy              session totals this process\n"
        "  cos energy --life       persisted lifetime rollup (~/.cos/energy_state.txt)\n"
        "  cos energy --json       JSON for session or lifetime (--life)\n"
        "  cos energy --persist    write lifetime state to disk\n"
        "Env: COS_ENERGY_GRID_G COS_ENERGY_EUR_KWH COS_ENERGY_TDP_W\n",
        fp);
    return fp == stderr ? 2 : 0;
}

static int usage_green(FILE *fp)
{
    fputs(
        "cos green — composite sustainability score from measured lifetime energy\n"
        "  cos green               print score, grade, and ratios\n"
        "  cos green --compare     extra counterfactual cloud vs local lines\n"
        "  cos green --json        machine-readable record\n"
        "Env: COS_GREEN_CACHE_HIT COS_GREEN_ABSTAIN COS_GREEN_SPIKE COS_GREEN_LOCALITY\n"
        "     COS_ENERGY_GRID_G (grid gCO2/kWh for CO2 math)\n",
        fp);
    return fp == stderr ? 2 : 0;
}

int cos_energy_main(int argc, char **argv)
{
    int                           base = argv_skip_energy_cmd(argc, argv);
    int                           i;
    int                           want_life = 0;
    int                           want_json = 0;
    int                           want_persist = 0;
    char                          rep[2048];
    char                         *js;
    const struct cos_energy_measurement *s;
    const struct cos_energy_measurement *life;

    if (argc < base)
        return usage_energy(stderr);

    if (argc > base
        && (strcmp(argv[base], "--help") == 0 || strcmp(argv[base], "-h") == 0))
        return usage_energy(stdout);

    for (i = base; i < argc; ++i) {
        if (strcmp(argv[i], "--life") == 0)
            want_life = 1;
        else if (strcmp(argv[i], "--json") == 0)
            want_json = 1;
        else if (strcmp(argv[i], "--persist") == 0)
            want_persist = 1;
        else {
            fputs("cos energy: unknown argument (try --help)\n", stderr);
            return 2;
        }
    }

    if (cos_energy_init(NULL) != 0) {
        fputs("cos energy: init failed\n", stderr);
        return 1;
    }

    if (want_life) {
        life = cos_energy_lifetime_total();
        if (want_json) {
            js = cos_energy_to_json(life);
            if (!js) {
                fputs("cos energy: JSON alloc failed\n", stderr);
                return 1;
            }
            puts(js);
            free(js);
        } else {
            if (cos_energy_lifetime_report(rep, (int)sizeof rep) != 0) {
                fputs("cos energy: report buffer error\n", stderr);
                return 1;
            }
            fputs(rep, stdout);
            cos_energy_print_receipt(life);
            cos_energy_print_savings_line(life);
        }
    } else {
        s = cos_energy_session_total();
        if (want_json) {
            js = cos_energy_to_json(s);
            if (!js) {
                fputs("cos energy: JSON alloc failed\n", stderr);
                return 1;
            }
            puts(js);
            free(js);
        } else {
            cos_energy_print_receipt(s);
            cos_energy_print_savings_line(s);
        }
    }

    if (want_persist && cos_energy_persist(NULL) != 0) {
        fputs("cos energy: persist failed\n", stderr);
        return 1;
    }

    return 0;
}

int cos_green_main(int argc, char **argv)
{
    int                           base = argv_skip_green_cmd(argc, argv);
    int                           i;
    int                           compare = 0;
    int                           want_json = 0;
    struct cos_green_score        g;
    char                         *js;
    const struct cos_energy_measurement *life;

    if (argc < base)
        return usage_green(stderr);

    if (argc > base
        && (strcmp(argv[base], "--help") == 0 || strcmp(argv[base], "-h") == 0))
        return usage_green(stdout);

    for (i = base; i < argc; ++i) {
        if (strcmp(argv[i], "--compare") == 0)
            compare = 1;
        else if (strcmp(argv[i], "--json") == 0)
            want_json = 1;
        else {
            fputs("cos green: unknown argument (try --help)\n", stderr);
            return 2;
        }
    }

    if (cos_energy_init(NULL) != 0) {
        fputs("cos green: init failed\n", stderr);
        return 1;
    }

    life = cos_energy_lifetime_total();
    g    = cos_green_calculate(life);

    if (want_json) {
        js = cos_green_to_json(&g);
        if (!js) {
            fputs("cos green: JSON alloc failed\n", stderr);
            return 1;
        }
        puts(js);
        free(js);
        return 0;
    }

    cos_green_print(&g);
    if (compare) {
        printf(
            "compare: GPT-class ref ≈ %.4f kWh/query | local ref ≈ %.6f kWh/query\n"
            "         vs_cloud_ratio (display) ≈ %.2fx (locality × cache uplift × "
            "vs_gpt5)\n",
            0.003, (double)0.00001, (double)g.vs_cloud_ratio);
    }

    return 0;
}
