/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* ULTRA-8 — unified continuous-learning counters (demo / offline ledger). */

#ifndef COS_ULTRA_CONTINUOUS_LEARN_H
#define COS_ULTRA_CONTINUOUS_LEARN_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void cos_ultra_continuous_emit_status(FILE *fp);

int cos_ultra_continuous_learn_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
