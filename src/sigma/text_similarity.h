/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#ifndef COS_TEXT_SIMILARITY_H
#define COS_TEXT_SIMILARITY_H

#include <stddef.h>

/* Jaccard similarity between two text strings (unique word sets).
 * Tokenizes by whitespace on normalized text (lowercase, alnum + space).
 * Returns 0.0 (no overlap) to 1.0 (identical sets). */
float cos_text_jaccard(const char *a, const char *b);

/* Normalize: lowercase, collapse whitespace, strip non-alphanumeric
 * (except space). Writes to out; NUL-terminated; caps at out_len bytes. */
void cos_text_normalize(const char *in, char *out, int out_len);

/* Copy the first sentence: up to first '.', '?', '!', or line break.
 * If none within 100 bytes, copy at most 100 chars. NUL-terminated. */
void cos_text_first_sentence(const char *in, char *out, size_t out_cap);

/* Runs built-in assertions; prints failures to stderr. */
void cos_text_similarity_self_test(void);

/* Returns 0 if all checks pass, else number of failures. */
int cos_text_similarity_self_check(void);

#endif
