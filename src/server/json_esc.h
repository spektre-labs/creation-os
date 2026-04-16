/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef JSON_ESC_H
#define JSON_ESC_H

#include <stddef.h>

/** Escape `in` for JSON string contents into `out`. Returns byte length (excluding NUL) or -1. */
int cos_json_escape_cstr(const char *in, char *out, size_t cap);

#endif /* JSON_ESC_H */
