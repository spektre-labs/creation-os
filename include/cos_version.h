/*
 * Creation OS — canonical version header (PROD-6).
 *
 * Every binary that wants to report a Creation-OS-wide release
 * identity (not a per-kernel `vNN` tag) includes this header and
 * prints COS_VERSION_STRING + COS_CODENAME + COS_TAGLINE.
 *
 * Semantic Versioning 2.0.0.
 *
 * Build-time consistency:
 *   scripts/check_version.sh diffs this header against
 *   CHANGELOG.md's latest entry and the Git tag, refusing to
 *   tag a release that drifts.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_VERSION_H
#define COS_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

#define COS_VERSION_MAJOR   1
#define COS_VERSION_MINOR   0
#define COS_VERSION_PATCH   0

#define COS_VERSION_STRING  "1.0.0"
#define COS_CODENAME        "Genesis"
#define COS_RELEASE_DATE    "2026-04-19"
#define COS_TAGLINE         "assert(declared == realized)"

#define COS_SIGMA_PRIMITIVES  20
#define COS_CHECK_TARGETS     38
#define COS_SUBSTRATES         4
#define COS_FORMAL_PROOFS      3   /* discharged (3/6)                 */
#define COS_FORMAL_PROOFS_TOTAL 6

/* Canonical one-line banner, for `cos --version`. */
#define COS_VERSION_BANNER \
    "Creation OS v" COS_VERSION_STRING " \"" COS_CODENAME "\" (" COS_RELEASE_DATE ")"

#ifdef __cplusplus
}
#endif

#endif /* COS_VERSION_H */
