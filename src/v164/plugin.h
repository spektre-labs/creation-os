/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v164 σ-Plugin — third-party extension host.
 *
 * Creation OS is modular (v162 compose).  v164 opens the
 * modularity to external extenders via a stable C ABI and a
 * sandboxed executor.
 *
 * Plugin contract (C ABI, stable across v164.x):
 *
 *     int  cos_plugin_init(cos_plugin_ctx_t *ctx);
 *     int  cos_plugin_process(cos_plugin_ctx_t *ctx,
 *                             const cos_plugin_input_t *in,
 *                             cos_plugin_output_t      *out);
 *     void cos_plugin_cleanup(cos_plugin_ctx_t *ctx);
 *
 * A manifest declares provides / requires / sigma_impact.  The
 * host loads the manifest, enforces the declared σ-budget, runs
 * the plugin inside a bounded sandbox (v0: in-process timeout
 * counter + deny-list of syscalls modeled by a capability bit
 * vector; v1: fork + seccomp-bpf), and returns either a
 * modified response or a σ-override.
 *
 * v164.0 (this file) ships:
 *   - 4 officially-provided baked plugins: web-search,
 *     calculator, file-reader, git.  Each has a deterministic
 *     simulated `process` that emits a response + a σ_plugin
 *     confidence score.
 *   - a registry with per-plugin σ_reputation (ring-buffered
 *     from the last N invocations' σ_plugin values).
 *   - sandbox with: timeout_ms, memory_mb, capability bitmap
 *     (NETWORK, FILE_READ, FILE_WRITE, SUBPROCESS, MODEL_WEIGHTS).
 *     An invocation that requests a capability the manifest
 *     does not declare is refused (and σ_plugin = 1.0).
 *   - CLI: install (simulated), list, disable, run.
 *
 * v164.1 (named, not shipped):
 *   - real dlopen("plugin.dylib") + symbol lookup
 *   - real fork + seccomp-bpf on Linux, sandbox_init on macOS
 *   - real `cos plugin install github.com/...` via git clone +
 *     cc -shared build
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V164_PLUGIN_H
#define COS_V164_PLUGIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V164_MAX_PLUGINS      16
#define COS_V164_MAX_NAME         32
#define COS_V164_MAX_MSG          192
#define COS_V164_MAX_CAPS          8
#define COS_V164_MAX_HISTORY      16

typedef enum {
    COS_V164_CAP_NETWORK         = 1 << 0,
    COS_V164_CAP_FILE_READ       = 1 << 1,
    COS_V164_CAP_FILE_WRITE      = 1 << 2,
    COS_V164_CAP_SUBPROCESS      = 1 << 3,
    COS_V164_CAP_MODEL_WEIGHTS   = 1 << 4,  /* always refused */
    COS_V164_CAP_MEMORY_READ     = 1 << 5,  /* v115 read-only */
} cos_v164_cap_t;

typedef struct {
    char     name[COS_V164_MAX_NAME];
    char     version[COS_V164_MAX_NAME];
    char     entry[COS_V164_MAX_NAME];       /* "libweather.dylib" */
    char     provides[COS_V164_MAX_NAME];    /* "tool.weather_forecast" */
    uint32_t required_caps;                  /* OR of COS_V164_CAP_* */
    int      timeout_ms;                     /* sandbox hard stop */
    int      memory_mb;                      /* sandbox memory ceiling */
    float    sigma_impact;                   /* declared σ impact */
    bool     official;                       /* shipped by spektre-labs */
    bool     enabled;
} cos_v164_manifest_t;

typedef struct {
    char     prompt[COS_V164_MAX_MSG];
    float    sigma_entropy;
    float    sigma_n_effective;
    float    sigma_coherence;
    uint32_t granted_caps;                   /* caps the host grants */
} cos_v164_input_t;

typedef enum {
    COS_V164_OK                  = 0,
    COS_V164_TIMED_OUT           = 1,
    COS_V164_CAP_REFUSED         = 2,
    COS_V164_CRASHED             = 3,
    COS_V164_DISABLED            = 4,
    COS_V164_UNKNOWN_PLUGIN      = 5,
} cos_v164_status_t;

typedef struct {
    cos_v164_status_t status;
    char              response[COS_V164_MAX_MSG];
    float             sigma_plugin;          /* plugin's self-reported σ */
    bool              sigma_override;        /* plugin forced an abstain */
    int               runtime_ms;            /* simulated wall clock */
    char              note[COS_V164_MAX_MSG];
} cos_v164_output_t;

typedef struct {
    cos_v164_manifest_t manifest;
    /* ring buffer of the last N σ_plugin observations */
    int                 history_len;
    float               history[COS_V164_MAX_HISTORY];
    float               sigma_reputation;    /* geomean of history */
    int                 n_invocations;
    int                 n_failures;
} cos_v164_entry_t;

typedef struct {
    int              n_plugins;
    cos_v164_entry_t plugins[COS_V164_MAX_PLUGINS];
    uint64_t         seed;
} cos_v164_registry_t;

/* Register the 4 official plugins + set initial σ_reputation
 * baselines. */
void cos_v164_registry_init(cos_v164_registry_t *r, uint64_t seed);

/* Load a new plugin from a baked manifest (simulated "install"). */
int  cos_v164_registry_install(cos_v164_registry_t *r,
                               const cos_v164_manifest_t *m);

/* Enable / disable by name.  Returns 0 on success. */
int  cos_v164_registry_set_enabled(cos_v164_registry_t *r,
                                   const char *name, bool on);

/* Look up by name. */
cos_v164_entry_t *cos_v164_registry_get(cos_v164_registry_t *r,
                                        const char *name);

/* Execute a plugin under the sandbox.  Updates σ_reputation
 * from the invocation's σ_plugin.  Refuses to execute if the
 * plugin is disabled, unknown, or if granted_caps ⊂
 * required_caps (missing cap). */
cos_v164_output_t cos_v164_registry_run(cos_v164_registry_t *r,
                                        const char *name,
                                        const cos_v164_input_t *in);

/* JSON serializer. */
size_t cos_v164_registry_to_json(const cos_v164_registry_t *r,
                                 char *buf, size_t cap);

const char *cos_v164_status_name(cos_v164_status_t s);

int cos_v164_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V164_PLUGIN_H */
