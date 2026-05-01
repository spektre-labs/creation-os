/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Offline — verified airplane-mode readiness.
 *
 * MODE_LOCAL_ONLY already exists in the pipeline (A5, sovereign).
 * This kernel adds a pre-flight check: before the user boards a
 * plane, opens a secure compartment, or simply wants to assert
 * that no bytes leave the laptop, `cos offline prepare` must be
 * able to answer "yes, everything you need is here".
 *
 * The verifier inspects five resources:
 *
 *   model    : BitNet weights shard present (non-zero file)
 *   engram   : σ-Engram persistence store readable
 *   rag      : σ-RAG index loaded and non-empty
 *   codex    : Atlantean Codex soul-file present
 *   persona  : User persona JSON persisted
 *
 * Each probe is pure: it takes a path and reports status / size
 * / optional 64-bit FNV-1a content hash so the caller can assert
 * byte-stability across runs.  No network call, no DNS, no
 * fork — this is a deterministic filesystem walk.
 *
 * The overall `cos_offline_verify` returns OK iff every enabled
 * probe succeeded.  Disabled probes (`.required = 0`) are reported
 * but do not gate the verdict, so users with a subset of the
 * stack (say, no persona yet) can still pass.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_OFFLINE_H
#define COS_SIGMA_OFFLINE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_OFFLINE_MAX_PROBES  16
#define COS_OFFLINE_PATH_MAX    512
#define COS_OFFLINE_NAME_MAX     32

enum cos_offline_status {
    COS_OFFLINE_OK         =  0,
    COS_OFFLINE_ERR_ARG    = -1,
    COS_OFFLINE_ERR_FULL   = -2,
    /* Per-probe failure codes propagated in cos_offline_probe_result_t */
    COS_OFFLINE_MISSING    = -10,  /* file not present                     */
    COS_OFFLINE_EMPTY      = -11,  /* file empty or below min_bytes        */
    COS_OFFLINE_UNREADABLE = -12,  /* permission / I/O                     */
};

typedef struct {
    char   name[COS_OFFLINE_NAME_MAX];   /* "model", "engram", ...     */
    char   path[COS_OFFLINE_PATH_MAX];   /* absolute or $HOME-relative */
    int    required;                     /* 1 = gates verdict          */
    size_t min_bytes;                    /* minimum acceptable size    */
} cos_offline_probe_t;

typedef struct {
    char    name[COS_OFFLINE_NAME_MAX];
    char    path[COS_OFFLINE_PATH_MAX];
    int     required;
    int     present;     /* 0/1 */
    int     status;      /* COS_OFFLINE_OK or a negative failure code    */
    size_t  size_bytes;
    uint64_t content_hash; /* FNV-1a64 of file contents; 0 if not read   */
} cos_offline_probe_result_t;

typedef struct {
    cos_offline_probe_t        probes[COS_OFFLINE_MAX_PROBES];
    int                        n;
} cos_offline_plan_t;

typedef struct {
    cos_offline_probe_result_t results[COS_OFFLINE_MAX_PROBES];
    int                        n;
    int                        n_ok;           /* passed probes    */
    int                        n_required_ok;  /* gating passed    */
    int                        n_required;     /* gating total     */
    int                        verdict;        /* 1 = ready, 0 = not */
} cos_offline_report_t;

/* -------- Plan helpers ----------------------------------------- */

/* Zero the plan. */
void cos_offline_plan_init(cos_offline_plan_t *plan);

/* Append a probe.  Returns COS_OFFLINE_OK or ERR_FULL. */
int cos_offline_plan_add(cos_offline_plan_t *plan,
                         const char *name,
                         const char *path,
                         int required,
                         size_t min_bytes);

/* Populate a plan with the default five-probe layout rooted at
 * `base` (typically $HOME/.cos).  If `base` is NULL, paths fall
 * back to repo-relative defaults used by the demo and tests. */
int cos_offline_plan_default(cos_offline_plan_t *plan, const char *base);

/* -------- Execution ------------------------------------------- */

int cos_offline_verify(const cos_offline_plan_t *plan,
                       cos_offline_report_t     *out);

/* -------- Reporting ------------------------------------------- */

int cos_offline_report_json(const cos_offline_report_t *rep,
                            char *dst, size_t cap);

const char *cos_offline_status_str(int status);

/* -------- Self-test ------------------------------------------ */
int cos_offline_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_OFFLINE_H */
