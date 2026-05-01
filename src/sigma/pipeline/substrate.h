/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Substrate — single vtable unifying every σ-gate backend.
 *
 * The claim of the H-series is that the σ-gate is substrate-free:
 * given logits / activations / intensities / spikes, the ACCEPT
 * or ABSTAIN decision is the same bit regardless of the medium.
 * σ-Substrate is the contract that makes the pipeline believe it.
 *
 * One interface:
 *
 *     typedef struct {
 *         const char *name;
 *         float  (*measure)(const float *activations, int n, void *ctx);
 *         int    (*gate)   (float sigma, float tau_accept);
 *         int    (*self_test)(void);
 *     } cos_sigma_substrate_t;
 *
 * Four bundled implementations:
 *
 *   SUBSTRATE_DIGITAL   — softmax-style 1 − max / Σ, standard
 *                         C path.  This is the reference.
 *   SUBSTRATE_BITNET    — 1-bit {−1, +1} encoding → max / sum
 *                         over absolute activations (σ collapses
 *                         to the digital answer modulo quantiser).
 *   SUBSTRATE_SPIKE     — wraps cos_sigma_spike_population_sigma
 *                         over a population of LIF neurons.
 *   SUBSTRATE_PHOTONIC  — wraps cos_sigma_photonic_sigma_from_
 *                         intensities.
 *
 * The pipeline never talks to the backends directly; it takes a
 * pointer-to-vtable.  Switching hardware is a single assignment.
 *
 * Contract: every backend must reach the SAME ACCEPT / ABSTAIN
 * decision when fed the canonical calibration vectors in
 * substrate_self_test().  That is the "Design once, run
 * anywhere" guarantee reduced to a runtime assertion.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SUBSTRATE_H
#define COS_SIGMA_SUBSTRATE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COS_SUB_DIGITAL  = 0,
    COS_SUB_BITNET   = 1,
    COS_SUB_SPIKE    = 2,
    COS_SUB_PHOTONIC = 3,
    COS_SUB__COUNT
} cos_substrate_kind_t;

typedef struct cos_sigma_substrate_s cos_sigma_substrate_t;

struct cos_sigma_substrate_s {
    const char *name;
    cos_substrate_kind_t kind;
    /* Compute σ for a vector of non-negative activations /
     * intensities / spike-counts of length n.  Return value is
     * clamped to [0, 1]. */
    float (*measure)(const float *activations, int n, void *ctx);
    /* ACCEPT if σ < tau_accept, ABSTAIN otherwise.  Backends
     * should not override this unless they need hysteresis or
     * photonic bistability; the default gate_default is provided
     * for convenience. */
    int   (*gate)(float sigma, float tau_accept);
    int   (*self_test)(void);
    void *ctx;
};

extern cos_sigma_substrate_t COS_SUBSTRATE_DIGITAL;
extern cos_sigma_substrate_t COS_SUBSTRATE_BITNET;
extern cos_sigma_substrate_t COS_SUBSTRATE_SPIKE;
extern cos_sigma_substrate_t COS_SUBSTRATE_PHOTONIC;

/* Look up by kind (NULL on out-of-range). */
cos_sigma_substrate_t *cos_sigma_substrate_lookup(cos_substrate_kind_t k);
const char            *cos_sigma_substrate_name(cos_substrate_kind_t k);

/* Default gate: 1 if sigma < tau_accept, 0 otherwise. */
int cos_sigma_substrate_default_gate(float sigma, float tau_accept);

/* Equivalence check: feed the same activation vector to all four
 * backends and assert they reach the same ACCEPT/ABSTAIN bit for
 * a given tau_accept.  Returns 0 on success; otherwise a code
 * identifying the disagreeing backend. */
int cos_sigma_substrate_equivalence(const float *activations, int n,
                                    float tau_accept,
                                    float out_sigma_per_kind[COS_SUB__COUNT],
                                    int   out_gate_per_kind [COS_SUB__COUNT]);

int cos_sigma_substrate_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_SUBSTRATE_H */
