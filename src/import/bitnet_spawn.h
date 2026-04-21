/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Spawn an external inference engine and capture stdout (POSIX).
 *
 * Default argv protocol (override with env vars below):
 *   argv = { exe, "-p", prompt, NULL }
 *
 * When CREATION_OS_BITNET_MODEL is set (llama-cli / BitNet gguf path), the
 * child argv becomes approximately:
 *   { exe, "--no-perf", "-m", MODEL, "-n", N_PREDICT, [optional -t/-ngl], ...ARGi..., "-p", prompt, NULL }
 * with N_PREDICT from CREATION_OS_BITNET_N_PREDICT or "128".  Set
 * CREATION_OS_BITNET_PERF=1 to omit "--no-perf".  Do not duplicate "-m" in
 * CREATION_OS_BITNET_ARG* when using CREATION_OS_BITNET_MODEL.
 *
 * Env overrides:
 *   CREATION_OS_BITNET_ARG0..CREATION_OS_BITNET_ARG9 — optional extra argv slots BEFORE "-p"
 *     (set to non-empty strings; unused slots ignored)
 *   CREATION_OS_BITNET_STDIN=1 — write prompt to child stdin instead of passing as argv
 */
#ifndef BITNET_SPAWN_H
#define BITNET_SPAWN_H

#include <stddef.h>

/** Capture stdout from `exe` into `out` (NUL-terminated). Returns 0 on success, child exit 0. */
int cos_bitnet_spawn_capture(const char *exe, const char *prompt, char *out, size_t cap);

/** Run argv[0] with argv NULL-terminated; capture stdout (and stderr if merge_stderr) into out. */
int cos_bitnet_spawn_argv_capture(char *const argv[], char *out, size_t cap, int merge_stderr);

#endif /* BITNET_SPAWN_H */
