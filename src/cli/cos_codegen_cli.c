/*
 * cos codegen — σ-gated self-modification proposals (sandbox-only by default).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "cos_codegen_cli.h"

#include "../sigma/codegen.h"
#include "../sigma/evolution_engine.h"
#include "../sigma/sovereign_limits.h"
#include "cos_think.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage_codegen(FILE *fp)
{
    fputs(
        "cos codegen — propose C modules under sandbox gates\n"
        "  cos codegen --goal \"…\"     generate → compile → test → gate → register\n"
        "  cos codegen --list           pending proposals\n"
        "  cos codegen --approve ID     mark proposal approved (human)\n"
        "  cos codegen --reject ID MSG    reject with reason\n"
        "Env: COS_CODEGEN_OFFLINE=1      deterministic candidate (CI)\n"
        "     COS_CODEGEN_REGRESSION=cmd  optional repo regression (make …)\n",
        fp);
    return fp == stderr ? 2 : 0;
}

static int argv_skip_codegen_cmd(int argc, char **argv)
{
    if (argc > 1 && argv[1] != NULL && strcmp(argv[1], "codegen") == 0)
        return 2;
    return 1;
}

static int evolve_code_base(int argc, char **argv)
{
    if (argc >= 4 && argv[1] != NULL && strcmp(argv[1], "evolve") == 0
        && argv[2] != NULL && strcmp(argv[2], "--code") == 0)
        return 3;
    if (argc >= 2 && argv[1] != NULL && strcmp(argv[1], "--code") == 0)
        return 2;
    return -1;
}

int cos_codegen_main(int argc, char **argv)
{
    int               base = argv_skip_codegen_cmd(argc, argv);
    int               i;
    struct cos_codegen_proposal P;
    struct cos_codegen_proposal list[COS_CODEGEN_MAX_PENDING];
    int               n;

    cos_sovereign_init(NULL);

    if (argc <= base)
        return usage_codegen(stderr);

    if (strcmp(argv[base], "--help") == 0 || strcmp(argv[base], "-h") == 0)
        return usage_codegen(stdout);

    if (strcmp(argv[base], "--list") == 0) {
        if (cos_codegen_list(list, COS_CODEGEN_MAX_PENDING, &n) != 0)
            return 1;
        printf("%d proposal(s)\n", n);
        for (i = 0; i < n; ++i)
            printf("  id=%d approved=%d sigma_gen=%.3f sigma_test=%.3f %s\n",
                   list[i].proposal_id, list[i].approved,
                   (double)list[i].sigma_confidence,
                   (double)list[i].sigma_after_test, list[i].filename);
        return 0;
    }

    if (strcmp(argv[base], "--approve") == 0) {
        int id;
        if (argc <= base + 1) {
            fputs("cos codegen: --approve requires ID\n", stderr);
            return 2;
        }
        id = atoi(argv[base + 1]);
        if (cos_codegen_get(id, &P) != 0) {
            fputs("cos codegen: unknown ID\n", stderr);
            return 3;
        }
        if (cos_codegen_approve(&P) != 0)
            return 4;
        printf("approved proposal %d\n", id);
        return 0;
    }

    if (strcmp(argv[base], "--reject") == 0) {
        int   id;
        char  reason[256];
        if (argc <= base + 1) {
            fputs("cos codegen: --reject requires ID and message\n", stderr);
            return 2;
        }
        id = atoi(argv[base + 1]);
        reason[0] = '\0';
        for (i = base + 2; i < argc; ++i) {
            if (reason[0])
                strncat(reason, " ", sizeof reason - strlen(reason) - 1);
            strncat(reason, argv[i], sizeof reason - strlen(reason) - 1);
        }
        if (cos_codegen_get(id, &P) != 0) {
            fputs("cos codegen: unknown ID\n", stderr);
            return 3;
        }
        if (cos_codegen_reject(&P, reason) != 0)
            return 4;
        printf("rejected proposal %d\n", id);
        return 0;
    }

    if (strcmp(argv[base], "--goal") == 0) {
        char              aug[3072];
        char              sub[COS_THINK_MAX_SUBTASKS][1024];
        int               nsub = 0;
        const char       *goal;

        if (argc <= base + 1) {
            fputs("cos codegen: --goal requires text\n", stderr);
            return 2;
        }
        goal = argv[base + 1];
        snprintf(aug, sizeof aug, "%s", goal);
        if (cos_think_decompose(goal, sub, &nsub) == 0 && nsub > 0)
            snprintf(aug, sizeof aug,
                     "%s\n(subtask hint from cos think: %s)", goal, sub[0]);
        memset(&P, 0, sizeof P);
        if (cos_codegen_propose(aug, &P) != 0) {
            fputs("cos codegen: propose failed\n", stderr);
            return 3;
        }
        if (P.approved == 2) {
            printf("REJECT (early): %s\n", P.rejection_reason);
            return 4;
        }
        if (cos_codegen_compile_sandbox(&P) != 0) {
            printf("REJECT compile: %s\n", P.rejection_reason);
            return 5;
        }
        if (cos_codegen_test_sandbox(&P) != 0)
            fputs("cos codegen: test harness warning\n", stderr);
        (void)cos_codegen_frama_check(&P);
        if (cos_codegen_check_regression(&P) != 0) {
            printf("REJECT regression: %s\n", P.rejection_reason);
            return 6;
        }
        if (cos_codegen_gate(&P) != 0) {
            printf("REJECT gate: %s\n", P.rejection_reason);
            return 7;
        }
        if (cos_codegen_register(&P) != 0) {
            fputs("cos codegen: registry full\n", stderr);
            return 8;
        }
        printf("PENDING id=%d (human review required) file=%s σ_gen=%.3f "
               "σ_test=%.3f\n",
               P.proposal_id, P.filename, (double)P.sigma_confidence,
               (double)P.sigma_after_test);
        return 0;
    }

    usage_codegen(stderr);
    return 2;
}

int cos_evolve_code_main(int argc, char **argv)
{
    int                             base = evolve_code_base(argc, argv);
    int                             i;
    int                             gens = 1;
    struct cos_evolution_generation *runs = NULL;

    cos_sovereign_init(NULL);

    if (base < 0) {
        fputs("usage: cos evolve --code [--gen N] [--report]\n", stderr);
        return 2;
    }

    for (i = base; i < argc; ++i) {
        if (strcmp(argv[i], "--gen") == 0 || strcmp(argv[i], "--generations") == 0) {
            if (i + 1 >= argc) {
                fputs("cos evolve --code: missing N\n", stderr);
                return 2;
            }
            gens = atoi(argv[++i]);
            if (gens <= 0)
                gens = 1;
            if (gens > 64)
                gens = 64;
            continue;
        }
        if (strcmp(argv[i], "--report") == 0) {
            char path[768];
            const char *home = getenv("HOME");
            FILE       *f;
            char        line[512];
            if (!home || !home[0])
                home = "/tmp";
            snprintf(path, sizeof path, "%s/.cos/evolve_code/history.jsonl",
                     home);
            f = fopen(path, "r");
            if (!f) {
                printf("(no history at %s)\n", path);
                return 0;
            }
            while (fgets(line, sizeof line, f))
                fputs(line, stdout);
            fclose(f);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fputs("cos evolve --code [--gen N] [--report]\n", stdout);
            return 0;
        }
    }

    runs = (struct cos_evolution_generation *)calloc(
        (size_t)gens, sizeof(struct cos_evolution_generation));
    if (!runs)
        return 1;

    if (cos_evolution_run(gens, runs) != 0) {
        free(runs);
        return 3;
    }
    cos_evolution_print_report(runs, gens);
    free(runs);
    return 0;
}

#ifdef COS_CODEGEN_CLI_MAIN
int main(int argc, char **argv)
{
    return cos_codegen_main(argc, argv);
}
#endif
