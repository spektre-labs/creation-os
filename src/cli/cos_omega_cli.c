/*
 * cos omega — unified Ω-loop driver.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "cos_omega_cli.h"

#include "../sigma/omega_loop.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void cos_omega_sig_handler(int sig)
{
    (void)sig;
    cos_omega_signal_halt_requested = 1;
}

static int argv_skip_omega_cmd(int argc, char **argv)
{
    if (argc > 1 && argv[1] != NULL && strcmp(argv[1], "omega") == 0)
        return 2;
    return 1;
}

static int omega_status_file(char *buf, size_t cap)
{
    const char *h = getenv("HOME");
    if (!h || !h[0])
        h = "/tmp";
    return snprintf(buf, cap, "%s/.cos/omega/status.txt", h);
}

static int omega_halt_touch(void)
{
    char dir[512];
    char path[768];
    const char *h = getenv("HOME");

    if (!h || !h[0])
        h = "/tmp";
    snprintf(dir, sizeof dir, "%s/.cos/omega", h);
    if (mkdir(dir, 0700) != 0 && errno != EEXIST)
        return -1;
    snprintf(path, sizeof path, "%s/halt", dir);
    {
        FILE *f = fopen(path, "w");
        if (!f)
            return -2;
        fclose(f);
    }
    return 0;
}

static int usage_omega(FILE *fp)
{
    fputs(
        "cos omega — unified Ω-loop (perceive → think → gate → learn)\n"
        "  cos omega                  run until halt file, σ thresholds, or SIGINT\n"
        "  cos omega --turns N        stop after N turns (default: infinite)\n"
        "  cos omega --hours H        wall-clock cap (sets COS_OMEGA_HOURS_CAP)\n"
        "  cos omega --status         show last persisted ~/.cos/omega/status.txt\n"
        "  cos omega --halt           request graceful stop (~/.cos/omega/halt)\n"
        "  cos omega --report         print report (after a run or from last state)\n"
        "  cos omega --sim            synthetic σ step (no live think / perception I/O)\n"
        "  cos omega --light          minimal Ω for low-RAM hosts (60s turn cap, max 1 rethink,\n"
        "                             no TTT / federation / codegen / embodiment / consciousness)\n"
        "  cos omega --evolve         rule-based τ / temperature adaptation (JSONL evolver.jsonl)\n"
        "  cos omega --patterns       periodic pattern_extract → ~/.cos/patterns.json\n"
        "  cos omega --autonomous     rotate graded prompt bank (CREATION_OS_ROOT + CSV)\n"
        "  cos omega --minutes M      wall cap (sets COS_OMEGA_MINUTES_CAP; pairs with --hours)\n"
        "  cos omega --verbose        stderr detail for evolver / autonomous\n"
        "Env: COS_OMEGA_GOAL  per-turn goal string (optional)\n"
        "     COS_OMEGA_SIM=1       same as --sim\n"
        "     COS_OMEGA_HOURS_CAP  hours wall clock (alternative to --hours)\n"
        "     COS_OMEGA_MINUTES_CAP short wall cap (alternative to --minutes)\n"
        "     COS_OMEGA_TURN_TIMEOUT_S  per-turn wall budget (default 120; --light uses 60)\n",
        fp);
    return fp == stderr ? 2 : 0;
}

int cos_omega_main(int argc, char **argv)
{
    int                       base = argv_skip_omega_cmd(argc, argv);
    int                       i;
    struct cos_omega_config   cfg;
    struct cos_omega_state    st;
    char                     *rep;
    int                       verbose_cli = 0;

    memset(&cfg, 0, sizeof cfg);
    {
        const char *es = getenv("COS_OMEGA_SIM");
        if (es && es[0] == '1')
            cfg.simulation_mode = 1;
    }

    if (argc < base)
        return usage_omega(stderr);

    if (argc > base
        && (strcmp(argv[base], "--help") == 0 || strcmp(argv[base], "-h") == 0))
        return usage_omega(stdout);

    for (i = base; i < argc; ++i) {
        if (strcmp(argv[i], "--turns") == 0 && i + 1 < argc) {
            cfg.max_turns = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--hours") == 0 && i + 1 < argc) {
            char ebuf[64];
            snprintf(ebuf, sizeof ebuf, "%s", argv[++i]);
            setenv("COS_OMEGA_HOURS_CAP", ebuf, 1);
        } else if (strcmp(argv[i], "--minutes") == 0 && i + 1 < argc) {
            char ebuf[64];
            snprintf(ebuf, sizeof ebuf, "%s", argv[++i]);
            setenv("COS_OMEGA_MINUTES_CAP", ebuf, 1);
        } else if (strcmp(argv[i], "--evolve") == 0) {
            cfg.enable_evolver = 1;
        } else if (strcmp(argv[i], "--patterns") == 0) {
            cfg.enable_pattern_extract = 1;
        } else if (strcmp(argv[i], "--autonomous") == 0) {
            cfg.autonomous_mode = 1;
            cfg.consolidate_interval = 100;
            cfg.enable_evolver         = 1;
            cfg.enable_pattern_extract = 1;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose_cli = 1;
        } else if (strcmp(argv[i], "--sim") == 0) {
            cfg.simulation_mode = 1;
        } else if (strcmp(argv[i], "--light") == 0) {
            cfg.turn_timeout_s       = 60;
            cfg.max_rethinks         = 1;
            cfg.enable_ttt           = 0;
            cfg.enable_search        = 0;
            cfg.enable_codegen       = 0;
            cfg.enable_federation    = 0;
            cfg.enable_physical      = 0;
            cfg.enable_consciousness = 0;
        } else if (strcmp(argv[i], "--status") == 0) {
            char path[768];
            FILE *f;

            if (omega_status_file(path, sizeof path) >= (int)sizeof path)
                return 3;
            f = fopen(path, "r");
            if (!f) {
                fputs("cos omega: no status file yet\n", stderr);
                return 4;
            }
            {
                char ln[512];
                while (fgets(ln, sizeof ln, f))
                    fputs(ln, stdout);
            }
            fclose(f);
            return 0;
        } else if (strcmp(argv[i], "--halt") == 0) {
            if (omega_halt_touch() != 0) {
                fputs("cos omega: could not write halt file\n", stderr);
                return 5;
            }
            fputs("cos omega: halt requested\n", stdout);
            return 0;
        } else if (strcmp(argv[i], "--report") == 0) {
            cfg.simulation_mode       = 1;
            cfg.enable_consciousness  = 0;
            cfg.enable_federation     = 0;
            if (cos_omega_init(&cfg, &st) != 0)
                return 6;
            rep = cos_omega_report(&st);
            if (!rep)
                return 7;
            fputs(rep, stdout);
            free(rep);
            return 0;
        } else {
            fprintf(stderr, "cos omega: unknown flag `%s`\n", argv[i]);
            return usage_omega(stderr);
        }
    }

    if (verbose_cli)
        cfg.verbose_evolver = 1;

    if (cos_omega_init(&cfg, &st) != 0) {
        fputs("cos omega: init failed\n", stderr);
        return 8;
    }

    {
        struct sigaction sa_old_int, sa_old_term, sa_new;

        memset(&sa_new, 0, sizeof sa_new);
        sa_new.sa_handler = cos_omega_sig_handler;
        sigemptyset(&sa_new.sa_mask);
        sigaction(SIGINT, &sa_new, &sa_old_int);
        sigaction(SIGTERM, &sa_new, &sa_old_term);

        cos_omega_signal_halt_requested = 0;

        if (cos_omega_run(&st) != 0)
            fputs("cos omega: run aborted\n", stderr);

        sigaction(SIGINT, &sa_old_int, NULL);
        sigaction(SIGTERM, &sa_old_term, NULL);
    }

    if (cos_omega_finish_session(&st) != 0) {
        fputs("cos omega: finish_session failed\n", stderr);
        return 9;
    }

    return 0;
}

#if defined(COS_OMEGA_MAIN)
int main(int argc, char **argv)
{
    return cos_omega_main(argc, argv);
}
#endif
