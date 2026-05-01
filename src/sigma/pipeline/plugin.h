/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Plugin — extensibility ABI for Creation OS.
 *
 * The plugin architecture lets a third party extend σ-pipeline in
 * three orthogonal ways without recompiling the core:
 *
 *   (1) Tools             — new entries for the A1 agent to call
 *   (2) Substrate         — new (digital / bitnet / spike /
 *                            photonic / …) backend under the
 *                            substrate vtable
 *   (3) σ-signal          — a caller-supplied function that maps
 *                            model state to an extra σ component,
 *                            merged into σ_product by the caller
 *   (4) Codex extension   — a text blob appended to the Codex
 *                            system prompt at load time
 *
 * A plugin is delivered as a shared object (.so / .dylib on POSIX,
 * .dll on Windows) that exports *exactly one* symbol:
 *
 *     int cos_sigma_plugin_entry(cos_sigma_plugin_t *out)
 *
 * The entry populates `out` (strings may point into the plugin's
 * read-only data), returns COS_PLUGIN_OK on success, and is
 * invoked exactly once per dlopen.
 *
 * The host (this module) owns the registry: the loaded plugins,
 * their load order, and a canonical JSON dump used by
 * `cos plugin list`.  Plugins may also be registered *statically*
 * via cos_sigma_plugin_register(), which is the path used by
 * self-tests and any built-in plugin baked into the binary.
 *
 * Safety stance
 * -------------
 *   * A plugin is trust-equivalent to the host process; it should
 *     be treated as first-party code once installed.
 *   * The loader refuses to load a plugin whose reported ABI
 *     version mismatches COS_SIGMA_PLUGIN_ABI.
 *   * Unload is best-effort: we dlclose in LIFO order, but if any
 *     still-live tool / substrate holds references, the unload
 *     fails and the plugin stays loaded (with a status reported).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_PLUGIN_H
#define COS_SIGMA_PLUGIN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_SIGMA_PLUGIN_ABI 1

#ifndef COS_PLUGIN_REGISTRY_CAP
#define COS_PLUGIN_REGISTRY_CAP 32
#endif

#ifndef COS_PLUGIN_MAX_TOOLS
#define COS_PLUGIN_MAX_TOOLS    16
#endif

enum cos_plugin_status {
    COS_PLUGIN_OK           =  0,
    COS_PLUGIN_ERR_ARG      = -1,
    COS_PLUGIN_ERR_DLOPEN   = -2,
    COS_PLUGIN_ERR_DLSYM    = -3,
    COS_PLUGIN_ERR_ABI      = -4,
    COS_PLUGIN_ERR_FULL     = -5,
    COS_PLUGIN_ERR_DUP      = -6,
    COS_PLUGIN_ERR_NOTFOUND = -7,
};

/* Caller-supplied callback type for a plugin tool.  argv is
 * NULL-terminated; the tool fills `out_stdout` (caller frees with
 * free()) and returns an exit-code convention (0 = ok). */
typedef int (*cos_plugin_tool_fn)(const char *const *argv,
                                  char **out_stdout,
                                  char **out_stderr);

typedef struct {
    const char        *name;           /* short verb, unique     */
    const char        *doc;            /* one-liner help         */
    int                risk_level;     /* σ-sandbox risk to use  */
    cos_plugin_tool_fn run;            /* invocation             */
} cos_plugin_tool_t;

/* Substrate hook — opaque; the substrate vtable is defined by the
 * existing substrate.h.  We only carry the pointer through. */
typedef struct cos_sigma_substrate *cos_plugin_substrate_ref_t;

/* σ-signal: maps an opaque host context to one σ component. */
typedef float (*cos_plugin_sigma_fn)(void *host_ctx);

typedef struct cos_sigma_plugin {
    int          abi_version;     /* must == COS_SIGMA_PLUGIN_ABI */
    const char  *name;             /* unique name                 */
    const char  *version;          /* semver string               */
    const char  *author;           /* optional                    */

    /* Tools: up to COS_PLUGIN_MAX_TOOLS; `n_tools` is authoritative */
    cos_plugin_tool_t tools[COS_PLUGIN_MAX_TOOLS];
    int               n_tools;

    cos_plugin_substrate_ref_t substrate;   /* may be NULL         */
    cos_plugin_sigma_fn        custom_sigma;/* may be NULL         */
    const char                *codex_extension; /* may be NULL     */

    /* Filled by the host; plugins must not touch. */
    void        *_lib_handle;
    int          _static;          /* 1 iff registered without dlopen */
} cos_sigma_plugin_t;

typedef int (*cos_sigma_plugin_entry_fn)(cos_sigma_plugin_t *out);

/* -------- Registry ------------------------------------------------- */

/* Zero-initialise the internal registry.  Safe to call multiple
 * times (returns OK without doing anything if already clean). */
void cos_sigma_plugin_registry_reset(void);

/* Statically register an already-populated plugin structure.  Used
 * by self-tests and by any plugin compiled into the host binary
 * (no dlopen / shared-library infrastructure needed). */
int cos_sigma_plugin_register(const cos_sigma_plugin_t *plugin);

/* Load a plugin from a shared-library path, dlopen'ing it and
 * calling cos_sigma_plugin_entry.  On success the plugin appears
 * in the registry in load order. */
int cos_sigma_plugin_load(const char *path);

/* Remove a plugin by name; best-effort dlclose if not static. */
int cos_sigma_plugin_remove(const char *name);

/* Introspection --------------------------------------------------- */

/* Number of plugins currently registered (static + dlopen'd). */
int cos_sigma_plugin_count(void);

/* Read-only view of the idx-th plugin in load order; NULL on OOB. */
const cos_sigma_plugin_t *cos_sigma_plugin_at(int idx);

/* Emit a canonical JSON listing into `out` (NUL-terminated).
 * Returns the number of bytes written (excluding the terminator);
 * returns -1 if out_cap is too small. */
int cos_sigma_plugin_list_json(char *out, size_t out_cap);

/* Self-test.  Exercises registry bounds, duplicate rejection, and
 * happy-path static registration.  dlopen is NOT exercised here;
 * the binary ./creation_os_sigma_plugin dlopen's a sibling demo
 * and the smoke test verifies that path. */
int cos_sigma_plugin_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_PLUGIN_H */
