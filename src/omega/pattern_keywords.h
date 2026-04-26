/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * Lightweight prompt → domain bucket (no I/O; safe for cos-think).
 */
#ifndef COS_OMEGA_PATTERN_KEYWORDS_H
#define COS_OMEGA_PATTERN_KEYWORDS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void cos_pattern_keyword_domain(const char *prompt, char *dom, size_t dcap);

#ifdef __cplusplus
}
#endif
#endif /* COS_OMEGA_PATTERN_KEYWORDS_H */
