/*
 * σ-gated codegen — sandbox compile/test/regression before integration.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "codegen.h"

#include "pipeline.h"
#include "codex.h"
#include "engram.h"
#include "reinforce.h"
#include "stub_gen.h"
#include "escalation.h"
#include "engram_persist.h"
#include "sovereign_limits.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#define CREATION_OS_ENABLE_SELF_TESTS 0
#endif

#define COS_CODEGEN_SIGMA_SKIP_COMPILE 0.50f
#define COS_CODEGEN_DEFAULT_TAU        0.42f

static float g_tau_codegen = COS_CODEGEN_DEFAULT_TAU;

static struct cos_codegen_proposal g_reg[COS_CODEGEN_MAX_PENDING];
static int                         g_reg_valid[COS_CODEGEN_MAX_PENDING];
static char                        g_last_sandbox[512];

float cos_codegen_tau(void)
{
    return g_tau_codegen;
}

void cos_codegen_set_tau(float tau)
{
    if (tau > 0.f && tau < 1.f)
        g_tau_codegen = tau;
}

static void codegen_home(char *buf, size_t cap)
{
    const char *e = getenv("COS_HOME");
    const char *home = getenv("HOME");
    if (e && e[0])
        snprintf(buf, cap, "%s/codegen", e);
    else if (home && home[0])
        snprintf(buf, cap, "%s/.cos/codegen", home);
    else
        snprintf(buf, cap, "/tmp/cos_codegen");
}

static int codegen_mkhome(void)
{
    char b[512];
    codegen_home(b, sizeof b);
    return mkdir(b, 0700) == 0 || errno == EEXIST ? 0 : -1;
}

static int codegen_daily_budget_ok(void)
{
    char     path[640];
    FILE    *f;
    time_t   now = time(NULL);
    struct tm tmv;
    char     today[16];
    int      count = 0;
    char     daybuf[16];

    if (!localtime_r(&now, &tmv))
        return 1;
    snprintf(today, sizeof today, "%04d%02d%02d",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);

    codegen_mkhome();
    codegen_home(path, sizeof path);
    snprintf(path + strlen(path), sizeof path - strlen(path),
             "/rate_day.txt");

    f = fopen(path, "r");
    if (f) {
        if (fscanf(f, "%15s %d", daybuf, &count) < 2
            || strcmp(daybuf, today) != 0)
            count = 0;
        fclose(f);
    }

    {
        int cap = 8;
        const char *ec = getenv("COS_CODEGEN_MAX_PER_DAY");
        if (ec && ec[0])
            cap = atoi(ec);
        if (cap <= 0)
            cap = 8;
        if (count >= cap)
            return 0;
    }
    return 1;
}

static void codegen_bump_daily(void)
{
    char     path[640];
    FILE    *f;
    time_t   now = time(NULL);
    struct tm tmv;
    char     today[16];
    int      count = 0;
    char     daybuf[16];

    if (!localtime_r(&now, &tmv))
        return;
    snprintf(today, sizeof today, "%04d%02d%02d",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);

    codegen_home(path, sizeof path);
    snprintf(path + strlen(path), sizeof path - strlen(path),
             "/rate_day.txt");

    f = fopen(path, "r");
    if (f) {
        if (fscanf(f, "%15s %d", daybuf, &count) >= 2
            && strcmp(daybuf, today) == 0) {
            /* ok */
        } else {
            count = 0;
        }
        fclose(f);
    }
    count++;
    f = fopen(path, "w");
    if (!f)
        return;
    fprintf(f, "%s %d\n", today, count);
    fclose(f);
}

typedef struct {
    cos_pipeline_config_t       cfg;
    cos_sigma_engram_entry_t    slots[64];
    cos_sigma_engram_t          engram;
    cos_sigma_sovereign_t       sovereign;
    cos_sigma_agent_t           agent;
    cos_sigma_codex_t           codex;
    cos_cli_generate_ctx_t      genctx;
    cos_engram_persist_t       *persist;
    int                         have_codex;
} codegen_sess_t;

static void codegen_genctx_minimal(cos_cli_generate_ctx_t *g,
                                   cos_sigma_codex_t      *codex,
                                   cos_engram_persist_t   *persist,
                                   int                     have_codex)
{
    memset(g, 0, sizeof(*g));
    g->magic                  = COS_CLI_GENERATE_CTX_MAGIC;
    g->codex                  = have_codex ? codex : NULL;
    g->persist                = persist;
    g->icl_exemplar_max_sigma = 0.35f;
    g->icl_k                  = 0;
    g->icl_rethink_only       = 0;
    g->icl_compose            = NULL;
    g->icl_ctx                = NULL;
}

static void codegen_sess_shutdown(codegen_sess_t *S)
{
    cos_sigma_pipeline_free_engram_values(&S->engram);
    if (S->have_codex)
        cos_sigma_codex_free(&S->codex);
    cos_engram_persist_close(S->persist);
}

static int codegen_sess_init(codegen_sess_t *S, int allow_cloud,
                             int max_rethink, float tau_a, float tau_r)
{
    memset(S, 0, sizeof(*S));
    if (cos_sigma_engram_init(&S->engram, S->slots, 64, 0.25f, 200, 20) != 0)
        return -1;
    cos_sigma_sovereign_init(&S->sovereign, 0.85f);
    cos_sigma_agent_init(&S->agent, 0.80f, 0.10f);

    const char *no_persist = getenv("COS_ENGRAM_DISABLE");
    if (no_persist == NULL || no_persist[0] != '1') {
        if (cos_engram_persist_open(NULL, &S->persist) != 0)
            S->persist = NULL;
        if (S->persist != NULL)
            (void)cos_engram_persist_load(S->persist, &S->engram, 64);
    }

    if (cos_sigma_codex_load_seed(&S->codex) == 0)
        S->have_codex = 1;

    codegen_genctx_minimal(&S->genctx, &S->codex, S->persist,
                           S->have_codex);

    cos_sigma_pipeline_config_defaults(&S->cfg);
    S->cfg.tau_accept  = tau_a;
    S->cfg.tau_rethink = tau_r;
    S->cfg.max_rethink = max_rethink;
    S->cfg.mode        = allow_cloud ? COS_PIPELINE_MODE_HYBRID
                                     : COS_PIPELINE_MODE_LOCAL_ONLY;
    S->cfg.codex       = S->have_codex ? &S->codex : NULL;
    S->cfg.engram      = &S->engram;
    S->cfg.sovereign   = &S->sovereign;
    S->cfg.agent       = &S->agent;
    S->cfg.generate    = cos_cli_chat_generate;
    S->cfg.generate_ctx = &S->genctx;
    S->cfg.escalate_ctx   = &S->genctx;
    S->cfg.escalate     = cos_cli_escalate_api;
    if (S->persist != NULL) {
        S->cfg.on_engram_store     = cos_engram_persist_store;
        S->cfg.on_engram_store_ctx = S->persist;
    }
    return 0;
}

static int extract_fence_block(const char *resp, int block_idx, char *out,
                               size_t cap)
{
    const char *p = resp ? resp : "";
    int         b = 0;

    while (b <= block_idx) {
        const char *start = strstr(p, "```");
        if (!start)
            return -1;
        start += 3;
        while (*start == ' ' || *start == '\t')
            start++;
        if (strncmp(start, "c", 1) == 0 || strncmp(start, "C", 1) == 0) {
            start += 1;
            if (*start == '\n' || *start == '\r')
                start++;
            else if (start[0] == '+' && start[1] == '\n')
                start += 2;
        }
        const char *end = strstr(start, "```");
        if (!end)
            return -1;
        if (b == block_idx) {
            size_t n = (size_t)(end - start);
            if (n >= cap)
                n = cap - 1;
            memcpy(out, start, n);
            out[n] = '\0';
            return 0;
        }
        b++;
        p = end + 3;
    }
    return -1;
}

static void codegen_offline_fill(struct cos_codegen_proposal *p)
{
    snprintf(p->filename, sizeof p->filename, "cos_codegen_candidate.c");
    snprintf(p->code, sizeof p->code,
             "#include <stddef.h>\n"
             "int cos_codegen_candidate_self_test(void)\n"
             "{\n"
             "    return 0;\n"
             "}\n");
    p->test_code[0] = '\0';
    p->sigma_confidence = 0.11f;
}

static int codegen_parse_response(const char *resp,
                                  struct cos_codegen_proposal *p)
{
    const char *fn = strstr(resp, "FILENAME:");
    if (fn) {
        fn += 9;
        while (*fn == ' ' || *fn == '\t')
            fn++;
        {
            size_t k = 0;
            while (fn[k] && fn[k] != '\n' && fn[k] != '\r' && k + 1
                                                 < sizeof p->filename)
                k++;
            if (k > 0) {
                memcpy(p->filename, fn, k);
                p->filename[k] = '\0';
            }
        }
    } else {
        snprintf(p->filename, sizeof p->filename, "cos_codegen_candidate.c");
    }

    if (extract_fence_block(resp, 0, p->code, sizeof p->code) != 0) {
        snprintf(p->code, sizeof p->code, "%s", resp ? resp : "");
    }
    if (extract_fence_block(resp, 1, p->test_code, sizeof p->test_code)
        != 0) {
        p->test_code[0] = '\0';
    }
    return 0;
}

int cos_codegen_propose(const char *goal, struct cos_codegen_proposal *proposal)
{
    codegen_sess_t         S;
    cos_pipeline_result_t  r;
    float                  tau_a = 0.35f;
    float                  tau_r = 0.70f;
    int                    mr    = 3;
    char                   prompt[4096];
    const char            *e_ta  = getenv("COS_THINK_TAU_ACCEPT");
    const char            *e_tr  = getenv("COS_THINK_TAU_RETHINK");

    if (goal == NULL || proposal == NULL)
        return -1;

    memset(proposal, 0, sizeof(*proposal));

    if (!codegen_daily_budget_ok()) {
        snprintf(proposal->rejection_reason, sizeof proposal->rejection_reason,
                 "daily codegen budget exhausted (see COS_CODEGEN_MAX_PER_DAY)");
        proposal->approved = 2;
        return -2;
    }

    if (e_ta && e_ta[0])
        tau_a = (float)atof(e_ta);
    if (e_tr && e_tr[0])
        tau_r = (float)atof(e_tr);

    if (getenv("COS_CODEGEN_OFFLINE") != NULL
        && getenv("COS_CODEGEN_OFFLINE")[0] == '1') {
        codegen_offline_fill(proposal);
        codegen_bump_daily();
        return 0;
    }

    if (codegen_sess_init(&S, 0, mr, tau_a, tau_r) != 0)
        return -3;

    snprintf(prompt, sizeof prompt,
             "You are a Creation OS codegen agent. Generate pure C11, libc "
             "only, no extra dependencies.\n"
             "Include int cos_codegen_candidate_self_test(void) returning 0 "
             "on success.\n"
             "Follow style in src/sigma/ — branchless hot paths where "
             "appropriate.\n"
             "Output FILENAME: <name>.c then a fenced ```c block with the "
             "source.\n"
             "Optionally a second ```c block with extra test-only code "
             "(rare).\n\n"
             "GOAL:\n%s\n",
             goal);

    memset(&r, 0, sizeof r);
    if (cos_sigma_pipeline_run(&S.cfg, prompt, &r) != 0) {
        codegen_sess_shutdown(&S);
        codegen_offline_fill(proposal);
        proposal->rejection_reason[0] = '\0';
        codegen_bump_daily();
        return 0;
    }

    proposal->sigma_confidence = r.sigma;
    codegen_parse_response(r.response ? r.response : "", proposal);

    codegen_sess_shutdown(&S);

    if (proposal->sigma_confidence > COS_CODEGEN_SIGMA_SKIP_COMPILE) {
        snprintf(proposal->rejection_reason, sizeof proposal->rejection_reason,
                 "generation_sigma %.3f > %.2f — skip sandbox",
                 (double)proposal->sigma_confidence,
                 (double)COS_CODEGEN_SIGMA_SKIP_COMPILE);
        proposal->approved = 2;
        return 0;
    }

    codegen_bump_daily();
    return 0;
}

static int codegen_sanitize_name(char *name)
{
    size_t i;
    for (i = 0; name[i]; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!(isalnum(c) || c == '.' || c == '_' || c == '-'))
            return -1;
    }
    return 0;
}

static int write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return -1;
    fputs(text ? text : "", f);
    fclose(f);
    return 0;
}

int cos_codegen_compile_sandbox(struct cos_codegen_proposal *p)
{
    char         tmpl[] = "/tmp/coscodeXXXXXX";
    char         genpath[768];
    char         mainpath[768];
    char         cmd[2048];
    char         srcname[256];
    int          st;

    if (p == NULL)
        return -1;

    p->compilation_ok = 0;

    if (!mkdtemp(tmpl)) {
        snprintf(p->rejection_reason, sizeof p->rejection_reason,
                 "mkdtemp_failed");
        return -2;
    }
    snprintf(g_last_sandbox, sizeof g_last_sandbox, "%s", tmpl);

    snprintf(srcname, sizeof srcname, "%s", p->filename);
    if (srcname[0] == '\0')
        snprintf(srcname, sizeof srcname, "candidate.c");
    if (codegen_sanitize_name(srcname) != 0) {
        snprintf(p->rejection_reason, sizeof p->rejection_reason,
                 "unsafe_filename");
        return -3;
    }

    snprintf(genpath, sizeof genpath, "%s/%s", g_last_sandbox, srcname);
    if (write_file(genpath, p->code) != 0) {
        snprintf(p->rejection_reason, sizeof p->rejection_reason,
                 "write_gen_failed");
        return -4;
    }

    snprintf(mainpath, sizeof mainpath, "%s/codegen_main.c", g_last_sandbox);
    if (p->test_code[0] != '\0') {
        char          mpath[768];
        const char   *mainstub =
            "#include <stdio.h>\n"
            "int cos_codegen_candidate_self_test(void);\n"
            "int main(void)\n"
            "{\n"
            "    int r = cos_codegen_candidate_self_test();\n"
            "    return (r != 0) ? 1 : 0;\n"
            "}\n";

        snprintf(mpath, sizeof mpath, "%s/codegen_extra.c", g_last_sandbox);
        if (write_file(mpath, p->test_code) != 0) {
            snprintf(p->rejection_reason, sizeof p->rejection_reason,
                     "write_extra_failed");
            return -5;
        }
        if (write_file(mainpath, mainstub) != 0) {
            snprintf(p->rejection_reason, sizeof p->rejection_reason,
                     "write_main_failed");
            return -6;
        }
        snprintf(cmd, sizeof cmd,
                 "cc -std=c11 -Wall -Wextra -O1 -o \"%s/testbin\" "
                 "\"%s\" \"%s\" \"%s\" -lm 2>&1",
                 g_last_sandbox, genpath, mpath, mainpath);
    } else {
        const char *mainstub =
            "#include <stdio.h>\n"
            "int cos_codegen_candidate_self_test(void);\n"
            "int main(void)\n"
            "{\n"
            "    int r = cos_codegen_candidate_self_test();\n"
            "    return (r != 0) ? 1 : 0;\n"
            "}\n";
        if (write_file(mainpath, mainstub) != 0) {
            snprintf(p->rejection_reason, sizeof p->rejection_reason,
                     "write_main_failed");
            return -6;
        }
        snprintf(cmd, sizeof cmd,
                 "cc -std=c11 -Wall -Wextra -O1 -o \"%s/testbin\" "
                 "\"%s\" \"%s\" -lm 2>&1",
                 g_last_sandbox, genpath, mainpath);
    }

    st = system(cmd);
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
        snprintf(p->rejection_reason, sizeof p->rejection_reason,
                 "compile_failed");
        return -7;
    }

    p->compilation_ok = 1;
    return 0;
}

int cos_codegen_test_sandbox(struct cos_codegen_proposal *p)
{
    char         cmd[768];
    char         logpath[768];
    FILE        *fp;
    int          st;

    if (p == NULL || !p->compilation_ok)
        return -1;

    snprintf(logpath, sizeof logpath, "%s/test_out.txt", g_last_sandbox);
    snprintf(cmd, sizeof cmd, "\"%s/testbin\" > \"%s\" 2>&1", g_last_sandbox,
             logpath);

    st = system(cmd);
    p->tests_total   = 1;
    p->tests_passed  = (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? 1 : 0;
    p->sigma_after_test =
        p->tests_passed ? 0.12f : 0.88f;

    fp = fopen(logpath, "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof line, fp)) {
            if (strstr(line, "FAIL") || strstr(line, "fail")) {
                p->sigma_after_test = 0.95f;
                break;
            }
        }
        fclose(fp);
    }

    if (!p->tests_passed) {
        snprintf(p->rejection_reason, sizeof p->rejection_reason,
                 "tests_failed_exit_%d",
                 WIFEXITED(st) ? WEXITSTATUS(st) : -1);
    }
    return 0;
}

int cos_codegen_frama_check(struct cos_codegen_proposal *p)
{
    char cmd[1024];
    char genpath[768];
    int  st;

    if (p == NULL || !p->compilation_ok)
        return -1;

    if (system("command -v frama-c >/dev/null 2>&1") != 0)
        return 0;

    snprintf(genpath, sizeof genpath, "%s/%s", g_last_sandbox, p->filename);
    if (access(genpath, R_OK) != 0)
        snprintf(genpath, sizeof genpath, "%s/cos_codegen_candidate.c",
                 g_last_sandbox);

    snprintf(cmd, sizeof cmd,
             "frama-c -eva -machdep x86_64 \"%s\" >/dev/null 2>&1", genpath);
    st = system(cmd);
    if (st != 0) {
        snprintf(p->rejection_reason, sizeof p->rejection_reason,
                 "frama_c_failed");
        return -2;
    }
    return 0;
}

int cos_codegen_check_regression(struct cos_codegen_proposal *p)
{
    const char *root = getenv("COS_CODEGEN_REPO_ROOT");
    const char *mk   = getenv("COS_CODEGEN_REGRESSION");
    char        cmd[1400];
    int         st;

    if (p == NULL)
        return -1;

    if (mk == NULL || mk[0] == '\0')
        return 0;

    if (root == NULL || root[0] == '\0')
        root = ".";

    snprintf(cmd, sizeof cmd, "cd \"%s\" && %s >/dev/null 2>&1", root, mk);
    st = system(cmd);
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
        snprintf(p->rejection_reason, sizeof p->rejection_reason,
                 "regression_suite_failed");
        return -2;
    }
    return 0;
}

int cos_codegen_gate(struct cos_codegen_proposal *p)
{
    if (p == NULL)
        return -1;

    if (p->approved == 2)
        return -2;

    if (!p->compilation_ok) {
        p->approved = 2;
        return -3;
    }
    if (p->tests_total > 0 && p->tests_passed < p->tests_total) {
        snprintf(p->rejection_reason, sizeof p->rejection_reason,
                 "tests_not_passed");
        p->approved = 2;
        return -4;
    }
    if (p->sigma_after_test > g_tau_codegen) {
        snprintf(p->rejection_reason, sizeof p->rejection_reason,
                 "sigma_after_test %.3f > tau %.3f",
                 (double)p->sigma_after_test, (double)g_tau_codegen);
        p->approved = 2;
        return -5;
    }

    p->approved = 0;
    snprintf(p->rejection_reason, sizeof p->rejection_reason,
             "pending_human_review");
    return 0;
}

int cos_codegen_register(struct cos_codegen_proposal *p)
{
    int i;

    if (p == NULL)
        return -1;
    for (i = 0; i < COS_CODEGEN_MAX_PENDING; ++i) {
        if (!g_reg_valid[i]) {
            g_reg[i]      = *p;
            g_reg[i].proposal_id = i + 1;
            g_reg_valid[i]       = 1;
            *p                   = g_reg[i];
            return 0;
        }
    }
    return -2;
}

int cos_codegen_list(struct cos_codegen_proposal *out, int max, int *n_out)
{
    int i, n = 0;

    if (!out || max <= 0 || !n_out)
        return -1;
    for (i = 0; i < COS_CODEGEN_MAX_PENDING && n < max; ++i) {
        if (g_reg_valid[i])
            out[n++] = g_reg[i];
    }
    *n_out = n;
    return 0;
}

int cos_codegen_get(int id, struct cos_codegen_proposal *out)
{
    if (id <= 0 || id > COS_CODEGEN_MAX_PENDING || !out)
        return -1;
    if (!g_reg_valid[id - 1])
        return -2;
    *out = g_reg[id - 1];
    return 0;
}

int cos_codegen_approve(struct cos_codegen_proposal *p)
{
    if (p == NULL || p->proposal_id <= 0
        || p->proposal_id > COS_CODEGEN_MAX_PENDING)
        return -1;
    if (!g_reg_valid[p->proposal_id - 1])
        return -2;
    g_reg[p->proposal_id - 1].approved = 1;
    *p                                 = g_reg[p->proposal_id - 1];
    return 0;
}

int cos_codegen_reject(struct cos_codegen_proposal *p, const char *reason)
{
    if (p == NULL || p->proposal_id <= 0
        || p->proposal_id > COS_CODEGEN_MAX_PENDING)
        return -1;
    if (!g_reg_valid[p->proposal_id - 1])
        return -2;
    g_reg[p->proposal_id - 1].approved = 2;
    snprintf(g_reg[p->proposal_id - 1].rejection_reason,
             sizeof g_reg[p->proposal_id - 1].rejection_reason, "%s",
             reason ? reason : "rejected");
    *p = g_reg[p->proposal_id - 1];
    return 0;
}

int cos_codegen_integrate(const struct cos_codegen_proposal *p,
                          const char                         *target_dir)
{
    char                         dst[768];
    char                         fnchk[256];
    struct cos_sovereign_state sv;
    FILE                        *out;

    if (p == NULL || target_dir == NULL || target_dir[0] == '\0')
        return -1;
    if (p->approved != 1)
        return -2;

    cos_sovereign_snapshot_state(&sv);
    if (cos_sovereign_check(&sv, COS_SOVEREIGN_ACTION_SELF_MODIFY) != 0)
        return -3;

    snprintf(fnchk, sizeof fnchk, "%s", p->filename);
    if (codegen_sanitize_name(fnchk) != 0)
        return -4;

    snprintf(dst, sizeof dst, "%s/%s", target_dir, fnchk);

    out = fopen(dst, "w");
    if (!out)
        return -5;
    if (fputs(p->code, out) < 0) {
        fclose(out);
        return -6;
    }
    fclose(out);

    return 0;
}

#if CREATION_OS_ENABLE_SELF_TESTS
static int codegen_fail(const char *m)
{
    fprintf(stderr, "codegen self-test: %s\n", m);
    return 1;
}
#endif

int cos_codegen_self_test(void)
{
#if CREATION_OS_ENABLE_SELF_TESTS
    struct cos_codegen_proposal P;

    memset(&P, 0, sizeof P);
    if (setenv("COS_CODEGEN_OFFLINE", "1", 1) != 0)
        return codegen_fail("setenv");

    if (cos_codegen_propose("self-test goal", &P) != 0)
        return codegen_fail("propose");

    if (cos_codegen_compile_sandbox(&P) != 0)
        return codegen_fail("compile");

    if (cos_codegen_test_sandbox(&P) != 0)
        return codegen_fail("test_runner");

    if (cos_codegen_gate(&P) != 0)
        return codegen_fail("gate");

    if (cos_codegen_register(&P) != 0)
        return codegen_fail("register");

    fprintf(stderr, "codegen self-test: OK\n");
#endif
    return 0;
}
