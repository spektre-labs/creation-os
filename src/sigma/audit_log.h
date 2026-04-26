/* Append-only JSONL audit log with optional hash chain (prev line digest).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_AUDIT_LOG_H
#define COS_AUDIT_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Minimal row (Step 3 API).  Omits preview/codex/temperature; chain_prev still set. */
void cos_audit_write(const char *audit_id, const char *prompt_hash, float sigma,
                     float sigma_calibrated, const char *action, const char *model,
                     const char *response_hash, int latency_ms, float tau_accept,
                     float tau_rethink, int constitutional_halt);

/* Full cos-serve row: preview snippet, codex hash, temperature.  Returns 0 on success. */
int cos_audit_append_serve_row(const char *audit_id, const char *prompt_hash,
                               const char *input_preview, double sigma,
                               double sigma_calibrated, const char *action,
                               const char *model, const char *response_hash,
                               double latency_ms, float temperature,
                               float tau_accept, float tau_rethink,
                               int constitutional_halt, const char *codex_hash);

#ifdef __cplusplus
}
#endif
#endif
