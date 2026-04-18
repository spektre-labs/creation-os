/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v151 σ-Code-Agent — self-writing TDD loop.
 *
 * v146 emits kernel skeletons.  v151 turns the emit into a
 * test-driven-development cycle that actually invokes the system
 * compiler on the generated C and executes the generated test
 * binary before reporting a σ-gated PR candidate:
 *
 *   Phase A — compile `tests/test.c` ALONE (no kernel.c).  The
 *             test references cos_vN_*_init / cos_vN_*_self_test
 *             symbols from kernel.c, so link MUST fail.  A Phase-
 *             A success would mean the test is trivial (does not
 *             exercise the impl) — the agent flags that as a
 *             σ_code penalty.
 *
 *   Phase B — compile kernel.c + tests/test.c together.  The
 *             v146-emitted skeleton is known good, so link must
 *             succeed.
 *
 *   Phase C — execute the Phase-B binary.  Self-test runs + exit
 *             code 0 is required for PASS.
 *
 *   σ_code_composite = geomean of per-phase σ contributions:
 *     σ_A = 0.10 (expected fail) or 0.90 (trivial test pass)
 *     σ_B = 0.10 (compile ok)    or 0.90 (compile broken)
 *     σ_C = 0.10 (run exit 0)    or 0.90 (non-zero exit)
 *
 *   σ-gate: σ_code_composite ≤ τ_code ⇒ PENDING (PR candidate);
 *           else GATED_OUT (agent refuses to open a PR).
 *
 * Human-in-the-loop: approve / reject routes through v146, and
 * three consecutive rejects latch paused=true.  v151.1 wires real
 * ASAN/UBSAN compile flags, LCOV coverage ≥ 80 %, and git branch
 * emission for the PR.  v151.0 is pure C11 + system()/popen().
 */
#ifndef COS_V151_CODE_AGENT_H
#define COS_V151_CODE_AGENT_H

#include <stddef.h>
#include <stdint.h>

#include "genesis.h"        /* v146 */

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V151_PATH_MAX   512
#define COS_V151_NAME_MAX   64
#define COS_V151_GAP_MAX    128

typedef enum {
    COS_V151_STATUS_PENDING = 0,
    COS_V151_STATUS_APPROVED,
    COS_V151_STATUS_REJECTED,
    COS_V151_STATUS_GATED_OUT
} cos_v151_status_t;

typedef struct cos_v151_tdd_report {
    char  name   [COS_V151_NAME_MAX];
    char  gap    [COS_V151_GAP_MAX];
    int   version;
    int   candidate_idx;

    float sigma_code_base;        /* from v146 emit                */
    /* per-phase signals — 1 = expected outcome observed          */
    int   phase_a_fail_as_expected;
    int   phase_b_compile_ok;
    int   phase_c_test_pass;

    /* σ contributions in [0, 1] (lower is better).               */
    float sigma_a;
    float sigma_b;
    float sigma_c;
    float sigma_code_composite;   /* geomean of σ_A, σ_B, σ_C    */
    float tau_code;

    cos_v151_status_t status;

    char  workdir[COS_V151_PATH_MAX];
    char  bin_preimpl[COS_V151_PATH_MAX];
    char  bin_full   [COS_V151_PATH_MAX];
    char  log        [COS_V151_PATH_MAX];
} cos_v151_tdd_report_t;

typedef struct cos_v151_agent {
    cos_v146_state_t genesis;     /* shared v146 state           */
    int    n_cycles;
    int    n_approved;
    int    n_rejected;
    int    n_gated_out;
    int    consecutive_rejects;
    int    paused;
    uint64_t prng;
} cos_v151_agent_t;

void  cos_v151_init(cos_v151_agent_t *a, uint64_t seed);

/* One full TDD cycle: v146 emit + write, Phase A/B/C, σ-gate.
 * `workroot` must be an existing directory; a subdir
 * `<workroot>/v<version>/` is created and populated.  cc is
 * taken from the environment variable "CC" if set, else "cc".
 * Returns 0 on success (report populated), −1 on any error. */
int   cos_v151_run_tdd_cycle(cos_v151_agent_t *a,
                             int version,
                             const char *name,
                             const char *gap,
                             const char *workroot,
                             float tau_code,
                             cos_v151_tdd_report_t *rep);

/* HITL controls.  Approve/reject uses the report's candidate_idx
 * against the embedded v146 state.  Three consecutive rejects
 * latch paused=true. */
int   cos_v151_approve(cos_v151_agent_t *a,
                       cos_v151_tdd_report_t *rep);
int   cos_v151_reject (cos_v151_agent_t *a,
                       cos_v151_tdd_report_t *rep);
void  cos_v151_resume (cos_v151_agent_t *a);

int   cos_v151_report_to_json(const cos_v151_tdd_report_t *r,
                              char *buf, size_t cap);
int   cos_v151_agent_to_json (const cos_v151_agent_t *a,
                              char *buf, size_t cap);

int   cos_v151_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
