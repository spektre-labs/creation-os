/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v138 σ-Proof — ACSL annotation shape validator + Frama-C hook.
 *
 *   Tier 1  (always runs, pure C): validate_acsl() parses ACSL
 *           annotation blocks (the "slash-star-at ... star-slash"
 *           comments) out of a σ-gate source file and asserts:
 *
 *             - every annotated function block has at least one
 *               `requires` predicate,
 *             - every annotated function has either `ensures` OR
 *               a `behavior ... ensures ...` stanza,
 *             - at least one σ-domain predicate of the form
 *               `0.0f <= sigma <= 1.0f` appears,
 *             - the partition is total: both `emit` and `abstain`
 *               behaviours (or their return-value equivalents) are
 *               discoverable in the file.
 *
 *   Tier 2  (opportunistic): try_frama_c() runs `frama-c -wp` on the
 *           same file when the binary is on $PATH.  If Frama-C isn't
 *           installed, the function returns COS_V138_PROOF_SKIPPED
 *           and the merge-gate reports "tier-2 skipped (frama-c not
 *           found)".  The tier-1 result is authoritative.
 *
 * This two-tier pattern mirrors v123's TLA+ integration: bounded,
 * always-green structural check first; unbounded external prover
 * wired in where the environment supports it.
 */
#ifndef COS_V138_PROOF_H
#define COS_V138_PROOF_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COS_V138_PROOF_OK        = 0,
    COS_V138_PROOF_FAIL      = 1,
    COS_V138_PROOF_SKIPPED   = 2,
} cos_v138_result_t;

typedef struct cos_v138_report {
    int n_annotations;
    int n_with_requires;
    int n_with_ensures;
    int has_domain_predicate;     /* found 0.0f <= x <= 1.0f              */
    int has_emit_behavior;        /* return 0 / behavior "emit"           */
    int has_abstain_behavior;     /* return 1 / behavior "abstain"        */
    int has_loop_invariant;       /* loop invariant clause present        */
    int has_disjoint_behaviors;   /* disjoint behaviors keyword           */
    char last_error[256];
} cos_v138_report_t;

cos_v138_result_t cos_v138_validate_acsl(const char *path,
                                         cos_v138_report_t *rep);

/* Tier 2: attempt Frama-C WP.  Returns SKIPPED on missing tool. */
cos_v138_result_t cos_v138_try_frama_c(const char *path,
                                       char *stdout_buf, size_t cap);

int cos_v138_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
