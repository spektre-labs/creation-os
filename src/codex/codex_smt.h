/*
 * Codex constraints — pattern layer (SMT-style naming, no libz3).
 *
 * Full SMT / Z3 integration is out of scope here; this unit forwards to
 * the existing constitutional rule engine (string + σ-aware checks).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_CODEX_CODEX_SMT_H
#define COS_CODEX_CODEX_SMT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Run constitutional checks (pattern / rule layer).
 *
 * @return 0 on success (violations may still be >0).
 */
int cos_codex_smt_evaluate(const char *response, float sigma,
                           char *detail, size_t detail_cap, int *violations_out,
                           int *mandatory_violations_out);

int cos_codex_smt_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_CODEX_CODEX_SMT_H */
