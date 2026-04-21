/* Shared stub generator for the reference CLIs.
 *
 * Creation OS ships the control plane (σ-pipeline orchestrator +
 * 15 primitives).  The reference CLIs (cos_chat, cos_benchmark,
 * cos_cost) exercise that control plane with a deterministic
 * stub that maps prompt-prefix keywords to scripted (text, σ)
 * tuples:
 *
 *   "low:<rest>"       → σ=0.05       (ACCEPT)
 *   "mid:<rest>"       → σ=0.55       (plateau → RETHINK ×3 → ABST)
 *   "high:<rest>"      → σ=0.92       (immediate ABSTAIN)
 *   "improve:<rest>"   → σ=0.55/0.45/0.35   (RETHINK → ACCEPT)
 *   anything else      → σ=0.30       (RETHINK → ACCEPT)
 *
 * Escalation returns σ=0.08 at €0.012 per call.
 *
 * `cos_cli_chat_generate()` is the **chat** entry point: optional
 * `CREATION_OS_BITNET_EXE` subprocess (see `bitnet_spawn.h`), then
 * the canonical arithmetic demo prompt **What is 2+2?** → `"4"`
 * with low σ, then the prefix stub rules above.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_CLI_STUB_GEN_H
#define COS_CLI_STUB_GEN_H

#include "pipeline.h"

#ifdef __cplusplus
extern "C" {
#endif

int cos_cli_stub_generate(const char *prompt, int round, void *ctx,
                          const char **out_text, float *out_sigma,
                          double *out_cost_eur);

int cos_cli_stub_escalate(const char *prompt, void *ctx,
                          const char **out_text, float *out_sigma,
                          double *out_cost_eur,
                          uint64_t *out_bytes_sent,
                          uint64_t *out_bytes_recv);

int cos_cli_chat_generate(const char *prompt, int round, void *ctx,
                          const char **out_text, float *out_sigma,
                          double *out_cost_eur);

#ifdef __cplusplus
}
#endif

#endif /* COS_CLI_STUB_GEN_H */
