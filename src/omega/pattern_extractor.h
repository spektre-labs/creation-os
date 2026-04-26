/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * Domain buckets from graded prompts + episodic σ (ATLAS-style pamphlet
 * input). Writes ~/.cos/patterns.json unless path overridden.
 */
#ifndef COS_OMEGA_PATTERN_EXTRACTOR_H
#define COS_OMEGA_PATTERN_EXTRACTOR_H

#ifdef __cplusplus
extern "C" {
#endif

/** Scan recent engram episodes, join graded CSV by prompt hash, emit JSON.
 *  Returns 0 on success, -1 on I/O failure (partial write possible). */
int cos_pattern_extract_write(const char *path_out);

#ifdef __cplusplus
}
#endif

#endif /* COS_OMEGA_PATTERN_EXTRACTOR_H */
