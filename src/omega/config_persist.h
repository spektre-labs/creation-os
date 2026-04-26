/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * Persist Ω / GVU runtime fields to ~/.cos/config.json (auditable JSON).
 */
#ifndef COS_OMEGA_CONFIG_PERSIST_H
#define COS_OMEGA_CONFIG_PERSIST_H

#include "evolver.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cos_omega_runtime_config {
    cos_evolver_config_t ev;
    char                 preferred_model[96];
    int                  last_episode;
    float                last_kappa;
    int                  ignition_count;
} cos_omega_runtime_config_t;

/** Defaults + optional merge from ~/.cos/config.json (missing keys ignored). */
void cos_omega_runtime_config_default(cos_omega_runtime_config_t *c);

/** Read ~/.cos/config.json into `c` (already defaulted). Returns 0 or -1. */
int cos_omega_runtime_config_load(cos_omega_runtime_config_t *c);

/** Atomic write ~/.cos/config.json. Returns 0 on success. */
int cos_omega_runtime_config_save(const cos_omega_runtime_config_t *c);

#ifdef __cplusplus
}
#endif
#endif /* COS_OMEGA_CONFIG_PERSIST_H */
