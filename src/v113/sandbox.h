/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v113 σ-Sandbox — bounded subprocess execution with σ-gated entry.
 *
 * What v113 provides:
 *   - A C API that runs a Python, bash, or sh snippet in a child
 *     process with:
 *       * an absolute wall-clock deadline (SIGKILL on timeout),
 *       * an `RLIMIT_CPU` + `RLIMIT_AS` envelope (soft resource
 *         bounds; hard enforcement is deferred to containers /
 *         Firecracker / Wasmtime in future revisions),
 *       * stdin wired to `/dev/null`, stdout + stderr captured into
 *         caller-provided buffers with truncation-safe semantics,
 *       * a `PATH=/usr/bin:/bin` reset so sandboxed code can't pull
 *         in user-site tooling by accident,
 *       * a minimal env passed through (`LANG`, `HOME` → /tmp).
 *   - An `execute_code` tool definition that v112 can route to,
 *     integrating the σ-gate: if σ_product on the code-generation
 *     step is above τ_code, the server returns a diagnostic
 *     *before* running the code.
 *   - An "execution receipt" JSON shape that records inputs, exit
 *     status, captured streams, wall time, and σ.
 *
 * Non-goals (and honesty):
 *   - This is NOT a hardened sandbox.  We're matching the surface of
 *     LLM-in-Sandbox (Cheng et al. 2025): three meta-capabilities
 *     (execute, edit, submit) plus σ.  For a real security boundary,
 *     run Creation OS inside Firecracker / gVisor / the v108 Docker
 *     image, and point this kernel at an in-container shell.
 *   - We do not attempt seccomp-bpf on macOS (unavailable).  On
 *     Linux, the merge-gate smoke does not assume seccomp either;
 *     hardening is a compile-time option (COS_V113_SECCOMP=1,
 *     deferred).
 */
#ifndef COS_V113_SANDBOX_H
#define COS_V113_SANDBOX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V113_CODE_MAX      65536
#define COS_V113_LANG_MAX      32
#define COS_V113_STREAM_MAX    32768

typedef enum cos_v113_language {
    COS_V113_LANG_UNKNOWN = 0,
    COS_V113_LANG_PYTHON,
    COS_V113_LANG_BASH,
    COS_V113_LANG_SH,
    COS_V113_LANG_TEXT       /* diagnostics only — echoed back */
} cos_v113_language_t;

typedef struct cos_v113_request {
    const char *code;        /* NUL-terminated */
    size_t      code_len;    /* bytes (<= COS_V113_CODE_MAX) */
    cos_v113_language_t language;
    int         timeout_sec; /* 1..120; 30 default */
    /* Optional σ-gate input: if sigma_product_in >= 0, the sandbox
     * refuses to execute when sigma_product_in > tau_code and
     * returns an "uncertain_code" diagnostic instead. */
    float       sigma_product_in;   /* -1.f to disable gate */
    float       tau_code;           /* default 0.60 when unset */
} cos_v113_request_t;

typedef struct cos_v113_result {
    int     executed;           /* 1 iff child actually ran */
    int     gated_out;          /* 1 iff σ_product > τ_code */
    int     timed_out;          /* 1 iff killed by deadline */
    int     exit_code;          /* child exit status (-1 if killed) */
    double  wall_seconds;
    size_t  stdout_len;
    char    stdout_buf[COS_V113_STREAM_MAX];
    size_t  stderr_len;
    char    stderr_buf[COS_V113_STREAM_MAX];
    /* σ-gate echo (propagated into the receipt). */
    float   sigma_product;
    float   tau_code;
    char    gate_reason[96];
} cos_v113_result_t;

/* Defaults for the request struct; call before filling in. */
void cos_v113_request_defaults(cos_v113_request_t *req);

/* Parse the `language` field (case-insensitive) into the enum.
 * Returns COS_V113_LANG_UNKNOWN if unrecognised. */
cos_v113_language_t cos_v113_lang_from_string(const char *s);

const char *cos_v113_lang_name(cos_v113_language_t lang);

/* Run the request.  Returns 0 on completion (including σ-gate
 * abstention — the result struct has the detail), non-zero on
 * fatal OS errors (fork failure, etc.).  `result` is zeroed on
 * entry.  Thread-hostile: uses signals + fork + waitpid.  Callers
 * that want concurrency should run one sandbox per process. */
int cos_v113_sandbox_run(const cos_v113_request_t *req,
                         cos_v113_result_t *result);

/* Serialize the result into an OpenAI-shaped JSON receipt.
 * Shape:
 *   {"executed":bool,"gated_out":bool,"timed_out":bool,
 *    "exit_code":int,"wall_seconds":number,
 *    "stdout":"...","stderr":"...",
 *    "sigma_product":number,"tau_code":number,
 *    "gate_reason":"..."}
 * Returns bytes written, -1 on overflow. */
int cos_v113_result_to_json(const cos_v113_result_t *r,
                            char *out, size_t cap);

/* Parse a POST /v1/sandbox/execute request body.  Accepts:
 *   {"code":"...","language":"python","timeout":30,
 *    "sigma_product":0.42,"tau_code":0.60}
 * Writes the parsed request into `out`; returns 0 on success,
 * -1 on malformed body. */
int cos_v113_parse_request(const char *body, cos_v113_request_t *out);

/* Pure-C smoke: parsers + a short-lived child process (`echo`),
 * verifies capture + deadline math.  Returns 0 iff all-pass. */
int cos_v113_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_V113_SANDBOX_H */
