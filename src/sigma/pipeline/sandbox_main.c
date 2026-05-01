/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Sandbox demo + pinned JSON receipt.
 *
 * Four canonical scenarios:
 *   s1  /bin/echo hello                (risk 0)   → exit 0, stdout "hello\n"
 *   s2  /bin/sleep 5, max=300 ms       (risk 3)   → timed_out=1
 *   s3  /bin/rm -rf /, allowlist=sleep (risk 3)   → DISALLOWED
 *   s4  /bin/sleep 1 without consent   (risk 4)   → DISALLOWED
 *
 * Pinned by benchmarks/sigma_pipeline/check_sigma_sandbox.sh.
 */

#include "sandbox.h"

#include <stdio.h>
#include <string.h>

static void print_result(const char *label, int rc,
                         const cos_sandbox_result_t *r) {
    printf("{\"scenario\":\"%s\","
           "\"rc\":%d,"
           "\"status\":%d,"
           "\"exit_code\":%d,"
           "\"killed_signal\":%d,"
           "\"timed_out\":%s,"
           "\"memory_exceeded\":%s,"
           "\"network_isolated\":%s,"
           "\"stdout_len\":%zu,"
           "\"stderr_len\":%zu,"
           "\"stdout_truncated\":%s,"
           "\"stderr_truncated\":%s",
           label, rc,
           r->status,
           r->exit_code, r->killed_signal,
           r->timed_out         ? "true" : "false",
           r->memory_exceeded   ? "true" : "false",
           r->network_isolated  ? "true" : "false",
           r->stdout_len, r->stderr_len,
           r->stdout_truncated  ? "true" : "false",
           r->stderr_truncated  ? "true" : "false");
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int self = cos_sandbox_self_test();

    printf("{\"kernel\":\"sigma_sandbox\",\"self_test_rc\":%d,"
           "\"scenarios\":[",
           self);

    /* --- s1 --------------------------------------------------- */
    {
        const char *A[] = {"/bin/echo", "hello", NULL};
        cos_sandbox_config_t cfg;
        cos_sandbox_config_default(&cfg, COS_SANDBOX_RISK_CALC);
        cos_sandbox_result_t r = {0};
        int rc = cos_sandbox_exec(&cfg, A, &r);
        print_result("risk0_echo", rc, &r);
        printf(",\"stdout_starts\":\"%s\"}",
               (r.stdout_buf && r.stdout_len >= 5 &&
                strncmp(r.stdout_buf, "hello", 5) == 0) ? "hello" : "<other>");
        cos_sandbox_result_free(&r);
    }
    /* --- s2 --------------------------------------------------- */
    {
        const char *A[] = {"/bin/sleep", "5", NULL};
        const char *ALLOW[] = {"sleep", NULL};
        cos_sandbox_config_t cfg;
        cos_sandbox_config_default(&cfg, COS_SANDBOX_RISK_SHELL);
        cfg.allowed_commands = ALLOW;
        cfg.max_runtime_ms   = 300;
        cos_sandbox_result_t r = {0};
        int rc = cos_sandbox_exec(&cfg, A, &r);
        printf(",");
        print_result("risk3_sleep_timeout", rc, &r);
        printf(",\"wall_ms_le\":2000}");
        cos_sandbox_result_free(&r);
    }
    /* --- s3 --------------------------------------------------- */
    {
        const char *A[] = {"/bin/rm", "-rf", "/", NULL};
        const char *ALLOW[] = {"sleep", NULL};
        cos_sandbox_config_t cfg;
        cos_sandbox_config_default(&cfg, COS_SANDBOX_RISK_SHELL);
        cfg.allowed_commands = ALLOW;
        cos_sandbox_result_t r = {0};
        int rc = cos_sandbox_exec(&cfg, A, &r);
        printf(",");
        print_result("risk3_disallowed_rm", rc, &r);
        printf("}");
        cos_sandbox_result_free(&r);
    }
    /* --- s4 --------------------------------------------------- */
    {
        const char *A[] = {"/bin/sleep", "1", NULL};
        const char *ALLOW[] = {"sleep", NULL};
        cos_sandbox_config_t cfg;
        cos_sandbox_config_default(&cfg, COS_SANDBOX_RISK_IRREVERSIBLE);
        cfg.allowed_commands = ALLOW;
        cfg.user_consent     = 0;
        cos_sandbox_result_t r = {0};
        int rc = cos_sandbox_exec(&cfg, A, &r);
        printf(",");
        print_result("risk4_no_consent", rc, &r);
        printf("}");
        cos_sandbox_result_free(&r);
    }

    printf("],\"pass\":%s}\n", self == 0 ? "true" : "false");
    return self == 0 ? 0 : 1;
}
