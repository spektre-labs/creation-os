/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v123 σ-Formal — offline TLA+ model check harness for the Creation
 * OS runtime invariants.  The TLA spec lives at
 *    specs/v123/sigma_governance.tla
 * and declares seven named invariants: TypeOK, AbstainSafety,
 * EmitCoherence, MemoryMonotonicity, SwarmConsensus, NoSilentFailure,
 * ToolSafety.
 *
 * This C module is intentionally small: it *structurally validates*
 * the spec (parseable + declares every required invariant name +
 * CONSTANTS block + SPECIFICATION) so the merge-gate can enforce
 * "the σ-contract is well-formed" on every CI runner, even the ones
 * without Java or `tla2tools.jar`.  The full exhaustive model check
 * is run by benchmarks/v123/check_v123_formal_tlc.sh when
 * `tlc` or `TLA_TOOLS_JAR` are available; it SKIPs cleanly
 * otherwise.
 *
 * Rationale (doc audit item): v85 TLA-invariants were only checked at
 * runtime.  v123 closes that gap with an offline model-check path
 * and a CI-visible structural contract independent of tool install.
 */
#ifndef COS_V123_FORMAL_H
#define COS_V123_FORMAL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V123_SPEC_PATH_DEFAULT   "specs/v123/sigma_governance.tla"
#define COS_V123_CFG_PATH_DEFAULT    "specs/v123/sigma_governance.cfg"

typedef struct cos_v123_validation {
    int spec_exists;
    int cfg_exists;
    int has_module_header;
    int has_specification;
    int has_constants_block;
    int has_vars_tuple;
    int invariants_declared;  /* bitmask: bit i → kRequiredInvariants[i] */
    int missing_mask;
} cos_v123_validation_t;

extern const char *const COS_V123_REQUIRED_INVARIANTS[7];

/* Validate `spec_path` (TLA+) against the σ-governance contract.
 * Returns 0 when all seven invariants are declared and the spec is
 * well-formed; otherwise returns -1 and populates `missing_mask`.
 * `cfg_path` is optional — pass NULL to skip .cfg validation. */
int cos_v123_validate(const char *spec_path, const char *cfg_path,
                      cos_v123_validation_t *out);

/* JSON summary; returns bytes written or negative on overflow. */
int cos_v123_validation_to_json(const cos_v123_validation_t *v,
                                char *out, size_t cap);

/* Pure-C self-test: runs validation on the in-tree spec and asserts:
 *   - module header present
 *   - SPECIFICATION keyword present
 *   - CONSTANTS block present
 *   - vars tuple declared
 *   - all seven invariants declared by name
 * Returns 0 on pass. */
int cos_v123_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
