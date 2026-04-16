/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v44 lab: SSE / NDJSON helpers for per-token σ streaming (format contract only).
 */
#ifndef CREATION_OS_V44_SIGMA_STREAM_H
#define CREATION_OS_V44_SIGMA_STREAM_H

#include "../sigma/decompose.h"

#include <stddef.h>

/** One SSE `data: {...}\\n\\n` line: token + compact σ. Returns bytes written or -1. */
int v44_stream_format_token_line(const char *token_utf8, const sigma_decomposed_t *s, char *out, size_t cap);

/** Alert payload (e.g. mid-stream retraction signal). Returns bytes written or -1. */
int v44_stream_format_sigma_alert(const char *alert, const char *action, char *out, size_t cap);

#endif /* CREATION_OS_V44_SIGMA_STREAM_H */
