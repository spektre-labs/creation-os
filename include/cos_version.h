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

#define COS_VERSION_MAJOR   3
#define COS_VERSION_MINOR   2
#define COS_VERSION_PATCH   0

#define COS_VERSION_STRING  "3.2.0"
#define COS_CODENAME        "HORIZON"
#define COS_RELEASE_DATE    "2026-04-22"
#define COS_TAGLINE         "assert(declared == realized)"

#define COS_SIGMA_PRIMITIVES  20
#define COS_CHECK_TARGETS     56   /* +check-swarm +check-sandbox over v3.1.0 */
#define COS_SUBSTRATES         4
#define COS_FORMAL_PROOFS      6   /* 6/6 discharged; Lean 4 core, zero sorry, zero Mathlib */
#define COS_FORMAL_PROOFS_TOTAL 6

/* Canonical one-line banner, for `cos --version`. */
#define COS_VERSION_BANNER \
    "Creation OS v" COS_VERSION_STRING " \"" COS_CODENAME "\" (" COS_RELEASE_DATE ")"

#ifdef __cplusplus
}
#endif

#endif /* COS_VERSION_H */
