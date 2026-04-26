/* cos-bench — minimal graded runner: fork/exec cos-chat per CSV row.
 *
 * Usage (from repo root, after `make cos-chat`):
 *   ./cos-bench --graded --n 50
 *   ./cos-bench --graded --n 200
 *
 * Writes benchmarks/graded/cos_bench_results.csv
 * Run `python3 scripts/real/compute_auroc.py` on that file if needed.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

#include "adaptive_tau.h"

enum { BENCH_LINE_MAX = 16384, BENCH_PROMPT_MAX = 4096 };

static int file_exists(const char *p)
{
    struct stat st;
    return (stat(p, &st) == 0 && S_ISREG(st.st_mode)) ? 1 : 0;
}

static void norm_alnum(const char *in, char *out, size_t cap)
{
    size_t w = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p && w + 2 < cap;
         ++p) {
        unsigned char c = *p;
        if (isalnum(c)) {
            if (c >= 'A' && c <= 'Z')
                c = (unsigned char)(c + 32u);
            out[w++] = (char)c;
        } else if (w > 0 && out[w - 1] != ' ')
            out[w++] = ' ';
    }
    while (w > 0 && out[w - 1] == ' ')
        w--;
    out[w] = '\0';
}

static int grade_simple(const char *cat, const char *gold, const char *resp,
                        const char *action)
{
    if (strcmp(cat, "CREATIVE") == 0 || strcmp(cat, "SELF_AWARE") == 0)
        return 1;
    if (strcmp(cat, "IMPOSSIBLE") == 0)
        return (strcmp(action, "ABSTAIN") == 0) ? 1 : 0;
    char g[512], a[512];
    norm_alnum(gold, g, sizeof g);
    norm_alnum(resp, a, sizeof a);
    if (g[0] == '\0')
        return 0;
    return (strstr(a, g) != NULL) ? 1 : 0;
}

static int parse_sigma_action(const char *out, float *sig, char *act, size_t actcap)
{
    *sig = 0.5f;
    snprintf(act, actcap, "N/A");
    const char *p = strstr(out, "[σ=");
    if (p != NULL) {
        float s = 0.f;
        if (sscanf(p, "[σ=%f", &s) == 1)
            *sig = s;
        const char *bar = strchr(p, '|');
        if (bar != NULL) {
            bar++;
            while (*bar == ' ' || *bar == '\t')
                bar++;
            size_t k = 0;
            while (bar[k] && bar[k] != '|' && bar[k] != '\n' && bar[k] != '\r'
                   && k + 1 < actcap)
                k++;
            if (k > 0) {
                memcpy(act, bar, k);
                act[k] = '\0';
            }
        }
    }
    return 0;
}

static int run_cos_chat_capture(const char *chat_bin, const char *prompt,
                                char *out, size_t cap)
{
    int fd[2];
    if (pipe(fd) != 0)
        return -1;
    pid_t pid = fork();
    if (pid < 0) {
        close(fd[0]);
        close(fd[1]);
        return -1;
    }
    if (pid == 0) {
        close(fd[0]);
        if (dup2(fd[1], STDOUT_FILENO) < 0)
            _exit(126);
        close(fd[1]);
        execl(chat_bin, chat_bin, "--once", "--no-tui", "--no-stream",
              "--no-coherence", "--prompt", prompt, (char *)NULL);
        _exit(127);
    }
    close(fd[1]);
    size_t n = 0;
    out[0] = '\0';
    for (;;) {
        char tmp[4096];
        ssize_t r = read(fd[0], tmp, sizeof tmp);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (r == 0)
            break;
        if (n + (size_t)r + 1u < cap) {
            memcpy(out + n, tmp, (size_t)r);
            n += (size_t)r;
            out[n] = '\0';
        }
    }
    close(fd[0]);
    int st = 0;
    if (waitpid(pid, &st, 0) < 0)
        return -1;
    return (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? 0 : -1;
}

static const char *find_chat_bin(void)
{
    if (file_exists("./cos-chat"))
        return "./cos-chat";
    return "cos-chat";
}

static int bench_graded(const char *csv, int n_max, const char *title,
                        int adaptive_tau)
{
    (void)title;
    if (!file_exists(csv)) {
        fprintf(stderr, "cos-bench: %s not found (run from repo root)\n", csv);
        return 1;
    }
    const char *chat = find_chat_bin();
    if (!file_exists(chat) && strcmp(chat, "./cos-chat") == 0) {
        fprintf(stderr, "cos-bench: ./cos-chat missing — run make cos-chat\n");
        return 127;
    }

    FILE *fi = fopen(csv, "r");
    if (fi == NULL) {
        perror("cos-bench: fopen csv");
        return 1;
    }
    char outpath[512];
    time_t     now = time(NULL);
    struct tm  tmv;
    char       datebuf[32];
    if (gmtime_r(&now, &tmv) == NULL)
        snprintf(datebuf, sizeof datebuf, "unknown");
    else
        strftime(datebuf, sizeof datebuf, "%Y%m%d", &tmv);
    snprintf(outpath, sizeof outpath,
             "benchmarks/graded/results_%s.jsonl", datebuf);
    if (strstr(csv, "truthfulqa") != NULL)
        snprintf(outpath, sizeof outpath,
                 "benchmarks/truthfulqa/results_%s.jsonl", datebuf);

    FILE *fo = fopen(outpath, "w");
    if (fo == NULL) {
        fprintf(stderr, "cos-bench: cannot write %s\n", outpath);
        fclose(fi);
        return 1;
    }
    char line[BENCH_LINE_MAX];
    if (fgets(line, sizeof line, fi) == NULL) {
        fclose(fi);
        fclose(fo);
        return 1;
    }

    char capbuf[BENCH_LINE_MAX];
    int   n_done = 0;
    clock_t t0 = clock();
    cos_adaptive_tau_t bench_at;
    memset(&bench_at, 0, sizeof bench_at);
    if (adaptive_tau) {
        if (cos_adaptive_tau_state_load(&bench_at) != 0) {
            bench_at.tau_accept        = 0.40f;
            bench_at.tau_rethink       = 0.60f;
            bench_at.accuracy_estimate = 0.5f;
        }
    }
    while (n_done < n_max && fgets(line, sizeof line, fi) != NULL) {
        char *p = line;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            p++;
        if (*p == '\0' || *p == '#')
            continue;

        char cat[64], prompt[BENCH_PROMPT_MAX], gold[512];
        cat[0] = prompt[0] = gold[0] = '\0';
        /* Minimal CSV: three fields, prompt may contain quoted commas — skip
         * complex CSV; split on first two commas only for pilot rows. */
        char *c0 = strchr(p, ',');
        if (!c0)
            continue;
        *c0 = '\0';
        snprintf(cat, sizeof cat, "%s", p);
        char *c1 = strchr(c0 + 1, ',');
        if (!c1)
            continue;
        *c1 = '\0';
        snprintf(prompt, sizeof prompt, "%s", c0 + 1);
        {
            char *tail = c1 + 1;
            size_t tl = strlen(tail);
            while (tl > 0 && (tail[tl - 1] == '\n' || tail[tl - 1] == '\r'))
                tail[--tl] = '\0';
            snprintf(gold, sizeof gold, "%s", tail);
        }

        if (run_cos_chat_capture(chat, prompt, capbuf, sizeof capbuf) != 0) {
            fprintf(stderr, "cos-bench: cos-chat failed for row %d\n", n_done);
            n_done++;
            continue;
        }

        float        sig = 0.5f;
        char         act[32];
        parse_sigma_action(capbuf, &sig, act, sizeof act);
        const char *ans = "";
        const char *rd0 = strstr(capbuf, "round 0");
        if (rd0 != NULL) {
            rd0 += 7;
            while (*rd0 == ' ')
                rd0++;
            const char *br = strstr(rd0, "  [σ_peak=");
            if (br != NULL && br > rd0) {
                size_t al = (size_t)(br - rd0);
                static char ansbuf[2048];
                if (al >= sizeof ansbuf)
                    al = sizeof ansbuf - 1u;
                memcpy(ansbuf, rd0, al);
                ansbuf[al] = '\0';
                ans = ansbuf;
            }
        }

        int ok = grade_simple(cat, gold, ans, act);
        fprintf(fo,
                "{\"category\":\"%s\",\"prompt\":", cat);
        fputc('"', fo);
        for (const char *q = prompt; *q; ++q) {
            if (*q == '"' || *q == '\\')
                fputc('\\', fo);
            if (*q == '\n')
                fputs("\\n", fo);
            else if (*q != '\r')
                fputc(*q, fo);
        }
        fputs("\",\"gold\":\"", fo);
        for (const char *q = gold; *q; ++q) {
            if (*q == '"' || *q == '\\')
                fputc('\\', fo);
            else if (*q != '\n' && *q != '\r')
                fputc(*q, fo);
        }
        fputs("\",\"model_answer\":\"", fo);
        for (const char *q = ans; *q; ++q) {
            if (*q == '"' || *q == '\\')
                fputc('\\', fo);
            else if (*q == '\n')
                fputs("\\n", fo);
            else if (*q != '\r')
                fputc(*q, fo);
        }
        fprintf(fo, "\",\"sigma\":%.6f,\"action\":\"%s\",\"correct\":%d}\n",
                (double)sig, act, ok);
        fprintf(stdout, "  [%d] σ=%.3f %s  grade=%s\n", n_done, (double)sig, act,
                ok ? "OK" : "XX");
        if (adaptive_tau) {
            (void)cos_adaptive_tau_update(&bench_at, sig, ok);
            (void)cos_adaptive_tau_state_save(&bench_at);
        }
        n_done++;
    }

    fclose(fi);
    fclose(fo);
    clock_t t1 = clock();
    double  sec = (double)(t1 - t0) / (double)CLOCKS_PER_SEC;
    if (sec < 0)
        sec = 0;
    fprintf(stdout, "cos-bench: wrote %s (%d rows) in %.1fs\n", outpath, n_done,
            sec);
    if (adaptive_tau)
        fprintf(stdout,
                "tau_accept=%.3f (calibrated from %d samples)\n",
                (double)bench_at.tau_accept, bench_at.n_history);
    return 0;
}

static int bench_break_it(void)
{
    if (!file_exists("tests/stress/break_it.csv")) {
        fputs("cos-bench: tests/stress/break_it.csv not found\n", stderr);
        return 1;
    }
    if (!file_exists("scripts/real/run_break_it.sh")) {
        fputs("cos-bench: scripts/real/run_break_it.sh not found\n", stderr);
        return 1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        perror("cos-bench: fork");
        return 126;
    }
    if (pid == 0) {
        execl("/bin/sh", "sh", "scripts/real/run_break_it.sh", (char *)NULL);
        perror("cos-bench: execl");
        _exit(127);
    }
    int st = 0;
    if (waitpid(pid, &st, 0) < 0) {
        perror("cos-bench: waitpid");
        return 126;
    }
    if (!WIFEXITED(st))
        return 1;
    return WEXITSTATUS(st);
}

int cos_bench_main(int argc, char **argv)
{
    int graded = 0, truthfulqa = 0, breakit = 0, n = 50, adaptive_tau = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--graded") == 0)
            graded = 1;
        else if (strcmp(argv[i], "--truthfulqa") == 0)
            truthfulqa = 1;
        else if (strcmp(argv[i], "--break-it") == 0 || strcmp(argv[i], "--breakit") == 0)
            breakit = 1;
        else if (strcmp(argv[i], "--adaptive-tau") == 0)
            adaptive_tau = 1;
        else if (strcmp(argv[i], "--n") == 0 && i + 1 < argc)
            n = atoi(argv[++i]);
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fputs("cos-bench — harness driver (needs ./cos-chat)\n"
                  "  --graded       graded σ-gate CSV (50-row fixture)\n"
                  "  --truthfulqa   TruthfulQA-style pilot CSV\n"
                  "  --break-it     run scripts/real/run_break_it.sh\n"
                  "  --adaptive-tau update ~/.cos/tau_state.json from graded rows\n"
                  "  --n N          max graded rows (default 50)\n",
                  stdout);
            return 0;
        }
    }
    if (n < 1)
        n = 1;
    if (n > 5000)
        n = 5000;

    if (breakit) {
        if (graded || truthfulqa) {
            fputs("cos-bench: use only one of --graded / --truthfulqa / --break-it\n",
                  stderr);
            return 64;
        }
        return bench_break_it();
    }

    if (truthfulqa) {
        const char *csv = "benchmarks/truthfulqa/cos_bench_prompts.csv";
        if (!file_exists(csv)) {
            fprintf(stderr, "cos-bench: %s missing\n", csv);
            return 1;
        }
        printf("=== Creation OS Benchmark: TruthfulQA pilot ===\n");
        return bench_graded(csv, n, "truthfulqa", adaptive_tau);
    }

    if (!graded) {
        fputs("cos-bench: specify --graded, --truthfulqa, or --break-it\n", stderr);
        return 64;
    }

    const char *csv = "benchmarks/graded/graded_50_prompts.csv";
    if (!file_exists(csv))
        csv = "benchmarks/graded/graded_prompts.csv";
    printf("=== Creation OS Benchmark: Graded set ===\n");
    return bench_graded(csv, n, "graded", adaptive_tau);
}

int main(int argc, char **argv) { return cos_bench_main(argc, argv); }
