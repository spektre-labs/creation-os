/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* AGI-4 / AGI-6 — Ω-loop live entry points (--daemon added in AGI-6). */

#ifndef COS_SIGMA_OMEGA_AGI_LIVE_H
#define COS_SIGMA_OMEGA_AGI_LIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Returns -1 if argv does not request an AGI Ω mode; otherwise the
 * process exit code (0–255). */
int cos_agi_omega_live_dispatch(int argc, char **argv);

#ifdef __cplusplus
}
#endif
#endif
