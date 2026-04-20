/*
 * σ-Sandbox — risk-level isolated tool execution.
 *
 * A1 (agent / tool calling) needs to run shell commands the model
 * decided on.  In production that must happen inside a defined
 * envelope so a confused — or hostile — decision cannot harm the
 * host.  σ-Sandbox is that envelope.
 *
 * Risk lattice (coupled to P18 agent autonomy)
 * --------------------------------------------
 *   risk 0  (calculator, pure compute)
 *     - no chroot, no rlimit, no netns; fastest path
 *   risk 1  (read-only filesystem — stat / cat)
 *     - chdir to sandbox root, cpu + memory caps, no network
 *   risk 2  (write to sandbox; no host writes)
 *     - chdir + file-size cap + cpu + memory + no network
 *   risk 3  (arbitrary shell)
 *     - chdir + cpu + memory + file-size + wall-clock + netns
 *     - allowlist required (the command verb must match)
 *   risk 4  (irreversible, e.g. rm / git push)
 *     - risk 3 plus the caller must have pre-obtained explicit
 *       user consent; σ-Sandbox only records the consent receipt.
 *
 * What the sandbox enforces *here*
 * --------------------------------
 *   - Allowlist: the command verb (argv[0]) is looked up in
 *     cfg->allowed_commands; if absent → ERR_DISALLOWED.
 *   - fork + execvp with the caller's argv.
 *   - RLIMIT_CPU    (cfg->max_cpu_ms / 1000 rounded up)
 *   - RLIMIT_AS     (cfg->max_memory_mb × 1 MiB)
 *   - RLIMIT_FSIZE  (cfg->max_file_write_mb × 1 MiB)
 *   - Wall-clock timeout cfg->max_runtime_ms — the parent polls
 *     the child and SIGKILLs past the deadline; the result field
 *     timed_out is set.
 *   - chdir(cfg->chroot_path) if non-NULL (chroot(2) if euid == 0;
 *     otherwise best-effort chdir so host paths are still relative
 *     from the sandbox root).
 *   - Network: on Linux, unshare(CLONE_NEWNET) if
 *     cfg->network_allowed == 0.  On other POSIX hosts the
 *     allowlist + lack of DNS in the sandbox dir is the only
 *     enforcement; the report.network_isolated field records
 *     whether true kernel isolation was applied.
 *
 * All enforcement is best-effort; any syscall failure still
 * yields a well-formed result (no partial output lost), and the
 * status code makes clear what could not be enforced.
 *
 * σ-coupling
 * ----------
 *   sigma_sandbox_exec does not measure σ itself; it returns
 *   deterministic facts that σ-agent (A1) consumes:
 *     * timed_out, memory_exceeded, exit_code
 *     * stdout / stderr captured
 *     * wall_ms actually spent
 *     * network_isolated flag
 *   σ-agent then feeds those into σ_tool_risk and decides whether
 *   to keep, retry, or escalate.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SANDBOX_H
#define COS_SIGMA_SANDBOX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum cos_sandbox_status {
    COS_SANDBOX_OK            =  0,
    COS_SANDBOX_ERR_ARG       = -1,
    COS_SANDBOX_ERR_DISALLOWED= -2,
    COS_SANDBOX_ERR_FORK      = -3,
    COS_SANDBOX_ERR_EXEC      = -4,
    COS_SANDBOX_ERR_PIPE      = -5,
    COS_SANDBOX_ERR_OOM       = -6,
};

enum cos_sandbox_risk {
    COS_SANDBOX_RISK_CALC        = 0,
    COS_SANDBOX_RISK_READ        = 1,
    COS_SANDBOX_RISK_WRITE       = 2,
    COS_SANDBOX_RISK_SHELL       = 3,
    COS_SANDBOX_RISK_IRREVERSIBLE= 4,
};

typedef struct {
    int          risk_level;          /* 0..4, see enum             */
    const char  *chroot_path;         /* NULL = no chdir/chroot     */
    int          network_allowed;     /* 0 => attempt netns isolate */
    int          max_runtime_ms;      /* wall-clock cap             */
    int          max_cpu_ms;          /* CPU time cap               */
    int          max_memory_mb;       /* RLIMIT_AS cap (MiB)        */
    int          max_file_write_mb;   /* RLIMIT_FSIZE cap (MiB)     */
    size_t       max_stdout_bytes;    /* capture cap for stdout     */
    size_t       max_stderr_bytes;    /* capture cap for stderr     */
    const char *const *allowed_commands; /* NULL-terminated         */
    int          user_consent;        /* required for risk 4        */
} cos_sandbox_config_t;

typedef struct {
    int    status;              /* COS_SANDBOX_OK or negative       */
    int    exit_code;           /* 0..255; -1 if signaled           */
    int    killed_signal;       /* signal that terminated, if any   */
    int    timed_out;            /* wall or CPU deadline hit        */
    int    memory_exceeded;      /* child OOM'd (SIGKILL+ENOMEM)    */
    int    network_isolated;     /* kernel-enforced netns applied   */
    int    stdout_truncated;
    int    stderr_truncated;
    uint64_t wall_ms;            /* actual wall time spent          */
    char  *stdout_buf;           /* NUL-terminated; owned by caller */
    char  *stderr_buf;           /* NUL-terminated; owned by caller */
    size_t stdout_len;
    size_t stderr_len;
} cos_sandbox_result_t;

/* -------- Config helpers ------------------------------------------ */

/* Populate `cfg` with sane defaults for the given risk level.  The
 * allowed_commands list is left NULL and must be set by the caller
 * for risk ≥ 3. */
void cos_sandbox_config_default(cos_sandbox_config_t *cfg, int risk_level);

/* Check whether argv[0] is in cfg->allowed_commands (by basename
 * match).  Returns 1 if allowed, 0 otherwise.  When
 * allowed_commands == NULL, risk 0..2 return 1 (no allowlist for
 * the low-risk tiers), risk 3..4 return 0. */
int  cos_sandbox_allowed(const cos_sandbox_config_t *cfg,
                         const char *const *argv);

/* -------- Execution ----------------------------------------------- */

/* Run `argv` under `cfg`.  `argv` is a NULL-terminated array in the
 * execvp sense; argv[0] is the verb.  Writes captured output and
 * status into `out`; caller frees `out->stdout_buf` and
 * `out->stderr_buf` with free(). */
int cos_sandbox_exec(const cos_sandbox_config_t *cfg,
                     const char *const *argv,
                     cos_sandbox_result_t *out);

/* -------- Cleanup -------------------------------------------------- */
void cos_sandbox_result_free(cos_sandbox_result_t *r);

/* -------- Self-test ----------------------------------------------- */
int cos_sandbox_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_SANDBOX_H */
