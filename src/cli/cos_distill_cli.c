/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * cos distill / cos proconductor — front-end for σ-gated multi-teacher flows.
 */

#include "cos_distill_cli.h"

#include "../sigma/distill.h"
#include "../sigma/proconductor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage_distill(FILE *o) {
    fputs(
        "usage:\n"
        "  cos distill --prompt \"...\" [--only claude,gpt,...]\n"
        "  cos distill --batch path/to/prompts.txt [--only ...]\n"
        "  cos distill --teachers\n"
        "  cos distill --stats\n"
        "  cos distill --export path.jsonl [--format jsonl|alpaca|sharegpt]\n",
        o);
}

static void usage_pc(FILE *o) {
    fputs(
        "usage:\n"
        "  cos proconductor --goal \"...\" [--only claude,...]\n"
        "  cos proconductor --history [N]\n",
        o);
}

static int parse_format(const char *s) {
    if (!s || !strcmp(s, "jsonl")) return COS_DISTILL_EXPORT_JSONL;
    if (!strcmp(s, "alpaca")) return COS_DISTILL_EXPORT_ALPACA;
    if (!strcmp(s, "sharegpt")) return COS_DISTILL_EXPORT_SHAREGPT;
    return COS_DISTILL_EXPORT_JSONL;
}

int cos_distill_main(int argc, char **argv) {
    const char *prompt       = NULL;
    const char *batch        = NULL;
    const char *only         = NULL;
    const char *export_path  = NULL;
    const char *fmt_str      = "jsonl";
    int mode_rank = 0, mode_stats = 0;

    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--prompt") && i + 1 < argc)
            prompt = argv[++i];
        else if (!strcmp(argv[i], "--batch") && i + 1 < argc)
            batch = argv[++i];
        else if (!strcmp(argv[i], "--only") && i + 1 < argc)
            only = argv[++i];
        else if (!strcmp(argv[i], "--export") && i + 1 < argc)
            export_path = argv[++i];
        else if (!strcmp(argv[i], "--format") && i + 1 < argc)
            fmt_str = argv[++i];
        else if (!strcmp(argv[i], "--teachers"))
            mode_rank = 1;
        else if (!strcmp(argv[i], "--stats"))
            mode_stats = 1;
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            usage_distill(stdout);
            return 0;
        }
    }

    if (cos_distill_init() != 0) {
        fputs("cos distill: database init failed\n", stderr);
        return 2;
    }

    if (mode_rank) {
        struct cos_distill_teacher ts[16];
        int nf = 0;
        cos_distill_teacher_ranking(ts, 16, &nf);
        printf("σ-trust EMA ranking (lower is better)\n");
        for (int i = 0; i < nf; i++)
            printf("  %2d  %-12s  trust_ema=%.4f  queries=%d  best=%d  avail=%d\n",
                   i + 1, ts[i].name, (double)ts[i].trust_ema,
                   ts[i].total_queries, ts[i].best_count, ts[i].available);
        return 0;
    }

    if (mode_stats) {
        long nr;
        double ai, ast;
        if (cos_distill_stats(&nr, &ai, &ast) != 0) {
            fputs("cos distill: stats query failed\n", stderr);
            return 2;
        }
        printf("stored examples: %ld\navg improvement (σ_loc−σ_t): %.6f\navg "
               "σ_teacher: %.6f\n",
               nr, ai, ast);
        return 0;
    }

    if (export_path) {
        if (cos_distill_export_training_data(export_path, parse_format(fmt_str))
            != 0) {
            fputs("cos distill: export failed\n", stderr);
            return 2;
        }
        printf("exported → %s\n", export_path);
        return 0;
    }

    if (prompt) {
        struct cos_distill_example ex;
        memset(&ex, 0, sizeof ex);
        if (cos_distill_run(prompt, only, &ex) != 0) {
            fputs("cos distill: run failed\n", stderr);
            return 2;
        }
        printf("teacher=%s  σ_local=%.4f  σ_teacher=%.4f  improvement=%.4f\n",
               ex.teacher, (double)ex.sigma_local, (double)ex.sigma_teacher,
               (double)ex.improvement);
        printf("--- best text ---\n%s\n", ex.best_response);
        return 0;
    }

    if (batch) {
        int nd = 0, sk = 0;
        if (cos_distill_batch(batch, only, &nd, &sk) != 0) {
            fputs("cos distill: batch failed\n", stderr);
            return 2;
        }
        printf("distilled rows: %d  skipped: %d\n", nd, sk);
        return 0;
    }

    usage_distill(stderr);
    return 1;
}

int cos_proconductor_main(int argc, char **argv) {
    const char *goal = NULL;
    const char *only = NULL;
    int hist = 0, hist_n = 20;

    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--goal") && i + 1 < argc)
            goal = argv[++i];
        else if (!strcmp(argv[i], "--only") && i + 1 < argc)
            only = argv[++i];
        else if (!strcmp(argv[i], "--history")) {
            hist = 1;
            if (i + 1 < argc && argv[i + 1][0] >= '0' &&
                argv[i + 1][0] <= '9') {
                hist_n = atoi(argv[i + 1]);
                i++;
            }
        } else if (!strcmp(argv[i], "--help")) {
            usage_pc(stdout);
            return 0;
        }
    }

    if (hist) {
        if (cos_distill_init() != 0) return 2;
        cos_proconductor_load_history(hist_n);
        return 0;
    }

    if (!goal) {
        usage_pc(stderr);
        return 1;
    }

    struct cos_proconductor_session S;
    memset(&S, 0, sizeof S);
    if (cos_proconductor_run(goal, only, &S) != 0) {
        fputs("cos proconductor: run failed\n", stderr);
        return 2;
    }
    cos_proconductor_print_report(&S);
    return 0;
}
