/*
 * Demo plugin for σ-Plugin smoke tests.
 *
 * Compiled as a shared library (.dylib on macOS, .so on Linux);
 * its single exported symbol is `cos_sigma_plugin_entry`.  The
 * σ-Plugin loader calls that symbol exactly once after dlopen and
 * populates its own registry from the returned struct.
 *
 * This plugin adds two trivial tools (`demo_echo`, `demo_rev`),
 * a custom σ-signal that returns a constant, and a Codex
 * extension string.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "plugin.h"

#include <stdlib.h>
#include <string.h>

static int demo_echo(const char *const *argv,
                     char **out_stdout, char **out_stderr) {
    (void)out_stderr;
    const char *s = (argv && argv[1]) ? argv[1] : "";
    size_t n = strlen(s);
    char *dst = (char *)malloc(n + 2);
    if (!dst) return 1;
    memcpy(dst, s, n);
    dst[n]     = '\n';
    dst[n + 1] = '\0';
    if (out_stdout) *out_stdout = dst;
    else free(dst);
    return 0;
}

static int demo_rev(const char *const *argv,
                    char **out_stdout, char **out_stderr) {
    (void)out_stderr;
    const char *s = (argv && argv[1]) ? argv[1] : "";
    size_t n = strlen(s);
    char *dst = (char *)malloc(n + 1);
    if (!dst) return 1;
    for (size_t i = 0; i < n; i++) dst[i] = s[n - 1 - i];
    dst[n] = '\0';
    if (out_stdout) *out_stdout = dst;
    else free(dst);
    return 0;
}

static float demo_sigma(void *ctx) {
    (void)ctx;
    return 0.137f;  /* sealed constant; the smoke test checks this */
}

__attribute__((visibility("default")))
int cos_sigma_plugin_entry(cos_sigma_plugin_t *out) {
    if (!out) return COS_PLUGIN_ERR_ARG;
    memset(out, 0, sizeof *out);
    out->abi_version     = COS_SIGMA_PLUGIN_ABI;
    out->name            = "demo";
    out->version         = "0.1.0";
    out->author          = "Creation OS";
    out->n_tools         = 2;
    out->tools[0].name   = "demo_echo";
    out->tools[0].doc    = "print argv[1] + newline";
    out->tools[0].risk_level = 0;
    out->tools[0].run    = demo_echo;
    out->tools[1].name   = "demo_rev";
    out->tools[1].doc    = "print argv[1] reversed";
    out->tools[1].risk_level = 0;
    out->tools[1].run    = demo_rev;
    out->custom_sigma    = demo_sigma;
    out->codex_extension = "demo plugin loaded";
    return COS_PLUGIN_OK;
}
