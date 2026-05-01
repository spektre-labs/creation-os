/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * cos-exec — run shell commands behind a digital-twin preflight (HORIZON-2).
 *
 *   cos exec --simulate "cp A B"     check preconditions; run if σ_twin ok
 *   cos exec --simulate --dry-run "cp A B"   preflight only
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "../sigma/twin/digital_twin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static void join_argv(char *out, size_t cap, int argc, char **argv) {
    out[0] = '\0';
    size_t len = 0;
    for (int j = 0; j < argc; j++) {
        if (j > 0) {
            if (len + 1 >= cap) return;
            out[len++] = ' ';
            out[len] = '\0';
        }
        size_t L = strlen(argv[j]);
        if (len + L >= cap) return;
        memcpy(out + len, argv[j], L);
        len += L;
        out[len] = '\0';
    }
}

static int sh_c_capture(const char *cmd, char *buf, size_t cap) {
    if (!cmd || !buf || cap < 2) return -1;
    int p[2];
    if (pipe(p) != 0) return -1;
    pid_t pid = fork();
    if (pid < 0) {
        close(p[0]);
        close(p[1]);
        return -1;
    }
    if (pid == 0) {
        close(p[0]);
        dup2(p[1], STDOUT_FILENO);
        dup2(p[1], STDERR_FILENO);
        close(p[1]);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(126);
    }
    close(p[1]);
    size_t n = 0;
    for (;;) {
        ssize_t r = read(p[0], buf + n, cap - 1 - n);
        if (r <= 0) break;
        n += (size_t)r;
        if (n >= cap - 1) break;
    }
    buf[n] = '\0';
    close(p[0]);
    int st = 0;
    if (waitpid(pid, &st, 0) < 0) return -1;
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

static void usage(void) {
    fprintf(stderr,
            "usage: cos-exec --simulate [--dry-run] [--] <command>\n"
            "       cos-exec --self-test\n"
            "\n"
            "  --simulate   run digital-twin preflight (cp SRC DST modeled)\n"
            "  --dry-run    never exec /bin/sh (preflight only)\n"
            "  --           end of flags; rest is the command\n"
            "\n"
            "If σ_twin < %.2f and not --dry-run, runs: /bin/sh -c \"CMD\"\n",
            (double)COS_TWIN_EXECUTE_MAX_SIGMA);
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0)
        return cos_digital_twin_self_test() != 0 ? 1 : 0;

    int sim = 0, dry = 0;
    int i = 1;
    for (; i < argc; i++) {
        if (strcmp(argv[i], "--simulate") == 0) sim = 1;
        else if (strcmp(argv[i], "--dry-run") == 0) dry = 1;
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        } else if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        } else
            break;
    }

    if (!sim) {
        usage();
        return 2;
    }

    char cmd[4096];
    if (i < argc && strchr(argv[i], ' ') == NULL && i + 1 == argc) {
        /* Single argv element: treat as full command string. */
        snprintf(cmd, sizeof cmd, "%s", argv[i]);
    } else if (i < argc) {
        join_argv(cmd, sizeof cmd, argc - i, argv + i);
    } else {
        fprintf(stderr, "cos-exec: missing command\n");
        return 2;
    }

    cos_digital_twin_report_t rep;
    if (cos_digital_twin_simulate(cmd, stdout, &rep) != 0) {
        fprintf(stderr, "cos-exec: empty command\n");
        return 2;
    }

    int safe = rep.sigma_twin < COS_TWIN_EXECUTE_MAX_SIGMA;
    if (safe && !dry)
        fprintf(stdout, "[twin: σ_twin=%.2f | SAFE → executing]\n",
                (double)rep.sigma_twin);
    else if (!safe)
        fprintf(stdout, "[twin: σ_twin=%.2f | UNSAFE → aborted]\n",
                (double)rep.sigma_twin);
    else
        fprintf(stdout, "[twin: σ_twin=%.2f | SAFE (dry-run, not executing)]\n",
                (double)rep.sigma_twin);

    if (!safe || dry)
        return safe ? 0 : 3;

    char out[16384];
    int ex = sh_c_capture(cmd, out, sizeof out);
    if (out[0] != '\0')
        fputs(out, stdout);
    size_t ol = strlen(out);
    if (ol > 0 && out[ol - 1] != '\n')
        fputc('\n', stdout);
    if (ex != 0)
        fprintf(stderr, "cos-exec: /bin/sh exit=%d\n", ex);
    return ex != 0 ? ex : 0;
}
