/*
 * Digital twin — pre-execution precondition checks (HORIZON-2).
 *
 * Before running a mutating shell command, inspect the filesystem:
 * source existence + size, destination parent writability, free
 * space (statvfs), optional non-blocking flock on the source.
 * Aggregate failed checks into σ_twin ∈ [0,1] using a simple
 * discrete rule (0 failures → ~0, 1 → 0.5, 2+ → 0.9).
 *
 * v0 scope: `cp SRC DST` (flags like -r are skipped heuristically).
 * Other verbs return σ_twin ≈ 0.45 with a "unparsed" notice so the
 * caller can fall back to manual review or future parsers.
 *
 * This is not a full symbolic executor — it is a cheap latent check
 * that catches the common "copy missing file" class before side
 * effects.  See CLAIM_DISCIPLINE: no claim of completeness.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_DIGITAL_TWIN_H
#define COS_DIGITAL_TWIN_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Abort real execution when σ_twin >= this (cos-exec default). */
#define COS_TWIN_EXECUTE_MAX_SIGMA 0.50f

typedef struct {
    int   n_failed;      /* hard precondition failures              */
    int   n_warnings;    /* e.g. flock contention                   */
    float sigma_twin;    /* aggregated uncertainty                  */
    int   parsed_cp;     /* 1 if cp-specific logic ran              */
    char  dest_resolved[512];
} cos_digital_twin_report_t;

/* Simulate one shell line.  Emits human lines to `out` (may be
 * stdout).  Fills `rep`.  Returns 0 on success, -1 on empty line. */
int cos_digital_twin_simulate(const char *shell_cmd, FILE *out,
                              cos_digital_twin_report_t *rep);

/* Deterministic self-test (temp files under /tmp).  Returns 0 on OK. */
int cos_digital_twin_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_DIGITAL_TWIN_H */
