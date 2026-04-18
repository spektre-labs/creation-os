/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v151 σ-Code-Agent — implementation.  See codeagent.h.
 */
#include "codeagent.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

/* Used to compose σ_code for the report — kept here as a single
 * knob so the CLI + merge-gate stay in sync. */
#define COS_V151_SIGMA_OK   0.10f
#define COS_V151_SIGMA_BAD  0.90f

static void safe_copy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static int ensure_dir(const char *p) {
    if (mkdir(p, 0755) == 0) return 0;
    if (errno == EEXIST) return 0;
    return -1;
}

/* Run a shell command, capture tail of combined output into `log`.
 * Returns WEXITSTATUS or −1 on spawn failure. */
static int run_cmd(const char *cmd, FILE *log) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        if (log) fputs(line, log);
    }
    int rc = pclose(fp);
    if (rc == -1) return -1;
    if (WIFEXITED(rc)) return WEXITSTATUS(rc);
    return 127;
}

static float geomean3(float a, float b, float c) {
    if (a < 1e-6f) a = 1e-6f;
    if (b < 1e-6f) b = 1e-6f;
    if (c < 1e-6f) c = 1e-6f;
    double g = exp((log((double)a) + log((double)b)
                  + log((double)c)) / 3.0);
    return (float)g;
}

/* ---- lifecycle --------------------------------------------- */
void cos_v151_init(cos_v151_agent_t *a, uint64_t seed) {
    if (!a) return;
    memset(a, 0, sizeof(*a));
    cos_v146_init(&a->genesis);
    a->prng = seed ? seed : 0x15100001ULL;
}

void cos_v151_resume(cos_v151_agent_t *a) {
    if (!a) return;
    a->paused = 0;
    a->consecutive_rejects = 0;
    cos_v146_resume(&a->genesis);
}

/* ---- TDD cycle --------------------------------------------- */
int cos_v151_run_tdd_cycle(cos_v151_agent_t *a,
                           int version,
                           const char *name,
                           const char *gap,
                           const char *workroot,
                           float tau_code,
                           cos_v151_tdd_report_t *rep) {
    if (!a || !name || !gap || !workroot || !rep) return -1;
    if (a->paused) return -2;
    memset(rep, 0, sizeof(*rep));
    if (tau_code <= 0.0f) tau_code = 0.35f;
    rep->tau_code = tau_code;
    rep->version  = version;
    safe_copy(rep->name, sizeof(rep->name), name);
    safe_copy(rep->gap,  sizeof(rep->gap),  gap);

    /* Create <workroot>.  v146_write_candidate appends its own
     * /v<version>/ subdir underneath. */
    if (ensure_dir(workroot) != 0) return -1;
    char base[COS_V151_PATH_MAX];
    snprintf(base, sizeof(base), "%s/v%d", workroot, version);
    safe_copy(rep->workdir, sizeof(rep->workdir), base);

    /* Step 1: call v146 to emit and write the skeleton in place. */
    char skeleton[8];
    int cand_idx = cos_v146_generate(&a->genesis, version,
                                     name, gap,
                                     a->prng ^ 0xC0DE151AULL,
                                     /* τ_merge is v146's own
                                      * σ-gate; use a lenient
                                      * value here so we only
                                      * σ-gate on the composite
                                      * σ_code after Phase C. */
                                     0.99f,
                                     skeleton, sizeof(skeleton));
    if (cand_idx < 0) return -1;
    rep->candidate_idx = cand_idx;
    const cos_v146_candidate_t *c = &a->genesis.candidates[cand_idx];
    rep->sigma_code_base = c->sigma_code;
    safe_copy(rep->name, sizeof(rep->name), c->name);
    safe_copy(rep->gap,  sizeof(rep->gap),  c->gap);

    if (cos_v146_write_candidate(&a->genesis, cand_idx, workroot) != 0)
        return -1;

    /* Open the per-cycle log. */
    snprintf(rep->log, sizeof(rep->log), "%s/tdd.log", base);
    FILE *log = fopen(rep->log, "w");
    if (!log) return -1;
    fprintf(log, "v151 σ-code-agent TDD log for v%d %s\n",
            version, name);

    /* Resolve CC. */
    const char *cc = getenv("CC");
    if (!cc || !cc[0]) cc = "cc";

    /* Step 2 — Phase A: compile tests/test.c ALONE (no kernel.c).
     * Test should FAIL to link because it calls cos_vN_*_init /
     * cos_vN_*_self_test which live in kernel.c. */
    snprintf(rep->bin_preimpl, sizeof(rep->bin_preimpl),
             "%s/preimpl", base);
    char cmd_a[1536];
    snprintf(cmd_a, sizeof(cmd_a),
        "%s -O0 -std=c11 -I%s -o %s %s/tests/test.c 2>&1",
        cc, base, rep->bin_preimpl, base);
    fprintf(log, "\n# Phase A cmd: %s\n", cmd_a);
    int rc_a = run_cmd(cmd_a, log);
    fprintf(log, "# Phase A exit: %d\n", rc_a);
    /* We EXPECT failure (linker unresolved).  rc_a != 0 == good. */
    rep->phase_a_fail_as_expected = (rc_a != 0) ? 1 : 0;
    rep->sigma_a = rep->phase_a_fail_as_expected
                   ? COS_V151_SIGMA_OK : COS_V151_SIGMA_BAD;

    /* Step 3 — Phase B: compile kernel.c + tests/test.c together. */
    snprintf(rep->bin_full, sizeof(rep->bin_full),
             "%s/full", base);
    char cmd_b[2048];
    snprintf(cmd_b, sizeof(cmd_b),
        "%s -O0 -Wall -std=c11 -I%s -o %s %s/kernel.c %s/tests/test.c 2>&1",
        cc, base, rep->bin_full, base, base);
    fprintf(log, "\n# Phase B cmd: %s\n", cmd_b);
    int rc_b = run_cmd(cmd_b, log);
    fprintf(log, "# Phase B exit: %d\n", rc_b);
    rep->phase_b_compile_ok = (rc_b == 0) ? 1 : 0;
    rep->sigma_b = rep->phase_b_compile_ok
                   ? COS_V151_SIGMA_OK : COS_V151_SIGMA_BAD;

    /* Step 4 — Phase C: run the Phase-B binary. */
    int rc_c = 127;
    if (rep->phase_b_compile_ok) {
        char cmd_c[COS_V151_PATH_MAX + 8];
        snprintf(cmd_c, sizeof(cmd_c), "%s", rep->bin_full);
        fprintf(log, "\n# Phase C cmd: %s\n", cmd_c);
        rc_c = run_cmd(cmd_c, log);
        fprintf(log, "# Phase C exit: %d\n", rc_c);
    } else {
        fprintf(log, "\n# Phase C skipped (Phase B failed)\n");
    }
    rep->phase_c_test_pass = (rc_c == 0) ? 1 : 0;
    rep->sigma_c = rep->phase_c_test_pass
                   ? COS_V151_SIGMA_OK : COS_V151_SIGMA_BAD;

    /* Step 5 — composite σ + σ-gate. */
    rep->sigma_code_composite =
        geomean3(rep->sigma_a, rep->sigma_b, rep->sigma_c);
    fprintf(log, "\n# σ_A=%.3f  σ_B=%.3f  σ_C=%.3f  σ_code=%.3f  τ=%.3f\n",
            rep->sigma_a, rep->sigma_b, rep->sigma_c,
            rep->sigma_code_composite, rep->tau_code);
    fclose(log);

    if (rep->sigma_code_composite <= rep->tau_code) {
        rep->status = COS_V151_STATUS_PENDING;
    } else {
        rep->status = COS_V151_STATUS_GATED_OUT;
        a->n_gated_out++;
    }
    a->n_cycles++;
    return 0;
}

/* ---- HITL ---------------------------------------------------- */
int cos_v151_approve(cos_v151_agent_t *a,
                     cos_v151_tdd_report_t *rep) {
    if (!a || !rep) return -1;
    if (a->paused) return -2;
    if (rep->status != COS_V151_STATUS_PENDING) return -3;
    int rc = cos_v146_approve(&a->genesis, rep->candidate_idx);
    if (rc != 0) return rc;
    rep->status = COS_V151_STATUS_APPROVED;
    a->n_approved++;
    a->consecutive_rejects = 0;
    return 0;
}

int cos_v151_reject(cos_v151_agent_t *a,
                    cos_v151_tdd_report_t *rep) {
    if (!a || !rep) return -1;
    if (rep->status != COS_V151_STATUS_PENDING) return -3;
    int rc = cos_v146_reject(&a->genesis, rep->candidate_idx);
    if (rc != 0 && rc != -2) return rc;  /* -2 if already gated  */
    rep->status = COS_V151_STATUS_REJECTED;
    a->n_rejected++;
    a->consecutive_rejects++;
    if (a->consecutive_rejects >= 3) a->paused = 1;
    return 0;
}

/* ---- JSON ---------------------------------------------------- */
static int append(char **p, char *end, const char *fmt, ...) {
    if (*p >= end) return -1;
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(*p, (size_t)(end - *p), fmt, ap);
    va_end(ap);
    if (n < 0) return -1;
    if (n >= (int)(end - *p)) { *p = end; return -1; }
    *p += n;
    return 0;
}

static const char *status_str(cos_v151_status_t s) {
    switch (s) {
        case COS_V151_STATUS_PENDING:   return "pending";
        case COS_V151_STATUS_APPROVED:  return "approved";
        case COS_V151_STATUS_REJECTED:  return "rejected";
        case COS_V151_STATUS_GATED_OUT: return "gated_out";
    }
    return "unknown";
}

int cos_v151_report_to_json(const cos_v151_tdd_report_t *r,
                            char *buf, size_t cap) {
    if (!r || !buf || cap < 64) return -1;
    char *p = buf, *end = buf + cap;
    if (append(&p, end,
        "{\"name\":\"%s\",\"gap\":\"%s\",\"version\":%d,"
        "\"candidate\":%d,\"phase_a_fail_ok\":%s,"
        "\"phase_b_compile_ok\":%s,\"phase_c_test_pass\":%s,"
        "\"sigma_a\":%.3f,\"sigma_b\":%.3f,\"sigma_c\":%.3f,"
        "\"sigma_code_base\":%.3f,\"sigma_code_composite\":%.3f,"
        "\"tau_code\":%.3f,\"status\":\"%s\",\"workdir\":\"%s\","
        "\"log\":\"%s\"}",
        r->name, r->gap, r->version, r->candidate_idx,
        r->phase_a_fail_as_expected ? "true" : "false",
        r->phase_b_compile_ok       ? "true" : "false",
        r->phase_c_test_pass        ? "true" : "false",
        r->sigma_a, r->sigma_b, r->sigma_c,
        r->sigma_code_base, r->sigma_code_composite,
        r->tau_code, status_str(r->status),
        r->workdir, r->log) < 0) return -1;
    return (int)(p - buf);
}

int cos_v151_agent_to_json(const cos_v151_agent_t *a,
                           char *buf, size_t cap) {
    if (!a || !buf || cap < 32) return -1;
    char *p = buf, *end = buf + cap;
    if (append(&p, end,
        "{\"n_cycles\":%d,\"n_approved\":%d,\"n_rejected\":%d,"
        "\"n_gated_out\":%d,\"consecutive_rejects\":%d,\"paused\":%s}",
        a->n_cycles, a->n_approved, a->n_rejected, a->n_gated_out,
        a->consecutive_rejects, a->paused ? "true" : "false") < 0) return -1;
    return (int)(p - buf);
}

/* ---- Self-test (skips compile paths unless $CC is runnable) - */
int cos_v151_self_test(void) {
    /* Structural contracts we can check without spawning cc:     */
    cos_v151_agent_t a;
    cos_v151_init(&a, 0x1510);
    if (a.paused) return 1;
    if (a.n_cycles) return 2;

    /* Drive the agent through three synthetic rejects via v146
     * directly (we can reject v146 candidates without running a
     * real TDD cycle) to prove the pause-latch wiring.            */
    for (int i = 0; i < 3; ++i) {
        char buf[8];
        int idx = cos_v146_generate(&a.genesis, 200 + i,
                                    "probe", "synthetic gap",
                                    (uint64_t)(0xAA00 + i),
                                    0.99f, buf, sizeof(buf));
        if (idx < 0) return 3;
        cos_v151_tdd_report_t fake = {0};
        fake.candidate_idx = idx;
        fake.status = COS_V151_STATUS_PENDING;
        if (cos_v151_reject(&a, &fake) != 0) return 4;
    }
    if (!a.paused) return 5;
    cos_v151_resume(&a);
    if (a.paused) return 6;

    /* JSON round-trip. */
    char buf[512];
    cos_v151_tdd_report_t rep = {0};
    rep.version = 300;
    safe_copy(rep.name, sizeof(rep.name), "example");
    safe_copy(rep.gap,  sizeof(rep.gap),  "example gap");
    rep.sigma_code_composite = 0.18f;
    rep.tau_code = 0.35f;
    rep.status = COS_V151_STATUS_PENDING;
    if (cos_v151_report_to_json(&rep, buf, sizeof(buf)) <= 0) return 7;
    if (cos_v151_agent_to_json (&a,  buf, sizeof(buf)) <= 0) return 8;
    return 0;
}
