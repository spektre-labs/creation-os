/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * GVU operator (Generator / Verifier / Updater) + κ (accuracy gain) audit.
 */
#ifndef COS_OMEGA_GVU_LOOP_H
#define COS_OMEGA_GVU_LOOP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Run GVU for `n_episodes` × `episode_len` turns each.
 * `port` > 0 overrides COS_BITNET_SERVER_PORT for this run.
 * `simulate`: no HTTP — deterministic stub (CI / offline).
 */
int cos_gvu_run(int n_episodes, int episode_len, int port, int verbose,
                int simulate);

int cos_gvu_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_OMEGA_GVU_LOOP_H */
