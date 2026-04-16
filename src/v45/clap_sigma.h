/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v45 lab: internal activation probe placeholder (“σ_internal”) — no engine hooks in-tree yet.
 */
#ifndef CREATION_OS_V45_CLAP_SIGMA_H
#define CREATION_OS_V45_CLAP_SIGMA_H

#include <stdint.h>

enum { V45_MAX_LAYERS = 32 };

typedef struct {
    float attention_entropy_per_layer[V45_MAX_LAYERS];
    float hidden_state_norm_per_layer[V45_MAX_LAYERS];
    float layer_agreement;
    float sigma_internal;
} v45_internal_probe_t;

/** Deterministic toy fill for `n_layers` in [1, V45_MAX_LAYERS]. */
void v45_probe_internals_lab(uint32_t seed, int n_layers, v45_internal_probe_t *out);

#endif /* CREATION_OS_V45_CLAP_SIGMA_H */
