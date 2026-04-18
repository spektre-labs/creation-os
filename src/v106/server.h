/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v106 σ-Server — OpenAI-compatible HTTP layer over the v101 σ-bridge.
 *
 * Surfaces:
 *   POST /v1/chat/completions   (SSE streaming supported; non-stream JSON)
 *   POST /v1/completions        (text completion, same inference path)
 *   GET  /v1/models             (lists loaded GGUF files by model id)
 *   GET  /v1/sigma-profile      (last σ-channel profile + aggregator)
 *   GET  /health                (plain "ok\n" for orchestrators)
 *   GET  /                      (serves web/index.html if present; v108)
 *
 * The server is single-threaded on purpose.  The v101 bridge (and the
 * llama_context it wraps) is not thread-safe, and one-prompt-at-a-time
 * is the right surface for "run a local LLM on my laptop".  Multi-
 * tenant hosting is deliberately out of scope; for that, put a
 * reverse proxy in front of N server processes on N ports.
 *
 * Config surface is documented in `docs/v106/THE_SIGMA_SERVER.md`.
 * TL;DR:
 *     ~/.creation-os/config.toml
 *     [server]    host = "127.0.0.1"  port = 8080
 *     [sigma]     tau_abstain = 0.7   aggregator = "product"
 *     [model]     gguf_path   = "..."  n_ctx = 2048
 */
#ifndef COS_V106_SERVER_H
#define COS_V106_SERVER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Server-wide configuration.  All fields have sensible defaults in
 * `cos_v106_config_defaults`.  The config file loader in
 * `config.c` populates this struct from `~/.creation-os/config.toml`
 * (or a path passed via --config); CLI flags on top of that win over
 * the file; env vars (where applicable) win over CLI.
 */
typedef struct cos_v106_config {
    /* [server] */
    char    host[64];          /* default "127.0.0.1" */
    int     port;              /* default 8080        */

    /* [sigma] */
    double  tau_abstain;       /* default 0.7 on the aggregator scale.
                                * Abstention fires if sigma > tau. */
    char    aggregator[16];    /* "product" (default) or "mean" */

    /* [model] */
    char    gguf_path[1024];   /* path to .gguf — if empty, /v1/completions
                                * returns 503 "model not loaded". */
    int     n_ctx;             /* default 2048 */
    char    model_id[128];     /* user-visible model id in /v1/models;
                                * default = basename of gguf_path */

    /* [web] */
    char    web_root[1024];    /* default "web"; used to serve "/" */
} cos_v106_config_t;

/* Populate config with defaults.  Never fails. */
void cos_v106_config_defaults(cos_v106_config_t *cfg);

/* Load TOML-ish config from `path`.  Keys not present in the file
 * leave their current (default) values untouched.  Returns 0 on
 * success, -1 if the file cannot be opened, -2 on parse error.
 */
int  cos_v106_config_load(cos_v106_config_t *cfg, const char *path);

/* Default config path: "$HOME/.creation-os/config.toml".  Writes the
 * resolved path into `out` (cap must be >= 1024).  Returns 0 on
 * success, -1 if HOME is unset or the path would overflow. */
int  cos_v106_default_config_path(char *out, size_t cap);

/* Ensure the directory containing `path` exists.  Returns 0 on
 * success, -1 on stat/mkdir error. */
int  cos_v106_ensure_parent_dir(const char *path);

/* --- small JSON helpers (shared with server.c / openai.c) -------- */

/* Extract the raw value of a top-level string key from `body` into
 * `out` (null-terminated, cap >= 2).  Handles basic `\"`, `\\`,
 * `\n`, `\t` escapes; otherwise copies bytes verbatim.  Returns
 * the number of bytes written (excluding NUL), or -1 if not found.
 * Intentionally quadratic/naive — payload size is bounded by
 * Content-Length and we aim for zero deps, not fastest parser. */
int  cos_v106_json_get_string(const char *body, const char *key,
                              char *out, size_t cap);

/* Like _get_string but returns an integer; returns `fallback` if the
 * key is missing or unparsable. */
long cos_v106_json_get_int(const char *body, const char *key, long fallback);

/* Like _get_string but returns a double; returns `fallback` otherwise. */
double cos_v106_json_get_double(const char *body, const char *key, double fallback);

/* Returns 1 if `body` contains `"stream": true` (ignoring spaces),
 * else 0.  Matches only the literal boolean, not "true" inside a
 * string value. */
int  cos_v106_json_stream_flag(const char *body);

/* Extract the content of the LAST role:"user" message from a chat
 * completions request body.  Returns bytes written, or -1. */
int  cos_v106_json_extract_last_user(const char *body, char *out, size_t cap);

/* --- last σ cache ----------------------------------------------- */

/* Mutex-free single-writer single-reader snapshot of the σ state
 * observed on the most recent inference call.  The server updates it
 * after each call and /v1/sigma-profile reads it.  Monotonically
 * increasing `generation` lets a client detect torn reads (rare in
 * a single-threaded server, but free to protect against). */
typedef struct cos_v106_sigma_snapshot {
    uint64_t generation;
    float    sigma;             /* default aggregator output */
    char     aggregator[16];
    float    sigma_mean;        /* arithmetic mean, backward compat */
    float    sigma_product;     /* geometric mean, v105 default */
    float    sigma_max_token;
    float    sigma_profile[8];
    int      abstained;         /* 1 iff last call tripped τ */
    double   tau_abstain;       /* τ that was in force */
    char     model_id[128];
} cos_v106_sigma_snapshot_t;

/* Entry point.  Loads config, opens the bridge (or runs in stub mode
 * if gguf_path is empty / not found), binds to host:port, loops.
 * Returns 0 on clean shutdown, non-zero on bind failure / fatal
 * error.  Prints a one-line banner to stderr at startup. */
int cos_v106_server_run(const cos_v106_config_t *cfg);

#ifdef __cplusplus
}
#endif
#endif /* COS_V106_SERVER_H */
