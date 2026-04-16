/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v53 project-context loader — minimal `creation.md` parser (scaffold).
 *
 * Parses a tiny subset of creation.md: it counts the "Invariants"
 * section bullets and the "Conventions" section bullets, and extracts
 * the σ-profile table rows into (task_type, sigma_ceiling) pairs.
 *
 * This is deliberately conservative — no regex, no full markdown parse,
 * no external dependencies. It's enough for a harness to load the
 * invariants and refuse to start if the file is missing or malformed.
 */
#ifndef CREATION_OS_V53_HARNESS_PROJECT_CONTEXT_H
#define CREATION_OS_V53_HARNESS_PROJECT_CONTEXT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define V53_TASK_NAME_MAX 24

typedef struct {
    char  task[V53_TASK_NAME_MAX];
    float sigma_ceiling;
} v53_sigma_profile_row_t;

typedef struct {
    int   invariants_count;
    int   conventions_count;
    int   sigma_profile_rows;
    v53_sigma_profile_row_t sigma_profile[8];
    int   ok; /* 1 = file found + non-empty; 0 = missing / empty */
} v53_project_context_t;

/* Load creation.md from the given path. Never allocates — all parsing
 * is zero-copy against a caller-provided buffer. Returns `.ok = 0` on
 * any error (missing file, unreadable, empty). */
void v53_load_project_context(const char *path, v53_project_context_t *out);

/* Parse an already-loaded buffer (UTF-8). `len` in bytes. */
void v53_parse_project_context(const char *buf, size_t len,
                               v53_project_context_t *out);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V53_HARNESS_PROJECT_CONTEXT_H */
