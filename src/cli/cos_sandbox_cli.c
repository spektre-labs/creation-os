/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * cos-sandbox — σ-Sandbox front-door CLI (HORIZON-5).
 *
 *   cos-sandbox --self-test
 *   cos-sandbox --risk 0 -- /bin/echo hello
 *   cos-sandbox --risk 3 --max-runtime-ms 300 --allow sleep -- /bin/sleep 2
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "../sigma/pipeline/sandbox.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COS_SB_MAX_ALLOW 16
#define COS_SB_MAX_EXEC  64

static void usage(void) {
    fprintf(stderr,
            "usage: cos-sandbox --self-test\n"
            "       cos-sandbox --risk N [--max-runtime-ms MS] [--allow a,b]\n"
            "                     [--consent] [--chroot PATH] -- <argv0> [args...]\n"
            "\n"
            "  risk 0..2: no --allow required.  risk 3..4: --allow required (verbs).\n"
            "  risk 4: pass --consent after explicit user approval.\n"
            "  See src/sigma/pipeline/sandbox.h for the risk lattice.\n");
}

static int parse_allow_list(const char *csv, char (*store)[64],
                            const char *ptrs[COS_SB_MAX_ALLOW + 1]) {
    if (!csv || !csv[0]) return -1;
    char buf[512];
    if (snprintf(buf, sizeof buf, "%s", csv) >= (int)sizeof buf) return -1;
    int n = 0;
    char *save = NULL;
    for (char *t = strtok_r(buf, ",", &save); t != NULL;
         t = strtok_r(NULL, ",", &save)) {
        while (*t == ' ' || *t == '\t') t++;
        if (*t == '\0') continue;
        if (n >= COS_SB_MAX_ALLOW) return -1;
        snprintf(store[n], 64, "%s", t);
        ptrs[n] = store[n];
        n++;
    }
    ptrs[n] = NULL;
    return n > 0 ? 0 : -1;
}

static void print_run(const cos_sandbox_config_t *cfg, const char **argv,
                      int rc, const cos_sandbox_result_t *r) {
    fprintf(stdout, "[sandbox: risk=%d", cfg->risk_level);
    if (argv && argv[0]) fprintf(stdout, " cmd=%s", argv[0]);
    fprintf(stdout, "]\n");
    fprintf(stdout,
            "rc=%d status=%d exit_code=%d killed_signal=%d timed_out=%d "
            "memory_exceeded=%d network_isolated=%d wall_ms=%llu\n",
            rc, r->status, r->exit_code, r->killed_signal, r->timed_out,
            r->memory_exceeded, r->network_isolated,
            (unsigned long long)r->wall_ms);
    if (r->stdout_len)
        fprintf(stdout, "--- stdout ---\n%s", r->stdout_buf ? r->stdout_buf : "");
    if (r->stderr_len)
        fprintf(stdout, "--- stderr ---\n%s", r->stderr_buf ? r->stderr_buf : "");
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0)
        return cos_sandbox_self_test() != 0 ? 1 : 0;

    int risk = -1;
    int max_runtime_ms = -1;
    char allow_csv[512];
    allow_csv[0] = '\0';
    int consent = 0;
    const char *chroot_path = NULL;
    int dash = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            dash = i;
            break;
        }
        if (strcmp(argv[i], "--risk") == 0 && i + 1 < argc) {
            risk = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--max-runtime-ms") == 0 && i + 1 < argc) {
            max_runtime_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--allow") == 0 && i + 1 < argc) {
            snprintf(allow_csv, sizeof allow_csv, "%s", argv[++i]);
        } else if (strcmp(argv[i], "--consent") == 0) {
            consent = 1;
        } else if (strcmp(argv[i], "--chroot") == 0 && i + 1 < argc) {
            chroot_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        } else {
            fprintf(stderr, "cos-sandbox: unknown flag %s\n", argv[i]);
            usage();
            return 2;
        }
    }

    if (dash < 0 || dash + 1 >= argc || !argv[dash + 1]) {
        usage();
        return 2;
    }
    if (risk < 0 || risk > 4) {
        fprintf(stderr, "cos-sandbox: --risk 0..4 required\n");
        return 2;
    }

    cos_sandbox_config_t cfg;
    cos_sandbox_config_default(&cfg, risk);
    if (max_runtime_ms >= 0) cfg.max_runtime_ms = max_runtime_ms;
    cfg.chroot_path   = chroot_path;
    cfg.user_consent  = consent ? 1 : 0;

    char allow_store[COS_SB_MAX_ALLOW][64];
    const char *allow_ptrs[COS_SB_MAX_ALLOW + 1];
    memset(allow_store, 0, sizeof allow_store);
    memset(allow_ptrs, 0, sizeof allow_ptrs);

    if (risk >= COS_SANDBOX_RISK_SHELL) {
        if (allow_csv[0] == '\0' || parse_allow_list(allow_csv, allow_store, allow_ptrs) != 0) {
            fprintf(stderr,
                    "cos-sandbox: --allow verb1,verb2 required for risk >= %d\n",
                    COS_SANDBOX_RISK_SHELL);
            return 2;
        }
        cfg.allowed_commands = allow_ptrs;
    }

    const char *execv[COS_SB_MAX_EXEC + 1];
    int ne = 0;
    for (int j = dash + 1; j < argc && ne < COS_SB_MAX_EXEC; j++)
        execv[ne++] = argv[j];
    execv[ne] = NULL;

    cos_sandbox_result_t r = {0};
    int rc = cos_sandbox_exec(&cfg, execv, &r);
    int child_exit = r.exit_code;
    int st = r.status;
    print_run(&cfg, execv, rc, &r);
    cos_sandbox_result_free(&r);

    fprintf(stdout, "\n  assert(declared == realized);\n  1 = 1.\n");
    return (rc == COS_SANDBOX_OK && st == COS_SANDBOX_OK && child_exit == 0)
               ? 0
               : 1;
}
