/*
 * cos-evolve — σ-guided parameter evolution CLI (OMEGA-1 … OMEGA-6).
 *
 * Subcommands:
 *   evolve            run N generations on a fixture → keep/revert
 *   memory-top        list top-N accepted mutations from SQLite
 *   memory-stats      aggregate: (n_total, n_accepted, best_delta, …)
 *   calibrate-auto    binary-sweep τ on a labeled σ-fixture
 *   discover          run a JSONL experiment list, append results
 *   omega             recursive loop: evolve + calibrate + discover
 *   daemon            foreground watchdog loop
 *   demo              deterministic 100-generation showcase
 *   self-test         link-level sanity for all evolve primitives
 *
 * Every subcommand is bit-reproducible given a fixed fixture and seed.
 * No subcommand writes to the repo unless --git-commit is passed;
 * the default persistence target is $COS_HOME (default ~/.cos/evolve).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "../sigma/evolve/cos_evolve.h"
#include "../sigma/evolve/opt_memory.h"
#include "../sigma/evolve/self_calibrate.h"
#include "../sigma/evolve/discover.h"

/* ------------------ paths + fs ------------------ */

static int mkpath(const char *p) {
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", p);
    for (char *q = buf + 1; *q; q++) {
        if (*q == '/') {
            *q = '\0';
            mkdir(buf, 0700);
            *q = '/';
        }
    }
    return mkdir(buf, 0700);
}

static const char *evolve_home(char *buf, size_t cap) {
    const char *env = getenv("COS_EVOLVE_HOME");
    if (env && *env) {
        snprintf(buf, cap, "%s", env);
    } else {
        const char *home = getenv("HOME");
        if (!home) home = "/tmp";
        snprintf(buf, cap, "%s/.cos/evolve", home);
    }
    mkpath(buf);
    return buf;
}

/* ------------------ util ------------------ */

static void die(const char *msg) {
    fprintf(stderr, "cos-evolve: %s\n", msg);
    exit(1);
}

static void append_log_jsonl(const char *path, const char *verb,
                             const cos_evolve_params_t *p,
                             const cos_evolve_score_t  *s,
                             const char *mutation_desc,
                             int accepted) {
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f,
        "{\"ts\":%lld,\"verb\":\"%s\",\"accepted\":%d,"
        "\"mutation\":\"%s\","
        "\"w_logprob\":%.6f,\"w_entropy\":%.6f,"
        "\"w_perplexity\":%.6f,\"w_consistency\":%.6f,\"tau\":%.6f,"
        "\"brier\":%.6f,\"accuracy_accept\":%.6f,\"coverage\":%.6f,"
        "\"n_items\":%d}\n",
        (long long)time(NULL), verb, accepted, mutation_desc ? mutation_desc : "",
        p->w_logprob, p->w_entropy, p->w_perplexity, p->w_consistency, p->tau,
        s->brier, s->accuracy_accept, s->coverage, s->n_items);
    fclose(f);
}

/* ------------------ evolve subcommand ------------------ */

static int usage(void) {
    fprintf(stderr,
        "usage: cos-evolve <command> [options]\n"
        "\n"
        "Commands:\n"
        "  evolve          --fixture F --generations N [--seed S] [--kernel K]\n"
        "  memory-top      [--kernel K] [--limit N]\n"
        "  memory-stats    [--kernel K]\n"
        "  calibrate-auto  --fixture F [--mode balanced|risk] [--alpha α]\n"
        "  discover        --experiments F\n"
        "  omega           --fixture F --rounds N [--kernel K...]\n"
        "  daemon          --fixture F [--interval S] [--max-iters N]\n"
        "  demo\n"
        "  self-test\n"
        "\n"
        "Env: COS_EVOLVE_HOME (default ~/.cos/evolve) — persistence dir.\n");
    return 1;
}

static int cmd_evolve(int argc, char **argv) {
    const char *fixture = NULL, *kernel = "sigma_router";
    int gens = 200;
    uint64_t seed = 0x1337BEEFULL;

    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--fixture") && i + 1 < argc) fixture = argv[++i];
        else if (!strcmp(argv[i], "--generations") && i + 1 < argc) gens = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = strtoull(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--kernel") && i + 1 < argc) kernel = argv[++i];
        else return usage();
    }
    if (!fixture) die("--fixture required");

    char home[512]; evolve_home(home, sizeof home);
    char params_path[1024], log_path[1024], db_path[1024];
    snprintf(params_path, sizeof params_path, "%s/params.json",      home);
    snprintf(log_path,    sizeof log_path,    "%s/evolve_log.jsonl", home);
    snprintf(db_path,     sizeof db_path,     "%s/memory.db",        home);

    cos_evolve_params_t p;
    cos_sigma_evolve_params_default(&p);
    cos_sigma_evolve_params_load_json(params_path, &p);   /* ok if absent */

    char err[256] = {0};
    int n = 0;
    cos_evolve_item_t *items = cos_sigma_evolve_load_fixture_jsonl(
            fixture, &n, err, sizeof err);
    if (!items || n <= 0) {
        fprintf(stderr, "cos-evolve: fixture: %s (n=%d)\n",
                err[0] ? err : "empty", n);
        free(items);
        return 2;
    }

    cos_evolve_score_t s_start;
    cos_sigma_evolve_score(&p, items, n, &s_start);

    cos_evolve_policy_t pol;
    cos_sigma_evolve_policy_default(&pol);
    pol.generations = gens;
    pol.rng_seed    = seed;

    cos_evolve_report_t rep;
    if (cos_sigma_evolve_run(&p, items, n, &pol, &rep) != 0) {
        free(items);
        die("evolve run failed");
    }

    cos_sigma_evolve_params_save_json(params_path, &p);

    /* Record the net (start → best) transition in opt_memory. */
    cos_opt_memory_t *mem = NULL;
    if (cos_opt_memory_open(db_path, &mem) == 0) {
        char desc[128];
        snprintf(desc, sizeof desc,
                 "gens=%d seed=0x%llx n_accept=%d n_revert=%d",
                 gens, (unsigned long long)seed,
                 rep.n_accepted, rep.n_reverted);
        cos_opt_memory_record(mem, kernel, desc,
                              s_start.brier, rep.best_score.brier,
                              rep.best_score.brier + 1e-6f < s_start.brier,
                              NULL);
        cos_opt_memory_close(mem);
    }

    append_log_jsonl(log_path, "evolve", &p, &rep.best_score,
                     "aggregate run",
                     rep.best_score.brier + 1e-6f < s_start.brier);

    printf("cos-evolve: fixture=%s n=%d kernel=%s gens=%d\n",
           fixture, n, kernel, gens);
    printf("  start  : brier=%.6f acc@τ=%.3f cov=%.3f τ=%.3f\n",
           s_start.brier, s_start.accuracy_accept, s_start.coverage, p.tau);
    printf("  best   : brier=%.6f acc@τ=%.3f cov=%.3f τ=%.3f\n",
           rep.best_score.brier, rep.best_score.accuracy_accept,
           rep.best_score.coverage, p.tau);
    printf("  weights: lp=%.3f ent=%.3f ppl=%.3f cons=%.3f\n",
           p.w_logprob, p.w_entropy, p.w_perplexity, p.w_consistency);
    printf("  n_accepted=%d n_reverted=%d\n",
           rep.n_accepted, rep.n_reverted);
    printf("  params : %s\n", params_path);
    printf("  log    : %s\n", log_path);
    printf("  memory : %s\n", db_path);

    free(items);
    return 0;
}

/* ------------------ memory-top / memory-stats ------------------ */

static int cmd_memory_top(int argc, char **argv) {
    const char *kernel = NULL;
    int limit = 10;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--kernel") && i + 1 < argc) kernel = argv[++i];
        else if (!strcmp(argv[i], "--limit") && i + 1 < argc) limit = atoi(argv[++i]);
        else return usage();
    }
    if (limit <= 0) limit = 10;
    if (limit > 64) limit = 64;

    char home[512]; evolve_home(home, sizeof home);
    char db_path[1024];
    snprintf(db_path, sizeof db_path, "%s/memory.db", home);

    cos_opt_memory_t *m = NULL;
    if (cos_opt_memory_open(db_path, &m) != 0) die("cannot open memory.db");
    cos_opt_memory_row_t rows[64];
    int n = cos_opt_memory_top_accepted(m, kernel, rows, limit);
    if (n < 0) { cos_opt_memory_close(m); die("query failed"); }

    printf("top-%d accepted mutations%s%s:\n",
           n, kernel ? " for " : "", kernel ? kernel : "");
    for (int i = 0; i < n; i++) {
        printf("  %3d  Δ=%+.6f  %-20s  %s\n",
               i + 1, rows[i].delta, rows[i].kernel, rows[i].mutation);
    }
    cos_opt_memory_close(m);
    return 0;
}

static int cmd_memory_stats(int argc, char **argv) {
    const char *kernel = NULL;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--kernel") && i + 1 < argc) kernel = argv[++i];
        else return usage();
    }
    char home[512]; evolve_home(home, sizeof home);
    char db_path[1024];
    snprintf(db_path, sizeof db_path, "%s/memory.db", home);
    cos_opt_memory_t *m = NULL;
    if (cos_opt_memory_open(db_path, &m) != 0) die("cannot open memory.db");
    cos_opt_memory_stats_t st;
    if (cos_opt_memory_stats(m, kernel, &st) != 0) {
        cos_opt_memory_close(m); die("stats failed");
    }
    printf("optimisation memory%s%s:\n",
           kernel ? " for " : "", kernel ? kernel : "");
    printf("  total              : %d\n", st.n_total);
    printf("  accepted           : %d\n", st.n_accepted);
    printf("  best Δbrier        : %+.6f\n", st.best_delta);
    printf("  mean Δbrier (acc.) : %+.6f\n", st.mean_delta_accepted);
    cos_opt_memory_close(m);
    return 0;
}

/* ------------------ calibrate-auto ------------------ */

static int cmd_calibrate_auto(int argc, char **argv) {
    const char *fixture = NULL;
    const char *mode_s = "balanced";
    float alpha = 0.50f;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--fixture") && i + 1 < argc) fixture = argv[++i];
        else if (!strcmp(argv[i], "--mode") && i + 1 < argc) mode_s = argv[++i];
        else if (!strcmp(argv[i], "--alpha") && i + 1 < argc) alpha = (float)atof(argv[++i]);
        else return usage();
    }
    if (!fixture) die("--fixture required");

    char err[256] = {0};
    int n = 0;
    cos_evolve_item_t *items = cos_sigma_evolve_load_fixture_jsonl(
            fixture, &n, err, sizeof err);
    if (!items || n <= 0) {
        fprintf(stderr, "cos-evolve: fixture: %s\n", err[0] ? err : "empty");
        free(items);
        return 2;
    }

    /* Score current params → per-row σ_combined, then self-calibrate. */
    char home[512]; evolve_home(home, sizeof home);
    char params_path[1024];
    snprintf(params_path, sizeof params_path, "%s/params.json", home);
    cos_evolve_params_t p;
    cos_sigma_evolve_params_default(&p);
    cos_sigma_evolve_params_load_json(params_path, &p);

    cos_calib_point_t *pts = calloc((size_t)n, sizeof *pts);
    for (int i = 0; i < n; i++) {
        float s =
              p.w_logprob     * items[i].sigma_logprob
            + p.w_entropy     * items[i].sigma_entropy
            + p.w_perplexity  * items[i].sigma_perplexity
            + p.w_consistency * items[i].sigma_consistency;
        if (s < 0.f) s = 0.f; if (s > 1.f) s = 1.f;
        pts[i].sigma   = s;
        pts[i].correct = items[i].correct;
    }

    cos_calib_mode_t mode = COS_CALIB_BALANCED;
    if (!strcmp(mode_s, "risk")) mode = COS_CALIB_RISK_BOUNDED;

    cos_calib_result_t r;
    if (cos_sigma_self_calibrate(pts, n, mode, alpha, &r) != 0) {
        free(items); free(pts); die("calibrate failed");
    }

    float tau_before = p.tau;
    p.tau = r.tau;
    cos_sigma_evolve_params_save_json(params_path, &p);

    printf("cos-calibrate-auto: n=%d mode=%s alpha=%.3f\n", n, mode_s, alpha);
    printf("  τ: %.3f → %.3f\n", tau_before, r.tau);
    printf("  false_accept=%.3f  false_reject=%.3f  coverage=%.3f  loss=%.3f\n",
           r.false_accept, r.false_reject, r.accept_rate, r.best_loss);

    free(items); free(pts);
    return 0;
}

/* ------------------ discover ------------------ */

static cos_discover_expect_kind_t parse_expect_kind(const char *s) {
    if (!s) return COS_DISCOVER_EXIT_0;
    if (!strcmp(s, "exit_0"))       return COS_DISCOVER_EXIT_0;
    if (!strcmp(s, "exit_nz"))      return COS_DISCOVER_EXIT_NZ;
    if (!strcmp(s, "contains"))     return COS_DISCOVER_CONTAINS;
    if (!strcmp(s, "not_contains")) return COS_DISCOVER_NOT_CONTAINS;
    return COS_DISCOVER_EXIT_0;
}

static const char *json_find_key(const char *line, const char *key);
static int json_copy_string(const char *line, const char *key,
                            char *out, size_t cap);

static int cmd_discover(int argc, char **argv) {
    const char *experiments = NULL;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--experiments") && i + 1 < argc) experiments = argv[++i];
        else return usage();
    }
    if (!experiments) die("--experiments required");

    FILE *f = fopen(experiments, "r");
    if (!f) die("cannot open experiments file");

    char home[512]; evolve_home(home, sizeof home);
    char log_path[1024];
    snprintf(log_path, sizeof log_path, "%s/discoveries.jsonl", home);

    int n_total = 0, n_confirmed = 0, n_rejected = 0, n_inconclusive = 0;
    char line[4096];
    while (fgets(line, sizeof line, f)) {
        const char *p = line;
        while (*p && (*p == ' ' || *p == '\t')) p++;
        if (*p != '{') continue;
        cos_discover_experiment_t ex = {0};
        json_copy_string(line, "id",         ex.id,         sizeof ex.id);
        json_copy_string(line, "hypothesis", ex.hypothesis, sizeof ex.hypothesis);
        json_copy_string(line, "cmd",        ex.cmd,        sizeof ex.cmd);
        json_copy_string(line, "expect",     ex.expect,     sizeof ex.expect);
        char kind[32] = "exit_0";
        json_copy_string(line, "expect_kind", kind, sizeof kind);
        ex.expect_kind = parse_expect_kind(kind);
        if (!ex.cmd[0]) continue;

        cos_discover_result_t r;
        if (cos_discover_run_one(&ex, &r) != 0) {
            n_inconclusive++;
            continue;
        }
        n_total++;
        switch (r.verdict) {
        case COS_DISCOVER_CONFIRMED:    n_confirmed++;    break;
        case COS_DISCOVER_REJECTED:     n_rejected++;     break;
        case COS_DISCOVER_INCONCLUSIVE: n_inconclusive++; break;
        }
        cos_discover_log_append_jsonl(log_path, &ex, &r);
        printf("  [%s] %-24s exit=%d t=%.0fms  %s\n",
               r.verdict == COS_DISCOVER_CONFIRMED ? "CONFIRM" :
               r.verdict == COS_DISCOVER_REJECTED  ? "REJECT " :
                                                     "INCONCL",
               ex.id, r.exit_code, r.elapsed_ms, ex.hypothesis);
    }
    fclose(f);
    printf("discover: total=%d confirmed=%d rejected=%d inconclusive=%d\n",
           n_total, n_confirmed, n_rejected, n_inconclusive);
    printf("  log: %s\n", log_path);
    return 0;
}

/* ------------------ omega: recursive loop ------------------ */

static int cmd_omega(int argc, char **argv) {
    const char *fixture = NULL;
    int rounds = 4;
    int gens_per_round = 100;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--fixture") && i + 1 < argc) fixture = argv[++i];
        else if (!strcmp(argv[i], "--rounds") && i + 1 < argc) rounds = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--gens") && i + 1 < argc) gens_per_round = atoi(argv[++i]);
        else return usage();
    }
    if (!fixture) die("--fixture required");

    char err[256] = {0};
    int n = 0;
    cos_evolve_item_t *items = cos_sigma_evolve_load_fixture_jsonl(
            fixture, &n, err, sizeof err);
    if (!items || n <= 0) { free(items); die("fixture load failed"); }

    char home[512]; evolve_home(home, sizeof home);
    char params_path[1024];
    snprintf(params_path, sizeof params_path, "%s/params.json", home);

    cos_evolve_params_t p;
    cos_sigma_evolve_params_default(&p);
    cos_sigma_evolve_params_load_json(params_path, &p);

    cos_evolve_score_t s_start;
    cos_sigma_evolve_score(&p, items, n, &s_start);

    printf("cos-omega recursive: fixture=%s n=%d rounds=%d gens/round=%d\n",
           fixture, n, rounds, gens_per_round);
    printf("  round 0: brier=%.6f τ=%.3f\n", s_start.brier, p.tau);

    int total_accept = 0, total_revert = 0;
    cos_evolve_score_t s_latest = s_start;
    for (int r = 1; r <= rounds; r++) {
        cos_evolve_policy_t pol;
        cos_sigma_evolve_policy_default(&pol);
        pol.generations = gens_per_round;
        pol.rng_seed    = 0xC0DEC0DEULL + (uint64_t)r;
        cos_evolve_report_t rep;
        if (cos_sigma_evolve_run(&p, items, n, &pol, &rep) != 0) break;
        total_accept += rep.n_accepted;
        total_revert += rep.n_reverted;
        s_latest = rep.best_score;

        /* τ auto-calibration between evolution rounds. */
        cos_calib_point_t *pts = calloc((size_t)n, sizeof *pts);
        for (int i = 0; i < n; i++) {
            float s =
                  p.w_logprob     * items[i].sigma_logprob
                + p.w_entropy     * items[i].sigma_entropy
                + p.w_perplexity  * items[i].sigma_perplexity
                + p.w_consistency * items[i].sigma_consistency;
            if (s < 0.f) s = 0.f; if (s > 1.f) s = 1.f;
            pts[i].sigma = s; pts[i].correct = items[i].correct;
        }
        cos_calib_result_t calib;
        cos_sigma_self_calibrate(pts, n, COS_CALIB_BALANCED, 0.5f, &calib);
        p.tau = calib.tau;
        free(pts);

        printf("  round %d: brier=%.6f acc@τ=%.3f cov=%.3f τ=%.3f  "
               "(+%d/-%d)\n",
               r, rep.best_score.brier, rep.best_score.accuracy_accept,
               rep.best_score.coverage, p.tau,
               rep.n_accepted, rep.n_reverted);
    }
    cos_sigma_evolve_params_save_json(params_path, &p);

    printf("cos-omega: brier %.6f → %.6f  Δ=%+.6f  (+%d accepted / -%d reverted)\n",
           s_start.brier, s_latest.brier, s_latest.brier - s_start.brier,
           total_accept, total_revert);
    free(items);
    return 0;
}

/* ------------------ daemon (foreground with watchdog) ------------------ */

static volatile sig_atomic_t g_stop = 0;
static void on_sig(int s) { (void)s; g_stop = 1; }

static int cmd_daemon(int argc, char **argv) {
    const char *fixture = NULL;
    int interval = 5;
    int max_iters = 20;
    int gens = 80;
    float rise_limit = 0.02f;   /* if brier rises this much vs rolling min
                                 * across >= 3 iterations → halt.        */
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--fixture") && i + 1 < argc) fixture = argv[++i];
        else if (!strcmp(argv[i], "--interval") && i + 1 < argc) interval = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--max-iters") && i + 1 < argc) max_iters = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--gens") && i + 1 < argc) gens = atoi(argv[++i]);
        else return usage();
    }
    if (!fixture) die("--fixture required");

    signal(SIGINT,  on_sig);
    signal(SIGTERM, on_sig);

    char err[256] = {0};
    int n = 0;
    cos_evolve_item_t *items = cos_sigma_evolve_load_fixture_jsonl(
            fixture, &n, err, sizeof err);
    if (!items || n <= 0) { free(items); die("fixture load failed"); }

    char home[512]; evolve_home(home, sizeof home);
    char params_path[1024], daemon_log[1024];
    snprintf(params_path, sizeof params_path, "%s/params.json", home);
    snprintf(daemon_log,  sizeof daemon_log,  "%s/omega_daemon.log", home);

    cos_evolve_params_t p;
    cos_sigma_evolve_params_default(&p);
    cos_sigma_evolve_params_load_json(params_path, &p);

    cos_evolve_score_t s_prev;
    cos_sigma_evolve_score(&p, items, n, &s_prev);
    float rolling_min = s_prev.brier;
    int   rise_streak = 0;

    FILE *lf = fopen(daemon_log, "a");
    if (lf) {
        fprintf(lf, "[%lld] daemon start brier=%.6f τ=%.3f\n",
                (long long)time(NULL), s_prev.brier, p.tau);
        fclose(lf);
    }

    const char *halt_reason = "max_iters";
    int it = 0;
    for (it = 0; it < max_iters && !g_stop; it++) {
        cos_evolve_params_t p_before = p;
        cos_evolve_policy_t pol;
        cos_sigma_evolve_policy_default(&pol);
        pol.generations = gens;
        pol.rng_seed    = 0xDADA0000ULL + (uint64_t)it;
        cos_evolve_report_t rep;
        cos_sigma_evolve_run(&p, items, n, &pol, &rep);

        /* Watchdog: brier monotone envelope. */
        if (rep.best_score.brier < rolling_min) {
            rolling_min = rep.best_score.brier;
            rise_streak = 0;
        } else if (rep.best_score.brier > rolling_min + rise_limit) {
            rise_streak++;
        } else {
            rise_streak = 0;
        }

        FILE *lf2 = fopen(daemon_log, "a");
        if (lf2) {
            fprintf(lf2,
                "[%lld] iter=%d brier=%.6f acc@τ=%.3f cov=%.3f τ=%.3f "
                "rolling_min=%.6f rise_streak=%d\n",
                (long long)time(NULL), it,
                rep.best_score.brier, rep.best_score.accuracy_accept,
                rep.best_score.coverage, p.tau,
                rolling_min, rise_streak);
            fclose(lf2);
        }
        printf("daemon iter=%d brier=%.6f τ=%.3f rise=%d\n",
               it, rep.best_score.brier, p.tau, rise_streak);

        if (rise_streak >= 3) {
            /* Roll back to the pre-iteration snapshot. */
            p = p_before;
            halt_reason = "brier_rise";
            break;
        }
        cos_sigma_evolve_params_save_json(params_path, &p);
        s_prev = rep.best_score;
        if (interval > 0 && it + 1 < max_iters && !g_stop) {
            sleep((unsigned)interval);
        }
    }
    if (g_stop) halt_reason = "signal";

    FILE *lf3 = fopen(daemon_log, "a");
    if (lf3) {
        fprintf(lf3, "[%lld] daemon stop reason=%s iters=%d\n",
                (long long)time(NULL), halt_reason, it);
        fclose(lf3);
    }
    printf("daemon halt: reason=%s iters=%d log=%s\n",
           halt_reason, it, daemon_log);
    free(items);
    return 0;
}

/* ------------------ demo + self-test ------------------ */

static void write_demo_fixture(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    /* 40 deterministic rows: 20 correct with low σ, 20 incorrect with
     * high σ.  σ_logprob is the sharpest signal, σ_perplexity the
     * noisiest — the same shape as the internal synthetic self-test. */
    for (int i = 0; i < 40; i++) {
        int correct = (i & 1) == 0;
        float base = correct ? 0.12f : 0.82f;
        float lp   = base + ((i * 13) % 17) / 17.f * 0.10f - 0.05f;
        float ent  = base + ((i * 29) % 31) / 31.f * 0.25f - 0.125f;
        float ppl  = base + ((i * 41) % 47) / 47.f * 0.40f - 0.20f;
        float cons = base + ((i * 17) % 19) / 19.f * 0.20f - 0.10f;
        if (lp < 0.f) lp = 0.f; if (lp > 1.f) lp = 1.f;
        if (ent < 0.f) ent = 0.f; if (ent > 1.f) ent = 1.f;
        if (ppl < 0.f) ppl = 0.f; if (ppl > 1.f) ppl = 1.f;
        if (cons < 0.f) cons = 0.f; if (cons > 1.f) cons = 1.f;
        fprintf(f,
            "{\"sigma_logprob\":%.4f,\"sigma_entropy\":%.4f,"
            "\"sigma_perplexity\":%.4f,\"sigma_consistency\":%.4f,"
            "\"correct\":%d}\n",
            lp, ent, ppl, cons, correct);
    }
    fclose(f);
}

static int cmd_demo(void) {
    char home[512]; evolve_home(home, sizeof home);
    char fx[1024];
    snprintf(fx, sizeof fx, "%s/demo_fixture.jsonl", home);
    write_demo_fixture(fx);
    printf("cos-evolve demo: fixture written to %s\n", fx);
    char *ar_evolve[] = { "--fixture", fx, "--generations", "300",
                          "--seed", "0xC0DE", "--kernel", "sigma_router", NULL };
    int argc = 0; while (ar_evolve[argc]) argc++;
    int rc = cmd_evolve(argc, ar_evolve);
    if (rc) return rc;
    char *ar_calib[] = { "--fixture", fx, "--mode", "balanced", "--alpha", "0.5", NULL };
    argc = 0; while (ar_calib[argc]) argc++;
    rc = cmd_calibrate_auto(argc, ar_calib);
    if (rc) return rc;
    char *ar_omega[] = { "--fixture", fx, "--rounds", "3", "--gens", "120", NULL };
    argc = 0; while (ar_omega[argc]) argc++;
    return cmd_omega(argc, ar_omega);
}

static int cmd_self_test(void) {
    int rc;
    if ((rc = cos_sigma_evolve_self_test()) != 0) {
        fprintf(stderr, "evolve self-test failed rc=%d\n", rc); return 1;
    }
    if ((rc = cos_sigma_opt_memory_self_test()) != 0) {
        fprintf(stderr, "opt_memory self-test failed rc=%d\n", rc); return 1;
    }
    if ((rc = cos_sigma_self_calibrate_self_test()) != 0) {
        fprintf(stderr, "self_calibrate self-test failed rc=%d\n", rc); return 1;
    }
    if ((rc = cos_discover_self_test()) != 0) {
        fprintf(stderr, "discover self-test failed rc=%d\n", rc); return 1;
    }
    printf("cos-evolve self-test: PASS (evolve + memory + calibrate + discover)\n");
    return 0;
}

/* ------------------ tiny JSON helpers (for discover subcmd only) ----- */

static const char *json_find_key(const char *line, const char *key) {
    size_t kl = strlen(key);
    const char *p = line;
    while ((p = strchr(p, '"')) != NULL) {
        if (strncmp(p + 1, key, kl) == 0 && p[1 + kl] == '"') {
            const char *q = p + 1 + kl + 1;
            while (*q && (*q == ' ' || *q == '\t' || *q == ':')) q++;
            return q;
        }
        p++;
    }
    return NULL;
}

static int json_copy_string(const char *line, const char *key,
                            char *out, size_t cap) {
    const char *p = json_find_key(line, key);
    if (!p || *p != '"') { if (cap) out[0] = '\0'; return 0; }
    p++;
    size_t j = 0;
    while (*p && *p != '"' && j + 1 < cap) {
        if (*p == '\\' && p[1]) {
            char c = p[1];
            switch (c) {
            case 'n': out[j++] = '\n'; break;
            case 't': out[j++] = '\t'; break;
            case 'r': out[j++] = '\r'; break;
            case '"': out[j++] = '"';  break;
            case '\\': out[j++] = '\\';break;
            default: out[j++] = c; break;
            }
            p += 2;
        } else {
            out[j++] = *p++;
        }
    }
    out[j] = '\0';
    return (int)j;
}

/* ------------------ main ------------------ */

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    const char *cmd = argv[1];
    int sub_argc = argc - 2;
    char **sub_argv = argv + 2;

    if (!strcmp(cmd, "evolve"))         return cmd_evolve(sub_argc, sub_argv);
    if (!strcmp(cmd, "memory-top"))     return cmd_memory_top(sub_argc, sub_argv);
    if (!strcmp(cmd, "memory-stats"))   return cmd_memory_stats(sub_argc, sub_argv);
    if (!strcmp(cmd, "calibrate-auto")) return cmd_calibrate_auto(sub_argc, sub_argv);
    if (!strcmp(cmd, "discover"))       return cmd_discover(sub_argc, sub_argv);
    if (!strcmp(cmd, "omega"))          return cmd_omega(sub_argc, sub_argv);
    if (!strcmp(cmd, "daemon"))         return cmd_daemon(sub_argc, sub_argv);
    if (!strcmp(cmd, "demo"))           return cmd_demo();
    if (!strcmp(cmd, "self-test"))      return cmd_self_test();
    return usage();
}
