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
 * Swapping the stub for a real BitNet bridge only touches the two
 * function pointers in cos_pipeline_config_t.  The rest of the
 * CLI code is unchanged.
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

#ifdef __cplusplus
}
#endif

#endif /* COS_CLI_STUB_GEN_H */
